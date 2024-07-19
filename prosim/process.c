#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include "process.h"
#include "prio_q.h"

//made a priority queue to store finished processes
static prio_q_t *finished;

//initializing a mutex lock
static pthread_mutex_t finished_mutex_lock = PTHREAD_MUTEX_INITIALIZER;

// ADDED FOR YOUR REFERENCE (DEFINED IN PROCESS.H)
//typedef struct {
//    prio_q_t *blocked;
//    prio_q_t *ready;
//    pthread_mutex_t blocked_mutex;
//    pthread_mutex_t ready_mutex;
//    int time;
//    int next_proc_id;
//    int quantum;
//} process;

enum {
    PROC_NEW = 0,
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_FINISHED
};

static char *states[] = {"new", "ready", "running", "blocked", "finished"};


/* Initialize the simulation
 * @params:
 *   quantum: the CPU quantum to use in the situation
 * @returns:
 *   returns 1
 */

// made a static priority queue to store processes
static process * processes;

extern int process_init(int cpu_quantum, int numNodes) {
    /* Set up the queues and store the quantum
     * Assume the queues will be allocated
     */
    processes = calloc(numNodes, sizeof (process));
    finished = prio_q_new();
    assert(processes);
    for (int i = 0; i < numNodes; i++) {
        processes[i].quantum = cpu_quantum;
        processes[i].blocked = prio_q_new();
        processes[i].ready = prio_q_new();
        // initializing the mutex locks
        pthread_mutex_init(&processes[i].blocked_mutex, NULL);
        pthread_mutex_init(&processes[i].ready_mutex, NULL);

        // assigning time and proc_id to the processes;
        processes[i].time = 0;
        processes[i].next_proc_id = 1;
    }
    return 1;
}

/* Print state of process
 * @params:
 *   proc: process' context
 * @returns:
 *   none
 */
static void print_process(context *proc) {
    printf("[%2.2d] %5.5d: process %d %s\n",proc->node, processes[proc->node-1].time, proc->id, states[proc->state]);
}

/* Compute priority of process, depending on whether SJF or priority based scheduling is used
 * @params:
 *   proc: process' context
 * @returns:
 *   priority of process
 */
static int actual_priority(context *proc) {
    if (proc->priority < 0) {
        /* SJF means duration of current DOOP is the priority
         */
        return proc->duration;
    }
    return proc->priority;
}

/* Insert process into appropriate queue based on the primitive it is performing
 * @params:
 *   proc: process' context
 *   next_op: if true, current primitive is done, so move IP to next primitive.
 * @returns:
 *   none
 */
static void insert_in_queue(context *proc, int next_op) {
    /* If current primitive is done, move to next
     */
    if (next_op) {
        context_next_op(proc);
        proc->duration = context_cur_duration(proc);
    }

    int op = context_cur_op(proc);

    /* 3 cases:
     * 1. If DOOP, process goes into ready queue
     * 2. If BLOCK, process goes into blocked queue
     * 3. If HALT, process goes to the static finished queue
     */
    if (op == OP_DOOP) {
        proc->state = PROC_READY;
        prio_q_add(processes[proc->node -1].ready, proc, actual_priority(proc));
        proc->wait_count++;
        proc->enqueue_time = processes[proc->node -1].time;
    } else if (op == OP_BLOCK) {
        /* Use the duration field of the process to store their wake-up time.
         */
        proc->state = PROC_BLOCKED;
        proc->duration += processes[proc->node -1].time;
        prio_q_add(processes[proc->node-1].blocked, proc, proc->duration);
    } else {
        // adding locks to ensure that the static finished queue does not get corrupted
        pthread_mutex_lock(&finished_mutex_lock);
        proc->state = PROC_FINISHED;

        // calculating priority as advised in the assignment pdf
        int proc_priority = (processes[proc->node -1].time * 10000) + (proc->node * 100) + proc->id;

        // adding processes in the finished queue once it is finished
        prio_q_add(finished, proc, proc_priority);
        pthread_mutex_unlock(&finished_mutex_lock);
    }

    // printing the current stats of the process
    print_process(proc);
}

/* Admit a process into the simulation
 * @params:
 *   proc: pointer to the program context of the process to be admitted
 * @returns:
 *   returns 1
 */
