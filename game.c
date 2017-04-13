#include <stdio.h>
#include <stdlib.h>
#include "./game.h"
#include "./list.h"
#include "./lcgrand.h"

#define DEAD 0
#define LIVE 1
#define PLAGUE_D 2
#define PLAGUE_L 3

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
}

int *** map;
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
    d->cor.x = uniform(0,s->map_size-1,s->SEED_DVL_GEN_X);
    d->cor.y = uniform(0,s->map_size-1,s->SEED_DVL_GEN_Y);
    d->cor.z = uniform(0,s->map_size-1,s->SEED_DVL_GEN_Z);
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

//임의 정의 함수
/////////////////////////////////////////////////////////////////////////////////
//기존 정의 함수

void init_resources (struct setup *s) {
    int i,j,k;
    int p = (s->map_size/2)-1;

    //맵 데이터 동적할당
    map = (int***)malloc_3d(s->map_size,sizeof(int));
    
    //맵 이니셜라이징
    for(i = 0; i<s->map_size;i++)
    { 
        for(j=0;j<s->map_size;j++)
        { 
             for(k=0;k<s->map_size;k++)
             {
                 map[i][j][k] = uniform(DEAD,LIVE,s->SEED_MAP);
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
    devil * tem;
    int i;
    int size = list_size(devil_list);

    if(size==0)
    {
        //새로운 devil생성
        tem = (devil*)malloc(sizeof(devil));
        devil_init(s,tem);
        devil_list->list_push_back(&devil_list,&(tem->el));
        size++;

        //해당 devil지역 plague화
        map[tem->cor.x][tem->cor.y][tem->cor.z]+=plagued;
    }else
    {
        tem = list_entry(list_begin(devil_list),struct devil,el);

        for(i=0;i<size;i++)
        {
            //devil을 랜덤하게 이동
            unit_mov(s, -(uniform(0,2,SEED_DVL_MOV_X)-1), -(uniform(0,2,SEED_DVL_MOV_Y)-1), -(uniform(0,2,SEED_DVL_MOV_Z)-1),&(tem->cor));
            
            //해당 지역 plague화
            map[tem->cor.x][tem->cor.y][tem->cor.z]+=plagued;

            //다음 devil을 가져옴
            tem = list_entry(list_next(tem),struct devil,el);
        }
    }

       

}

void live_dead_stage (struct setup *s) {

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
