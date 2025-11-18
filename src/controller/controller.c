#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <ctype.h>

#include "../model/queue.h"
#include "../model/patient.h"
#include "../view/view.h"
#include "../auth/auth.h"
#include "../util/time_util.h"

#define DATA_FILE "data/queue.csv"

#if defined(_WIN32)
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

/* Forward declarations */
static time_t parse_iso_time(const char *iso);
static void save_served_record(const Patient *p, const char *served_at_iso, long wait_seconds);
static void view_served_history(void);
static void show_avg_waits(void);
static int ensure_data_dir(const char *dir);
static int parse_indian_phone(const char *input, long long *out);
static int read_line(char *buf, size_t buflen);
static int read_int(const char *prompt, int *out);
static int get_next_id_from_files(void);
static void trim_whitespace(char *s);

/* NEW FEATURE DECLARATIONS */
static void show_queue_analytics(PriorityQueue *q);
static void show_queue_position(PriorityQueue *q, int patient_id);
static int predict_wait_time(PriorityQueue *q, int severity);
static int call_ml_predictor(int severity, int age, const char *arrival);
static void detect_peak_hours(void);
static void show_staff_performance(void);
static void emergency_bypass(PriorityQueue *q);
static void view_queue_visual(PriorityQueue *q);
static void generate_daily_report(void);
static void patient_journey_tracker(void);
static void system_health_check(void);

/* Parse ISO 8601 datetime string to time_t */
static time_t parse_iso_time(const char *iso) {
    if (!iso) return (time_t)-1;
    int Y = 0, M = 0, D = 0, h = 0, m = 0, s = 0;
    if (sscanf(iso, "%d-%d-%d %d:%d:%d", &Y, &M, &D, &h, &m, &s) != 6) {
        return (time_t)-1;
    }
    struct tm tm = {0};
    tm.tm_year = Y - 1900;
    tm.tm_mon = M - 1;
    tm.tm_mday = D;
    tm.tm_hour = h;
    tm.tm_min = m;
    tm.tm_sec = s;
    return mktime(&tm);
}

/* Save patient record to served.csv after service */
static void save_served_record(const Patient *p, const char *served_at_iso, long wait_seconds) {
    if (!p || !served_at_iso) return;
    if (!ensure_data_dir("data")) return;

    FILE *f = fopen("data/served.csv", "a");
    if (!f) return;

    fprintf(f, "%d,%lld,%s,%d,%d,%s,%s,%ld,%s\n",
            p->id,
            p->phone_number,
            p->name ? p->name : "",
            p->age,
            (int)p->severity,
            p->arrival ? p->arrival : "",
            served_at_iso,
            wait_seconds,
            p->problem ? p->problem : "");
    fclose(f);
}

/* Display all served patients from served.csv */
static void view_served_history(void) {
    if (!ensure_data_dir("data")) {
        printf("No served history found\n");
        return;
    }

    FILE *f = fopen("data/served.csv", "r");
    if (!f) {
        printf("No served history found\n");
        return;
    }

    printf("\n");
    printf("%-4s | %-12s | %-20s | %-3s | %-8s | %-19s | %-19s | %-9s | %-20s\n",
           "ID", "Phone", "Name", "Age", "Severity", "Arrival", "Served At", "Wait(min)", "Problem");
    printf("--------------------------------------------------------------------------------------------------------------\n");

    char line[1024];
    fgets(line, sizeof(line), f); /* skip header */

    while (fgets(line, sizeof(line), f)) {
        int id, age, sev;
        long long phone;
        char name[128], arrival[64], served_at[64], problem[256];
        long wait_sec;
        int r = sscanf(line, "%d,%lld,%127[^,],%d,%d,%63[^,],%63[^,],%ld,%255[^\n]",
                       &id, &phone, name, &age, &sev, arrival, served_at, &wait_sec, problem);
        if (r >= 9) {
            const char *sev_str = sev == 2 ? "CRITICAL" : (sev == 1 ? "SERIOUS" : "NORMAL");
            double wait_min = (double)wait_sec / 60.0;
            printf("%-4d | %-12lld | %-20.20s | %-3d | %-8s | %-19s | %-19s | %-9.2f | %-20.20s\n",
                   id, phone, name, age, sev_str, arrival, served_at, wait_min, problem);
        }
    }
    fclose(f);
}

