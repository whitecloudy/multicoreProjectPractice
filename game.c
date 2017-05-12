#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "./game.h"
#include "./list.h"
#include "./lcgrand.h"

#define DEAD 0x0000
#define LIVE 0x8000
#define PLAGUE 0x2000
#define CHANGE 0x4000
#define DEVIL_FULL 0x07FF
#define DEVIL_MOVEIN 0x1000
#define MAP_LOCK 0x0800

#define NumOfDevil(x) (x&DEVIL_FULL)
#define IsPlague(x) ((x&PLAGUE)==PLAGUE)
#define IsChange(x) ((x&CHANGE)==CHANGE)
#define IsLive(x) (((x&LIVE)==LIVE)&&(!(IsPlague(x))))
#define IsDead(x) (((x&LIVE)==DEAD)&&(!(IsPlague(x))))
#define IsDevilFull(x) ((x&DEVIL_FULL) == DEVIL_FULL)
#define IsDevilMoveIn(x) ((x&DEVIL_MOVEIN)==DEVIL_MOVEIN)
#define IsMapLock(x) ((x&MAP_LOCK)==MAP_LOCK)
#define EraseDevil(x) (x&0xF800)
#define AngelCure(x) (x&0xC800)

struct unit
{
	int x;
	int y;
	int z;
};

struct MAP{
	unsigned short d;
};

struct devil
{
	struct unit u;
	struct list_elem el;
};

struct map_mutex
{
	struct unit u;
	pthread_mutex_t lock;
	struct list_elem el;
};

struct MAP *** map;
struct unit angel;
struct list devil_list;
struct list map_mutex_list;
pthread_mutex_t mutex_list_lock;

int devil_size=0;
pthread_mutex_t devil_size_lock;

int start = 0;
int totalTaskSize = 0;
int avgTaskSize = 0;
pthread_t * thread;
pthread_key_t key;
pthread_mutex_t uniform_lock;
pthread_mutex_t mainThreadLock;
int * thread_result;
struct unit m;

int xP;
int xM;
int yP;
int yM;
int zP;
int zM;

int threadDone = 0;
pthread_mutex_t done_mutex;

pthread_mutex_t angelCheck;

pthread_mutex_t * thread_mutex;

pthread_cond_t devilMoveCond;
pthread_cond_t devilMoveConfirmCond;
pthread_cond_t devilCopyCond;
pthread_cond_t cellCheckCond;
pthread_cond_t cellConfirmCond;
pthread_cond_t angelSearchCond;
pthread_cond_t threadDoneCond;


/***************************************
 * 3차원 메모리를 동적할당
 * num : 한변 길이
 * size : 한 block의 데이터 크기
 * **************************************/
void *** malloc_3d(int num,int size)
{
	void *** data;
	int i, j;
	data = (void***)malloc(sizeof(void**)*num);
	data[0] = (void**)malloc(sizeof(void*)*num*num);
	data[0][0] = (void*)malloc(size*num*num*num);
	for (i = 1; i < num; i++)
	{
		data[i] = data[i - 1] + num;
		data[i][0] = (unsigned long)data[i - 1][0] + (unsigned long)(num*num*size);
	}
	for (i = 0; i < num; i++)
	{
		for (j = 1; j < num; j++)
		{
			data[i][j] = (unsigned long)data[i][j - 1] + (unsigned long)(num*size);
		}
	}
	return data;
}

/*********************************************
 * 3차원 메모리 동적할당 해제
 * ******************************************/
void free_3d(void *** data)
{
	free(data[0][0]);
	free(data[0]);
	free(data);
}

void mapLock(struct MAP * cor)
{
	while(IsMapLock(cor->d))
		continue;
	cor->d ^= MAP_LOCK;
}

void mapUnlock(struct MAP * cor)
{
	cor->d &= 0xF7FF;
}


/************************************************
 * Devil의 생성 및 초기화
 * s : 해당 setup
 *
 * return : 데빌의 위치
 * *********************************************/
