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
static int current_slot[MAX_PRIO];  /* Current slot usage for each priority */
static int current_prio = 0;        /* Current priority level being served */
#endif

int queue_empty(void) {
#ifdef MLQ_SCHED
	unsigned long prio;
	for (prio = 0; prio < MAX_PRIO; prio++)
		if(!empty(&mlq_ready_queue[prio])) 
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

#ifdef MLQ_SCHED
/* 
 *  Stateful design for routine calling
 *  based on the priority and our MLQ policy
 *  We implement stateful here using transition technique
 *  State representation   prio = 0 .. MAX_PRIO, curr_slot = 0..(MAX_PRIO - prio)
 */
struct pcb_t * get_mlq_proc(void) {
	struct pcb_t * proc = NULL;

	pthread_mutex_lock(&queue_lock);
	
	/* MLQ Policy: Always check from highest priority (0) to lowest */
	for (int prio = 0; prio < MAX_PRIO; prio++) {
		/* Check if this priority queue has processes and available slots */
		if (!empty(&mlq_ready_queue[prio]) && 
		    current_slot[prio] < slot[prio]) {
			
			/* Get process from this priority queue */
			proc = dequeue(&mlq_ready_queue[prio]);
			if (proc != NULL) {
				current_slot[prio]++;
				current_prio = prio; /* Update current priority */
				break;
			}
		}
	}
	
	/* If no process found, reset all slots and try again */
	if (proc == NULL) {
		for (int i = 0; i < MAX_PRIO; i++) {
			current_slot[i] = 0;
		}
		
		/* Try again from highest priority */
		for (int prio = 0; prio < MAX_PRIO; prio++) {
			if (!empty(&mlq_ready_queue[prio])) {
				proc = dequeue(&mlq_ready_queue[prio]);
				if (proc != NULL) {
					current_slot[prio] = 1;
					current_prio = prio;
					break;
				}
			}
		}
	}
	
	if (proc != NULL) {
		enqueue(&running_list, proc);
	}
	
	pthread_mutex_unlock(&queue_lock);
	return proc;	
}

void put_mlq_proc(struct pcb_t * proc) {
	if (proc == NULL) return;
	
	proc->krnl->ready_queue = &ready_queue;
	proc->krnl->mlq_ready_queue = mlq_ready_queue;
	proc->krnl->running_list = &running_list;

	/* Put process back to appropriate priority queue with synchronization */
	pthread_mutex_lock(&queue_lock);
	
	/* Remove from running list first if it exists there */
	purgequeue(&running_list, proc);
	
	/* Add back to appropriate priority queue */
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

	/* Add new process to appropriate priority queue with synchronization */
	pthread_mutex_lock(&queue_lock);
	
	/* Ensure priority is valid */
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
	
	/* Get process from ready_queue with highest priority */
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

	/* Put process back to ready queue with synchronization */
	pthread_mutex_lock(&queue_lock);
	
	/* Remove from running list first */
	purgequeue(&running_list, proc);
	
	/* Add to run queue for next scheduling */
	enqueue(&run_queue, proc);
	
	pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t * proc) {
	if (proc == NULL) return;
	
	proc->krnl->ready_queue = &ready_queue;
	proc->krnl->running_list = &running_list;

	/* Add new process to ready queue with synchronization */
	pthread_mutex_lock(&queue_lock);
	enqueue(&ready_queue, proc);
	pthread_mutex_unlock(&queue_lock);	
}
#endif


