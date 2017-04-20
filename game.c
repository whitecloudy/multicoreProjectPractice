#include <stdio.h>
#include <stdlib.h>
#include "./game.h"
#include "./list.h"
#include "./lcgrand.h"

#define DEAD 0
#define LIVE 1
#define PLAGUE_D 2
#define PLAGUE_L 3
#define DEAD_TO_LIVE 4
#define LIVE_TO_DEAD 5

#define changed 4
#define plagued 2
#define deplagued -2

struct unit
{
    int x;
    int y;
    int z;
};

struct devil
{
    struct unit cor;
    struct list_elem el;
};

struct MAP{
	int status;
	struct devil * d;
	struct devil * moveIn;
};

struct MAP *** map;
struct unit angel;
struct list devil_list;
int devil_size=0;
int start = 0;

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

/**********************************************
 * Devil의 생성 및 초기화
 * s : 해당 setup
 * *********************************************/
struct devil * devil_init(struct setup *s)
{
	struct devil * d = (struct devil *)malloc(sizeof(struct devil));
	//devil의 초기 위치 설정
    d->cor.x = uniform(0,s->map_size-1,s->SEED_DVL_GEN_X);
    d->cor.y = uniform(0,s->map_size-1,s->SEED_DVL_GEN_Y);
    d->cor.z = uniform(0,s->map_size-1,s->SEED_DVL_GEN_Z);
	//devil 맵 입력
	map[d->cor.x][d->cor.y][d->cor.z].d = d;
	//devil 리스트 입력
	list_push_back(&devil_list,&(d->el));

	devil_size++;

	return d;
}

/*************************************************
 * Devil을 제거하는 함수
 * 제거후 다음 devil값을 반환
 * ***********************************************/
struct devil * devil_remove(struct devil *d)
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
void unit_mov(struct setup * s, int x, int y, int z, struct unit * cor)
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

/**************************************************************
 * devil의 이동을 관장함
 * s : 해당 setup
 * x, y, z : 이동해야하는 양
 * d : 이동할 devil
 * ************************************************************/
void devil_mov(struct setup * s, int x ,int y, int z, struct devil * d)
{
	//나가는 devil 빼기
	map[d->cor.x][d->cor.y][d->cor.z].d = NULL;
	//이동
	unit_mov(s,x,y,z,&(d->cor));
	//들어온 devil 넣기
	map[d->cor.x][d->cor.y][d->cor.z].moveIn = d;
}

/***************************************************************
 * 입력받은 좌표의 셀 주위를 확인한 후 live or dead를 결정
 * ********************************************************/
void cell_check(struct setup * s, int x, int y, int z)
{
	int i,j,k;
	int xs,ys,zs;
	int p,q,r;
	int count = 0;

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

	//DEAD, LIVE 카운트
	if(map[x][y][z].status == DEAD)	//DEAD일 경우
	{
		//이웃 셀 중 LIVE인 것들을 체크
		for(i=xs;i<=p;i++)
		{
			for(j=ys;j<=q;j++)
			{
				for(k=zs;k<=r;k++)
				{
					//자기자신은 세지 않는다
					if((i==0)&&(j==0)&&(k==0))
						continue;
					//라이브 셀 카운트
					if((map[x+i][y+j][z+k].status % changed) == LIVE)
						count++;
				}
			}
		}
		//카운트 된 것 확인 후 만약 조건 부합시 DEAD_TO_LIVE로
		if((count > s->live_min)&&(count < s->live_max))
		{
			map[x][y][z].status += changed;
		}
	}else if(map[x][y][z].status == LIVE)	//LIVE일 경우
	{			
		//이웃 셀 중 DEAD인 것들을 체크
		for(i=xs;i<=p;i++)
		{
			for(j=ys;j<=q;j++)
			{
				for(k=zs;k<=r;k++)
				{	//자기자신은 세지 않는다
					if((i==0)&&(j==0)&&(k==0))
						continue;
					//데드 셀 카운트
					if((map[x+i][y+j][z+k].status % changed) == LIVE)
						count++;
				}
			}
		}
		//카운트 된 것 확인 후 만약 조건 부합시 LIVE_TO_DEAD로
		if((count > s->dead_max)||(count < s->dead_min))
		{
			map[x][y][z].status += changed;
		}
	}
}

/***************************************************************
 * 입력받은 좌표의 셀 주위를 전염시킴
 * ********************************************************/