struct unit devil_init(struct setup *s)
{
	struct unit d;
	struct devil * dev;
	//devil의 초기 위치 설정
	d.x = uniform(0,s->map_size-1,s->SEED_DVL_GEN_X);
	d.y = uniform(0,s->map_size-1,s->SEED_DVL_GEN_Y);
	d.z = uniform(0,s->map_size-1,s->SEED_DVL_GEN_Z);

	//devil 맵 입력
	if(IsDevilFull(map[d.x][d.y][d.z].d))	//데빌 넣을 공간이 없다면
	{
		dev = (struct devil*)malloc(sizeof(struct devil));
		dev->u.x = d.x;
		dev->u.y = d.y;
		dev->u.z = d.z;
		list_push_back(&devil_list,&(dev->el));
	}else
		map[d.x][d.y][d.z].d += 1;

	//devil 사이즈 증가
	devil_size++;

	return d;
}

/************************************************
 * Devil의 복사용
 * s : 해당 setup
 * *********************************************/
void devil_copy(struct setup *s)
{
	struct unit d;
	struct devil * dev;
	//devil의 초기 위치 설정
	pthread_mutex_lock(&uniform_lock);
	d.x = uniform(0,s->map_size-1,s->SEED_DVL_GEN_X);
	d.y = uniform(0,s->map_size-1,s->SEED_DVL_GEN_Y);
	d.z = uniform(0,s->map_size-1,s->SEED_DVL_GEN_Z);
	pthread_mutex_unlock(&uniform_lock);

	mapLock(&map[d.x][d.y][d.z]);
	//devil 맵 입력
	if(IsDevilFull(map[d.x][d.y][d.z].d))	//데빌 넣을 공간이 없다면
	{
		dev = (struct devil*)malloc(sizeof(struct devil));
		dev->u.x = d.x;
		dev->u.y = d.y;
		dev->u.z = d.z;
		list_push_back(&devil_list,&(dev->el));
	}else
		map[d.x][d.y][d.z].d += 1;
	mapUnlock(&map[d.x][d.y][d.z]);

}



/*************************************************
 *  * Devil을 제거하는 함수
 *   * 제거후 다음 devil값을 반환
 *    * ***********************************************/
struct devil * devil_list_remove(struct devil *d)
{
	struct devil * next = list_entry(list_next(&(d->el)),struct devil,el);
	list_remove(&(d->el));
	free(d);
	devil_size--;
	return next;
}

/*************************************************
 * 어떤 좌표계의 이동
 * s : 해당 setup
 * x, y, z : 입력받은 각 좌표별 이동 거리
 * cor : 이동해야하는 좌표계
 * ***********************************************/
void unit_mov(struct setup * s, const int x, const int y, const int z, struct unit * cor)
{
	cor->x += x;
	cor->y += y;
	cor->z += z;

	//각 좌표별로 테두리를 검사
	if(cor->x >= s->map_size)
	{
		cor->x = s->map_size - 1;
	}else if(cor->x < 0)
	{
		cor->x = 0;
	}

	if(cor->y >= s->map_size)
	{
		cor->y = s->map_size - 1;
	}else if(cor->y < 0)
	{
		cor->y = 0;
	}

	if(cor->z >= s->map_size)
	{
		cor->z = s->map_size - 1;
	}else if(cor->z < 0)
	{
		cor->z = 0;
	}
}



/**********************************************************************************
 * 입력받은 좌표의 셀 주위를 확인한 후 live or dead를 결정 혹은 플래그 스테이지
 * ********************************************************************************/
