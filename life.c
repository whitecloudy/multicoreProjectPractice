#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include "./setup.h"
#include "./game.h"

struct setup s;
int total_loop;

static void initialize (void);
static void end (void);
static void parameter_setup (struct setup *s);
static double mySecond (void);

void main (void) {
	int i = 0;
	double total_time;

	/* System setup start */
	initialize();
	/* System setup end */

	total_time = mySecond();

	while(i++ < total_loop) {
		devil_stage(&s);
		live_dead_stage(&s);
		plague_stage(&s);
		angel_stage(&s);
		// Body
	}
	total_time = mySecond() - total_time;

	fprintf(stderr, "%lf\n", total_time);

	end();
}

static void initialize (void) {
	parameter_setup(&s);	// Parameter setting
	init_resources(&s);
	print_init_map(&s);
	print_init_pos(&s);
}


static void end (void) {
	print_fin_map(&s);
	print_fin_pos(&s);
	free_resources(&s);
}

static void parameter_setup (struct setup *s) {
	FILE *rfp = fopen("./input.life", "r");
	char trash[32];

	/* Parameters about Game of Life */
	fscanf(rfp, "%s %d", trash, &total_loop);
	fscanf(rfp, "%s %d", trash, &s->map_size);
	fscanf(rfp, "%s %d", trash, &s->dead_min);
	fscanf(rfp, "%s %d", trash, &s->dead_max);
	fscanf(rfp, "%s %d", trash, &s->live_min);
	fscanf(rfp, "%s %d", trash, &s->live_max);
	/* Random seeds for map */
	fscanf(rfp, "%s %d", trash, &s->SEED_MAP);
	/* Random seeds for devil */
	fscanf(rfp, "%s %d", trash, &s->SEED_DVL_GEN_X);
	fscanf(rfp, "%s %d", trash, &s->SEED_DVL_GEN_Y);
	fscanf(rfp, "%s %d", trash, &s->SEED_DVL_GEN_Z);
	fscanf(rfp, "%s %d", trash, &s->SEED_DVL_MOV_X);
	fscanf(rfp, "%s %d", trash, &s->SEED_DVL_MOV_Y);
	fscanf(rfp, "%s %d", trash, &s->SEED_DVL_MOV_Z);
	
	fclose(rfp);
}

static double mySecond (void) {
	struct timeval tp;
	struct timezone tzp;
	int i;

	i = gettimeofday(&tp, &tzp);

	return ((double)tp.tv_sec + (double)tp.tv_usec * 1.e-6);
}
