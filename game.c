#include <stdio.h>
#include <stdlib.h>
#include "./game.h"
#include "./list.h"
#include "./lcgrand.h"

#define DEAD 0x00
#define LIVE 0x80
#define PLAGUE 0x40
#define CHANGE 0x20
#define DEVIL_FULL 0x1F

#define NumOfDevil(x) (x&0x1F)
#define IsPlague(x) ((x&PLAGUE)==PLAGUE)
#define IsChange(x) ((x&CHANGE)==CHANGE)
#define IsLive(x) (((x&LIVE)==LIVE)&&(!(IsPlague(x))))
#define IsDead(x) (((x&LIVE)==DEAD)&&(!(IsPlague(x))))
#define EraseDevil(x) (x&0xE0)
#define AngelCure(x) (x&0xC0)


struct unit
{
    int x;
    int y;
    int z;
};

struct MAP{
    unsigned char d;
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
 *
 * return : 데빌의 위치
 * *********************************************/
struct unit devil_init(struct setup *s)
{
    struct unit d;
	//devil의 초기 위치 설정
    d.x = uniform(0,s->map_size-1,s->SEED_DVL_GEN_X);
    d.y = uniform(0,s->map_size-1,s->SEED_DVL_GEN_Y);
    d.z = uniform(0,s->map_size-1,s->SEED_DVL_GEN_Z);
	//devil 맵 입력
	map[d.x][d.y][d.z].d += 1;

    //devil 사이즈 증가
	devil_size++;