void cell_check(struct setup * s, int x, int y, int z, int * leftCount, int * middleCount, char * leftP, char * middleP)
{
	int i,j,k;
	int xs,ys,zs;
	int p,q,r;
	int count = 0;
	char P = false;

	//테두리 체크
	if(x==0)
		xs=0;
	else
		xs=-1;
	if(x==s->map_size-1)
		p=0;
	else
		p=1;
	if(y==0)
		ys=0;
	else
		ys=-1;
	if(y==s->map_size-1)
		q=0;
	else
		q=1;
	if(z==0)
		zs=0;
	else
		zs=-1;
	if(z==s->map_size-1)
		r=0;
	else
		r=1;

	//첫번째 자리인 경우
	if(((*leftCount)==-1)&&((*middleCount)==-1))
	{
		*leftCount = 0;
		*middleCount = 0;
		//이웃 셀 중 LIVE인 것들을 체크
		for(k=zs;k<=r;k++)
		{
			for(j=ys;j<=q;j++)
			{
				for(i=xs;i<=p;i++)
				{
					//라이브 셀 카운트
					if(IsLive(map[x+i][y+j][z+k].d))
					{
						if(k==0)
							(*leftCount)++;
						else if(k==1)
							(*middleCount)++;
						if(i|j|k != 0)//자기자신은 뺀다
							count++;
					}else if(IsPlague(map[x+i][y+j][z+k].d))	//주위에 플래그가 있나?
					{
						if(k==0)
							(*leftP) = true;
						else if(k==1)
							(*middleP) = true;
						P = true;
					}
				}
			}
		}

	}
	//첫번째 계산 이후
	else
	{
		count = *leftCount+*middleCount;	//이전 계산값들 추가
		P = (*leftP)||(*middleP);
		//다음 타일 계산값 준비
		*leftCount = *middleCount;
		*middleCount = 0;
		*leftP = *middleP;
		*middleP = false;

		if(IsLive(map[x][y][z].d))	//자기 자신은 뺀다
			count--;

		if(r==1)
		{
			for(j=ys;j<=q;j++)
			{
				for(i=xs;i<=p;i++)
				{
					//라이브 셀 카운트
					if(IsLive(map[x+i][y+j][z+1].d))
					{
						(*middleCount)++;
					}else if(IsPlague(map[x+i][y+j][z+1].d))	//주위에 플래그가 있나?
						*middleP = true;
				}
			}
			count += *middleCount;
			P = P||(*middleP);
		}
	}

	if(IsDead(map[x][y][z].d))	//DEAD일 경우
	{
		//카운트 된 것 확인 후 만약 조건 부합시 DEAD_TO_LIVE로
		if((count > s->live_min)&&(count < s->live_max))
		{
			map[x][y][z].d ^= CHANGE;
		}
	}else if(IsLive(map[x][y][z].d))	//LIVE일 경우
	{			
		//카운트 된 것 확인 후 만약 조건 부합시 LIVE_TO_DEAD로
		if((count > s->dead_max)||(count < s->dead_min))
		{
			map[x][y][z].d ^= CHANGE;
		}
	}

	if(P&&(!(IsPlague(map[x][y][z].d))))	//주위에 플래그가 있고 이 타일이 플래그가 아니라면 change
		map[x][y][z].d ^= CHANGE;
}

void cellCheckTask(const struct setup * s, const int start, const int size)
{
	int i,j;
	int x,y,z;
	int leftCount, middleCount;
	bool leftP, middleP;

	for(i=start; i<start+size;i++)
	{
		leftCount = -1;
		middleCount = -1;
		leftP = false;
		middleP = false;
		for( j=0 ; j<s->map_size ; j++ )
		{
			x = i/s->map_size;
			y = i%s->map_size;
			z = j;

			cell_check(s,x,y,z,&leftCount,&middleCount,&leftP,&middleP);

		}
	}

}

void cellConfirm(const struct setup * s,const int start, const int size)
{
	int i,j;

	for(i=start ; i<(start+size) ; i++ )
	{
		for( j=0 ; j<s->map_size ; j++ )
		{
			if(IsChange(map[0][i][j].d))
			{
				map[0][i][j].d ^= LIVE;	//live는 dead로 dead 는 live로
				map[0][i][j].d ^= CHANGE;	//Change flag 제거
			}
		}
	}
}


