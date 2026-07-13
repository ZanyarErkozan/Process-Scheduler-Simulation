    // middle east technical university - northern cyprus campus
    // cng334 - introduction to operating systems
    // assignment 1 - process scheduler simulation
    // name       : zanyar erkozan
    // student id : 2584985
    // date       : april 2026
    // description:
    // i implemented a process scheduler simulation in two ways
    // both in this single file:
    // part 1 (parts a-e): i used fork() + mmap() with map_shared
    // so that the child process can sort the array and the parent
    // can see the result without copying any data
    // part 2 (part f): i re-implemented the same thing using
    // posix threads threads share the same address space so
    // i did not need mmap() at all - a global array is enough
    // compile:
    // gcc -wall -wextra -std=c11 -o zanyar-erkozan-2584985 \
    // zanyar-erkozan-2584985c -lpthread
    // run:
    // /zanyar-erkozan-2584985

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>        // int_max - i need this for finding the earliest arrival
#include <unistd.h>        // fork() getpid()
#include <sys/wait.h>      // wait()
#include <sys/mman.h>      // mmap() munmap()
#include <pthread.h>       // pthreads for part f
#include <time.h>          // time() for seeding random numbers

    // map_anonymous is map_anon on some systems (macos)
    // this guard makes the code portable
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

    // number of processes - fixed at 10 as the assignment says
#define N 10

    // process structure - exactly as defined in the assignment
typedef struct {
    int pid;               // process id: 1 to 10
    int burst_time;        // how long it needs the cpu: [5 25] ms
    int priority;          // scheduling priority: [1 10] higher = more urgent
    int arrival_time;      // when it enters the ready queue: [0 20] ms
} Process;

    // global array for the thread-based version (part f)
    // i put this here because threads share the same memory space
    // so a global variable is all i need - no mmap required
static Process g_arr[N];

    // thread argument struct
    // at first i tried to just pass 'n' 'mode' and 'algorithm' directly
    // into pthread_create() but the compiler threw a bunch of errors
    // i realized pthread_create() only accepts exactly one void* argument
    // so i had to pack them all into this struct as a workaround to make it work
typedef struct {
    int n;              // number of processes
    int mode;           // which sort mode to use
    int algorithm;      // which scheduling algorithm to use
} ThreadArgs;

    // forward declarations
void  initProcesses(Process queue[], int n);
void  sortProcesses(Process queue[], int n, int mode);
void  executeProcesses(Process queue[], int n, int algorithm);
static void printTable(Process queue[], int n);
static void *threadSort(void *param);
static void *threadExecute(void *param);
void  run_fork_version(void);
void  run_thread_version(void);

    // printtable - helper i wrote to avoid repeating the same
    // printf lines every time i want to show the process list
static void printTable(Process queue[], int n) {
    printf("  %-6s %-14s %-12s %-10s\n",
           "PID", "Arrival (ms)", "Burst (ms)", "Priority");
    printf("  %-6s %-14s %-12s %-10s\n",
           "---", "------------", "----------", "--------");
    for (int i = 0; i < n; i++) {
        printf("  P%-5d %-14d %-12d %-10d\n",
               queue[i].pid,
               queue[i].arrival_time,
               queue[i].burst_time,
               queue[i].priority);
    }
    printf("\n");
}

    // part a) initprocesses
    // initially my burst times were coming out as 0 or 2 and processes
    // were finishing almost instantly i tried printing rand() % 25 and
    // realized i needed to shift the whole range so i changed it to
    // rand() % 21 + 5 which perfectly forces it into the [5 25] bounds
void initProcesses(Process queue[], int n) {
    printf("+------------------------------------------+\n");
    printf("|  Part a) initProcesses  (n = %d)          |\n", n);
    printf("+------------------------------------------+\n\n");

    for (int i = 0; i < n; i++) {
        queue[i].pid          = i + 1;

            // rand() % 21 gives 0-20 then +5 shifts it to [5 25]
        queue[i].burst_time   = rand() % 21 + 5;

            // rand() % 10 gives 0-9 then +1 shifts it to [1 10]
        queue[i].priority     = rand() % 10 + 1;

            // rand() % 21 gives [0 20] directly
        queue[i].arrival_time = rand() % 21;
    }

    printf("  Generated process table:\n");
    printTable(queue, n);
}

    // part b) sortprocesses
    // i originally considered implementing merge sort for o(n log n) performance
    // but then i realized n=10 is so incredibly small that the overhead of recursive
    // calls is actually slower than just doing a simple o(n^2) insertion sort
    // it's also much less risky to code
    // mode = 1: sort by arrival_time ascending
    // mode = 2: sort by priority descending
