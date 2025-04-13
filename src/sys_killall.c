/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

 #include "common.h"
 #include "syscall.h"
 #include "stdio.h"
 #include "libmem.h"
 
 #include "queue.h" //! include more
 #include <string.h>
 #include <stdlib.h>
 
 void free_process_memory(struct pcb_t *proc)
 {
     for (int i = 0; i < PAGING_MAX_SYMTBL_SZ; i++)
     {
         if (proc->mm->symrgtbl[i].rg_start != 0 || proc->mm->symrgtbl[i].rg_end != 0)
         {
             libfree(proc, i);
         }
     }
 }
 
 int __sys_killall(struct pcb_t *caller, struct sc_regs *regs)
 {
     char proc_name[100];
     uint32_t data;
  
     // hardcode for demo only
     uint32_t memrg = regs->a1;
  
     /* TODO: Get name of the target proc */
     // proc_name = libread..
     int i = 0;
     data = 0;
     while (i < sizeof(proc_name)-1)
     {
         if (libread(caller, memrg, i, &data) != 1) return -1;
         if ((char)data == '\0') break;
         proc_name[i] = (char)data;
         i++;
     }
     proc_name[i] = '\0';
 
     if (strlen(proc_name) == 0) return -1;
 
     printf("The procname retrieved from memregionid %d is \"%s\"\n", memrg, proc_name);
  
     /* TODO: Traverse proclist to terminate the proc
      *       stcmp to check the process match proc_name
      */
     // caller->running_list
     // caller->mlq_ready_queu
 
     /* TODO Maching and terminating
      *       all processes with given
      *        name in var proc_name
      */
 
     if (caller->running_list)
     {
         for (int i = 0; i < caller->running_list->size; i++)
         {
             struct pcb_t *proc = caller->running_list->proc[i];
             if (proc && strstr(proc->path, proc_name))
             {
                 printf("  -> Kill running: %s (PID %d)\n", proc->path, proc->pid);
                 free_process_memory(proc);
                 caller->running_list->proc[i] = NULL;
             }
         }
     }
 
     if (caller->mlq_ready_queue)
     {
         for (int prio = 0; prio < MAX_PRIO; prio++)
         {
             struct queue_t *q = &caller->mlq_ready_queue[prio];
             for (int j = 0; j < q->size; j++)
             {
                 struct pcb_t *proc = q->proc[j];
                 if (proc && strstr(proc->path, proc_name))
                 {
                     printf("  -> Kill mlq ready queue: %s (PID %d)\n", proc->path, proc->pid);
                     free_process_memory(proc);
                     q->proc[j] = NULL;
                 }
             }
         }
     }
 
     // Chua ro co can kill ca ready_queue hay khong
     /*else if (caller->ready_queue)
     {
         for (int i = 0; i < caller->ready_queue->size; ++i)
         {
             struct pcb_t *p = caller->ready_queue->proc[i];
             if (p && strstr(p->path, proc_name))
             {
                 printf("  -> Kill ready queue: %s (PID %d)\n", p->path, p->pid);
                 free_process_memory(p);
                 caller->ready_queue->proc[i] = NULL;
             }
         }
     }*/
 
     return 0;
 }
 