void cell_transmit(struct setup * s, int x, int y, int z)
{
	int i,j,k;
	int xs,ys,zs;
	int p,q,r;
	int count = 0;

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

	//이웃 셀 전염
	for(i=xs;i<=p;i++)
	{
		for(j=ys;j<=q;j++)
		{
			for(k=zs;k<=r;k++)
			{
				if(map[x+i][y+j][z+k].status<2)
					map[x+i][y+j][z+k].status += changed;
			}
		}
	}
}

/*****************************************************
 * A와 B를 비교하는 함수
 * return : B가 더 크거나 같으면 1 A가 더 크면 0을 반환
 * ***************************************************/
int compare(struct unit A, struct unit B)
{
	if(A.x > B.x)
		return 0;
	else if(A.x < B.x)
		return 1;
	else if(A.y > B.y)
		return 0;
	else if(A.y < B.y)
		return 1;
	else if(A.z > B.z)
		return 0;
	else
		return 1;
}

/********************************************************
 * 근접한 두개의 list_elem의 위치를 바꾸는 함수
 * ****************************************************/
void close_swap(struct list_elem *a, struct list_elem * b)
{
	struct list_elem *a_prev = a->prev;
	struct list_elem *a_next = a->next;
	struct list_elem *b_prev = b->prev;
	struct list_elem *b_next = b->next;

	a->prev = b;
	a->next = b_next;
	b->prev = a_prev;
	b->next = a;
	a_prev->next = b;
	b_next->prev = a;
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
				if(map[i][j][k].status == LIVE)
					fprintf(save,"L ");
				else if(map[i][j][k].status == DEAD)
					fprintf(save,"D ");
				else if((map[i][j][k].status == PLAGUE_D)||(map[i][j][k].status==PLAGUE_L))
					fprintf(save,"P ");
			}
			fprintf(save,"\n");
		}
		fprintf(save,"\n");
	}
}

/*****************************************************
 * 처음에 엔젤과 데빌의 위치를 파일에 저장하는 함수
 ****************************************************/
void init_pos_print(struct setup * s, FILE * save)
{
	struct devil * d = list_entry(list_begin(&devil_list),struct devil,el);

	fprintf(save,"[Angel]\n");
	fprintf(save, "(%d, %d, %d)\n\n",angel.x, angel.y, angel.z);
	fprintf(save,"[Devil]\n");
	fprintf(save,"(%d, %d, %d)\n",d->cor.x,d->cor.y,d->cor.z);
}

/************************************************
 * 엔젤과 데빌의 위치를 파일에 저장하는 함수
 * *********************************************/
void pos_print(struct setup * s, FILE * save)
{
	struct devil * d;
	struct devil * tem,next;
	struct list x_same, y_same;
	int i;
	int x,y,z;
	int x_size=0, y_size=0;
	int num;

	list_init(&x_same);
	list_init(&y_same);

	fprintf(save,"[Angel]\n");
	fprintf(save, "(%d, %d, %d)\n\n",angel.x, angel.y, angel.z);
	fprintf(save,"[Devil]\n");

	//devil 프린트
	for(x=0;x<s->map_size;x++)
	{
		d=list_entry(list_begin(&devil_list),struct devil,el);
		num = devil_size;
		for(i=0;i<num;i++)
		{
			if((d->cor.x)==x)
			{
				//맞는 x값을 찾았으므로 이를 devil_list에서 빼낸다.
				tem = d;
				list_remove(&(d->el));
				d = list_entry(list_next(&(d->el)),struct devil,el);
				devil_size--;

				//해당 devil을 넘겨준다
				list_push_back(&x_same,&(tem->el));
				x_size++;
			
			}else
				d = list_entry(list_next(&(d->el)),struct devil,el);
		}
		for(y=0;y<s->map_size;y++)
		{
			num = x_size;
			d = list_entry(list_begin(&x_same),struct devil, el);
			for(i=0;i<num;i++)
			{
				if((d->cor.y)==y)
				{
					tem = d;
					list_remove(&(d->el));
					d = list_entry(list_next(&(d->el)),struct devil,el);
					x_size--;

					list_push_back(&y_same,&(tem->el));
					y_size++;
				}else
					d = list_entry(list_next(&(d->el)),struct devil,el);
			}
			for(z=0;z<s->map_size;z++)
			{
				num = y_size;
				d = list_entry(list_begin(&y_same),struct devil,el);
				for(i=0;i<num;i++)
				{
					if((d->cor.z) == z)
					{
						tem = list_entry(list_next(&(d->el)),struct devil,el);
						fprintf(save,"(%d, %d, %d)\n",x,y,z);
						list_remove(&(d->el));
						free(d);
						y_size--;
						d = tem;
					}else
					{
						d = list_entry(list_next(&(d->el)),struct devil,el);
					}
				}
			}
		}
	}
}

