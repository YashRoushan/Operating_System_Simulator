//
// Created by Alex Brodsky on 2023-05-07.
//

#ifndef PROSIM_PROCESS_H
#define PROSIM_PROCESS_H
#include "context.h"
#include "prio_q.h"
#include "pthread.h"


// made a struct to make multiple instances of processes in the scheduler
typedef struct {
    prio_q_t *blocked;
    prio_q_t *ready;
    pthread_mutex_t blocked_mutex;
    pthread_mutex_t ready_mutex;
    int time;
    int next_proc_id;
    int quantum;
} process;

/* Initialize the simulation
 * @params:
 *   quantum: the CPU quantum to use in the situation
 * @returns:
 *   returns 1
 */
extern int process_init(int cpu_quantum, int numNodes);

/* Admit a process into the simulation
 * @params:
 *   proc: pointer to the program context of the process to be admitted
 * @returns:
 *   returns 1
 */
extern int process_admit(context *proc);

/* Perform the simulation
 * @params:
 *   none
 * @returns:
 *   returns 1
 */
extern int process_simulate(context *proc);

extern void context_stats(FILE *fout);
#endif //PROSIM_PROCESS_H
