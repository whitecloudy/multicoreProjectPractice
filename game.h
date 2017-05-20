#ifndef __LIB_GAME_H__
#define __LIB_GAME_H__

#include "./setup.h"

void init_resources (struct setup *s);
void run_game (struct setup *s);
void print_init_map (struct setup *s);
void print_init_pos (struct setup *s);
void print_fin_map (struct setup *s);
void print_fin_pos (struct setup *s);
void free_resources (struct setup *s);

#endif

