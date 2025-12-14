/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "queue.h"
#include "sched.h"
#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>
static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;

static struct queue_t running_list;
#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
static int slot[MAX_PRIO];
static int current_slot[MAX_PRIO]; 
static int current_prio = 0;
static pthread_mutex_t dispatch_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

int queue_empty(void) {
#ifdef MLQ_SCHED
	unsigned long prio;
	for (prio = 0; prio < MAX_PRIO; prio++)
		if (!empty(&mlq_ready_queue[prio])) 
			return -1;
#endif
	return (empty(&ready_queue) && empty(&run_queue));
}

void init_scheduler(void) {
#ifdef MLQ_SCHED
    int i ;
	for (i = 0; i < MAX_PRIO; i ++) {
		mlq_ready_queue[i].size = 0;
		slot[i] = MAX_PRIO - i; 
		current_slot[i] = 0;
	}
	current_prio = 0;
#endif
	ready_queue.size = 0;
	run_queue.size = 0;
	running_list.size = 0;
	pthread_mutex_init(&queue_lock, NULL);
}

void finish_proc(struct pcb_t * proc) {
    if (proc == NULL) return;
    pthread_mutex_lock(&queue_lock);
    purgequeue(&running_list, proc);
    pthread_mutex_unlock(&queue_lock);
}

#ifdef MLQ_SCHED
/* 
 *  Stateful design for routine calling
 *  based on the priority and our MLQ policy
 *  We implement stateful here using transition technique
 *  State representation   prio = 0 .. MAX_PRIO, curr_slot = 0..(MAX_PRIO - prio)
 */
struct pcb_t * get_mlq_proc(void) {
    struct pcb_t * proc = NULL;
    pthread_mutex_lock(&dispatch_lock);
    pthread_mutex_lock(&queue_lock);
    
    int prio = current_prio;
    int attempts = 0;
    
    while (attempts < MAX_PRIO) {
        if (!empty(&mlq_ready_queue[prio]) && current_slot[prio] < slot[prio]) {
            proc = dequeue(&mlq_ready_queue[prio]);
            if (proc != NULL) {
                current_slot[prio]++;
                enqueue(&running_list, proc);
                
                if (current_slot[prio] >= slot[prio]) {
                    current_slot[prio] = 0;
                    current_prio = (prio + 1) % MAX_PRIO;
                }
                
                pthread_mutex_unlock(&queue_lock);
                pthread_mutex_unlock(&dispatch_lock);
                return proc;
            }
        }

        prio = (prio + 1) % MAX_PRIO;
        attempts++;
    
        if (prio == 0) {
            for (int i = 0; i < MAX_PRIO; i++) {
                current_slot[i] = 0;
            }
            current_prio = 0;
        }
    }
    
    pthread_mutex_unlock(&queue_lock);
    pthread_mutex_unlock(&dispatch_lock);
    return proc;	
}

void put_mlq_proc(struct pcb_t * proc) {
	if (proc == NULL) return;
	proc->krnl->ready_queue = &ready_queue;
	proc->krnl->mlq_ready_queue = mlq_ready_queue;
	proc->krnl->running_list = &running_list;
	pthread_mutex_lock(&queue_lock);
	purgequeue(&running_list, proc);
	if (proc->prio < MAX_PRIO) {
		enqueue(&mlq_ready_queue[proc->prio], proc);
	}
	pthread_mutex_unlock(&queue_lock);
}

void add_mlq_proc(struct pcb_t * proc) {
	if (proc == NULL) return;
	
	proc->krnl->ready_queue = &ready_queue;
	proc->krnl->mlq_ready_queue = mlq_ready_queue;
	proc->krnl->running_list = &running_list;
	pthread_mutex_lock(&queue_lock);
	if (proc->prio < MAX_PRIO) {
		enqueue(&mlq_ready_queue[proc->prio], proc);
	} else {
		printf("Warning: Process PID %d has invalid priority %d\\n", 
		       proc->pid, proc->prio);
	}
	
	pthread_mutex_unlock(&queue_lock);	
}

