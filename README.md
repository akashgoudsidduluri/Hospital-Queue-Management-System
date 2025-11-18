# Hospital Queue Management System (Priority Queue) - C (MVC)

**Description:**  
A simple hospital queue management system implemented in C using MVC architecture.  
It uses a linked-list based priority queue (higher severity first) and persists queue data to a CSV file.

## **Quick Start**

### **Prerequisites**
- GCC compiler installed
- Windows/Linux/macOS

### **Compile**
```bash
gcc -I./src -o hospital_queue src/main.c src/controller/controller.c src/auth/auth.c src/model/patient.c src/model/queue.c src/view/view.c src/util/time_util.c
```

### **Run**
```bash
./hospital_queue.exe
```

### **Login Credentials**

| Username | Password |
|----------|----------|
| `akash`  | (empty)  |
| `nitiin` | (empty)  |

**Note:** Just press Enter when prompted for password.

---

**Features (Use cases):**
1. Register new patient (with severity: 2=Critical,1=Serious,0=Normal)
2. View waiting list (ordered by severity and arrival)
3. Serve next patient (dequeue)
4. Search patient by ID or name
5. View statistics (total added, waiting, served)
6. Persist queue to CSV and load from CSV
7. Patient Registration with authentication
8. Priority-based Queue Management
9. Real-time Analytics (10 NEW AI features)
10. CSV Data Persistence
11. Staff Performance Tracking

**How to compile:**
```bash
make
```

**How to run:**
```bash
./hospital_queue
```

**Project structure:**
- src/: C source files
  - model/: patient and queue (data layer)
  - view/: console UI functions
  - controller/: app logic (glue)
  - util/: helpers (time, csv)
- data/queue.csv : sample storage file created at runtime
- docs/: documentation (abstract, algorithm, implementation, future scope, learning)

**Team & Submission notes:**  
- Two students per team. Make sure both present.  
- Upload to each member's GitHub and GitLab.