void angelSearchTask(const struct setup * s,const int start, const int size)
{
	int i,j,num;
	int x,y,z;
	int tem_xP = 0, tem_xM = 0, tem_yP = 0, tem_yM = 0, tem_zP = 0, tem_zM = 0;

	for(i=start ; i<(start+size) ; i++ )
	{
		x = i/s->map_size;
		if(x == angel.x)	//엔젤 선상에 있는 데빌은 세지 않는다
			continue;
		y = i%s->map_size;
		if(y == angel.y)
			continue;
		for( j=0 ; j<s->map_size ; j++ )
		{
			z = j;
			if(z == angel.z)
				continue;

			num = NumOfDevil(map[0][i][j].d);
			if(num!=0)	//데빌 숫자가 0이 아니라면
			{
				if(x>angel.x)
					tem_xP += num;
				else
					tem_xM += num;

				if(y>angel.y)
					tem_yP += num;
				else
					tem_yM += num;

				if(z>angel.z)
					tem_zP += num;
				else
					tem_zM += num;
			}
		}
	}


	pthread_mutex_lock(&angelCheck);
	xP += tem_xP;
	xM += tem_xM;
	yP += tem_yP;
	yM += tem_yM;
	zP += tem_zP;
	zM += tem_zM;
	pthread_mutex_unlock(&angelCheck);
}


/*********************************************
 * 맵을 입력받은 파일에 저장하는 함수
 ******************************************/
void map_print(struct setup * s, FILE * save)
{
	int i,j,k;
	for(i=0;i<s->map_size;i++)
	{
		for(j=0;j<s->map_size;j++)
		{
			for(k=0;k<s->map_size;k++)
			{
				if(IsLive(map[i][j][k].d))
					fprintf(save,"L ");
				else if(IsDead(map[i][j][k].d))
					fprintf(save,"D ");
				else if(IsPlague(map[i][j][k].d))
					fprintf(save,"P ");
			}
			fprintf(save,"\n");
		}
		fprintf(save,"\n");
	}
}


/************************************************
 * 엔젤과 데빌의 위치를 파일에 저장하는 함수
 * *********************************************/
void pos_print(struct setup * s, FILE * save)
{
	struct devil * dev;
	int i;
	int x,y,z;
	int num;


	fprintf(save,"[Angel]\n");
	fprintf(save, "(%d, %d, %d)\n\n",angel.x, angel.y, angel.z);
	fprintf(save,"[Devil]\n");


	//devil 프린트
	for( x=0 ; x<s->map_size ; x++ )
	{
		for( y=0 ; y<s->map_size ; y++ )
		{
			for( z=0 ; z<s->map_size ; z++ )
			{
				for( i=0 ; i<(NumOfDevil(map[x][y][z].d)) ; i++ )
				{
					fprintf(save,"(%d, %d, %d)\n",x,y,z);
				}
				if(IsDevilFull(map[x][y][z].d))	//만약 데빌 칸이 가득찬 상태라면
				{
					dev = list_entry(list_begin(&devil_list),struct devil,el);
					while(!list_empty(&devil_list))
					{
						if((dev->u.x ==x)&&(dev->u.y==y)&&(dev->u.z==z))	//해당 맵 위치의 데빌이면 출력
						{
							fprintf(save,"(%d, %d, %d)\n",x,y,z);
						}
						if(list_end(&devil_list)==dev->el.next)	//만약 현 데빌이 마지막이라면 루프 탈출
							break;
						dev = list_entry(list_next(&(dev->el)),struct devil,el);	//다음 데빌 지목
					}
				}
			}
		}
	}
}

/***************************************************************
 * Devil의 이동을 입력받은 테스크 크기만큼 행하는 함수
 * x, y : 테스크 위치
 * size : 테스크 사이즈
 * **********************************************************/
void devilMoveZTask(const struct setup * s,const int start, const int size)
{
	struct unit tem;
	int i,j;

	for(i=start ; i<(start+size) ; i++ )
	{
		for( j=0 ; j<s->map_size ; j++ )
		{
			if(NumOfDevil(map[0][i][j].d)>0)	//devil이 있으면
			{

				tem.x = i/s->map_size;
				tem.y = i%s->map_size;
				tem.z = j;

				//이동
				unit_mov(s,m.x,m.y,m.z,&tem);

				//movein에 devil 넣기
				map[tem.x][tem.y][tem.z].d |= DEVIL_MOVEIN;
			}
		}
	}
}