void sortProcesses(Process queue[], int n, int mode) {
    if (mode == 1) {
        printf("+------------------------------------------------------+\n");
        printf("|  Part b) sortProcesses [mode=1: arrival_time asc.]   |\n");
        printf("+------------------------------------------------------+\n\n");
    } else {
        printf("+------------------------------------------------------+\n");
        printf("|  Part b) sortProcesses [mode=2: priority desc.]      |\n");
        printf("+------------------------------------------------------+\n\n");
    }

        // insertion sort - i iterate from the second element
    // and insert each one into its correct position
    for (int i = 1; i < n; i++) {
        Process key = queue[i];
        int j = i - 1;

        if (mode == 1) {
                // shift elements with larger arrival_time to the right
            while (j >= 0 && queue[j].arrival_time > key.arrival_time) {
                queue[j + 1] = queue[j];
                j--;
            }
        } else {
                // shift right if priority is lower or same priority
    // but the arrival_time is later (tie-break rule)
            while (j >= 0 &&
                   (queue[j].priority < key.priority ||
                    (queue[j].priority == key.priority &&
                     queue[j].arrival_time > key.arrival_time)))
            {
                queue[j + 1] = queue[j];
                j--;
            }
        }

        queue[j + 1] = key;
    }

    printf("  Sorted table:\n");
    printTable(queue, n);
}

    // part c) executeprocesses
    // this was the most complex part i simulate time step by step
    // and decide which process runs at each moment
    // algorithm = 1: shortest process next (non-preemptive)
    // algorithm = 2: preemptive priority scheduling
void executeProcesses(Process queue[], int n, int algorithm) {
    int finish_time[N] = {0};
    int completed[N]   = {0};
    int remaining[N];
    int done = 0;

    for (int i = 0; i < n; i++)
        remaining[i] = queue[i].burst_time;

    int t = INT_MAX;
    for (int i = 0; i < n; i++)
        if (queue[i].arrival_time < t)
            t = queue[i].arrival_time;

        // ---- algorithm 1: shortest process next ----
    if (algorithm == 1) {
        printf("+----------------------------------------------------------------+\n");
        printf("|  Part c) executeProcesses [algo=1: SPN, non-preemptive]       |\n");
        printf("+----------------------------------------------------------------+\n\n");
        printf("  -- Timeline --\n\n");

        while (done < n) {
            int sel = -1;
                // i know a priority queue (heap) is traditionally used here for o(logn)
    // extraction but since we only have n=10 a simple o(n) linear scan
    // is virtually instant and saves me from having to implement a heap array
            for (int i = 0; i < n; i++) {
                if (completed[i] || queue[i].arrival_time > t) continue;
                if (sel == -1 ||
                    queue[i].burst_time < queue[sel].burst_time ||
                    (queue[i].burst_time == queue[sel].burst_time &&
                     queue[i].arrival_time < queue[sel].arrival_time))
                {
                    sel = i;
                }
            }

            if (sel == -1) {
                int next = INT_MAX;
                for (int i = 0; i < n; i++)
                    if (!completed[i] && queue[i].arrival_time < next)
                        next = queue[i].arrival_time;
                t = next;
                continue;
            }

            for (int ms = 0; ms < queue[sel].burst_time; ms++)
                printf("  Time %3d ms  |  P%d running\n", t + ms, queue[sel].pid);

            t                 += queue[sel].burst_time;
            finish_time[sel]   = t;
            completed[sel]     = 1;
            done++;
        }

        // ---- algorithm 2: preemptive priority ----
    } else {
        printf("+----------------------------------------------------------------+\n");
        printf("|  Part c) executeProcesses [algo=2: Preemptive Priority]       |\n");
        printf("+----------------------------------------------------------------+\n\n");
        printf("  -- Timeline --\n\n");

        while (done < n) {
            int sel = -1;
                // using o(n) linear scan again since n=10
            for (int i = 0; i < n; i++) {
                if (completed[i] || remaining[i] == 0 || queue[i].arrival_time > t) continue;
                if (sel == -1 || queue[i].priority > queue[sel].priority)
                    sel = i;
            }

            if (sel == -1) { t++; continue; }

            printf("  Time %3d ms  |  P%d running  [priority=%d, remaining=%d ms]\n",
                   t, queue[sel].pid, queue[sel].priority, remaining[sel]);

            remaining[sel]--;
            t++;

            if (remaining[sel] == 0) {
                finish_time[sel] = t;
                completed[sel]   = 1;
                done++;
            }
        }
    }

    printf("+------------------------------------------------------------------+\n");
    printf("|  Part e) executeProcesses - Results & Averages                   |\n");
    printf("+------------------------------------------------------------------+\n\n");
    printf("\n  -- Scheduling Results --\n\n");
    printf("  %-6s %-12s %-10s %-10s %-14s %-10s\n",
           "PID", "Arrival", "Burst", "Finish", "Turnaround", "Waiting");
    printf("  %-6s %-12s %-10s %-10s %-14s %-10s\n",
           "---", "-------", "-----", "------", "----------", "-------");

    double sum_ta = 0, sum_wt = 0;
    for (int i = 0; i < n; i++) {
        int ta = finish_time[i] - queue[i].arrival_time;
        int wt = ta - queue[i].burst_time;
        sum_ta += ta;
        sum_wt += wt;
        printf("  P%-5d %-12d %-10d %-10d %-14d %-10d\n",
               queue[i].pid, queue[i].arrival_time, queue[i].burst_time,
               finish_time[i], ta, wt);
    }
    printf("\n  Avg Turnaround : %.2f ms\n", sum_ta / n);
    printf("  Avg Waiting    : %.2f ms\n\n", sum_wt / n);
}

    // thread worker for sorting (part f)