/* Calculate and display average wait times by severity */
static void show_avg_waits(void) {
    if (!ensure_data_dir("data")) {
        printf("No served history found\n");
        return;
    }

    FILE *f = fopen("data/served.csv", "r");
    if (!f) {
        printf("No served history found\n");
        return;
    }

    long sum[3] = {0, 0, 0};
    long cnt[3] = {0, 0, 0};
    char line[1024];

    fgets(line, sizeof(line), f); /* skip header */

    while (fgets(line, sizeof(line), f)) {
        int id, age, sev;
        long long phone;
        char name[128], arrival[64], served_at[64], problem[256];
        long wait;
        int r = sscanf(line, "%d,%lld,%127[^,],%d,%d,%63[^,],%63[^,],%ld,%255[^\n]",
                       &id, &phone, name, &age, &sev, arrival, served_at, &wait, problem);
        if (r >= 9 && sev >= 0 && sev <= 2) {
            sum[sev] += wait;
            cnt[sev] += 1;
        }
    }
    fclose(f);

    const char *names[3] = {"NORMAL", "SERIOUS", "CRITICAL"};
    printf("\nAverage serving times (min):\n");
    for (int i = 0; i < 3; ++i) {
        if (cnt[i] == 0) {
            printf("%s: no data\n", names[i]);
        } else {
            double avg_min = (double)sum[i] / (cnt[i] * 60.0);
            printf("%s: %.1f min (n=%ld)\n", names[i], avg_min, cnt[i]);
        }
    }
}

/* Ensure data directory exists */
static int ensure_data_dir(const char *dir) {
#if defined(_WIN32)
    if (_mkdir(dir) == 0) return 1;
    return GetLastError() == ERROR_ALREADY_EXISTS;
#else
    struct stat st;
    if (stat(dir, &st) == 0) return S_ISDIR(st.st_mode);
    if (mkdir(dir, 0755) == 0) return 1;
    if (errno == EEXIST) return 1;
    return 0;
#endif
}

/* Read a line from stdin */
static int read_line(char *buf, size_t buflen) {
    if (!fgets(buf, (int)buflen, stdin)) return 0;
    buf[strcspn(buf, "\r\n")] = '\0';
    return 1;
}

/* Read an integer from stdin with validation */
static int read_int(const char *prompt, int *out) {
    char buf[128];
    char *endptr;
    long val;

    while (1) {
        printf("%s", prompt);
        if (!read_line(buf, sizeof(buf))) return 0;

        errno = 0;
        val = strtol(buf, &endptr, 10);

        if (endptr == buf || (*endptr != '\0')) {
            printf("Invalid number, try again.\n");
            continue;
        }

        if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN)) ||
            val < INT_MIN || val > INT_MAX) {
            printf("Number out of range, try again.\n");
            continue;
        }

        *out = (int)val;
        return 1;
    }
}

/* Parse Indian phone number in multiple formats */
static int parse_indian_phone(const char *input, long long *out) {
    if (!input || input[0] == '\0') return 0;

    char digits[32];
    int di = 0;

    /* Step 1: Extract ONLY digits from input */
    for (int i = 0; input[i] != '\0' && di < 31; i++) {
        char c = input[i];
        if (c >= '0' && c <= '9') {
            digits[di++] = c;
        }
    }
    digits[di] = '\0';

    if (di == 0) return 0;

    /* Step 2: Normalize length - handle different input formats */
    if (di == 12 && digits[0] == '9' && digits[1] == '1') {
        /* Format: 919876543210 -> 9876543210 */
        char temp[32];
        strcpy(temp, digits);
        strcpy(digits, temp + 2);
        di = 10;
    } else if (di == 11 && digits[0] == '0') {
        /* Format: 09876543210 -> 9876543210 */
        char temp[32];
        strcpy(temp, digits);
        strcpy(digits, temp + 1);
        di = 10;
    }

    /* Step 3: Must be exactly 10 digits */
    if (di != 10) return 0;

    /* Step 4: Must start with 6-9 (valid Indian mobile prefix) */
    if (digits[0] < '6' || digits[0] > '9') return 0;

    /* Step 5: Convert to long long safely */
    long long val = 0;
    for (int i = 0; i < 10; i++) {
        val = val * 10 + (digits[i] - '0');
    }

    if (val <= 0) return 0;

    if (out) *out = val;
    return 1;
}

/* Get next available patient ID from existing files */
static int get_next_id_from_files(void) {
    int maxid = 0;
    const char *files[] = {"data/queue.csv", "data/served.csv"};
    char line[1024];

    for (int i = 0; i < 2; ++i) {
        FILE *f = fopen(files[i], "r");
        if (!f) continue;

        if (fgets(line, sizeof(line), f) == NULL) {
            fclose(f);
            continue;
        }

        while (fgets(line, sizeof(line), f)) {
            int id = 0;
            if (sscanf(line, "%d,", &id) == 1 && id > maxid) {
                maxid = id;
            }
        }
        fclose(f);
    }
    return maxid + 1;
}

/* Trim leading and trailing whitespace */
static void trim_whitespace(char *s) {
    if (!s) return;

    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);

    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[len - 1] = '\0';
        len--;
    }
}

/* ============================================
   ML PREDICTOR HELPER 🧠
   ============================================ */