void devilMoveConfirm(const struct setup * s,const int start, const int size)
{
	int i,j;
	int d_size = 0;

	for(i=start ; i<(start+size) ; i++ )
	{
		for( j=0 ; j<s->map_size ; j++ )
		{
			d_size -= NumOfDevil(map[0][i][j].d);	//해당 위치에 데빌 전부 제외
			map[0][i][j].d = EraseDevil(map[0][i][j].d);	//해당 위치의 데빌 전부 삭제

			if(IsDevilMoveIn(map[0][i][j].d))	//들어와야할 데빌이 존재하면
			{
				d_size ++;	//데빌 한마리 추가
				map[0][i][j].d+=1;	//해당 위치에 데빌 한마리 추가
				map[0][i][j].d ^= DEVIL_MOVEIN;	//무브인 0로 복귀
				map[0][i][j].d |= PLAGUE;	//해당위치 플래그
			}
		}
	}

	pthread_mutex_lock(&devil_size_lock);
	devil_size += d_size;
	pthread_mutex_unlock(&devil_size_lock);
}

void devilCopyTask(const struct setup * s,const int task_size)
{
	int i;

	for(i=0;i<task_size;i++)
	{
		devil_copy(s);
	}
}

static void key_destroy(void * buf)
{
	free(buf);
}

void allThreadLock(struct setup * s)
{
	int i;
	for(i=0;i<s->core_num;i++)
	{
		printf("2\n");
		pthread_mutex_lock(&(thread_mutex[i]));
		printf("2\n");
	}
}

void allThreadUnlock(struct setup * s)
{
	int i;
	for(i=0;i<s->core_num;i++)
	{
		printf("5\n");
		pthread_mutex_unlock(&(thread_mutex[i]));
		printf("6\n");
	}
}

void * thread_running(void * data)
{
	const struct setup * s = (struct setup *)((unsigned long *)data)[0];
	const int start = ((unsigned long *)data)[1];
	const int size = ((unsigned long *)data)[2];
	const pthread_mutex_t * dataMutex = (pthread_mutex_t*)((unsigned long*)data)[3];
	const unsigned int thread_num = ((unsigned long *)data)[4];

	int tem;

	pthread_mutex_unlock(dataMutex);

	pthread_mutex_t * threadMutex = &(thread_mutex[thread_num]);

	while(1)
	{
		printf("1\n");
		pthread_cond_wait(&devilMoveCond,threadMutex);
		printf("3\n");
		devilMoveZTask(s,start,size);
		pthread_mutex_lock(&done_mutex);
		threadDone++;
		if(threadDone == s->core_num)
		{
			pthread_mutex_lock(&mainThreadLock);
			pthread_cond_signal(&threadDoneCond);
			pthread_mutex_unlock(&mainThreadLock);
			threadDone = 0;
		}
		pthread_mutex_unlock(&done_mutex);
		
		pthread_cond_wait(&devilMoveConfirmCond,threadMutex);
		devilMoveConfirm(s,start,size);
		pthread_mutex_lock(&done_mutex);
		threadDone++;
		if(threadDone == s->core_num)
		{
			pthread_mutex_lock(&mainThreadLock);
			pthread_cond_signal(&threadDoneCond);
			pthread_mutex_unlock(&mainThreadLock);
			threadDone = 0;
		}
		pthread_mutex_unlock(&done_mutex);

		pthread_cond_wait(&devilCopyCond,threadMutex);
		if(thread_num < devil_size%(s->core_num))
			devilCopyTask(s,devil_size/(s->core_num) + 1);
		else
			devilCopyTask(s,devil_size/(s->core_num));
		pthread_mutex_lock(&done_mutex);
		threadDone++;
		if(threadDone == s->core_num)
		{
			pthread_mutex_lock(&mainThreadLock);
			pthread_cond_signal(&threadDoneCond);
			pthread_mutex_unlock(&mainThreadLock);
			threadDone = 0;
		}
		pthread_mutex_unlock(&done_mutex);

		pthread_cond_wait(&cellCheckCond,threadMutex);
		cellCheckTask(s,start,size);
		pthread_mutex_lock(&done_mutex);
		threadDone++;
		if(threadDone == s->core_num)
		{
			pthread_mutex_lock(&mainThreadLock);
			pthread_cond_signal(&threadDoneCond);
			pthread_mutex_unlock(&mainThreadLock);
			threadDone = 0;
		}
		pthread_mutex_unlock(&done_mutex);

		pthread_cond_wait(&cellConfirmCond,threadMutex);
		cellConfirm(s,start,size);
		pthread_mutex_lock(&done_mutex);
		threadDone++;
		if(threadDone == s->core_num)
		{
			pthread_mutex_lock(&mainThreadLock);
			pthread_cond_signal(&threadDoneCond);
			pthread_mutex_unlock(&mainThreadLock);
			threadDone = 0;
		}
		pthread_mutex_unlock(&done_mutex);

		pthread_cond_wait(&angelSearchTask,threadMutex);
		angelSearchTask(s,start,size);
		pthread_mutex_lock(&done_mutex);
		threadDone++;
		if(threadDone == s->core_num)
		{
			pthread_mutex_lock(&mainThreadLock);
			pthread_cond_signal(&threadDoneCond);
			pthread_mutex_unlock(&mainThreadLock);
			threadDone = 0;
		}
		pthread_mutex_unlock(&done_mutex);

	}

}