static void *threadSort(void *param) {
    ThreadArgs *args = (ThreadArgs *)param;
    printf("[Thread 1]  Starting sort (mode=%d)...\n\n", args->mode);
    sortProcesses(g_arr, args->n, args->mode);
    printf("[Thread 1]  Sort done.\n\n");
    return NULL;
}

    // thread worker for scheduling (part f)
static void *threadExecute(void *param) {
    ThreadArgs *args = (ThreadArgs *)param;
    printf("[Thread 2]  Starting scheduler (algorithm=%d)...\n\n",
           args->algorithm);
    executeProcesses(g_arr, args->n, args->algorithm);
    printf("[Thread 2]  Scheduling done.\n\n");
    return NULL;
}

    // parts a-e: fork + mmap version
    // i checked chapter 3 slides about IPC and the shared_mem.c example
    // and realized i needed MAP_SHARED here. without it they wouldn't
    // communicate because processes have isolated address spaces.
void run_fork_version(void) {
    printf("##########################################################\n");
    printf("##  PARTS a-e: Fork + mmap version                      ##\n");
    printf("##########################################################\n\n");

    Process *queue = mmap(NULL,
                          N * sizeof(Process),
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_ANONYMOUS,
                          -1, 0);
    if (queue == MAP_FAILED) {
        perror("mmap failed");
        return;
    }

    initProcesses(queue, N);

    printf("+------------------------------------------------------------------+\n");
    printf("|  Part d) Forking - child will sort, parent will schedule         |\n");
    printf("+------------------------------------------------------------------+\n\n");

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        munmap(queue, N * sizeof(Process));
        return;

    } else if (pid == 0) {
        printf("[Child  PID=%d]  I will sort the shared array by arrival_time.\n\n",
               (int)getpid());

        sortProcesses(queue, N, 1);

        printf("[Child  PID=%d]  Sorting done, exiting.\n\n", (int)getpid());
        exit(EXIT_SUCCESS);

    } else {
        printf("[Parent PID=%d]  Waiting for child (PID=%d)...\n\n",
               (int)getpid(), (int)pid);

        wait(NULL);

        printf("[Parent PID=%d]  Child finished. Shared memory has sorted array.\n\n",
               (int)getpid());
        printf("  Array received from child via shared memory:\n");
        printTable(queue, N);

        executeProcesses(queue, N, 1);
    }

    munmap(queue, N * sizeof(Process));
}

    // part f: thread-based version
    // as taught in chapter 7 slides, threads natively share the same
    // address space within a process. so i didn't need to use mmap here,
    // i just used a simple global array.
void run_thread_version(void) {
    printf("##########################################################\n");
    printf("##  PART f: Thread-based version (pthreads)             ##\n");
    printf("##########################################################\n\n");

    srand((unsigned int)time(NULL) + 1);
    initProcesses(g_arr, N);

    printf("+------------------------------------------------------------------+\n");
    printf("|  Thread 1: sortProcesses (mode=2: priority desc.)               |\n");
    printf("|  Thread 2: executeProcesses (algo=2: Preemptive Priority)       |\n");
    printf("+------------------------------------------------------------------+\n\n");

    pthread_t   t1, t2;
    ThreadArgs  args = { N, 2, 2 };

    if (pthread_create(&t1, NULL, threadSort, &args) != 0) {
        perror("pthread_create (t1) failed");
        return;
    }

    pthread_join(t1, NULL);
    printf("[Main]  Thread 1 joined. Array is fully sorted.\n\n");

    if (pthread_create(&t2, NULL, threadExecute, &args) != 0) {
        perror("pthread_create (t2) failed");
        return;
    }

    pthread_join(t2, NULL);
    printf("[Main]  Thread 2 joined. Done.\n\n");
}

int main(void) {
    printf("\n");
    printf("  CNG334 - Assignment 1\n");
    printf("  Zanyar Erkozan  |  2584985\n");

    srand((unsigned int)time(NULL));

    run_fork_version();

    printf("\n");

    run_thread_version();

    return 0;
}
