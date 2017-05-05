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
#define DEVIL_FULL 0x0FFF
#define DEVIL_MOVEIN 0x1000

#define NumOfDevil(x) (x&DEVIL_FULL)
#define IsPlague(x) ((x&PLAGUE)==PLAGUE)
#define IsChange(x) ((x&CHANGE)==CHANGE)
#define IsLive(x) (((x&LIVE)==LIVE)&&(!(IsPlague(x))))
#define IsDead(x) (((x&LIVE)==DEAD)&&(!(IsPlague(x))))
#define IsDevilFull(x) ((x&DEVIL_FULL) == DEVIL_FULL)
#define IsDevilMoveIn(x) ((x&DEVIL_MOVEIN)==DEVIL_MOVEIN)
#define EraseDevil(x) (x&0xF000)
#define AngelCure(x) (x&0xC000)

struct unit
{
    int x;
    int y;
    int z;
};

struct MAP{
    unsigned short d;
	pthread_mutex_t lock;
};

struct devil
{
	struct unit u;
	struct list_elem el;
};

struct MAP *** map;
struct unit angel;
struct list devil_list;
int devil_size=0;
int start = 0;
int totalTaskSize = 0;
int avgTaskSize = 0;
pthread_t * thread;
pthread_attr_t attr;
pthread_key_t key;

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
    d.x = uniform(0,s->map_size-1,s->SEED_DVL_GEN_X);
    d.y = uniform(0,s->map_size-1,s->SEED_DVL_GEN_Y);
    d.z = uniform(0,s->map_size-1,s->SEED_DVL_GEN_Z);
	
	pthread_mutex_lock(&(map[d.x][d.y][d.z].lock));
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
	pthread_mutex_unlock(&(map[d.x][d.y][d.z].lock));

}

void * devilCopyTask(void * data)
{
	int task_size = ((unsigned long *)data)[0];
	struct setup * s = (struct setup *)((unsigned long *)data)[1];
	int i;

	for(i=0;i<task_size;i++)
	{
		devil_copy(s);
	}
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

void * cellCheckTask(void * data)
{
	const struct setup * s = (struct setup *)((unsigned long *)data)[0];
	const int start = ((unsigned long *)data)[1];
	const int size = ((unsigned long *)data)[2];
	int i,j;
	int x,y,z;
	int leftCount, middleCount;
	bool leftP, middleP;
	pthread_mutex_unlock((pthread_mutex_t*)((unsigned long*)data)[3]);

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
void * devilMoveZTask(void * data)
{
	const struct setup * s = (struct setup *)((unsigned long *)data)[0];
	const int start = ((unsigned long *)data)[1];
	const int size = ((unsigned long *)data)[2];
	struct unit tem;
	const struct unit * m = ((unsigned long*)data)[3];
	int d_size=0;
	int i,j;

	pthread_mutex_unlock((pthread_mutex_t*)((unsigned long*)data)[4]);

	for(i=start ; i<(start+size) ; i++ )
	{
		for( j=0 ; j<s->map_size ; j++ )
		{
			if(NumOfDevil(map[0][i][j].d)>0)	//devil이 있으면
			{
				
				tem.x = i/s->map_size;
				tem.y = i%s->map_size;
				tem.z = j;

				//현 좌표 lock
				pthread_mutex_lock(&(map[0][i][j].lock));
				//나가는 devil 빼기
				d_size -= NumOfDevil(map[0][i][j].d);	//이동할 데빌 및 해당 자리 중복 데빌 제거
				map[0][i][j].d = EraseDevil(map[0][i][j].d);	//맵상에 존재하는 데빌 제거
				//현 좌표 unlock
				pthread_mutex_unlock(&(map[0][i][j].lock));

				//이동
				unit_mov(s,m->x,m->y,m->z,&tem);


				//movein에 devil 넣기
				pthread_mutex_lock(&(map[tem.x][tem.y][tem.z].lock));
				map[tem.x][tem.y][tem.z].d |= DEVIL_MOVEIN;
				pthread_mutex_unlock(&(map[tem.x][tem.y][tem.z].lock));
			}
		}
	}
	return d_size;
}

void * devilMoveConfirm(void * data)
{
	const struct setup * s = (struct setup *)((unsigned long *)data)[0];
	const int start = ((unsigned long *)data)[1];
	const int size = ((unsigned long *)data)[2];
	int i,j;
	int d_size = 0;

	pthread_mutex_unlock((pthread_mutex_t*)((unsigned long*)data)[4]);

	for(i=start ; i<(start+size) ; i++ )
	{
		for( j=0 ; j<s->map_size ; j++ )
		{
			if(IsDevilMoveIn(map[0][i][j].d))	//들어와야할 데빌이 존재하면
			{
				d_size -= NumOfDevil(map[0][i][j].d);	//해당 위치에 데빌 전부 제외
				d_size ++;	//데빌 한마리 추가
				map[0][i][j].d = EraseDevil(map[0][i][j].d) + 1;	//해당 위치의 데빌 전부 삭제후 데빌 1마리 추가
				map[0][i][j].d ^= DEVIL_MOVEIN;	//무브인 0로 복귀
				map[0][i][j].d |= PLAGUE;	//해당위치 플래그
			}
		}
	}

	return d_size;
}

static void key_destroy(void * buf)
{
	free(buf);
}

//임의 정의 함수
/////////////////////////////////////////////////////////////////////////////////
//기존 정의 함수

void run_game (struct setup *s) {
	int i = 0;

	while(i++ < s->total_loop) {
		devil_stage(s);
		live_dead_stage(s);
		plague_stage(s);
		angel_stage(s);
	}
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
				 map[i][j][k].d = (unsigned char)uniform(0,1,s->SEED_MAP)<<15;
				 pthread_mutex_init(&(map[i][j][k].lock),NULL);
				 
             }
        }

	}
	//멀티쓰레딩 준비
	thread = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t)*((s->core_num)-1));
	pthread_attr_init(&attr);
	totalTaskSize = s->map_size*s->map_size;
	avgTaskSize = totalTaskSize/(s->core_num);
    
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
}

