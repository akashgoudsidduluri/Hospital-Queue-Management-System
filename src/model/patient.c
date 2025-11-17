#include "patient.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

Patient* create_patient(int id, long long phone_number, const char *name, int age, const char *problem, Severity sev, const char *arrival) {
    printf("DEBUG:create_patient ENTER id=%d phone=%lld name='%s' age=%d sev=%d arrival='%s'\n",
           id, phone_number, name?name:"(null)", age, (int)sev, arrival?arrival:"(null)");
    fflush(stdout);

    Patient *p = malloc(sizeof(Patient));
    if (!p) {
        printf("DEBUG:create_patient malloc FAILED\n"); fflush(stdout); return NULL;
    }
    p->id = id;
    p->phone_number = phone_number; // store full 64-bit number
    p->name = strdup(name ? name : "");
    p->age = age;
    p->severity = sev;
    if (arrival) {
        strncpy(p->arrival, arrival, TIME_LEN-1);
        p->arrival[TIME_LEN-1] = '\0';
    } else p->arrival[0] = '\0';
    p->problem = strdup(problem ? problem : "");
    p->next = NULL;

    printf("DEBUG:create_patient EXIT p=%p name=%s phone=%lld\n", (void*)p, p->name?p->name:"(null)", p->phone_number);
    fflush(stdout);
    return p;
}

void free_patient(Patient* p) {
    if (!p) return;
    free(p);
}