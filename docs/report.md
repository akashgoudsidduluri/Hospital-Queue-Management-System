# Project Report — Hospital Queue Management System

## Project overview
This report documents the Hospital Queue Management System implemented in C (MVC). It contains design rationale, workflow, implementation details, sample outputs, and screenshots for submission.

## Contents
- Abstract
- Objectives
- Architecture and data model
- Core algorithms (enqueue, dequeue, search, persistence)
- Build & run instructions
- Workflow and UI walkthrough (step-by-step)
- Sample outputs (console excerpts)
- Limitations and future work

---

## Abstract
A compact CLI-based priority queue for hospital triage. Patients are ordered by severity (2=Critical,1=Serious,0=Normal). The project demonstrates data-structure design, file persistence (CSV), and modular C architecture suitable for a semester mini-project.

## Objectives
- Implement a priority queue using linked lists in C
- Persist queue and served history to CSV files
- Provide a console UI for registration, searching, serving, and reporting
- Produce clear documentation and demo outputs for evaluation

## Architecture & data model
- `Patient` struct: id, phone (long long), name, age, severity, arrival timestamp, problem description
- `PriorityQueue` (linked list): `head`, `tail`, `count`; insertion keeps severity ordering
- CSV storage: `data/queue.csv` (active queue), `data/served.csv` (served history), `data/users.csv` (auth)

## Core algorithms
- Enqueue: O(n) traverse to find insert position; stable for equal severity
- Dequeue: O(1) remove head
- Search: O(n) linear scan
- Save/Load: line-oriented CSV read/write preserving order

## Build & run (summary)
1. Build with `make` or the `gcc` command in `README.md`.
2. Run `./hospital_queue.exe` and login with demo credentials (press Enter for password).

## Workflow & UI walkthrough
1. Start program → Main Menu
2. `Register Patient` → input name, phone, age, severity, problem → patient added to queue
3. `View Waiting List` → shows table ordered by severity and arrival
4. `Call Next Patient` → dequeue and record served time to `data/served.csv`
5. `View Served History` → shows served patients and computed wait times

## Sample outputs
Below are console excerpts (replace with your actual console logs during demo):

```
MAIN MENU
1. Register Patient
2. View Waiting List
3. Call Next Patient
...
```

Example `data/served.csv` lines:
```
ID,Phone,Name,Age,Severity,Arrival,Served At,Wait(sec),Problem
1,9876500001,Rajesh Kumar,34,2,2025-11-15 09:00:00,2025-11-15 09:40:00,2400,high fever
2,9123456780,Sneha Sharma,28,1,2025-11-15 09:01:10,2025-11-15 09:24:00,1380,fracture pain
```

## Screenshots
Add PNG/JPEG screenshots to `docs/screenshots/` and reference them below. Example markdown:

![Register patient](screenshots/register.png)

![View waiting list](screenshots/waiting_list.png)

![Predicting waiting time](screenshots/waiting_time.png)



## Limitations
- Single-process CLI, no concurrency
- Linear-time operations
- Minimal validation on CSV inputs

## Future work
- Add Streamlit web UI and port persistence to SQLite
- Add unit tests and CI
- Make the predictor a separate Python microservice (prototype exists)

---

### How to produce `docs/report.pdf`
If you want a PDF for submission, convert the Markdown to PDF:
```bash
pandoc docs/report.md -o docs/report.pdf --pdf-engine=pdflatex
```
Or open `docs/report.md` in an editor and print to PDF.