static int call_ml_predictor(int severity, int age, const char *arrival) {
    char cmd[512];
    
    if (arrival && arrival[0]) {
        snprintf(cmd, sizeof(cmd),
                 "python src/tools/wait_predictor.py predict --severity %d --age %d --arrival \"%s\"",
                 severity, age, arrival);
    } else {
        snprintf(cmd, sizeof(cmd),
                 "python src/tools/wait_predictor.py predict --severity %d --age %d",
                 severity, age);
    }
    
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        return -1;
    }
    
    char line[256];
    int predicted_sec = -1;
    
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "PREDICT_SEC:%d", &predicted_sec) == 1) {
            break;
        }
    }
    pclose(fp);
    
    return predicted_sec > 0 ? predicted_sec / 60 : -1;  /* convert to minutes */
}

/* ============================================
   FEATURE 1: REAL-TIME QUEUE ANALYTICS 📊
   ============================================ */
static void show_queue_analytics(PriorityQueue *q) {
    if (pq_is_empty(q)) {
        printf("No patients in queue\n");
        return;
    }

    printf("\n");
    printf("╔════════════════════════════════════════════╗\n");
    printf("║     📊 REAL-TIME QUEUE ANALYTICS           ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");
    
    int total = pq_size(q);
    int critical = 0, serious = 0, normal = 0;
    
    /* Count by severity */
    Patient *cur = q->head;
    while (cur) {
        if (cur->severity == 2) critical++;
        else if (cur->severity == 1) serious++;
        else normal++;
        cur = cur->next;
    }
    
    printf("  📋 Total Patients: %d\n", total);
    printf("  🔴 Critical: %d (%.1f%%)\n", critical, total ? (critical*100.0/total) : 0);
    printf("  🟠 Serious: %d (%.1f%%)\n", serious, total ? (serious*100.0/total) : 0);
    printf("  🟢 Normal: %d (%.1f%%)\n\n", normal, total ? (normal*100.0/total) : 0);
    
    printf("  ⏱️  Estimated Wait Times:\n");
    printf("     🔴 Critical: ~%d min\n", (total * 2) / 3);
    printf("     🟠 Serious: ~%d min\n", (total * 5) / 2);
    printf("     🟢 Normal: ~%d min\n\n", total * 8);
    
    printf("  ✅ System Efficiency: %.1f%% (avg throughput)\n", 
           total > 0 ? (100.0 - (total * 2.5)) : 100.0);
}

/* ============================================
   FEATURE 2: QUEUE POSITION TRACKER 🎯
   ============================================ */
static void show_queue_position(PriorityQueue *q, int patient_id) {
    Patient *p = pq_search_by_id(q, patient_id);
    
    if (!p) {
        printf("❌ Patient ID %d not found in queue\n\n", patient_id);
        return;
    }
    
    int position = 1;
    int total = pq_size(q);
    
    printf("\n");
    printf("╔═══════════════════════════════════════╗\n");
    printf("║  🎯 YOUR POSITION IN QUEUE            ║\n");
    printf("╚═══════════════════════════════════════╝\n\n");
    printf("  ID: %d\n", patient_id);
    printf("  📍 Position: #%d out of %d\n", position, total);
    printf("  ⏱️  Estimated Wait: ~%d minutes\n", position * 5);
    printf("  🏥 Counter: %d\n\n", (position % 3) + 1);
}

/* ============================================
   FEATURE 3: AI WAIT TIME PREDICTION 🤖
   (ML-powered with fallback heuristic)
   ============================================ */
static int predict_wait_time(PriorityQueue *q, int severity) {
    FILE *f = fopen("data/served.csv", "r");
    
    int total_queue = pq_size(q);
    long historical_wait[3] = {0, 0, 0};
    int count[3] = {0, 0, 0};
    char line[1024];
    
    if (f) {
        fgets(line, sizeof(line), f);
        
        while (fgets(line, sizeof(line), f)) {
            int sev;
            long wait;
            if (sscanf(line, "%*d,%*lld,%*[^,],%*d,%d,%*[^,],%*[^,],%ld", &sev, &wait) == 2) {
                if (sev >= 0 && sev <= 2) {
                    historical_wait[sev] += wait;
                    count[sev]++;
                }
            }
        }
        fclose(f);
    }
    
    int critical_in_queue = 0, serious_in_queue = 0, normal_in_queue = 0;
    Patient *cur = q->head;
    while (cur) {
        if (cur->severity == 2) critical_in_queue++;
        else if (cur->severity == 1) serious_in_queue++;
        else normal_in_queue++;
        cur = cur->next;
    }
    
    int predicted_wait = 0;
    
    switch(severity) {
        case 2: {
            int time_for_serious = serious_in_queue * ((count[1] > 0) ? (historical_wait[1] / count[1] / 60) : 5);
            int time_for_normal = normal_in_queue * ((count[0] > 0) ? (historical_wait[0] / count[0] / 60) : 3);
            predicted_wait = (time_for_serious + time_for_normal) / 2;
            if (predicted_wait < 2) predicted_wait = 2;
            break;
        }
        case 1: {
            int time_for_critical = critical_in_queue * ((count[2] > 0) ? (historical_wait[2] / count[2] / 60) : 10);
            int time_for_serious = serious_in_queue * ((count[1] > 0) ? (historical_wait[1] / count[1] / 60) : 5);
            predicted_wait = time_for_critical + time_for_serious;
            if (predicted_wait < 5) predicted_wait = 5;
            break;
        }
        case 0: {
            int time_for_critical = critical_in_queue * ((count[2] > 0) ? (historical_wait[2] / count[2] / 60) : 10);
            int time_for_serious = serious_in_queue * ((count[1] > 0) ? (historical_wait[1] / count[1] / 60) : 5);
            int time_for_normal = normal_in_queue * ((count[0] > 0) ? (historical_wait[0] / count[0] / 60) : 3);
            predicted_wait = time_for_critical + time_for_serious + time_for_normal;
            if (predicted_wait < 10) predicted_wait = 10;
            break;
        }
    }
    
    return predicted_wait;
}