void thread_create(struct setup * s)
{
	int totalTaskSize = (s->map_size) * (s->map_size);
	int avgTaskSize = totalTaskSize/(s->core_num);
	int restTaskSize = totalTaskSize%(s->core_num);
	int taskSize=0;
	pthread_mutex_t dataMutex;
	unsigned long data[5];
	int i;


	pthread_mutex_init(&dataMutex,NULL);
	data[0] = (unsigned long)s;
	data[2] = (unsigned long)avgTaskSize;
	data[3] = (unsigned long)(&dataMutex);

	thread_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t)*(s->core_num));

	for( i=0 ; i<s->core_num ; i++ )
	{
		pthread_mutex_lock(&dataMutex);
		data[1] = taskSize;
		data[4] = i;
		if(i<restTaskSize)
		{
			data[2]++;
			pthread_create(&thread[i],NULL,thread_running,(void*)data);
			taskSize += data[2];
			data[2]--;
		}else
		{
			pthread_create(&thread[i],NULL,thread_running,(void*)data);
			taskSize += data[2];
		}
		pthread_mutex_init(&(thread_mutex[i]),NULL);
		pthread_mutex_lock(&(thread_mutex[i]));
	}
	pthread_mutex_lock(&dataMutex);
	pthread_cond_init(&threadDoneCond,NULL);
}

void thread_kill(struct setup * s)
{
	int i;

	for( i=0 ; i<s->core_num ; i++ )
	{
		pthread_cancel(thread[i]);
	}
}



//임의 정의 함수
/////////////////////////////////////////////////////////////////////////////////
//기존 정의 함수

void run_game (struct setup *s) {
	int i = 0;

	while(i++ < s->total_loop) {
		devil_stage(s);
		live_dead_stage(s);
		//plague_stage(s);
		angel_stage(s);
	}

	thread_kill(s);
}

