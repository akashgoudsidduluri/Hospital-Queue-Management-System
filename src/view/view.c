#include "view.h"
#include <stdio.h>
#include <string.h>

void view_show_menu(void) {
    printf("\n\n\n=== Hospital Queue Management ===\n");
    printf("1. Register new patient : \n");
    printf("2. Show waiting list : \n");
    printf("3. Call next patient (serve) : \n");
    printf("4. View next patient (peek) : \n");
    printf("5. Search patient by ID or name : \n");
    printf("6. Save waiting list to CSV : \n");
    printf("7. View statistics\n");
    printf("8. Clear queue (delete all) : \n");
    printf("9. View served history\n");
    printf("10. Show average wait times\n");
    printf("11. Exit\n");
}

void view_show_patient(const Patient* p) {
    if (!p) {
        printf("No patient found\n");
        return;
    }

    const char *sev_str;
    switch ((int)p->severity) {
        case 2: sev_str = "CRITICAL"; break;
        case 1: sev_str = "SERIOUS";  break;
        default: sev_str = "NORMAL";  break;
    }

    printf("\nID: %d\n", p->id);
    printf("Phone Number: %lld\n", p->phone_number);
    printf("Patient name: %s\n", p->name);
    printf("Patient Age: %d\n", p->age);
    printf("Severity: %s\n", sev_str);
    printf("Arrival: %s\n", p->arrival);
    printf("Problem: %s\n\n", p->problem);
}

void view_show_list(PriorityQueue* q) {
    if (!q || !q->head) {
        printf("Queue is empty\n");
        return;
    }

    printf("\n");
    printf("%-4s | %-25s | %-5s | %-8s | %-19s | %-20s\n",
           "ID", "Name", "Age", "Severity", "Arrival", "Problem");
    printf("----------------------------------------------------------------------------------------------------------\n");

    Patient* cur = q->head;
    while (cur) {
        const char *sev_str = cur->severity == 2 ? "CRITICAL" : (cur->severity == 1 ? "SERIOUS" : "NORMAL");
        printf("%-4d | %-25.25s | %-5d | %-8s | %-19s | %-20.20s\n",
               cur->id, cur->name, cur->age, sev_str, cur->arrival, cur->problem);
        cur = cur->next;
    }

    int count = 0;
    cur = q->head;
    while (cur) { count++; cur = cur->next; }
    printf("\nTotal waiting: %d\n", count);
}

void view_show_stats(int totalAdded, int served, PriorityQueue* q) {
    int waiting = 0;
    Patient* cur = q->head;
    while (cur) { waiting++; cur = cur->next; }

    printf("\n--- Stats ---\n");
    printf("Total registered: %d\n", totalAdded);
    printf("Total served: %d\n", served);
    printf("Currently waiting: %d\n", waiting);
}

void clear_queue_with_confirmation(PriorityQueue* q) {
    printf("Are you sure? (y/n): ");
    char buf[8];
    if (fgets(buf, sizeof(buf), stdin)) {
        if (buf[0] == 'y' || buf[0] == 'Y') {
            pq_free_all(q);
            pq_init(q);
            printf("Queue cleared\n");
        }
    }
}
