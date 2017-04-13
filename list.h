#ifndef __LIB_LIST_H__
#define __LIB_LIST_H__

typedef int bool;
#define true 1
#define false 0
#include "./game.h"

struct list_elem {
	struct list_elem *prev;     /* Previous list element. */
	struct list_elem *next;     /* Next list element. */
    struct unit d;
};

/* List */
struct list {
	struct list_elem head;      /* List head. */
	struct list_elem tail;      /* List tail. */
};

#define offsetof(type, member) ((size_t) &((type *)0)->member)
#define list_entry(LIST_ELEM, STRUCT, MEMBER)			\
        ((STRUCT *) ((unsigned char *) &(LIST_ELEM)->next	\
                     - offsetof (STRUCT, MEMBER.next)))
/* List initialization.

   A list may be initialized by calling list_init():

       struct list my_list;
       list_init (&my_list);

   or with an initializer using LIST_INITIALIZER:

       struct list my_list = LIST_INITIALIZER (my_list); */
#define LIST_INITIALIZER(NAME) { { NULL, &(NAME).tail }, \
                                 { &(NAME).head, NULL } }

void list_init (struct list *);

/* List traversal. */
struct list_elem *list_begin (struct list *);
struct list_elem *list_next (struct list_elem *);
struct list_elem *list_end (struct list *);

struct list_elem *list_rbegin (struct list *);
struct list_elem *list_prev (struct list_elem *);
struct list_elem *list_rend (struct list *);

struct list_elem *list_head (struct list *);
struct list_elem *list_tail (struct list *);

/* List insertion. */
void list_insert (struct list_elem *, struct list_elem *);
void list_splice (struct list_elem *before, struct list_elem *first, struct list_elem *last);
void list_push_front (struct list *, struct list_elem *);
void list_push_back (struct list *, struct list_elem *);

/* List removal. */
struct list_elem *list_remove (struct list_elem *);
struct list_elem *list_pop_front (struct list *);
struct list_elem *list_pop_back (struct list *);

/* List elements. */
struct list_elem *list_front (struct list *);
struct list_elem *list_back (struct list *);

/* List properties. */
bool list_empty (struct list *);

/* Miscellaneous. */
void list_reverse (struct list *);

/* Compares the value of two list elements A and B, given
   auxiliary data AUX.  Returns true if A is less than B, or
   false if A is greater than or equal to B. */
typedef bool list_less_func (const struct list_elem *a, const struct list_elem *b, void *aux);

/* Operations on lists with ordered elements. */
void list_sort (struct list *, list_less_func *, void *aux);
void list_insert_ordered (struct list *, struct list_elem *, list_less_func *, void *aux);
void list_unique (struct list *, struct list *duplicates, list_less_func *, void *aux);

/* Max and min. */
struct list_elem *list_max (struct list *, list_less_func *, void *aux);
struct list_elem *list_min (struct list *, list_less_func *, void *aux);

/* swap */
void list_swap(struct list_elem *a, struct list_elem *b);
int list_size(struct list *llist);
/* find */
struct list_elem* find_elem(struct list *llist, int position);

#endif
