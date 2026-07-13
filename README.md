# Process Scheduler Simulation

**Author:** Zanyar Erkozan


---

## 📋 Overview

This project implements a **CPU process scheduler simulation** in C, demonstrating two different approaches to inter-process/inter-thread communication:

- **Part 1 (a–e):** Uses `fork()` + `mmap()` with `MAP_SHARED` — the child process sorts an array and the parent reads the result via shared memory.
- **Part 2 (f):** Re-implements the same logic using **POSIX threads (pthreads)** — threads naturally share the same address space so a global array is sufficient (no `mmap` needed).

---

## ⚙️ Features

| Feature | Description |
|---|---|
| `initProcesses()` | Randomly generates 10 processes with burst time [5–25 ms], priority [1–10], arrival time [0–20 ms] |
| `sortProcesses()` | Insertion sort — Mode 1: by arrival time (ASC), Mode 2: by priority (DESC) |
| `executeProcesses()` | Algorithm 1: **Shortest Process Next (SPN)** non-preemptive · Algorithm 2: **Preemptive Priority Scheduling** |
| Fork version | Child sorts shared memory array, parent waits and schedules |
| Thread version | Thread 1 sorts, Thread 2 schedules using a global array |

---

## 🛠️ Compile & Run

```bash
# Compile
gcc -Wall -Wextra -std=c11 -o scheduler zanyar-erkozan-2584985.c -lpthread

# Run
./scheduler
```

> **Note:** Requires a Linux/Unix environment (uses `fork()`, `mmap()`, `pthreads`).

---

## 📊 Scheduling Algorithms

### Algorithm 1 – Shortest Process Next (SPN)
- Non-preemptive
- At each scheduling point, the ready process with the **shortest burst time** is selected
- Tie-break: earlier arrival time wins

### Algorithm 2 – Preemptive Priority Scheduling
- At every millisecond tick, the **highest priority** ready process runs
- Remaining time tracked per process
- Calculates turnaround time and waiting time for all processes

---

## 📁 Files

| File | Description |
|---|---|
| `zanyar-erkozan-2584985.c` | Full C source code |
| `zanyar_erkozan_2584985_report.pdf` | Project report |

---

## 📚 Concepts Demonstrated

- `fork()` system call and parent/child process communication
- `mmap()` with `MAP_SHARED | MAP_ANONYMOUS` for shared memory IPC
- POSIX threads (`pthread_create`, `pthread_join`)
- CPU scheduling algorithms: SPN and Preemptive Priority
- Turnaround time and waiting time calculations