//임의 정의 함수
/////////////////////////////////////////////////////////////////////////////////
//기존 정의 함수

void init_resources (struct setup *s) {
    int i,j,k;
    int p = (s->map_size/2)-1;
	struct devil * tem;

    //맵 데이터 동적할당
    map = (struct MAP ***)malloc_3d(s->map_size,sizeof(struct MAP));
    
    //맵 이니셜라이징
    for(i = 0; i<s->map_size;i++)
    { 
        for(j=0;j<s->map_size;j++)
        { 
             for(k=0;k<s->map_size;k++)
             {
                 map[i][j][k].status = uniform(DEAD,LIVE,s->SEED_MAP);
				 map[i][j][k].d = NULL;
				 map[i][j][k].moveIn = NULL;
             }
        }

    }
    
    //angel 좌표 초기화
    angel.x = p;
    angel.y = p;
    angel.z = p;

    //devil list 초기화
    list_init(&devil_list);

	//새로운 devil생성
	tem = devil_init(s); 

	//해당 devil지역 plague화
	map[tem->cor.x][tem->cor.y][tem->cor.z].status+=plagued;

}

void devil_stage (struct setup *s) {
    struct devil * tem;
    int i, num;
	int x,y,z;

    if(devil_size==0)//devil이 하나도 없는 상황
    {

        //새로운 devil생성
        tem = devil_init(s); 

        //해당 devil지역 plague화
		if(map[tem->cor.x][tem->cor.y][tem->cor.z].status<2)	//만약 해당 지역이 plague가 아니라면
			map[tem->cor.x][tem->cor.y][tem->cor.z].status+=plagued;
    }else
	{
		//시작 지점 저장
		tem = list_entry(list_begin(&devil_list),struct devil,el);

		//devil랜덤 이동 계산
		x=-(uniform(0,2,s->SEED_DVL_MOV_X)-1);
		y=-(uniform(0,2,s->SEED_DVL_MOV_Y)-1);
		z=-(uniform(0,2,s->SEED_DVL_MOV_Z)-1);
		

        for(i=0;i<devil_size;i++)
        {
			//지목된 devil이 가지고 있는 해당 위치의 devil포인터가 다르면 해당 데빌이 중복된것으로 판단
			while(map[tem->cor.x][tem->cor.y][tem->cor.z].d != tem)
			{
				tem = devil_remove(tem);	//데빌 삭제
			}
			//devil을 이동
			devil_mov(s,x,y,z,tem);

			//해당 지역 plague화
			if(map[tem->cor.x][tem->cor.y][tem->cor.z].status<2)	//만약 해당 지역이 plague가 아니라면
				map[tem->cor.x][tem->cor.y][tem->cor.z].status+=plagued;

			//다음 devil을 가져옴
			tem = list_entry(list_next(&(tem->el)),struct devil,el);
        }
		
		for(x=0;x<s->map_size;x++)
		{
			for(y=0;y<s->map_size;y++)
			{
				for( z=0 ; z<s->map_size ; z++ )
				{
					if( map[x][y][z].moveIn != NULL )
					{
						map[x][y][z].d = map[x][y][z].moveIn;
						map[x][y][z].moveIn = NULL;
					}
				}
			}
		}
				
    }

	num = devil_size;
	
	//현 데빌의 수만큼 데빌을 생성(데빌의 숫자는 2배가 됨)
	for(i=0;i<num;i++)
	{
		tem = devil_init(s); 
	}
}

void live_dead_stage (struct setup *s) {
	int i,j,k;
	//각 셀을 돌아가면서 확인
	for(i = 0;i<s->map_size;i++)
	{
		for(j=0;j<s->map_size;j++)
		{
			for(k=0;k<s->map_size;k++)
			{
				cell_check(s,i,j,k);
			}
		}
	}

	//확인이 끝나면 변경해야 될 셀들을 찾아서 변경
	for(i = 0;i<s->map_size;i++)
	{
		for(j=0;j<s->map_size;j++)
		{
			for(k=0;k<s->map_size;k++)
			{
				if(map[i][j][k].status ==DEAD_TO_LIVE)
					map[i][j][k].status = LIVE;
				else if(map[i][j][k].status == LIVE_TO_DEAD)
					map[i][j][k].status = DEAD;
			}
		}
	}
}