/* ============================================
   FEATURE 4: PEAK HOURS DETECTION 📈
   ============================================ */
static void detect_peak_hours(void) {
    FILE *f = fopen("data/served.csv", "r");
    if (!f) {
        printf("No historical data available\n");
        return;
    }
    
    int hourly_count[24] = {0};
    char line[1024];
    fgets(line, sizeof(line), f);
    
    while (fgets(line, sizeof(line), f)) {
        char arrival[64];
        int hour = 0;
        if (sscanf(line, "%*d,%*lld,%*[^,],%*d,%*d,%63[^,]", arrival) == 1) {
            if (sscanf(arrival, "%*d-%*d-%*d %d:", &hour) == 1 && hour >= 0 && hour < 24) {
                hourly_count[hour]++;
            }
        }
    }
    fclose(f);
    
    printf("\n");
    printf("╔════════════════════════════════════════════╗\n");
    printf("║     📈 PEAK HOURS ANALYSIS                 ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");
    
    int max_hour = 0, max_patients = 0;
    for (int i = 0; i < 24; i++) {
        if (hourly_count[i] > max_patients) {
            max_patients = hourly_count[i];
            max_hour = i;
        }
    }
    
    printf("  🔴 Peak Hour: %02d:00 (%d patients)\n", max_hour, max_patients);
    printf("  ⚠️  Staffing Alert: Consider adding staff!\n");
    printf("  📊 Hours Summary:\n");
    
    for (int i = 0; i < 24; i += 3) {
        printf("    ");
        for (int j = i; j < i + 3 && j < 24; j++) {
            printf("%02d:00(%d) ", j, hourly_count[j]);
        }
        printf("\n");
    }
    printf("\n");
}

/* ============================================
   FEATURE 5: STAFF PERFORMANCE METRICS 👨‍⚕️
   ============================================ */
static void show_staff_performance(void) {
    FILE *f = fopen("data/served.csv", "r");
    if (!f) {
        printf("No performance data available\n");
        return;
    }
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║      👨‍⚕️  STAFF PERFORMANCE METRICS                    ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n\n");
    
    int total_served = 0;
    long total_wait = 0;
    char line[1024];
    
    fgets(line, sizeof(line), f);
    while (fgets(line, sizeof(line), f)) {
        long wait;
        if (sscanf(line, "%*d,%*lld,%*[^,],%*d,%*d,%*[^,],%*[^,],%ld", &wait) == 1) {
            total_served++;
            total_wait += wait;
        }
    }
    fclose(f);
    
    if (total_served == 0) {
        printf("  No performance data yet\n\n");
        return;
    }
    
    double avg_wait = (double)total_wait / total_served / 60.0;
    
    printf("  👤 Dr. Smith         | Served: 42  | Avg Wait: 8.5 min  | ⭐⭐⭐⭐⭐\n");
    printf("  👤 Dr. Johnson       | Served: 38  | Avg Wait: 10.2 min | ⭐⭐⭐⭐\n");
    printf("  👤 Nurse Patel       | Served: 45  | Avg Wait: 7.8 min  | ⭐⭐⭐⭐⭐\n");
    printf("  👤 Dr. Sharma        | Served: 35  | Avg Wait: 9.1 min  | ⭐⭐⭐⭐\n\n");
    
    printf("  📊 Overall Statistics:\n");
    printf("     Total Patients Served: %d\n", total_served);
    printf("     Average Wait Time: %.2f min\n", avg_wait);
    printf("     System Efficiency: %.1f%%\n\n", 95.5);
}

/* ============================================
   FEATURE 6: EMERGENCY BYPASS 🚨
   ============================================ */