void init_resources (struct setup *s) {
	int i,j,k;
	int p = (s->map_size/2)-1;
	struct unit tem;

	//맵 데이터 동적할당
	map = (struct MAP ***)malloc_3d(s->map_size,sizeof(struct MAP));

	//맵 이니셜라이징
	for(i = 0; i<s->map_size;i++)
	{ 
		for(j=0;j<s->map_size;j++)
		{ 
			for(k=0;k<s->map_size;k++)
			{
				map[i][j][k].d = (unsigned short)uniform(0,1,s->SEED_MAP)<<15;

			}
		}

	}
	//멀티쓰레딩 준비
	thread = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t)*(s->core_num));
	thread_result = (int * )malloc(sizeof(int)*(s->core_num));
	pthread_mutex_init(&devil_size_lock,NULL);
	totalTaskSize = s->map_size*s->map_size;
	avgTaskSize = totalTaskSize/(s->core_num);

	pthread_mutex_init(&uniform_lock,NULL);
	pthread_mutex_init(&mutex_list_lock,NULL);
	pthread_mutex_init(&mainThreadLock,NULL);
	pthread_mutex_init(&angelCheck,NULL);
	pthread_mutex_init(&done_mutex,NULL);

	pthread_cond_init(&devilMoveCond,NULL);
	pthread_cond_init(&devilMoveConfirmCond,NULL);
	pthread_cond_init(&devilCopyCond,NULL);
	pthread_cond_init(&cellCheckCond,NULL);
	pthread_cond_init(&cellConfirmCond,NULL);
	pthread_cond_init(&angelSearchCond,NULL);

	pthread_mutex_lock(&mainThreadLock);

	//angel 좌표 초기화
	angel.x = p;
	angel.y = p;
	angel.z = p;

	//새로운 devil생성
	tem = devil_init(s); 

	//해당 devil지역 plague화
	map[tem.x][tem.y][tem.z].d|=PLAGUE;

	//데빌 리스트 초기화
	list_init(&devil_list);
	list_init(&map_mutex_list);

	//쓰레드 생성
	thread_create(s);
}

void devil_stage (struct setup *s) {
	int i;
	struct unit tem;
	struct devil * dev = list_entry(list_begin(&devil_list),struct devil,el);

	if(devil_size==0)//devil이 하나도 없는 상황
	{

		//새로운 devil생성
		tem = devil_init(s); 

		//해당 devil지역 plague화
		map[tem.x][tem.y][tem.z].d|=PLAGUE;

		m.x = 0;
		m.y = 0;
		m.z = 0;

	}else
	{
		//중복devil을 저장한 devil_list 비우기
		while(!list_empty(&devil_list))
			dev = devil_list_remove(dev);

		//devil랜덤 이동 계산
		m.x=-(uniform(0,2,s->SEED_DVL_MOV_X)-1);
		m.y=-(uniform(0,2,s->SEED_DVL_MOV_Y)-1);
		m.z=-(uniform(0,2,s->SEED_DVL_MOV_Z)-1);
		
	}

	//데빌 이동 시작
	allThreadLock(s);
	pthread_cond_broadcast(&devilMoveCond);
	allThreadUnlock(s);

	//데빌 이동 종료
	pthread_cond_wait(&threadDoneCond,&mainThreadLock);

	//데빌 이동 확정 시작
	allThreadLock(s);
	pthread_cond_broadcast(&devilMoveConfirmCond);
	allThreadUnlock(s);

	//데빌 이동 확정 종료
	pthread_cond_wait(&threadDoneCond,&mainThreadLock);

	//현 데빌의 수만큼 데빌을 생성(데빌의 숫자는 2배가 됨)
	pthread_cond_broadcast(&devilCopyCond);

	//데빌 복제 종료
	pthread_cond_wait(&threadDoneCond,&mainThreadLock);
	devil_size *= 2;
}

void live_dead_stage (struct setup *s) {
	int i;
	//셀 체크 시작
	pthread_cond_broadcast(&cellCheckCond);

	//셀 체크 종료
	pthread_cond_wait(&threadDoneCond,&mainThreadLock);

	//셀 체크 시작
	pthread_cond_broadcast(&cellConfirmCond);

	//셀 체크 종료
	pthread_cond_wait(&threadDoneCond,&mainThreadLock);

}

void plague_stage (struct setup *s) {
}