void plague_stage (struct setup *s) {
	int i,j,k;
	
	//각 셀들을 방문해 해당 셀이 플레그인지 확인 후 이웃 셀 전염
	for(i=0;i<s->map_size;i++)
	{
		for(j=0;j<s->map_size;j++)
		{
			for(k=0;k<s->map_size;k++)
			{
				if((map[i][j][k].status == PLAGUE_D)||(map[i][j][k].status == PLAGUE_L))	//이 셀이 플레그인가?
					cell_transmit(s,i,j,k);	//전염
			}
		}
	}

	//전염이 끝나면 변경해야 될 셀들을 찾아서 변경
	for(i = 0;i<s->map_size;i++)
	{
		for(j=0;j<s->map_size;j++)
		{
			for(k=0;k<s->map_size;k++)
			{
				if(map[i][j][k].status ==DEAD_TO_LIVE)
					map[i][j][k].status = LIVE;
				else if(map[i][j][k].status == LIVE_TO_DEAD)
					map[i][j][k].status = DEAD;
			}
		}
	}
}

void angel_stage (struct setup *s) {
	//스코프 및 이동거리 초기화
	int scope = devil_size/(5*(s->map_size)*(s->map_size));
	int moveLength = s->map_size/10;
	int x,y,z;
	int i,j,k,p,q,r,num, biggest=0;
	int xP=0,xM=0,yP=0,yM=0,zP=0,zM=0;

	if((moveLength/2)>scope)
		scope = moveLength/2;

	//최초 데빌을 지목
	struct devil * cursor= list_entry(list_begin(&devil_list),struct devil,el);

	//각 데빌의 위치를 검색
	for( i=0 ; i<devil_size ; i++ )
	{
		if(cursor->cor.x > angel.x)
		{
			xP++;
			if(xP>biggest)
				biggest = xP;
		}else if(cursor->cor.x < angel.x)
		{
			xM++;
			if(xM>biggest)
				biggest = xM;
		}

		if(cursor->cor.y > angel.y)
		{
			yP++;
			if(yP>biggest)
				biggest = yP;

		}else if( cursor->cor.y < angel.y )
		{
			yM++;
			if(yM>biggest)
				biggest = yM;
		}

		if(cursor->cor.z > angel.z)
		{
			zP++;
			if(zP>biggest)
				biggest = zP;
		}else if( cursor->cor.z < angel.z )
		{
			zM++;
			if(zM>biggest)
				biggest = zM;
		}
		//다음 데빌로 이동
		cursor = list_entry(list_next(&(cursor->el)),struct devil,el);
	}

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
				//플래그 지역 디플레그
				if( (map[x][y][z].status==PLAGUE_D)||(map[x][y][z].status==PLAGUE_L) )
				{
					map[x][y][z].status+=deplagued;
				}
				//해당 지역 데빌 삭제
				map[x][y][z].d = NULL;
			}
		}
	}

	
	//scope범위에 해당하는 데빌을 데빌 리스트에서 삭제
	num = devil_size;
	cursor = list_entry(list_begin(&devil_list),struct devil,el);
	for(xP=0;xP<devil_size;xP++)
	{
		while((cursor->cor.x>=i)&&(cursor->cor.x<=p)&&(cursor->cor.y>=j)&&(cursor->cor.y<=q)&&(cursor->cor.z>=k)&&(cursor->cor.z<=r))
		{
			cursor = devil_remove(cursor);
		}

		cursor = list_entry(list_next(&(cursor->el)),struct devil,el);
	}
}


void print_init_map (struct setup *s) {
	FILE * file = fopen("Initial_map.txt","w");
	map_print(s,file);
	fclose(file);
}

void print_init_pos (struct setup *s) {
	FILE * file = fopen("Initial_pos.txt","w");
	init_pos_print(s,file);
	fclose(file);
}

void print_fin_map (struct setup *s) {
	FILE * file = fopen("Final_map.txt","w");
	map_print(s,file);
	fclose(file);

}

void print_fin_pos (struct setup *s) {
	FILE * file = fopen("Final_pos.txt","w");
	fprintf(stderr,"%d\n",devil_size);
	pos_print(s,file);
	fclose(file);
}

void free_resources (struct setup *s) {
	struct devil * d = list_entry(list_begin(&devil_list),struct devil,el);
	int i = 0;
	free_3d(map);
}
