# Documentation (Review & Final Deliverable)

## Abstract
Hospital Queue Management System implemented in C with MVC pattern. Uses a priority queue so critical patients are served first. Data persists to CSV so queue survives restarts.

## Algorithm (core)
- **Enqueue with priority**: Insert patient node into linked list such that list stays ordered by `severity` (higher first). For equal severity, append to end of that severity group to preserve arrival order.
- **Dequeue**: Remove head node (highest priority, earliest arrival).
- **Search**: Linear scan through list (O(n)).
- **Persist**: Write CSV header then one line per patient; load reads CSV and re-enqueues in order.

## Implementation (files)
- `src/model/patient.h/c` : patient struct and constructor
- `src/model/queue.h/c` : priority queue functions (init, enqueue, dequeue, peek, search, save/load)
- `src/view/view.h/c` : functions that print menus and outputs
- `src/controller/controller.c` : main loop, input parsing, calling model & view
- `src/util/time_util.c` : timestamp helper
- `Makefile`, `README.md`

## Screenshots / Sample Output
(Use the program and capture screenshots; for this deliverable include console screenshots showing add/view/serve.)

## Future Scope
- Add GUI (ncurses or Java Swing via JNI)
- Multi-department queues, doctor assignment, estimated waiting time
- Networked client-server model for multiple terminals
- Analytics & charts (export to CSV/Excel)

## Learning Outcomes
- Implemented linked-list priority queue in C
- MVC separation in procedural language
- File I/O (CSV), basic CLI UI, modular coding with headers
