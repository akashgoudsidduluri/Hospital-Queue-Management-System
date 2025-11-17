#ifndef PATIENT_H
#define PATIENT_H

#include "../util/time_util.h"

#define NAME_LEN 128
#define PROB_LEN 256

typedef enum { NORMAL = 0, SERIOUS = 1, CRITICAL = 2 } Severity;

typedef struct Patient {
    int id;
    long long phone_number;   // changed to 64-bit
    char *name;
    int age;
    Severity severity;
    char arrival[TIME_LEN];
    char *problem;
    struct Patient *next;
} Patient;

Patient* create_patient(int id, long long phone_number, const char *name, int age, const char *problem, Severity sev, const char *arrival);
void free_patient(Patient* p);

#endif
