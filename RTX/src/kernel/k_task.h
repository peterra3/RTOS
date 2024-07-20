/*
 ****************************************************************************
 *
 *                  UNIVERSITY OF WATERLOO ECE 350 RTOS LAB
 *
 *                     Copyright 2020-2021 Yiqing Huang
 *                          All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice and the following disclaimer.
 *
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL COPYRIGHT HOLDERS AND CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 ****************************************************************************
 */

/**************************************************************************//**
 * @file        k_task.h
 * @brief       Task Management Header File
 *
 * @version     V1.2021.01
 * @authors     Yiqing Huang
 * @date        2021 JAN
 *
 * @details
 * @note        Starter code assumes there are only two privileged tasks
 *
 *****************************************************************************/

#ifndef K_TASK_H_
#define K_TASK_H_

#include "k_inc.h"
#include "k_HAL_CA.h"
#include "../app/kcd_task.h"

/*
 *==========================================================================
 *                            GLOBAL VARIABLES
 *==========================================================================
 */

extern TCB *gp_current_task;

/*
 *===========================================================================
 *                            FUNCTION PROTOTYPES
 *===========================================================================
 */

extern void task_null       (void);


// Implemented by Starter Code

int     k_tsk_init          (RTX_TASK_INFO *task_info, int num_tasks);
                                 /* initialize all tasks in the system */
int     k_tsk_create_new    (RTX_TASK_INFO *p_taskinfo, TCB *p_tcb, task_t tid);
                                 /* create a new task with initial context sitting on a dummy stack frame */
TCB *   scheduler           (void);  /* return the TCB of the next ready to run task */
void    k_tsk_switch        (TCB *); /* kernel thread context switch, two stacks */
int     k_tsk_run_new       (void);  /* kernel runs a new thread  */
int     k_tsk_yield         (void);  /* kernel tsk_yield function */

// Not implemented, to be done by students
int     k_tsk_create        (task_t *task, void (*task_entry)(void), U8 prio, U16 stack_size);
void    k_tsk_exit          (void);
int     k_tsk_set_prio      (task_t task_id, U8 prio);
int     k_tsk_get_info      (task_t task_id, RTX_TASK_INFO *buffer);
task_t  k_tsk_get_tid       (void);
int     k_tsk_create_rt     (task_t *tid, TASK_RT *task);
void    k_tsk_done_rt       (void);
void    k_tsk_suspend       (struct timeval_rt *tv);

// helper functions added by students
int getParentIndex(int index);
int getLeftChildIndex(int index);
int getRightChildIndex(int index);
int insertNode(TCB *node);
int hasGreaterPrioity(TCB* nodeA, TCB* nodeB);
int popMinNode(void);
int heapify(int curIndex);
task_t nextInsertionOrder(void);
void updateMinInsertionOrder(task_t order);
int isEarlier(task_t a, task_t b);
int isQHigherPrioThanP(U8 Q, U8 P);
int switchToTask(TCB* newTask);
int replaceTopNode(TCB *node);
void assignInsertionOrderToNode(TCB *node);
int removeElement(int index);
int getIndex(TCB* tcbPtr);


#endif // ! K_TASK_H_
