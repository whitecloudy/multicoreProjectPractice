#ifndef __LIB_SETUP_H__
#define __LIB_SETUP_H__

struct setup {
	int core_num;
	int total_loop;
	int map_size;
	int dead_min;
	int dead_max;
	int live_min;
	int live_max;
	int SEED_MAP;
	int SEED_DVL_GEN_X;
	int SEED_DVL_GEN_Y;
	int SEED_DVL_GEN_Z;
	int SEED_DVL_MOV_X;
	int SEED_DVL_MOV_Y;
	int SEED_DVL_MOV_Z;
};

#endif