extern int process_admit(context *proc) {
    /* Use a static variable to assign each process a unique process id.
     */
    proc->id = processes[proc->node -1].next_proc_id;
    processes[proc->node -1].next_proc_id++;
    proc->state = PROC_NEW;
    print_process(proc);
    insert_in_queue(proc, 1);
    return 1;
}

/* Perform the simulation
 * @params:
 *   none
 * @returns:
 *   returns 1
 */
extern int process_simulate(context *curr_proc) {
    context *cur = NULL;
    int cpu_quantum;

    // naming the priority queues in the processes queue so that the rest of
    // the code simulate professor Brodsky's code
    prio_q_t *ready = processes[curr_proc->node-1].ready;
    prio_q_t *blocked = processes[curr_proc->node-1].blocked;


    /* We can only stop when all processes are in the finished state
     * no processes are readdy, running, or blocked
     */
    while(!prio_q_empty(ready) || !prio_q_empty(blocked) || cur != NULL) {
        int preempt = 0;

        /* Step 1: Unblock processes
         * If any of the unblocked processes have higher priority than current running process
         *   we will need to preempt the current running process
         */

        // putting lock for the ready queue so that it is not accessed concurrently
        // otherwise the code will fail as we are trying to remove from a null queue
        // putting the lock before the while statement since the while statement is peeking in the queue
        // and if there is concurrent access to the queue then the !prio_q_empty(blocked) might
        // give you true but be false as some other thread deleted the process
        pthread_mutex_lock(&processes[curr_proc->node -1].blocked_mutex);

        while (!prio_q_empty(blocked)) {
            /* We can stop ff process at head of queue should not be unblocked
             */
            context *proc = prio_q_peek(blocked);
            if (proc->duration > processes[curr_proc->node-1].time) {
                break;
            }

            /* Move from blocked and reinsert into appropriate queue
             */
            // added locks around so that removing from the queue can be done safely
            prio_q_remove(blocked);
            insert_in_queue(proc, 1);

            /* preemption is necessary if a process is running, and it has lower priority than
             * a newly unblocked ready process.
             */
            preempt |= cur != NULL && proc->state == PROC_READY &&
                       actual_priority(cur) > actual_priority(proc);
        }

        // unlocking the blocked queue
        pthread_mutex_unlock(&processes[curr_proc->node -1].blocked_mutex);

        /* Step 2: Update current running process
         */
        if (cur != NULL) {
            cur->duration--;
            cpu_quantum--;

            /* Process stops running if it is preempted, has used up their quantum, or has completed its DOOP
             */
            if (cur->duration == 0 || cpu_quantum == 0 || preempt) {
                insert_in_queue(cur, cur->duration == 0);
                cur = NULL;
            }
        }

        /* Step 3: Select next ready process to run if none are running
         * Be sure to keep track of how long it waited in the ready queue
         */
        // putting lock for the ready queue so that it is not accessed concurrently
        // otherwise the code will fail as we are trying to remove from a null queue
        // putting the lock before the if statement since the if statement is peeking in the queue
        // and if there is concurrent access to the queue then the !prio_q_empty(ready) might
        // give you true but be false as some other thread deleted the process
        pthread_mutex_lock(&processes[curr_proc->node - 1].ready_mutex);
        if (cur == NULL && !prio_q_empty(ready)) {

            // added locks around so that removing from the queue can be done safely
            cur = prio_q_remove(ready);
            cur->wait_time += processes[curr_proc->node-1].time - cur->enqueue_time;
            cpu_quantum = processes[cur->node-1].quantum;
            cur->state = PROC_RUNNING;
            print_process(cur);
        }
        // unlocking the ready queue
        pthread_mutex_unlock(&processes[curr_proc->node - 1].ready_mutex);

        /* next clock tick
         */
        processes[curr_proc->node-1].time++;
    }

    return 1;
}


/* Outputs aggregate statistics about a process to the specified file.
 * Takes data from the finished priority queue
 * @params:
 *   fout: FILE into which the output should be written
 * @returns:
 *   none
 */
extern void context_stats( FILE *fout) {
    while(!prio_q_empty(finished)){
        context * cur = prio_q_remove(finished);
    int totalTime = cur->doop_time + cur->wait_time + cur->block_time;
    fprintf(fout,"| %5.5d | Proc %2.2d.%2.2d | Run %d, Block %d, Wait %d\n",totalTime, cur->node, cur->id,
            cur->doop_time, cur->block_time, cur->wait_time);
    }
}