void devil_stage (struct setup *s) {
	unsigned long data[5];
	int end_point;
	struct unit tem;
    int i,j,k, num;
	int x,y,z;
	void * d_size;
	struct devil * dev = list_entry(list_begin(&devil_list),struct devil,el);
	pthread_mutex_t lock;

	
    if(devil_size==0)//devil이 하나도 없는 상황
    {

        //새로운 devil생성
        tem = devil_init(s); 

        //해당 devil지역 plague화
		map[tem.x][tem.y][tem.z].d|=PLAGUE;
    }else
	{
		//중복devil을 저장한 devil_list 비우기
		while(!list_empty(&devil_list))
			dev = devil_list_remove(dev);

		//devil랜덤 이동 계산
		tem.x=-(uniform(0,2,s->SEED_DVL_MOV_X)-1);
		tem.y=-(uniform(0,2,s->SEED_DVL_MOV_Y)-1);
		tem.z=-(uniform(0,2,s->SEED_DVL_MOV_Z)-1);
		

		//쓰레드 할당 준비
		pthread_mutex_init(&lock,NULL);
		i = 0;
		j = 0; 
		data[0] = (unsigned long)s;
		data[3] = (unsigned long)&tem;
		data[4] = (unsigned long)&lock;
		
		//데빌 이동 시작
		while(i<totalTaskSize)
		{
			pthread_mutex_lock(&lock);
			data[1] = (unsigned long)i;
			data[2] = (unsigned long)avgTaskSize;

			if(j==((s->core_num)-1))	//다른 코어는 전부 할당을 완료했다면
			{
				devil_size += devilMoveZTask((void*)data);

			}else if(j==0)	//첫번째 쓰레드라면
			{
				data[2] += totalTaskSize%avgTaskSize;	//남는 잔여 task까지 부담시킨다
				//쓰레드에 테스크 할당
				pthread_create(&(thread[j]),NULL,devilMoveZTask,(void*)data);

			}else
			{
				//쓰레드에 테스크 할당
				pthread_create(&(thread[j]),NULL,devilMoveZTask,(void*)data);
			}

			//할당한 테스크 양 더하기
			i += data[2];
			j++;	//다음 코어로
			
		}
		
	
		//데빌 이동 종료
		for(i = 0;i<((s->core_num)-1);i++)
		{
			pthread_join(thread[i],&d_size);
			devil_size += (int)d_size;
		}
		
		i = 0;
		j = 0;
		//데빌 이동 확정 시작
		while(i<totalTaskSize)
		{
			pthread_mutex_lock(&lock);
			data[1] = (unsigned long)i;
			data[2] = (unsigned long)avgTaskSize;

			if(j==((s->core_num)-1))	//다른 코어는 전부 할당을 완료했다면
			{
				devil_size += devilMoveConfirm((void*)data);

			}else if(j==0)	//첫번째 쓰레드라면
			{
				data[2] += totalTaskSize%avgTaskSize;	//남는 잔여 task까지 부담시킨다
				//쓰레드에 테스크 할당
				pthread_create(&(thread[j]),NULL,devilMoveConfirm,(void*)data);

			}else
			{
				//쓰레드에 테스크 할당
				pthread_create(&(thread[j]),NULL,devilMoveConfirm,(void*)data);
			}

			//할당한 테스크 양 더하기
			i += data[2];
			j++;	//다음 코어로
			
		}

		for(i = 0;i<((s->core_num)-1);i++)
		{
			pthread_join(thread[i],&d_size);
			devil_size += (int)d_size;
		}
		//데빌 이동 확정 종료
	}

	num = devil_size;


	for(i=0;i<num;i++)
	{
		devil_init(s); 
	}

	/*
	//현 데빌의 수만큼 데빌을 생성(데빌의 숫자는 2배가 됨)
	if(num<(s->core_num*10))
	{
		for(i=0;i<num;i++)
		{
			devil_init(s); 
		}
	}else
	{
		j = num/s->core_num;
		for(i=0;i<s->core_num;i++)
		{
			data[0] = j;
			data[1] = (unsigned long)s;
			if(i==0)
			{
				data[0] += num%s->core_num;
				pthread_create(&(thread[i]),NULL,devilCopyTask,(void*)data);
			}else if(i==s->core_num-1)
			{
				devilCopyTask((void*)data);
			}else
			{
				pthread_create(&(thread[i]),NULL,devilCopyTask,(void*)data);
			}
		}
		
		for(i = 0;i<s->core_num;i++)
		{
			pthread_join(thread[i],NULL);
		}
		devil_size += num;
	}
	*/
}