	return d;
}

/*************************************************
 * Devil을 제거하는 함수
 * ***********************************************/
void devil_remove(struct MAP * m)
{
    m->d--;
	devil_size--;
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
 * devil의 이동과 함께 중복제거를 관장함
 * s : 해당 setup
 * x, y, z : 이동해야하는 양
 * cor : 이동할 devil 위치
 * ************************************************************/
void devil_mov(struct setup * s, int x ,int y, int z, struct unit * cor)
{
	//나가는 devil 빼기
	devil_size -= NumOfDevil(map[cor->x][cor->y][cor->z].d);	//이동할 데빌 및 해당 자리 중복 데빌 제거
	map[cor->x][cor->y][cor->z].d = EraseDevil(map[cor->x][cor->y][cor->z].d);
	//이동
	unit_mov(s,x,y,z,cor);
    //들어가는 곳 devil 넣기
	devil_size -= (NumOfDevil(map[cor->x][cor->y][cor->z].d)-1);	//들어올 자리에 중복될 데빌 제거 및 데빌 이동
    map[cor->x][cor->y][cor->z].d = EraseDevil(map[cor->x][cor->y][cor->z].d)+1;
}

/**********************************************************************************
 * 입력받은 좌표의 셀 주위를 확인한 후 live or dead를 결정 혹은 플래그 스테이지
 * ********************************************************************************/
void cell_check(struct setup * s, int x, int y, int z, int * leftCount, int * middleCount)
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
    
    //첫번째 자리인 경우
    if((*leftCount==-1)&&(*middleCount==-1))
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
                            *leftCount++;
                        else if(k==1)
                            *middleCount++;
                        if(i|j|k != 0)//자기자신은 뺀다
                            count++;
                    }
                }
            }
        }
        
    }
	//첫번째 계산 이후
	else
    {
        count = *leftCount+*middleCount;	//이전 계산값들 추가
		//다음 타일 계산값 준비
		*leftCount = *middleCount;
		*middleCount = 0;

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
						*middleCount++;
					}
				}
			}
			count += *middleCount;
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
	}else if(IsPlague(map[x][y][z].d))	//Plague일 경우
	{
		//이웃 셀 전염
		for(i=xs;i<=p;i++)
		{
			for(j=ys;j<=q;j++)
			{
				for(k=zs;k<=r;k++)
				{
					if(!(IsPlague(map[x+i][y+j][z+k].d)))
						map[x+i][y+j][z+k].d ^= CHANGE;
				}
			}
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
				if(!(IsPlague(map[x+i][y+j][z+k].d)))
					map[x+i][y+j][z+k].d |= CHANGE;
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
	struct devil * d;
	struct devil * tem,next;
	struct list x_same, y_same;
	int i;
	int x,y,z;
	int num;


	fprintf(save,"[Angel]\n");
	fprintf(save, "(%d, %d, %d)\n\n",angel.x, angel.y, angel.z);
	fprintf(save,"[Devil]\n");

	//devil 프린트
	for( x=0 ; x<s->map_size-1 ; x++ )
	{
		for( y=0 ; y<s->map_size-1 ; y++ )
		{
			for( z=0 ; z<s->map_size-1 ; z++ )
			{
				for( i=0 ; i<(NumOfDevil(map[x][y][z].d)) ; i++ )
				{
					fprintf(save,"(%d, %d, %d)\n",x,y,z);
				}
				//if(NumOfDevil(map[x][y][z].d)==0x1f)
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
				 map[i][j][k].d = (unsigned char)uniform(0,1,s->SEED_MAP)<<7;
             }
        }

    }
    
    //angel 좌표 초기화
    angel.x = p;
    angel.y = p;
    angel.z = p;

	//새로운 devil생성
	tem = devil_init(s); 

	//해당 devil지역 plague화
	map[tem.x][tem.y][tem.z].d|=PLAGUE;

}

void devil_stage (struct setup *s) {
    struct unit tem;
    int i,j,k, num;
	int ic, jc,kc;
	int ie,je,ke;
	int x,y,z;

    if(devil_size==0)//devil이 하나도 없는 상황
    {

        //새로운 devil생성
        tem = devil_init(s); 

        //해당 devil지역 plague화
		map[tem.x][tem.y][tem.z].d|=PLAGUE;
    }else
	{
		//devil랜덤 이동 계산
		x=-(uniform(0,2,s->SEED_DVL_MOV_X)-1);
		y=-(uniform(0,2,s->SEED_DVL_MOV_Y)-1);
		z=-(uniform(0,2,s->SEED_DVL_MOV_Z)-1);
		
		/*
        for(i=0;i<num;i++)
        {
			//지목된 devil이 가지고 있는 해당 위치의 devil포인터가 다르면 해당 데빌이 중복된것으로 판단
			if(map[tem->cor.x][tem->cor.y][tem->cor.z].d != tem)
			{
				tem = devil_remove(tem);	//데빌 삭제
			}else
			{
				//devil을 이동
				devil_mov(s,x,y,z,tem);

				//해당 지역 plague화
				if(map[tem->cor.x][tem->cor.y][tem->cor.z].status<2)	//만약 해당 지역이 plague가 아니라면
					map[tem->cor.x][tem->cor.y][tem->cor.z].status+=plagued;

				//다음 devil을 가져옴
				tem = list_entry(list_next(&(tem->el)),struct devil,el);
			}
		}
				

		//이동한 데빌들 이동을 확정
		num = devil_size;
		tem = list_entry(list_begin(&devil_list),struct devil,el);
		for(i =0;i<num;i++)
		{
		if(map[tem->cor.x][tem->cor.y][tem->cor.z].d==NULL)
		{
		map[tem->cor.x][tem->cor.y][tem->cor.z].d = tem;
		tem = list_entry(list_next(&(tem->el)),struct devil,el);
		}else
		{
		tem = devil_remove(tem);//중복된 데빌은 제거
			}
		}
		*/
		if(x>0)
		{
			i = s->map_size-1;
			ic = -1;
			ie = -1;
		}else
		{
			i = 0;
			ic = 1;
			ie = s->map_size;
		}

		if(y>0)
		{
			j = s->map_size-1;
			jc = -1;
			je = -1;
		}else
		{
			j = 0;
			jc = 1;
			je = s->map_size;
		}

		if(z>0)
		{
			k = s->map_size-1;
			kc = -1;
			ke = -1;
		}else
		{
			k = 0;
			kc = 1;
			ke = s->map_size;
		}

		for( ; i!=ie ; i+=ic )
		{
			tem.x = i;
			for(  ; j!=je ; j+=jc )
			{
				tem.y = j;
				for(  ; k!=ke ; k+=kc )
				{
					tem.z = k;

					if(NumOfDevil(map[i][j][k].d)>0)	//devil move
					{
						devil_mov(s,x,y,z,&tem);	//데빌 이동
						map[tem.x][tem.y][tem.z].d |= PLAGUE;
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
	int leftCount, middleCount;
	//각 셀을 돌아가면서 확인
	for(i = 0;i<s->map_size;i++)
	{
		for(j=0;j<s->map_size;j++)
		{
			leftCount = -1;
			middleCount = -1;
			for(k=0;k<s->map_size;k++)
			{
				cell_check(s,i,j,k,&leftCount,&middleCount);
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
				//데빌 삭제 및 플레그 제거
				AngelCure(map[i][j][k].d);
			}
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
	struct devil * d = list_entry(list_begin(&devil_list),struct devil,el);
	int i = 0;
	free_3d(map);
}
