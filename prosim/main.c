#include <stdio.h>
#include <stdlib.h>
#include "context.h"
#include "process.h"
#include <pthread.h>

// Define a struct to simulate thread data
typedef struct {
    int node_id;
    context **procs;  // pointers to process contexts managed by the thread
    int num_procs;    // number of processes the thread will be running
    pthread_mutex_t *thread_mutex; // Array of mutexes
    int quantum;
} ThreadData;

// Function to simulate the processes assigned to a particular node
void *simulateProcesses(void *arg) {
    ThreadData *threadData = (ThreadData *)arg;
    for (int i = 0; i < threadData->num_procs; ++i) {
        // Locking the mutex while simulating the process to avoid destructive interference
        pthread_mutex_lock(&threadData->thread_mutex[i]);
        process_simulate(threadData->procs[i]);
        // Unlocking the mutex
        pthread_mutex_unlock(&threadData->thread_mutex[i]);
    }
    return NULL;
}

int main() {
    int num_procs;
    int quantum;
    int numNodes;

    // Read in the header of the process description with minimal validation
    if (scanf("%d %d %d", &num_procs, &quantum, &numNodes) < 3) {
        fprintf(stderr, "Bad input, expecting number of process and quantum size\n");
        return -1;
    }

    // Allocate memory for threads and threadData
    pthread_t *threads = malloc(numNodes * sizeof(pthread_t));
    ThreadData *thread_data = malloc(numNodes * sizeof(ThreadData));
    pthread_mutex_t *thread_mutex = malloc(num_procs * sizeof(pthread_mutex_t));

    // Initialize the process
    context **procs = calloc(num_procs, sizeof(context *));
    process_init(quantum, numNodes);

    // Load and admit each process, if an error occurs, we just give up
    for (int i = 0; i < num_procs; i++) {
        procs[i] = context_load(stdin);
        if (!procs[i]) {
            fprintf(stderr, "Bad input, could not load program description\n");
            return -1;
        }
        process_admit(procs[i]);
    }

    // Initialize all mutexes
    for (int j = 0; j < num_procs; ++j) {
        pthread_mutex_init(&thread_mutex[j], NULL);
    }

    // Fill data in thread
    for (int i = 0; i < numNodes; ++i) {
        thread_data[i].node_id = i + 1;
        thread_data[i].procs = procs;
        thread_data[i].num_procs = num_procs;
        thread_data[i].quantum = quantum;
        thread_data[i].thread_mutex = thread_mutex; // Point to the array of mutexes

        // Create threads and simulate the processes in parallel
        if (pthread_create(&threads[i], NULL, simulateProcesses, &thread_data[i])) {
            fprintf(stderr, "Error in executing threads\n");
            return -1;
        }
    }

    // Wait for all threads to complete
    for (int i = 0; i < numNodes; ++i) {
        pthread_join(threads[i], NULL);
    }

    // Output the statistics for processes in order of admission in the finished priority queue
    context_stats(stdout);

    // Destroy all mutexes
    for (int j = 0; j < num_procs; ++j) {
        pthread_mutex_destroy(&thread_mutex[j]);
    }

    // Free all allocated memory
    free(threads);
    free(thread_data);
    free(thread_mutex);
    for (int i = 0; i < num_procs; ++i) {
        free(procs[i]);
    }
    free(procs);

    return 0;
}