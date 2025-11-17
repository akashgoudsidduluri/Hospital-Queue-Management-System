#ifndef QUEUE_H
#define QUEUE_H

#include "patient.h"

typedef struct PriorityQueue {
    Patient* head;
    Patient* tail;
    int count;
} PriorityQueue;

/* Queue operations */
void pq_init(PriorityQueue *q);
void pq_enqueue(PriorityQueue *q, Patient *p);
Patient* pq_dequeue(PriorityQueue *q);
Patient* pq_peek(PriorityQueue *q);
int pq_save_csv(PriorityQueue *q, const char *filename);
int pq_load_csv(PriorityQueue *q, const char *filename, int *nextId);
void pq_free_all(PriorityQueue *q);
void clear_queue_with_confirmation(PriorityQueue *q);

/* NEW HELPER FUNCTIONS */
int pq_size(PriorityQueue *q);
int pq_is_empty(PriorityQueue *q);
Patient* pq_search_by_id(PriorityQueue *q, int id);
Patient* pq_search_by_name(PriorityQueue *q, const char *name);

#endif