struct pcb_t * get_proc(void) {
	return get_mlq_proc();
}
void put_proc(struct pcb_t * proc) {
	return put_mlq_proc(proc);
}
void add_proc(struct pcb_t * proc) {
	return add_mlq_proc(proc);
}
#else
struct pcb_t * get_proc(void) {
	struct pcb_t * proc = NULL;
	pthread_mutex_lock(&queue_lock);
	if (!empty(&ready_queue)) {
		proc = dequeue(&ready_queue);
		if (proc != NULL) {
			enqueue(&running_list, proc);
		}
	}
	pthread_mutex_unlock(&queue_lock);
	return proc;
}

void put_proc(struct pcb_t * proc) {
	if (proc == NULL) return;
	proc->krnl->ready_queue = &ready_queue;
	proc->krnl->running_list = &running_list;
	pthread_mutex_lock(&queue_lock);
	purgequeue(&running_list, proc);
	enqueue(&run_queue, proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t * proc) {
	if (proc == NULL) return;
	proc->krnl->ready_queue = &ready_queue;
	proc->krnl->running_list = &running_list;
	pthread_mutex_lock(&queue_lock);
	enqueue(&ready_queue, proc);
	pthread_mutex_unlock(&queue_lock);	
}
#endif

struct pcb_t * get_proc_by_pid(int pid) {
    struct pcb_t * proc = NULL;
    pthread_mutex_lock(&queue_lock);
    for (int i = 0; i < running_list.size; i++) {
        if (running_list.proc[i] != NULL && running_list.proc[i]->pid == pid) {
            proc = running_list.proc[i];
            goto found;
        }
    }
#ifdef MLQ_SCHED
    for (int prio = 0; prio < MAX_PRIO; prio++) {
        for (int i = 0; i < mlq_ready_queue[prio].size; i++) {
            if (mlq_ready_queue[prio].proc[i] != NULL && 
                mlq_ready_queue[prio].proc[i]->pid == pid) {
                proc = mlq_ready_queue[prio].proc[i];
                goto found;
            }
        }
    }
#else
    for (int i = 0; i < ready_queue.size; i++) {
        if (ready_queue.proc[i] != NULL && ready_queue.proc[i]->pid == pid) {
            proc = ready_queue.proc[i];
            goto found;
        }
    }
#endif
found:
    pthread_mutex_unlock(&queue_lock);
    return proc;
}

/*
 * find_process_by_pid - Securely find a process by PID in kernel structure
 * This function implements the required PID-based access mechanism
 * to avoid direct PCB passing from userspace
 */
struct pcb_t *find_process_by_pid(struct krnl_t *krnl, uint32_t pid)
{
    struct pcb_t *proc = NULL;

    if (krnl == NULL) {
        return NULL;
    }

    pthread_mutex_lock(&queue_lock);

    /* Search in running_list first */
    if (krnl->running_list != NULL) {
        for (int i = 0; i < krnl->running_list->size; i++) {
            if (krnl->running_list->proc[i] != NULL &&
                krnl->running_list->proc[i]->pid == pid) {
                proc = krnl->running_list->proc[i];
                goto found_pid;
            }
        }
    }

    /* Search in ready queues */
#ifdef MLQ_SCHED
    for (int prio = 0; prio < MAX_PRIO; prio++) {
        for (int i = 0; i < krnl->mlq_ready_queue[prio].size; i++) {
            if (krnl->mlq_ready_queue[prio].proc[i] != NULL &&
                krnl->mlq_ready_queue[prio].proc[i]->pid == pid) {
                proc = krnl->mlq_ready_queue[prio].proc[i];
                goto found_pid;
            }
        }
    }
#else
    /* Search in standard ready queue */
    if (krnl->ready_queue != NULL) {
        for (int i = 0; i < krnl->ready_queue->size; i++) {
            if (krnl->ready_queue->proc[i] != NULL &&
                krnl->ready_queue->proc[i]->pid == pid) {
                proc = krnl->ready_queue->proc[i];
                goto found_pid;
            }
        }
    }
#endif

found_pid:
    pthread_mutex_unlock(&queue_lock);
    return proc;
}