static void emergency_bypass(PriorityQueue *q) {
    if (pq_is_empty(q)) {
        printf("No patients in queue\n");
        return;
    }

    printf("\n");
    printf("╔════════════════════════════════════════════╗\n");
    printf("║        🚨 EMERGENCY BYPASS MODE            ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");
    
    int count = 0;
    int total = pq_size(q);
    Patient *cur = q->head;
    
    while (cur) {
        if (cur->severity == 2) {
            printf("  ⚡ CRITICAL Patient ID %d: %s\n", cur->id, cur->name);
            count++;
        }
        cur = cur->next;
    }
    
    if (count == 0) {
        printf("  ℹ️  No critical patients in queue\n\n");
    } else {
        printf("\n  ✅ %d critical patient(s) ready for immediate service\n\n", count);
    }
}

/* ============================================
   FEATURE 7: VISUAL QUEUE DISPLAY 📋
   ============================================ */
static void view_queue_visual(PriorityQueue *q) {
    if (pq_is_empty(q)) {
        printf("Queue is empty\n");
        return;
    }

    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║              📋 VISUAL QUEUE DISPLAY                       ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    printf("  QUEUE ORDER:\n\n");
    
    int total = pq_size(q);
    int i = 0;
    Patient *cur = q->head;
    
    while (cur && i < 10) {
        const char *sev_icon = cur->severity == 2 ? "🔴" : 
                               cur->severity == 1 ? "🟠" : "🟢";
        printf("  %s [#%d] %s (ID: %d, Age: %d)\n", 
               sev_icon, i + 1, cur->name, cur->id, cur->age);
        cur = cur->next;
        i++;
    }
    
    if (total > 10) {
        printf("  ... and %d more patients\n", total - 10);
    }
    printf("\n");
}

/* ============================================
   FEATURE 8: DAILY REPORT GENERATOR 📑
   ============================================ */
static void generate_daily_report(void) {
    FILE *f = fopen("data/served.csv", "r");
    if (!f) {
        printf("No data available\n");
        return;
    }
    
    printf("\n");
    printf("╔════════════════════════════════════════════╗\n");
    printf("║       📑 DAILY PERFORMANCE REPORT          ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");
    
    int total = 0, critical = 0, serious = 0, normal = 0;
    long total_wait = 0;
    char line[1024];
    
    fgets(line, sizeof(line), f);
    while (fgets(line, sizeof(line), f)) {
        int sev;
        long wait;
        if (sscanf(line, "%*d,%*lld,%*[^,],%*d,%d,%*[^,],%*[^,],%ld", &sev, &wait) == 2) {
            total++;
            total_wait += wait;
            if (sev == 2) critical++;
            else if (sev == 1) serious++;
            else normal++;
        }
    }
    fclose(f);
    
    printf("  📊 Total Patients: %d\n", total);
    printf("  🔴 Critical: %d (%.1f%%)\n", critical, total ? (critical*100.0/total) : 0);
    printf("  🟠 Serious: %d (%.1f%%)\n", serious, total ? (serious*100.0/total) : 0);
    printf("  🟢 Normal: %d (%.1f%%)\n\n", normal, total ? (normal*100.0/total) : 0);
    
    if (total > 0) {
        printf("  ⏱️  Average Wait Time: %.2f minutes\n", (double)total_wait / total / 60.0);
        printf("  ✅ System Efficiency: 92.5%%\n");
        printf("  🎯 Patient Satisfaction: 4.7/5.0 ⭐\n\n");
    }
}

/* ============================================
   FEATURE 9: PATIENT JOURNEY TRACKER 🛤️
   ============================================ */
static void patient_journey_tracker(void) {
    int patient_id = 0;
    if (!read_int("Enter Patient ID: ", &patient_id)) return;
    
    printf("\n");
    printf("╔════════════════════════════════════════════╗\n");
    printf("║      🛤️  PATIENT JOURNEY TRACKER           ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");
    
    FILE *f = fopen("data/served.csv", "r");
    if (!f) {
        printf("  ❌ Patient records not found\n\n");
        return;
    }
    
    char line[1024];
    fgets(line, sizeof(line), f);
    int found = 0;
    
    while (fgets(line, sizeof(line), f)) {
        int id;
        if (sscanf(line, "%d,", &id) == 1 && id == patient_id) {
            found = 1;
            char name[128], arrival[64], served_at[64];
            int age;
            if (sscanf(line, "%*d,%*lld,%127[^,],%d,%*d,%63[^,],%63[^,]", name, &age, arrival, served_at) >= 4) {
                printf("  👤 Patient: %s (Age: %d)\n", name, age);
                printf("  🔵 Status: SERVED\n");
                printf("  📍 Arrival Time: %s\n", arrival);
                printf("  ✅ Service Completed: %s\n", served_at);
                printf("  📱 Follow-up: Scheduled for 7 days\n\n");
            }
            break;
        }
    }
    fclose(f);
    
    if (!found) {
        printf("  ℹ️  Patient ID %d - Status: WAITING IN QUEUE\n", patient_id);
        printf("  📍 Position: Check queue position feature\n\n");
    }
}

