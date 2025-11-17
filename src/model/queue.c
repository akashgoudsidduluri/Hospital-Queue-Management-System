#include "queue.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Ensure queue is initialized */
void pq_init(PriorityQueue *q) {
    if (!q) return;
    q->head = NULL;
    q->tail = NULL;
    q->count = 0;
}

/* Enqueue patient into priority queue (higher severity first).
   Added debug prints and safety checks to diagnose crash. */
void pq_enqueue(PriorityQueue *q, Patient *p) {
    printf("DEBUG(pq_enqueue): q=%p p=%p\n", (void*)q, (void*)p);
    fflush(stdout);

    if (!q) { printf("DEBUG: pq_enqueue - q is NULL\n"); fflush(stdout); return; }
    if (!p) { printf("DEBUG: pq_enqueue - p is NULL\n"); fflush(stdout); return; }

    /* defensive: ensure p->next is NULL before insert */
    p->next = NULL;

    /* empty queue */
    if (q->head == NULL) {
        q->head = q->tail = p;
        q->count = 1;
        printf("DEBUG: inserted as head, count=%d\n", q->count);
        fflush(stdout);
        return;
    }

    /* If new patient has higher severity than head, insert at front */
    if (p->severity > q->head->severity) {
        p->next = q->head;
        q->head = p;
        q->count++;
        printf("DEBUG: inserted at front, count=%d\n", q->count);
        fflush(stdout);
        return;
    }

    /* Find insertion point: keep list sorted by severity desc, FIFO within same severity */
    Patient *cur = q->head;
    while (cur->next != NULL && cur->next->severity >= p->severity) {
        cur = cur->next;
    }

    /* Insert after cur */
    p->next = cur->next;
    cur->next = p;
    if (p->next == NULL) q->tail = p;
    q->count++;

    printf("DEBUG: inserted after %p, new tail=%p, count=%d\n", (void*)cur, (void*)q->tail, q->count);
    fflush(stdout);
}

Patient* pq_dequeue(PriorityQueue* q) {
    if (!q || q->head == NULL) return NULL;
    Patient* p = q->head;
    q->head = p->next;
    if (q->head == NULL) q->tail = NULL;
    p->next = NULL;
    q->count--;
    return p;
}

Patient* pq_peek(PriorityQueue* q) {
    if (!q) return NULL;
    return q->head;
}

/* Get queue size */
int pq_size(PriorityQueue *q) {
    if (!q) return 0;
    return q->count;
}

/* Check if queue is empty */
int pq_is_empty(PriorityQueue *q) {
    if (!q) return 1;
    return q->count == 0;
}

/* Search patient by ID */
Patient* pq_search_by_id(PriorityQueue *q, int id) {
    if (!q) return NULL;
    Patient* cur = q->head;
    while (cur) {
        if (cur->id == id) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

/* Search patient by name (partial match) */
Patient* pq_search_by_name(PriorityQueue *q, const char *name) {
    if (!q || !name) return NULL;
    Patient* cur = q->head;
    while (cur) {
        if (cur->name && strstr(cur->name, name)) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

void pq_free_all(PriorityQueue* q) {
    if (!q) return;
    Patient* cur = q->head;
    while (cur) {
        Patient* nx = cur->next;
        free(cur);
        cur = nx;
    }
    q->head = q->tail = NULL;
    q->count = 0;
}

int pq_save_csv(PriorityQueue* q, const char* filepath) {
    if (!q || !filepath) return 0;
    FILE* f = fopen(filepath, "w");
    if (!f) return 0;
    fprintf(f, "id,phone,name,age,severity,arrival,problem\n");
    Patient* cur = q->head;
    while (cur) {
        fprintf(f, "%d,%lld,%s,%d,%d,%s,%s\n",
            cur->id,
            cur->phone_number,
            cur->name,
            cur->age,
            (int)cur->severity,
            cur->arrival,
            cur->problem);
        cur = cur->next;
    }
    fclose(f);
    return 1;
}

int pq_load_csv(PriorityQueue* q, const char* filepath, int* nextId) {
    if (!q || !filepath) return 0;
    FILE* f = fopen(filepath, "r");
    if (!f) return 0;
    char line[512];
    // skip header
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; }
    while (fgets(line, sizeof(line), f)) {
        int id, age, sev;
        long long phone;
        char name[128], problem[256], arrival[64];
        /* format: id,phone,name,age,sev,problem,arrival */
        int r = sscanf(line, "%d,%lld,%127[^,],%d,%d,%63[^,],%255[^\n]",
   &id, &phone, name, &age, &sev, arrival, problem);
        if (r >= 7) {
            Patient* p = create_patient(id, phone, name, age, problem, (Severity)sev, arrival);
            if (p) pq_enqueue(q, p);
            if (nextId) *nextId = id + 1;
        }
    }
    fclose(f);
    return 1;
}
