#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t *q)
{
        if (q == NULL)
                return 1;
        return (q->size == 0);
}

void enqueue(struct queue_t *q, struct pcb_t *proc)
{
        /* Add a new process to queue [q] */
        if (q == NULL || proc == NULL) {
                return;
        }
        
        if (q->size >= MAX_QUEUE_SIZE) {
                printf("Warning: Queue is full, cannot enqueue process PID %d\n", proc->pid);
                return;
        }
        
        q->proc[q->size] = proc;
        q->size++;
}

struct pcb_t *dequeue(struct queue_t *q)
{
        /* Return first process in queue (FIFO for MLQ)
         * or highest priority process for single queue
         */
        if (q == NULL || q->size == 0) {
                return NULL;
        }
        
#ifdef MLQ_SCHED
        /* For MLQ, use FIFO within each priority level */
        struct pcb_t *selected_proc = q->proc[0];
        
        /* Shift remaining processes */
        for (int i = 0; i < q->size - 1; i++) {
                q->proc[i] = q->proc[i + 1];
        }
        q->size--;
        
        return selected_proc;
#else
        /* For single queue, find highest priority process */
        int highest_prio_idx = 0;
        uint32_t highest_prio = q->proc[0]->priority;
        
        /* Find process with highest priority (lowest value) */
        for (int i = 1; i < q->size; i++) {
                if (q->proc[i]->priority < highest_prio) {
                        highest_prio = q->proc[i]->priority;
                        highest_prio_idx = i;
                }
        }
        
        struct pcb_t *selected_proc = q->proc[highest_prio_idx];
        
        /* Remove the selected process by shifting remaining processes */
        for (int i = highest_prio_idx; i < q->size - 1; i++) {
                q->proc[i] = q->proc[i + 1];
        }
        q->size--;
        
        return selected_proc;
#endif
}

struct pcb_t *purgequeue(struct queue_t *q, struct pcb_t *proc)
{
        /* Remove a specific item from queue */
        if (q == NULL || proc == NULL || q->size == 0) {
                return NULL;
        }
        
        /* Find the process in queue */
        for (int i = 0; i < q->size; i++) {
                if (q->proc[i]->pid == proc->pid) {
                        struct pcb_t *found_proc = q->proc[i];
                        
                        /* Shift remaining processes to fill the gap */
                        for (int j = i; j < q->size - 1; j++) {
                                q->proc[j] = q->proc[j + 1];
                        }
                        q->size--;
                        
                        return found_proc;
                }
        }
        
        return NULL; /* Process not found */
}