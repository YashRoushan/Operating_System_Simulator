// Assignment 2 code by Professor Alexander Brodsky

#include <stdio.h>
#include <stdlib.h>
#include "context.h"
#include "process.h"
#include <pthread.h>

static pthread_mutex_t main_mutex_lock = PTHREAD_MUTEX_INITIALIZER;
// making a struct to simulate a thread data
typedef struct {
    int node_id;
    context **procs;
//    pthread_mutex_t threadMutex;
    int num_procs;
    int quantum;
} ThreadData;

void * simulateProcesses(void *arg){
    ThreadData *threadData = (ThreadData *) arg;
    for (int i = 0; i < threadData->num_procs; ++i) {
//        pthread_mutex_lock(&threadData[i].threadMutex);
        pthread_mutex_lock(&main_mutex_lock);
        process_simulate(threadData->procs[i]);
//        pthread_mutex_unlock(&threadData[i].threadMutex);
        pthread_mutex_unlock(&main_mutex_lock);
    }
    return NULL;
}

int main() {
    int num_procs;
    int quantum;
    int numNodes;

    /* Read in the header of the process description with minimal validation
     */
    if (scanf("%d %d %d", &num_procs, &quantum, &numNodes) < 3) {
        fprintf(stderr, "Bad input, expecting number of process and quantum size\n");
        return -1;
    }

    pthread_t *threads = malloc(numNodes * sizeof (pthread_t));
//    pthread_t threads[numNodes];
    ThreadData * thread_data = malloc(numNodes * sizeof (ThreadData));

    /* We use an array of pointers to contexts to track the processes.
     */
    context **procs  = calloc(num_procs, sizeof(context *));

    process_init(quantum, numNodes); // we will do this in a separate method

    /* Load and admit each process, if an error occurs, we just give up.
     */
    for (int i = 0; i < num_procs; i++) {
        procs[i] = context_load(stdin);
        if (!procs[i]) {
            fprintf(stderr, "Bad input, could not load program description\n");
            return -1;
        }
        process_admit(procs[i]);
    }

    /* All the magic happens in here
     */
//    process_simulate();

//    filling data in thread
    for (int i = 0; i < numNodes; ++i) {
        thread_data[i].node_id = i+1;
        thread_data[i].procs = procs;
        thread_data[i].num_procs = num_procs;
        thread_data[i].quantum = quantum;
//        pthread_mutex_init(&thread_data[i].threadMutex, NULL);
        if(pthread_create(&threads[i], NULL, simulateProcesses, &thread_data[i])){
            fprintf(stderr, "Error in executing threads\n");
            return -1;
        }
    }

    for (int i = 0; i < numNodes; ++i) {
        pthread_join(threads[i], NULL);
    }

    /* Output the statistics for processes in order of amdmission.
     */
//    for (int i = 0; i < num_procs; i++) {
//        context_stats(procs[i], stdout);
//    }
    context_stats(stdout);

    free(threads);
    free(thread_data);
    for (int i = 0; i < num_procs; ++i) {
        free(procs[i]);
    }
    return 0;
}