void live_dead_stage (struct setup *s) {
	int i,j,k;
	pthread_mutex_t lock;
	unsigned long data[4];

	//쓰레드 할당 준비
	pthread_mutex_init(&lock,NULL);
	i = 0;
	j = 0; 
	data[0] = (unsigned long)s;
	data[3] = (unsigned long)&lock;

	//셀 체크 시작
	while(i<totalTaskSize)
	{
		pthread_mutex_lock(&lock);
		data[1] = (unsigned long)i;
		data[2] = (unsigned long)avgTaskSize;

		if(j==((s->core_num)-1))	//다른 코어는 전부 할당을 완료했다면
		{
			cellCheckTask((void*)data);

		}else if(j==0)	//첫번째 쓰레드라면
		{
			data[2] += totalTaskSize%avgTaskSize;	//남는 잔여 task까지 부담시킨다
			//쓰레드에 테스크 할당
			pthread_create(&(thread[j]),NULL,cellCheckTask,(void*)data);

		}else
		{
			//쓰레드에 테스크 할당
			pthread_create(&(thread[j]),NULL,cellCheckTask,(void*)data);
		}

		//할당한 테스크 양 더하기
		i += data[2];
		j++;	//다음 코어로

	}

	for(i = 0; i<(s->core_num-1);i++)
	{
		pthread_join(thread[i],NULL);
	}

	//확인이 끝나면 변경해야 될 셀들을 찾아서 변경
	for(i = 0;i<s->map_size;i++)
	{
		for(j=0;j<s->map_size;j++)
		{
			for(k=0;k<s->map_size;k++)
			{
				if(IsChange(map[i][j][k].d))
				{
					map[i][j][k].d ^= LIVE;	//live는 dead로 dead 는 live로
					map[i][j][k].d ^= CHANGE;	//Change flag 제거
				}
			}
		}
	}
}

void plague_stage (struct setup *s) {
	
}

void angel_stage (struct setup *s) {
	//스코프 및 이동거리 초기화
	int scope = devil_size/(5*(s->map_size)*(s->map_size));
	int moveLength = s->map_size/10;

	int x,y,z;
	int i,j,k,p,q,r,num, biggest=0;
	int xP=0,xM=0,yP=0,yM=0,zP=0,zM=0;

	struct devil * dev;
	struct list_elem * dev_el;

	if((moveLength/2)>scope)
		scope = moveLength/2;

	//각 데빌의 위치를 검색
	for( i=0 ; i<s->map_size ; i++ )
	{
		if(i==angel.x)	//엔젤 선상에 있는 데빌은 세지 않는다
			continue;
		for( j=0 ; j<s->map_size ; j++ )
		{
			if(j==angel.y)
				continue;
			for( k=0 ; k<s->map_size ; k++ )
			{
				if(k==angel.z)
					continue;
				num = NumOfDevil(map[i][j][k].d);
				if(num!=0)	//데빌 숫자가 0이 아니라면
				{
					if(i>angel.x)
						xP += num;
					else
						xM += num;

					if(j>angel.y)
						yP += num;
					else
						yM += num;

					if(k>angel.z)
						zP += num;
					else
						zM += num;
				}
			}
			
		}
		
	}

	if(!list_empty(&devil_list))	//만약 데빌 중복이 존재한다면
	{
		dev_el = list_begin(&devil_list);
		while(1)
		{
			if(list_end(&devil_list)==dev_el)	//만약 현 데빌이 마지막이라면 루프 탈출
				break;

			dev = list_entry(dev_el,struct devil,el);

			if(dev->u.x>angel.x)
				xP += num;
			else if(dev->u.x<angel.x)
				xM += num;

			if(dev->u.y>angel.y)
				yP += num;
			else if(dev->u.y<angel.y)
				yM += num;

			if(dev->u.z>angel.z)
				zP += num;
			else if(dev->u.z<angel.z)
				zM += num;

			
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