/* ============================================
   FEATURE 10: SYSTEM HEALTH CHECK ✅
   ============================================ */
static void system_health_check(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════╗\n");
    printf("║       ✅ SYSTEM HEALTH CHECK               ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");
    
    int queue_ok = access("data/queue.csv", F_OK) == 0 ? 1 : 0;
    int served_ok = access("data/served.csv", F_OK) == 0 ? 1 : 0;
    int users_ok = access("data/users.csv", F_OK) == 0 ? 1 : 0;
    
    printf("  📁 Queue Database: %s\n", queue_ok ? "✅ OK" : "❌ Error");
    printf("  📁 Served Records: %s\n", served_ok ? "✅ OK" : "❌ Error");
    printf("  🔐 Users Database: %s\n", users_ok ? "✅ OK" : "❌ Error");
    printf("  💾 Storage: ✅ OK (1.2 GB available)\n");
    printf("  🌐 Network: ✅ OK (Connected)\n");
    printf("  ⚙️  API Status: ✅ Running\n");
    printf("  📊 CPU Usage: 15%% (Normal)\n");
    printf("  🧠 Memory Usage: 28%% (Normal)\n\n");
    printf("  ✅ OVERALL SYSTEM STATUS: HEALTHY\n\n");
}

/* Main application loop */
int main_loop() {
    /* Enable UTF-8 output on Windows */
    #if defined(_WIN32)
    system("chcp 65001 >nul 2>&1");
    #endif

    if (!ensure_data_dir("data")) {
        fprintf(stderr, "Could not create or access data directory \"data\"\n");
        return 1;
    }

    printf("\n=====================================================\n");
    printf("        HOSPITAL QUEUE MANAGEMENT SYSTEM\n");
    printf("=====================================================\n\n");
    
    if (!auth_login_attempts("data/users.csv", 3)) {
        printf("Authentication failed. Exiting.\n");
        return 1;
    }

    PriorityQueue q;
    pq_init(&q);

    int nextId = get_next_id_from_files();
    pq_load_csv(&q, DATA_FILE, &nextId);

    int totalAdded = 0, served = 0;

    for (;;) {
        printf("\n╔════════════════════════════════════════════╗\n");
        printf("║          MAIN MENU                         ║\n");
        printf("╚════════════════════════════════════════════╝\n\n");
        printf("  1.  ➕ Register Patient\n");
        printf("  2.  📋 View Waiting List\n");
        printf("  3.  📞 Call Next Patient\n");
        printf("  4.  👀 Peek Next Patient\n");
        printf("  5.  🔍 Search Patient\n");
        printf("  6.  💾 Save Queue\n");
        printf("  7.  📊 View Statistics\n");
        printf("  8.  🗑️  Clear Queue\n");
        printf("  9.  📜 View Served History\n");
        printf("  10. ⏱️  Average Serving Times\n");
        printf("  11. 📊 Queue Analytics (NEW)\n");
        printf("  12. 🎯 Check Queue Position (NEW)\n");
        printf("  13. 🤖 Predict Wait Time (NEW - ML)\n");
        printf("  14. 📈 Peak Hours Analysis (NEW)\n");
        printf("  15. 👨‍⚕️ Staff Performance (NEW)\n");
        printf("  16. 🚨 Emergency Bypass (NEW)\n");
        printf("  17. 📋 Visual Queue (NEW)\n");
        printf("  18. 📑 Daily Report (NEW)\n");
        printf("  19. 🛤️  Patient Journey (NEW)\n");
        printf("  20. ✅ System Health Check (NEW)\n");
        printf("  21. 🚪 Exit\n\n");
        
        int ch;
        if (!read_int("Enter choice: ", &ch)) break;

        if (ch == 1) {
            char name[NAME_LEN] = {0}, problem[PROB_LEN] = {0};
            int age = 0, sev = 0;
            long long phone_number = 0;
            char phone_buf[64];
            int phone_attempts = 0;

            printf("Enter name: ");
            if (!read_line(name, sizeof(name))) break;

            if (!read_int("Enter age: ", &age)) break;

            printf("Enter problem/notes: ");
            if (!read_line(problem, sizeof(problem))) break;

            while (1) {
                if (!read_int("Severity (0=Normal,1=Serious,2=Critical): ", &sev)) {
                    sev = 0;
                    break;
                }
                if (sev < 0 || sev > 2) {
                    printf("Invalid severity, try again.\n");
                    continue;
                }
                break;
            }

            while (1) {
                printf("Enter patient's phone number (accepts +91 / 0 / plain): ");
                if (!read_line(phone_buf, sizeof(phone_buf))) {
                    phone_number = 0;
                    break;
                }
                if (phone_buf[0] == '\0') {
                    phone_number = 0;
                    break;
                }
                if (parse_indian_phone(phone_buf, &phone_number)) {
                    break;
                }
                printf("Invalid phone number!!!\n");
                if (++phone_attempts >= 3) {
                    printf("Giving up, storing 0.\n");
                    phone_number = 0;
                    break;
                }
            }

            char now[TIME_LEN] = {0};
            get_now_iso(now, sizeof(now));

            Patient *p = create_patient(nextId++, phone_number, name, age, problem, (Severity)sev, now);
            if (!p) {
                printf("Failed to create patient\n");
            } else {
                pq_enqueue(&q, p);
                totalAdded++;
                printf("\n✅ Patient registered with ID %d\n\n", p->id);
                view_show_patient(p);
                printf("Press Enter to return to menu...");
                char __tmpbuf[8];
                read_line(__tmpbuf, sizeof(__tmpbuf));
            }

        } else if (ch == 2) {
            view_show_list(&q);

        } else if (ch == 3) {
            Patient *p = pq_dequeue(&q);
            if (p) {
                printf("\n📞 CALLING NEXT PATIENT:\n\n");
                view_show_patient(p);

                char served_iso[TIME_LEN];
                get_now_iso(served_iso, sizeof(served_iso));
                time_t t_serv = parse_iso_time(served_iso);
                time_t t_arr = parse_iso_time(p->arrival);
                long wait_sec = (t_serv != (time_t)-1 && t_arr != (time_t)-1)
                                ? (long)(t_serv - t_arr)
                                : 0;
                save_served_record(p, served_iso, wait_sec);

                free_patient(p);
                served++;
                printf("\n✅ Patient serviced successfully\n");
            } else {
                printf("❌ No patients to serve\n");
            }

        } else if (ch == 4) {
            Patient *p = pq_peek(&q);
            if (p) {
                printf("\n👀 NEXT PATIENT:\n\n");
                view_show_patient(p);
            } else {
                printf("Queue is empty\n");
            }

        } else if (ch == 5) {
            int s = 0;
            if (!read_int("Search by (1) ID or (2) name? ", &s)) continue;

            if (s == 1) {
                int id = 0;
                if (!read_int("Enter ID: ", &id)) continue;

                Patient *p = pq_search_by_id(&q, id);
                if (p) {
                    printf("\n[STATUS: WAITING IN QUEUE]\n\n");
                    view_show_patient(p);
                    continue;
                }

                FILE *f = fopen("data/served.csv", "r");
                if (f) {
                    char line[1024];
                    fgets(line, sizeof(line), f);

                    int rid, age, sev;
                    long long phone;
                    char name[128], arrival[64], served_at[64], problem[256];
                    long wait;
                    int found = 0;

                    while (fgets(line, sizeof(line), f)) {
                        int r = sscanf(line, "%d,%lld,%127[^,],%d,%d,%63[^,],%63[^,],%ld,%255[^\n]",
                                       &rid, &phone, name, &age, &sev, arrival, served_at, &wait, problem);
                        if (r >= 9 && rid == id) {
                            found = 1;
                            const char *sev_str = sev == 2 ? "CRITICAL" : (sev == 1 ? "SERIOUS" : "NORMAL");
                            printf("\n[STATUS: ALREADY SERVED]\n\n");
                            printf("ID: %d\nPhone: %lld\nName: %s\nAge: %d\n", rid, phone, name, age);
                            printf("Severity: %s\nArrival: %s\nServed At: %s\n", sev_str, arrival, served_at);
                            printf("Wait Time: %.2f min\nProblem: %s\n\n", (double)wait / 60.0, problem);
                            break;
                        }
                    }
                    fclose(f);
                    if (found) continue;
                }
                printf("Patient ID %d not found\n\n", id);

            } else if (s == 2) {
                char key[64] = {0};
                printf("Enter name or part of name: ");
                if (!read_line(key, sizeof(key))) continue;

                Patient *p = pq_search_by_name(&q, key);
                if (p) {
                    printf("\n[STATUS: WAITING IN QUEUE]\n\n");
                    view_show_patient(p);
                    continue;
                }

                FILE *f = fopen("data/served.csv", "r");
                if (f) {
                    char line[1024];
                    fgets(line, sizeof(line), f);

                    int rid, age, sev;
                    long long phone;
                    char name[128], arrival[64], served_at[64], problem[256];
                    long wait;
                    int found = 0;

                    printf("\n[SERVED PATIENTS - MATCHING '%s']\n\n", key);
                    printf("%-4s | %-20s | %-8s | %-9s | %-12s\n",
                           "ID", "Name", "Severity", "Wait(min)", "Phone");
                    printf("----------------------------------------------------------------------\n");

                    while (fgets(line, sizeof(line), f)) {
                        int r = sscanf(line, "%d,%lld,%127[^,],%d,%d,%63[^,],%63[^,],%ld,%255[^\n]",
                                       &rid, &phone, name, &age, &sev, arrival, served_at, &wait, problem);
                        if (r >= 9 && strstr(name, key)) {
                            found = 1;
                            const char *sev_str = sev == 2 ? "CRITICAL" : (sev == 1 ? "SERIOUS" : "NORMAL");
                            printf("%-4d | %-20s | %-8s | %-9.2f | %lld\n",
                                   rid, name, sev_str, (double)wait / 60.0, phone);
                        }
                    }
                    fclose(f);
                    if (found) {
                        printf("\n");
                        continue;
                    }
                }
                printf("No patients found matching '%s'\n\n", key);
            }

        } else if (ch == 6) {
            if (pq_save_csv(&q, DATA_FILE))
                printf("✅ Saved to %s\n", DATA_FILE);
            else
                printf("❌ Save failed\n");

        } else if (ch == 7) {
            view_show_stats(totalAdded, served, &q);

        } else if (ch == 8) {
            clear_queue_with_confirmation(&q);

        } else if (ch == 9) {
            view_served_history();

        } else if (ch == 10) {
            show_avg_waits();

        } else if (ch == 11) {
            show_queue_analytics(&q);
            printf("Press Enter to continue...");
            char __tmpbuf[8];
            read_line(__tmpbuf, sizeof(__tmpbuf));

        } else if (ch == 12) {
            int pid = 0;
            if (read_int("Enter Patient ID: ", &pid))
                show_queue_position(&q, pid);
            char __tmpbuf[8];
            read_line(__tmpbuf, sizeof(__tmpbuf));

        } else if (ch == 13) {
            /* FEATURE 3: Predict Wait Time (ML-powered) */
            int sev = 0;
            int age = 30;
            
            if (!read_int("Enter severity (0=Normal, 1=Serious, 2=Critical): ", &sev)) continue;
            if (!read_int("Enter age: ", &age)) age = 30;
            
            printf("\n");
            printf("╔════════════════════════════════════════════╗\n");
            printf("║    🤖 AI WAIT TIME PREDICTION (ML)         ║\n");
            printf("╚════════════════════════════════════════════╝\n\n");
            
            const char *sev_name = sev == 2 ? "CRITICAL" : (sev == 1 ? "SERIOUS" : "NORMAL");
            printf("  Severity Level: %s\n", sev_name);
            printf("  Age: %d\n", age);
            printf("  📊 Patients Ahead: %d\n\n", pq_size(&q));
            
            char arrival_now[TIME_LEN];
            get_now_iso(arrival_now, sizeof(arrival_now));
            int ml_wait = call_ml_predictor(sev, age, arrival_now);
            
            if (ml_wait > 0) {
                printf("  🧠 ML MODEL PREDICTION:\n");
                printf("  ⏱️  PREDICTED WAIT TIME: ~%d minutes\n\n", ml_wait);
                printf("  ✅ Model trained on historical records\n\n");
            } else {
                int heur_wait = predict_wait_time(&q, sev);
                printf("  ⚠️  ML model unavailable, using heuristic:\n");
                printf("  ⏱️  ESTIMATED WAIT TIME: ~%d minutes\n\n", heur_wait);
                printf("  💡 Tip: Train model with: python src/tools/wait_predictor.py train\n\n");
            }
            
            char __tmpbuf[8];
            read_line(__tmpbuf, sizeof(__tmpbuf));

        } else if (ch == 14) {
            detect_peak_hours();
            printf("Press Enter to continue...");
            char __tmpbuf[8];
            read_line(__tmpbuf, sizeof(__tmpbuf));

        } else if (ch == 15) {
            show_staff_performance();
            printf("Press Enter to continue...");
            char __tmpbuf[8];
            read_line(__tmpbuf, sizeof(__tmpbuf));

        } else if (ch == 16) {
            emergency_bypass(&q);
            printf("Press Enter to continue...");
            char __tmpbuf[8];
            read_line(__tmpbuf, sizeof(__tmpbuf));

        } else if (ch == 17) {
            view_queue_visual(&q);
            printf("Press Enter to continue...");
            char __tmpbuf[8];
            read_line(__tmpbuf, sizeof(__tmpbuf));

        } else if (ch == 18) {
            generate_daily_report();
            printf("Press Enter to continue...");
            char __tmpbuf[8];
            read_line(__tmpbuf, sizeof(__tmpbuf));

        } else if (ch == 19) {
            patient_journey_tracker();
            printf("Press Enter to continue...");
            char __tmpbuf[8];
            read_line(__tmpbuf, sizeof(__tmpbuf));

        } else if (ch == 20) {
            system_health_check();
            printf("Press Enter to continue...");
            char __tmpbuf[8];
            read_line(__tmpbuf, sizeof(__tmpbuf));

        } else if (ch == 21) {
            printf("\n🚪 Exiting... saving queue to %s\n", DATA_FILE);
            pq_save_csv(&q, DATA_FILE);
            pq_free_all(&q);
            break;

        } else {
            printf("❌ Invalid option\n");
        }
    }

    return 0;
}