#ifndef SCHED_H
#define SCHED_H

#include "common.h"

#ifndef MLQ_SCHED
#define MLQ_SCHED
#endif

#define MAX_PRIO 140

int queue_empty(void);

void init_scheduler(void);
void finish_scheduler(void);

struct pcb_t * get_proc(void);
void put_proc(struct pcb_t * proc);
void add_proc(struct pcb_t * proc);

struct pcb_t * get_proc_by_pid(int pid);
void finish_proc(struct pcb_t * proc);

struct pcb_t *find_process_by_pid(struct krnl_t *krnl, uint32_t pid);

#endif