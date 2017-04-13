#include <stdio.h>
#include <stdlib.h>
#include "./list.h"

#define DEAD 0
#define LIVE 1
#define PLAGUE_D 2
#define PLAGUE_L 3

#define plagued(x) x+2
#define deplagued(x) x-2

struct unit
{
    int x;
    int y;
    int z;
};

int *** map;
struct unit angle;
struct list devils;


/***************************************
 * 3차원 메모리를 동적할당
 * num : 한변 길이
 * size : 한 block의 데이터 크기
 * **************************************/
void *** 3dMemoryAlloc(int num,int size)
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

void init_resources (struct setup *s) {
    int i,j,k;
    int p = (s->map_size/2)-1;

    //맵 데이터 동적할당
    map = (int***)3dMemoryAlloc(s->map_size,sizeof(int));
    
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
    list_init(&devils);
}

void devil_stage (struct setup *s) {

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
