#ifndef VIEW_H
#define VIEW_H

#include "../model/patient.h"
#include "../model/queue.h"

void view_show_menu(void);
void view_show_patient(const Patient* p);
void view_show_list(PriorityQueue* q);
void view_show_stats(int totalAdded, int served, PriorityQueue* q);
void clear_queue_with_confirmation(PriorityQueue* q);

#endif