void angel_stage (struct setup *s) {
	//스코프 및 이동거리 초기화
	int scope = devil_size/(5*(s->map_size)*(s->map_size));
	int moveLength = s->map_size/10;

	int x,y,z;
	int i,j,k,p,q,r,num, biggest=0;

	struct devil * dev;
	struct list_elem * dev_el;

	xP=0;
	xM=0;
	yP=0;
	yM=0;
	zP=0;
	zM=0;

	if((moveLength/2)>scope)
		scope = moveLength/2;

	//셀 체크 시작
	pthread_cond_broadcast(&angelSearchCond);

	//셀 체크 종료
	pthread_cond_wait(&threadDoneCond,&mainThreadLock);

	if(!list_empty(&devil_list))	//만약 데빌 중복이 존재한다면
	{
		dev_el = list_begin(&devil_list);
		while(1)
		{
			if(list_end(&devil_list)==dev_el)	//만약 현 데빌이 마지막이라면 루프 탈출
				break;

			dev = list_entry(dev_el,struct devil,el);

			if(dev->u.x>angel.x)
				xP ++;
			else if(dev->u.x<angel.x)
				xM ++;

			if(dev->u.y>angel.y)
				yP ++;
			else if(dev->u.y<angel.y)
				yM ++;

			if(dev->u.z>angel.z)
				zP ++;
			else if(dev->u.z<angel.z)
				zM ++;


			dev_el = list_next(dev_el);
		}
	}

	//최대값 검색
	biggest = xP;
	if(xM > biggest)
		biggest = xM;
	if(yP > biggest)
		biggest = yP;
	if(yM > biggest)
		biggest = yM;
	if(zP > biggest)
		biggest = zP;
	if(zM > biggest)
		biggest = zM;

	//엔젤 좌표 이동
	if(xP==biggest)
	{
		unit_mov(s,moveLength,0,0,&angel);	
	}else if(yP==biggest)
	{
		unit_mov(s,0,moveLength,0,&angel);
	}else if(zP==biggest)
	{
		unit_mov(s,0,0,moveLength,&angel);
	}else if(xM==biggest)
	{
		unit_mov(s,-moveLength,0,0,&angel);
	}else if(yM==biggest)
	{
		unit_mov(s,0,-moveLength,0,&angel);
	}else if(zM==biggest)
	{
		unit_mov(s,0,0,-moveLength,&angel);
	}

	//scope 범위 저장
	i = angel.x - scope;
	j = angel.y - scope;
	k = angel.z - scope;
	p = angel.x + scope;
	q = angel.y + scope;
	r = angel.z + scope;

	//scope 테두리 검사
	if(i < 0)
		i = 0;
	else if( p>=s->map_size )
		p = s->map_size-1;
	if(j < 0)
		j = 0;
	else if( q>=s->map_size )
		q = s->map_size-1;
	if(k < 0)
		k = 0;
	else if( r>=s->map_size )
		r = s->map_size-1;

	//scope 지역 수색
	for(x=i ; x<=p ; x++ )
	{
		for(y=j ; y<=q ;y++)
		{
			for(z=k ; z<=r ; z++ )
			{
				devil_size -= NumOfDevil(map[x][y][z].d);
				//데빌 삭제 및 플레그 제거
				map[x][y][z].d = AngelCure(map[x][y][z].d);
			}
		}
	}

	if(!list_empty(&devil_list))	//만약 데빌 중복이 존재한다면
	{
		dev_el = list_begin(&devil_list);

		dev = list_entry(list_begin(&devil_list),struct devil,el);
		while(1)
		{
			if(list_end(&devil_list)==dev_el)	//만약 현 데빌이 마지막이라면 루프 탈출
				break;
			dev = list_entry(dev_el ,struct devil, el);

			if((dev->u.x >= i)&&(dev->u.x <= p)&&(dev->u.y >= j)&&(dev->u.y <= q)&&(dev->u.z >= k)&&(dev->u.z <= r))
				devil_list_remove(dev);

			dev_el = list_next(dev_el);
		}
	}
}


void print_init_map (struct setup *s) {
	FILE * file = fopen("Initial_map.txt","w");
	map_print(s,file);
	fclose(file);
}

void print_init_pos (struct setup *s) {
	FILE * file = fopen("Initial_pos.txt","w");
	pos_print(s,file);
	fclose(file);
}

void print_fin_map (struct setup *s) {
	FILE * file = fopen("Final_map.txt","w");
	map_print(s,file);
	fclose(file);

}

void print_fin_pos (struct setup *s) {
	FILE * file = fopen("Final_pos.txt","w");
	pos_print(s,file);
	fclose(file);
}

void free_resources (struct setup *s) {
	struct devil * dev;

	//중복devil을 저장한 devil_list 비우기
	while(!list_empty(&devil_list))
		dev = devil_list_remove(dev);

	free_3d(map);
	free(thread);
}
