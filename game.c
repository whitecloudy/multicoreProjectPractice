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

#define plagued 2
#define deplagued -2

#define PLUS 0
#define STOP 1
#define MINUS 2

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
	devil * d;
};

MAP *** map;
struct unit angle;
struct list devil_list;


/***************************************
 * 3차원 메모리를 동적할당
 * num : 한변 길이
 * size : 한 block의 데이터 크기
 * **************************************/
void *** malloc_3d(int num,int size)
{
    void *** data;
    int i,j;

    
    data = (void***)malloc(sizeof(void**)*num);

    for(i = 0;i<size;i++)
    {
        data[i] = (void**)malloc(sizeof(void*)*num);
        for(j = 0; j<size;j++)       
        {           
            data[i][j] = (void*)malloc(size*num);
        }
    }
    return data;
}

/**********************************************
 * Devil의 초기화
 * s : 해당 setup
 * d : 초기화 하고자 하는 devil
 * *********************************************/
void devil_init(struct setup *s, struct devil* d)
{
	//devil의 초기 위치 설정
    d->cor.x = uniform(0,s->map_size-1,s->SEED_DVL_GEN_X);
    d->cor.y = uniform(0,s->map_size-1,s->SEED_DVL_GEN_Y);
    d->cor.z = uniform(0,s->map_size-1,s->SEED_DVL_GEN_Z);
	//devil 맵 입력
	map[d->cor.x][d->cor.y][d->cor.z].d = d;
	//devil 리스트 입력
	devil_list->list_push_back(&devil_list,&(d->el));
        
}

/*************************************************
 * Devil을 제거하는 함수
 * ***********************************************/
devil * devil_remove(struct devil *d)
{
	struct devil * next = list_entry(list_next(d),struct devil,el);
	list_remove(d->el);
	free(d);
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
	unit_mov(s,x,y,z,d->cor);
	//들어온 devil 넣기
	map[d->cor.x][d->cor.y][d->cor.z].d = d;
}

/***************************************************************
 * 입력받은 좌표의 셀 주위를 확인한 후 live or dead를 결정
 * ********************************************************/
void cell_check(struct setup * s, int x, int y, int z)
{
	int i,j,k;
	int p,q,r;
	int count = 0;

	//테두리 체크
	if(x==0)
		i=0;
	else
		i=-1;
	if(x==s->map_size-1)
		p=0;
	else
		p=1;
	if(y==0)
		j=0;
	else
		j=-1;
	if(y==s->map_size-1)
		q=0;
	else
		q=1;
	if(z==0)
		k=0;
	else
		k=-1;
	if(z==s->map_size-1)
		r=0;
	else
		r=1;

	//DEAD, LIVE 카운트
	if(map[x][y][z].status == DEAD)	//DEAD일 경우
	{
		//이웃 셀 중 LIVE인 것들을 체크
		for(;i<=p;i++)
		{
			for(j;j<=q;j++)
			{
				for(k;k<=r;k++)
				{
					if((map[x+i][y+j][z+k].status % 4) == LIVE)
						count++;
				}
			}
		}
		//카운트 된 것 확인 후 만약 조건 부합시 DEAD_TO_LIVE로
		if((count > s->live_min)&&(count < s->live_max))
		{
			map[x][y][z].status += 4;
		}
	}else if(map[x][y][z].status == LIVE)	//LIVE일 경우
	{			
		//이웃 셀 중 DEAD인 것들을 체크
		for(i=-1;i<=1;i++)
		{
			for(j=-1;j<=1;j++)
			{
				for(k=-1;k<=1;k++)
				{
					if((map[x+i][y+j][z+k].status % 4) == DEAD)
						count++;
				}
			}
		}
		//카운트 된 것 확인 후 만약 조건 부합시 LIVE_TO_DEAD로
		if((count > s->dead_max)||(count < s->dead_min))
		{
			map[x][y][z].status += 4;
		}
	}
}

//임의 정의 함수
/////////////////////////////////////////////////////////////////////////////////
//기존 정의 함수

void init_resources (struct setup *s) {
    int i,j,k;
    int p = (s->map_size/2)-1;

    //맵 데이터 동적할당
    map = (MAP***)malloc_3d(s->map_size,sizeof(MAP));
    
    //맵 이니셜라이징
    for(i = 0; i<s->map_size;i++)
    { 
        for(j=0;j<s->map_size;j++)
        { 
             for(k=0;k<s->map_size;k++)
             {
                 map[i][j][k].status = uniform(DEAD,LIVE,s->SEED_MAP);
				 map[i][j][k].d = NULL;
             }
        }

    }
    
    //angle 좌표 할당
    angle.x = p;
    angle.y = p;
    angle.z = p;

    //devil list 초기화
    list_init(&devil_list);
}

void devil_stage (struct setup *s) {
    devil * tem,tem1;
    int i, num;
    static int size = list_size(devil_list);	//데빌 숫자를 저장하는 변수
	int x,y,z;

    if(size==0)//devil이 하나도 없는 상황
    {
        //새로운 devil생성
        tem = (devil*)malloc(sizeof(devil));
        devil_init(s,tem);
		size++;

        //해당 devil지역 plague화
		if(map[tem->cor.x][tem->cor.y][tem->cor.z].status<2)	//만약 해당 지역이 plague가 아니라면
			map[tem->cor.x][tem->cor.y][tem->cor.z].status+=plagued;
    }else
	{
		//시작 지점 저장
		tem = list_entry(list_begin(devil_list),struct devil,el);

		//devil랜덤 이동 계산
		x=-(uniform(0,2,SEED_DVL_MOV_X)-1);
		y=-(uniform(0,2,SEED_DVL_MOV_Y)-1);
		z=-(uniform(0,2,SEED_DVL_MOV_Z)-1);
		
		num = size;

        for(i=0;i<num;i++)
        {
			//지목된 devil이 가지고 있는 해당 위치의 devil포인터가 현 devil와 다르다면 중첩된 것으로 판단
			if(map[tem->cor.x][tem->cor.y][tem->cor.z].d != tem)
			{
				tem = devil_remove(tem);	//데빌 삭제
				size--;
			}else
			{
				//devil을 이동
				size+=devil_mov(s,x,y,z,tem);

				//해당 지역 plague화
				if(map[tem->cor.x][tem->cor.y][tem->cor.z].status<2)	//만약 해당 지역이 plague가 아니라면
					map[tem->cor.x][tem->cor.y][tem->cor.z].status+=plagued;

				//다음 devil을 가져옴
				tem = list_entry(list_next(tem),struct devil,el);
			}
        }
    }

	num = size;
	
	//현 데빌의 수만큼 데빌을 생성(데빌의 숫자는 2배가 됨)
	for(i=0;i<num;i++)
	{
		tem = (devil*)malloc(sizeof(devil));
		devil_init(tem);
		size++;
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
				if(map[i][j][k] ==DEAD_TO_LIVE)
					map[i][j][k] = LIVE;
				else if(map[i][j][k] == LIVE_TO_DEAD)
					map[i][j][k] = DEAD;
			}
		}
	}
}

void plague_stage (struct setup *s) {

}

void angel_stage (struct setup *s) {

}

void print_init_map (struct setup *s) {

}

void print_init_pos (struct setup *s) {

}

void print_fin_map (struct setup *s) {

}

void print_fin_pos (struct setup *s) {

}

void free_resources (struct setup *s) {

}
