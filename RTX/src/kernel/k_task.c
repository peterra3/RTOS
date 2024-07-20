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
 * @file        k_task.c
 * @brief       task management C file
 *              l2
 * @version     V1.2021.01
 * @authors     Yiqing Huang
 * @date        2021 JAN
 *
 * @attention   assumes NO HARDWARE INTERRUPTS
 * @details     The starter code shows one way of implementing context switching.
 *              The code only has minimal sanity check.
 *              There is no stack overflow check.
 *              The implementation assumes only two simple privileged task and
 *              NO HARDWARE INTERRUPTS.
 *              The purpose is to show how context switch could be done
 *              under stated assumptions.
 *              These assumptions are not true in the required RTX Project!!!
 *              Understand the assumptions and the limitations of the code before
 *              using the code piece in your own project!!!
 *
 *****************************************************************************/

//#include "VE_A9_MP.h"
#include "Serial.h"
#include "k_task.h"
#include "k_rtx.h"

#ifdef DEBUG_0
#include "printf.h"
#endif /* DEBUG_0 */

// defined by students
#define READY_QUEUE_SIZE (g_num_active_tasks - 1)
/*
 *==========================================================================
 *                            GLOBAL VARIABLES
 *==========================================================================
 */

TCB             *gp_current_task = NULL;	// the current RUNNING task
TCB             g_tcbs[MAX_TASKS];			// an array of TCBs
RTX_TASK_INFO   g_null_task_info;			// The null task info
U32             g_num_active_tasks = 0;		// number of non-dormant tasks, note g_num_active_tasks - 1 is the number of elements in ready queue

// below are declared by us
// this is to track all the TIDs
// 0 is reserved for NULL task
// so when nextTidIndex == 0 our stack is actually empty
 task_t tids[MAX_TASKS];
 task_t nextTidIndex;

/* this is a stable binary min heap
 * g_num_active_tasks is also used to mark the end of the heap in the array container
 *
 * we use an array of pointers that points to the actual TCB address in the OS img
 * for simplicity and not having to swap the entire struct of variables when we swap up and down
 * */
 TCB* readyQueue[MAX_TASKS];

/* These variables are used to generate sequence used as insertion order into ready queue
 * curMinInsertionOrder represents the smallest insertion order in the ready queue
 * with this variable we can now make insertion order circular and avoid over flow
 * use unsigned short(U8) for full positive number range and smaller data size
 * because our MAX_TASKS is only around 160
 * */
 task_t curMinInsertionOrder = 0;
 task_t nextAvailableOrder = 0;


/*---------------------------------------------------------------------------
The memory map of the OS image may look like the following:

                       RAM_END+---------------------------+ High Address
                              |                           |
                              |                           |
                              |    Free memory space      |
                              |   (user space stacks      |
                              |         + heap            |
                              |                           |
                              |                           |
                              |                           |
 &Image$$ZI_DATA$$ZI$$Limit-->|---------------------------|-----+-----
                              |         ......            |     ^
                              |---------------------------|     |
                              |      U_STACK_SIZE         |     |
             g_p_stacks[15]-->|---------------------------|     |
                              |                           |     |
                              |  other kernel proc stacks |     |
                              |---------------------------|     |
                              |      U_STACK_SIZE         |  OS Image
              g_p_stacks[2]-->|---------------------------|     |
                              |      U_STACK_SIZE         |     |
              g_p_stacks[1]-->|---------------------------|     |
                              |      U_STACK_SIZE         |     |
              g_p_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |                           |  OS Image
                              |---------------------------|     |
                              |      K_STACK_SIZE         |     |                
             g_k_stacks[15]-->|---------------------------|     |
                              |                           |     |
                              |     other kernel stacks   |     |                              
                              |---------------------------|     |
                              |      K_STACK_SIZE         |  OS Image
              g_k_stacks[2]-->|---------------------------|     |
                              |      K_STACK_SIZE         |     |                      
              g_k_stacks[1]-->|---------------------------|     |
                              |      K_STACK_SIZE         |     |
              g_k_stacks[0]-->|---------------------------|     |
                              |   other  global vars      |     |
                              |---------------------------|     |
                              |        TCBs               |  OS Image
                      g_tcbs->|---------------------------|     |
                              |        global vars        |     |
                              |---------------------------|     |
                              |                           |     |          
                              |                           |     |
                              |                           |     |
                              |                           |     V
                     RAM_START+---------------------------+ Low Address
    
---------------------------------------------------------------------------*/ 

/*
 *===========================================================================
 *                            FUNCTIONS
 *===========================================================================
 */

/**************************************************************************//**
 * @brief   scheduler, pick the TCB of the next to run task
 *
 * @return  TCB pointer of the next to run task
 * @post    gp_curret_task is updated
 *
 *****************************************************************************/

TCB *scheduler(void)
{
    /* remember, don't remove the first prioity task from the min heap until it is finished
     * because if we remove it right away and the job gets preempted,
     * we would have to add it back and bubble up again
     */

	if (READY_QUEUE_SIZE <= 0) {
	    return &g_tcbs[0];
	}
	return readyQueue[0];

}



/**************************************************************************//**
 * @brief       initialize all boot-time tasks in the system,
 *
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       task_info   boot-time task information structure pointer
 * @param       num_tasks   boot-time number of tasks
 * @pre         memory has been properly initialized
 * @post        none
 *
 * @see         k_tsk_create_new
 *****************************************************************************/

int k_tsk_init(RTX_TASK_INFO *task_info, int num_tasks) {
	extern U32 SVC_RESTORE;

	RTX_TASK_INFO *p_taskinfo = &g_null_task_info;
	g_num_active_tasks = 0;

	// double check with TA that MAX_TASKS should include null task
	if (num_tasks > MAX_TASKS - 1) {
		return RTX_ERR;
	}

	// create the first task, which is the NULL task
	// null task doesn't need a mail box so no mailbox related fields initialization
	TCB *p_tcb = &g_tcbs[0];
	p_tcb->prio = PRIO_NULL;
	p_tcb->priv = 1;
	p_tcb->tid = TID_NULL;
	p_tcb->state = RUNNING;
	g_num_active_tasks++;
	gp_current_task = p_tcb;

	/* init the stack that stores all available tids, put all the numbers into every slot
	 suppose MAX_TASKS = 16
	 number of available tids should be MAX_TASKS - 1 = 15, because 0 is reserved for null task
	 tids[0] = 0
	 tids[1] = 1
	 ...
	 tids[14] = 14
	 tids[15] = 15

	 This must be done before start creating the task*/
	for (int i = 0; i < TID_KCD; i++) {
		tids[i] = i;
	}
	if (MAX_TASKS <= TID_UART_IRQ) {
		for (int i = TID_KCD + 1; i < MAX_TASKS; i++) {
			tids[i - 1] = i;
		}
		nextTidIndex = MAX_TASKS - 2;
	} else {
		for (int i = TID_KCD + 1; i < TID_UART_IRQ; i++) {
			tids[i - 1] = i;
		}
		for (int i = TID_UART_IRQ + 1; i < MAX_TASKS; i++) {
			tids[i - 2] = i;
		}
		nextTidIndex = MAX_TASKS - 3;
	}

	/* set all the tcbs in the os image to dormant so when we can check if a tcb is valid
	 * by referencing it with it's tid at the corresponding tcb
	 * except null task tcb at index 0
	 * */
	for(int i = 1 ; i < MAX_TASKS ; i++) {
		g_tcbs[i].state = DORMANT;
	}

	// create the rest of the tasks
	// Assume no preemption for initialization
	p_taskinfo = task_info;
	for (int i = 0; i < num_tasks; i++) {
		// TID_KCD reserved for kcd task
		task_t usedTid = p_taskinfo -> ptask == kcd_task ? TID_KCD : tids[nextTidIndex];
		TCB *p_tcb = &g_tcbs[usedTid];
		if (k_tsk_create_new(p_taskinfo, p_tcb, usedTid) == RTX_OK) {
			// Don't increment number of active task here. Handle it when pushing node in heap
			// g_num_active_tasks++;

			// moving tids top of stack index is not handled in k_tsk_create_new
			insertNode(p_tcb);
	        nextTidIndex--;
		}

		// note that pointer arithmetic depends on the size of its type
		// so p_taskinfo++ is actually p_taskinfo = p_taskinfo + sizeof(RTX_TASK_INFO)
		p_taskinfo++;
	}

	return RTX_OK;
}

/**************************************************************************//**
 * @brief       initialize a new task in the system,
 *              one dummy kernel stack frame, one dummy user stack frame
 *
 * @return      RTX_OK on success; RTX_ERR on failure
 * @param       p_taskinfo  task information structure pointer
 * @param       p_tcb       the tcb the task is assigned to
 * @param       tid         the tid the task is assigned to
 *
 * @details     From bottom of the stack,
 *              we have user initial context (xPSR, PC, SP_USR, uR0-uR12)
 *              then we stack up the kernel initial context (kLR, kR0-kR12)
 *              The PC is the entry point of the user task
 *              The kLR is set to SVC_RESTORE
 *              30 registers in total
 *
 *****************************************************************************/
int k_tsk_create_new(RTX_TASK_INFO *p_taskinfo, TCB *p_tcb, task_t tid)
{
	/* p_taskinfo should be filled and we copy the info over to p_tcb
	 * initialize all fields(except insertion order) in p_tcb in this function, to make initialization in k_tsk_create() cleaner
	 *
	 * note that p_taskinfo does not contain a tid, instead it is passed in separately.
	 * */

	p_taskinfo -> tid = tid;
	p_tcb -> tid = tid;
	p_tcb -> prio = p_taskinfo -> prio;
	p_tcb -> priv = p_taskinfo -> priv;
	p_tcb -> k_stack_size = p_taskinfo -> k_stack_size;
	p_tcb -> u_stack_size = p_taskinfo -> u_stack_size;
	p_tcb -> ptask = p_taskinfo -> ptask;

	// initialize the mail box related fields
	p_tcb -> mailbox = NULL;
	p_tcb -> mbTail = -1;
	p_tcb -> mbHead = 0;
	p_tcb -> mbCapacity = 0;
	p_tcb -> mbSize = 0;

    extern U32 SVC_RESTORE;

    U32 *sp;

    if (p_taskinfo == NULL || p_tcb == NULL)
    {
        return RTX_ERR;
    }

    p_tcb ->tid = tid;
    p_tcb->state = READY;
	p_taskinfo -> state = READY;


    /*---------------------------------------------------------------
     *  Step1: allocate kernel stack for the task
     *         stacks grows down, stack base is at the high address
     * -------------------------------------------------------------*/

    ///////sp = g_k_stacks[tid] + (K_STACK_SIZE >> 2) ;
    sp = k_alloc_k_stack(tid);

    p_taskinfo -> k_stack_hi = (U32) sp;
    p_tcb -> k_stack_hi = (U32) sp;

    // 8B stack alignment adjustment
    if ((U32)sp & 0x04) {   // if sp not 8B aligned, then it must be 4B aligned
        sp--;               // adjust it to 8B aligned
    }

    /*-------------------------------------------------------------------
     *  Step2: create task's user/sys mode initial context on the kernel stack.
     *         fabricate the stack so that the stack looks like that
     *         task executed and entered kernel from the SVC handler
     *         hence had the user/sys mode context saved on the kernel stack.
     *         This fabrication allows the task to return
     *         to SVC_Handler before its execution.
     *
     *         16 registers listed in push order
     *         <xPSR, PC, uSP, uR12, uR11, ...., uR0>
     * -------------------------------------------------------------*/

    // if kernel task runs under SVC mode, then no need to create user context stack frame for SVC handler entering
    // since we never enter from SVC handler in this case
    // uSP: initial user stack
    if ( p_taskinfo->priv == 0 ) { // unprivileged task
        // xPSR: Initial Processor State
        *(--sp) = INIT_CPSR_USER;
        // PC contains the entry point of the user/privileged task
        *(--sp) = (U32) (p_taskinfo->ptask);

        //********************************************************************//
        //*** allocate user stack from the user space, not implemented yet ***//
        //********************************************************************//

        // be careful that when NULL is casted to U32 it's 0s so we make the NULL check before casting it to U32
        U32* userStackStartPtr = k_alloc_p_stack(p_taskinfo -> u_stack_size);
        if (userStackStartPtr == NULL) {
            return RTX_ERR;
        }

        p_taskinfo -> u_stack_hi = (U32) userStackStartPtr;
        p_tcb -> u_stack_hi = (U32) userStackStartPtr;

        // not sure why they want to cast it to a U32
        // cuz we're on 32 bit processor technically a pointer is a U32
        *(--sp) = (U32) userStackStartPtr;

        // uR12, uR11, ..., uR0
        for ( int j = 0; j < 13; j++ ) {
            *(--sp) = 0x0;
        }
    } else {
    	// set both to 0 to avoid confusion in debugging
    	p_taskinfo -> u_stack_size = 0;
    	p_tcb -> u_stack_size = 0;
    }


    /*---------------------------------------------------------------
     *  Step3: create task kernel initial context on kernel stack
     *
     *         14 registers listed in push order
     *         <kLR, kR0-kR12>
     * -------------------------------------------------------------*/
    if ( p_taskinfo->priv == 0 ) {
        // user thread LR: return to the SVC handler
        *(--sp) = (U32) (&SVC_RESTORE);
    } else {
        // kernel thread LR: return to the entry point of the task
        *(--sp) = (U32) (p_taskinfo->ptask);
    }

    // kernel stack R0 - R12, 13 registers
    for ( int j = 0; j < 13; j++) {
        *(--sp) = 0x0;
    }

    // kernel stack CPSR
    *(--sp) = (U32) INIT_CPSR_SVC;
    p_tcb->ksp = sp;

    return RTX_OK;
}

/**************************************************************************//**
 * @brief       switching kernel stacks of two TCBs
 * @param:      p_tcb_old, the old tcb that was in RUNNING
 * @return:     RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre:        gp_current_task is pointing to a valid TCB
 *              gp_current_task->state = RUNNING
 *              gp_crrent_task != p_tcb_old
 *              p_tcb_old == NULL or p_tcb_old->state updated
 * @note:       caller must ensure the pre-conditions are met before calling.
 *              the function does not check the pre-condition!
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *
 *****************************************************************************/
__asm void k_tsk_switch(TCB *p_tcb_old)
{
        PUSH    {R0-R12, LR}
        MRS 	R1, CPSR
        PUSH 	{R1}
        STR     SP, [R0, #TCB_KSP_OFFSET]   ; save SP to p_old_tcb->ksp
        LDR     R1, =__cpp(&gp_current_task);
        LDR     R2, [R1]
        LDR     SP, [R2, #TCB_KSP_OFFSET]   ; restore ksp of the gp_current_task
        POP		{R0}
        MSR		CPSR_cxsf, R0
        POP     {R0-R12, PC}
}


/**************************************************************************//**
 * @brief       run a new thread. The caller becomes READY and
 *              the scheduler picks the next ready to run task.
 * @return      RTX_ERR on error and zero on success
 * @pre         gp_current_task != NULL && gp_current_task == RUNNING
 * @post        gp_current_task gets updated to next to run task
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * @attention   CRITICAL SECTION
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *****************************************************************************/
int k_tsk_run_new(void)
{
    TCB *p_tcb_old = NULL;
    
    if (gp_current_task == NULL) {
    	return RTX_ERR;
    }

    p_tcb_old = gp_current_task;
    gp_current_task = scheduler();
    
    if ( gp_current_task == NULL  ) {
        gp_current_task = p_tcb_old;        // revert back to the old task
        return RTX_ERR;
    }

    // at this point, gp_current_task != NULL and p_tcb_old != NULL
    if (gp_current_task != p_tcb_old) {
        gp_current_task->state = RUNNING;

        // if a to-be-switched-out task is in either dormant or BLK_MSG or SUSPENDED the state persist
        // else put it to ready
        p_tcb_old -> state = p_tcb_old -> state == RUNNING ? READY : p_tcb_old -> state;
    	k_tsk_switch(p_tcb_old);            // switch stacks
        }
    return RTX_OK;
}

/**************************************************************************//**
 * @brief       yield the cpu
 * @return:     RTX_OK upon success
 *              RTX_ERR upon failure
 * @pre:        gp_current_task != NULL &&
 *              gp_current_task->state = RUNNING
 * @post        gp_current_task gets updated to next to run task
 * @note:       caller must ensure the pre-conditions before calling.
 *****************************************************************************/
int k_tsk_yield(void)
{
	if (gp_current_task -> prio == PRIO_NULL) {
		return k_tsk_run_new();
	}

	switch (READY_QUEUE_SIZE) {
	case 0: {
		// nothing in ready queue, should let null task run
		return k_tsk_run_new();
	}
	case 1: {
		// only readyQueue[0] has something and that task called yield
		// no need to compare priority of anything and we should let it keep running
		return k_tsk_run_new();
	}
	case 2: {
		// there are 2 tasks in readyQueue[0] and readyQueue[1]
		// one current running task and one other ready task
		// compare current running task's priority to the ready task's

		TCB *A = gp_current_task;
		U8 P = A->prio;
		U8 leftChildPrio = readyQueue[1]->prio;
		if (!isQHigherPrioThanP(P, leftChildPrio)) {
			popMinNode();
			insertNode(A);
			return k_tsk_run_new();
		}

		return RTX_OK;
	}
	default: {
		// there are more including 3 nodes in the ready queue, compare both nodes

		TCB *A = gp_current_task;
		U8 P = A->prio;
		U8 leftChildPrio = readyQueue[1]->prio;
		U8 rightChildPrio = readyQueue[1]->prio;

		if (!(isQHigherPrioThanP(P,leftChildPrio) && isQHigherPrioThanP(P, rightChildPrio))) {
			/*	Otherwise, the highest-priority task in the ready queue starts running, and
			 *  A is added to the back of the ready queue
			 */

			popMinNode();
			insertNode(A);
			return k_tsk_run_new();
		}
		/* A continues running only if Q is strictly higher than the priority of the
		 * highest-priority task in the ready queue.
		 */
		return RTX_OK;
	}
	}
}

/*
 *===========================================================================
 *                             TO BE IMPLEMETED IN LAB2
 *===========================================================================
 */

int k_tsk_create(task_t *task, void (*task_entry)(void), U8 prio, U16 stack_size)
{
#ifdef DEBUG_0
    printf("k_tsk_create: entering...\n\r");
    printf("task = 0x%x, task_entry = 0x%x, prio=%d, stack_size = %d\n\r", task, task_entry, prio, stack_size);
#endif /* DEBUG_0 */

    // May be more failure cases
    if(nextTidIndex <= 0
    		|| stack_size < U_STACK_SIZE
			|| prio == PRIO_NULL
			|| prio == PRIO_RT
			|| task == NULL
			|| task_entry == NULL
			|| (stack_size % 8) != 0
			) {
    	// no more available tids, meaning we have hit the max amount of tasks
    	// requested stack size is less than U_STACK_SIZE which is the minimum
    	// invalid priority values
    	return RTX_ERR;
    }

	*task = tids[nextTidIndex];
	nextTidIndex--;

	RTX_TASK_INFO taskInfo;
	// k_stack_hi and u_stack_hi is initialized in k_tsk_create_new
	taskInfo.ptask = task_entry;
	taskInfo.k_stack_size = K_STACK_SIZE;

	// do not pad it for them, return error if whatever passed in is not 8 byte aligned
	// this is specified in the lab manual
	taskInfo.u_stack_size = stack_size;
	taskInfo.tid = *task;
	taskInfo.prio = prio;
	taskInfo.state = READY;
	taskInfo.priv = 0;

	if(k_tsk_create_new(&taskInfo, &g_tcbs[*task], *task) != RTX_OK) {
		// possibility that the requested is too big and k_mem_alloc returns error
		return RTX_ERR;
	}

	// task creation logic for preemption, written in variables like in the lab manual
	TCB *A = gp_current_task;
	TCB *B = &g_tcbs[*task];
	U8 P = A->prio;
	U8 Q = B->prio;

	if (isQHigherPrioThanP(Q, P)) {
		//	If Q > P, then B preempts A(current task) and starts running immediately. In this case,
		//	A is added to the back of the ready queue (i.e., A will be sorted as the last
		//	task among all tasks with priority P)

		// DO NOT switch the order, we need readyQueue overwrite to execute first
		if (replaceTopNode(B) != RTX_OK) return RTX_ERR;
		assignInsertionOrderToNode(B);

		// note that here A should get a new insertionOrder, latest among all other tasks with same priority
		updateMinInsertionOrder(A -> insertionOrder);
		insertNode(A);

		// start executing immediately
		if (switchToTask(B) != RTX_OK) return RTX_ERR;
	} else {
		//	If Q <= P, then B is added to the back of the ready queue. (i.e., B will be
		//	sorted as the last task among all tasks with priority Q)

		insertNode(B);
	}

    return RTX_OK;

}

void k_tsk_exit(void) 
{
#ifdef DEBUG_0
    printf("k_tsk_exit: entering...\n\r");
#endif /* DEBUG_0 */

    if(READY_QUEUE_SIZE == 0) return;

    gp_current_task -> state = DORMANT;

    if (gp_current_task -> priv == 0) {
		// deallocate stack memory
    	k_dealloc_p_stack((void*)gp_current_task->u_stack_hi);
    }

    // Need to deallocate mailbox if it exists
    if(gp_current_task->mbCapacity != 0)
    {
    	k_dealloc_p_stack(gp_current_task->mailbox);
    }

    tids[++nextTidIndex] = gp_current_task -> tid;

    popMinNode();
    k_tsk_run_new();
    return;
}

int k_tsk_set_prio(task_t task_id, U8 prio) 
{
#ifdef DEBUG_0
    printf("k_tsk_set_prio: entering...\n\r");
    printf("task_id = %d, prio = %d.\n\r", task_id, prio);
#endif /* DEBUG_0 */

    if (prio == PRIO_NULL || prio == PRIO_RT || task_id >= MAX_TASKS || task_id <= 0 || g_tcbs[task_id].state == DORMANT  ) {
    	//invalid priority values
	   return RTX_ERR;
    }

    TCB* targetTcb = &g_tcbs[task_id];
    U32 curTskId = k_tsk_get_tid();

    if (curTskId != task_id) {
    	// changing someone else's priority

		// logic for checking if a priority change is allowed based on task privilege level
		if (g_tcbs[curTskId].priv == 1) {
			// kernel task can change priority of any other tasks
			targetTcb -> prio = prio;
		} else {
			if (targetTcb -> priv == 1) {
				// user task can't change priority of kernel task
				return RTX_ERR;
			}
			targetTcb -> prio = prio;
		}

		// Priority change rule I
		TCB *A = gp_current_task; // this is the same as readyQueue[0]
		TCB *B = targetTcb;
		if (targetTcb -> state == BLK_MSG || targetTcb->state == SUSPENDED) return RTX_OK;
		U8 P = A -> prio;
		U8 Q = prio;
		if (isQHigherPrioThanP(Q, P)) {
			/* If Q > P, then B preempts A, and A is added to the back of the ready queue.
			 * note that here A is at the top of the ready queue
			 * */

			// DO NOT switch the order of the next following functions, we need readyQueue overwrite to execute first
			// note B does not get a new insertion order
			int indexOfB = getIndex(B);
			if (indexOfB == -1) return RTX_ERR;
			if (replaceTopNode(B) != RTX_OK || removeElement(indexOfB) != RTX_OK) return RTX_ERR;

			// ask TA if A gets a new insertion order, assume it does
			// should update min insertion order since we essentially popped a node and readded
			updateMinInsertionOrder(A -> insertionOrder);
			insertNode(A);

			// start executing immediately
			if (switchToTask(B) != RTX_OK) return RTX_ERR;
		} else {
			/* If Q <= P, then B is added to the back of the ready queue (even if Q is equal
			 * to B's current priority).
			 */

			insertNode(B);
		}
    } else {
    	// changing own priority
    	// no need to check if priority change is allowed based on privilege level cuz it's always allowed

        // Priority change rule II
    	TCB* A = targetTcb;
    	U8 Q = prio;
    	targetTcb -> prio = Q;

    	switch(READY_QUEUE_SIZE) {
    	case 0: break;
    	case 1: break;
    	case 2: {
        	TCB* leftChild = readyQueue[1];
        	if (!isQHigherPrioThanP(Q, leftChild -> prio)) {
        		popMinNode();
        		insertNode(A);
        		k_tsk_run_new();
        	}
        	break;
    	}
    	default: {
    		TCB* leftChild = readyQueue[1];
    		TCB* rightChild = readyQueue[2];
    		if (!(isQHigherPrioThanP(Q, leftChild -> prio) && isQHigherPrioThanP(Q, rightChild -> prio))) {
    			popMinNode();
    			insertNode(A);
        		k_tsk_run_new();
    		}
    		break;
    	}
    	}
    }

    return RTX_OK;    
}

int k_tsk_get_info(task_t task_id, RTX_TASK_INFO *buffer)
{
#ifdef DEBUG_0
    printf("k_tsk_get_info: entering...\n\r");
    printf("task_id = %d, buffer = 0x%x.\n\r", task_id, buffer);
#endif /* DEBUG_0 */    
	if (buffer == NULL || g_tcbs[task_id].state == DORMANT) {
		// task with task id of <task_id> does not exist
		return RTX_ERR;
	}

    // put all info in tcb into task info
    TCB targetTcb = g_tcbs[task_id];

    buffer->tid = task_id;
    buffer->prio = targetTcb.prio;
    buffer->state = targetTcb.state;
    buffer->priv = targetTcb.priv;
    buffer->ptask = targetTcb.ptask;
    buffer->k_stack_hi = targetTcb.k_stack_hi;
    buffer->u_stack_hi = targetTcb.u_stack_hi;
    buffer->k_stack_size = targetTcb.k_stack_size;
    buffer->u_stack_size = targetTcb.u_stack_size;

    return RTX_OK;     
}

task_t k_tsk_get_tid(void)
{
#ifdef DEBUG_0
    printf("k_tsk_get_tid: entering...\n\r");
#endif /* DEBUG_0 */ 
    return gp_current_task -> tid;
}

int k_tsk_ls(task_t *buf, int count){
#ifdef DEBUG_0
    printf("k_tsk_ls: buf=0x%x, count=%d\r\n", buf, count);
#endif /* DEBUG_0 */
    return 0;
}

/* following helper functions are for the binary stable min heap
 * Current Node	readyQueue[i]
 * Parent Node	readyQueue[(i-1)/2]
 * Left Child	readyQueue[(2*i) + 1]
 * Right Child	readyQueue[(2*i )+ 2]
 */
int getParentIndex(int index) {
	return (index - 1)/2;
}

int getLeftChildIndex(int index) {
    return (index*2 + 1);
}

int getRightChildIndex(int index) {
    return (index*2 + 2);
}

int replaceTopNode(TCB *node) {
	/* CAUSTION: this is hard overwriting the ready queue
	 * please only do this if you know what your doing
	 * in this case the highest priority task should be B
	 * so we should be ok to overwrite the top of the ready queue
	 *
	 * note that this function is not a proper heap function so
	 * does not handle assigning insertion order
	 * should call separately
	 */
	TCB* curTask = gp_current_task;
	if (curTask -> prio <= node -> prio) {
		// even cur task and new task has the same priority
		// the new task would still have a later insertionOrder than old task
		// so we should not allow overwrite
		return RTX_ERR;
	}
	readyQueue[0] = node;
	readyQueue[0] -> indexInReadyQueue = 0;

	return RTX_OK;
}

void assignInsertionOrderToNode(TCB *node) {
	node -> insertionOrder = nextInsertionOrder();
}

int insertNode(TCB *node) {
	if (READY_QUEUE_SIZE >= MAX_TASKS - 1) {
		// queue is full, can't insert no more
		// MAST_TASKS - 1 because of null task
		return RTX_ERR;
	}

	assignInsertionOrderToNode(node);
	readyQueue[READY_QUEUE_SIZE] = node;
	readyQueue[READY_QUEUE_SIZE] -> indexInReadyQueue = READY_QUEUE_SIZE;

	int curIndex = READY_QUEUE_SIZE;

	// note we compare insertion order if priority is the same
	while ( curIndex > 0 && hasGreaterPrioity(readyQueue[curIndex], readyQueue[getParentIndex(curIndex)])) {
	        // Swap with parent
	        TCB* temp = readyQueue[getParentIndex(curIndex)];

	        readyQueue[getParentIndex(curIndex)] = readyQueue[curIndex];
	        readyQueue[getParentIndex(curIndex)] -> indexInReadyQueue = getParentIndex(curIndex);

	        readyQueue[curIndex] = temp;
	        readyQueue[curIndex] -> indexInReadyQueue = curIndex;

	        // Update the current index of element
	        curIndex = getParentIndex(curIndex);
	    }

	// update index
	g_num_active_tasks++;

	return RTX_OK;
}

int hasGreaterPrioity(TCB* nodeA, TCB* nodeB) {
	/* does task A have higher priority than task B
	 * note that the smaller the number the higher the priority
	 * also we compare insertion order if priority is the same
	 * */
	return ( nodeA -> prio < nodeB -> prio || ( nodeA -> prio == nodeB -> prio && isEarlier(nodeA -> insertionOrder, nodeB -> insertionOrder) ) );
}

int popMinNode() {
    // This function deletes the node with minimum priority at the top of the heap
    if ( READY_QUEUE_SIZE <= 0 ) {
    	// note null task is always active
    	// <= 1 means there is no tasks in ready queue
    	return RTX_ERR;
    }

    // swap root node with the last node
    // last node removed by decreasing size by 1
    TCB* toBePopped = readyQueue[0];
    readyQueue[0] = readyQueue[READY_QUEUE_SIZE - 1];
    readyQueue[0] -> indexInReadyQueue = 0;

    g_num_active_tasks --;

    if(heapify(0)!= RTX_OK) {
    	return RTX_ERR;
    }

    updateMinInsertionOrder(toBePopped -> insertionOrder);
    return RTX_OK;
}

int removeElement(int index) {
	if (index < 0 || READY_QUEUE_SIZE <= 0) {
		// should not call this function when heap size is 0
		return RTX_ERR;
	}

	// swap with the last element
	readyQueue[index] = readyQueue[READY_QUEUE_SIZE - 1];
	readyQueue[index] -> indexInReadyQueue = index;

	g_num_active_tasks--;
	heapify(index);
	// purposely not call updateMinInsertionOrder here cuz the requirement for set_prio.
	// after replacedTop the node should not get a new insertion order
	return RTX_OK;
}

int getIndex(TCB* tcbPtr) {
    return tcbPtr -> indexInReadyQueue;
}

int heapify(int curIndex) {
	// this function only checks the cur node and everything under it
	if (READY_QUEUE_SIZE <= 1) {
		// were done if readyQueue has only one task in it
        return RTX_OK;
	}

	int leftIndex = getLeftChildIndex(curIndex);
	int rightIndex = getRightChildIndex(curIndex);

	// in the next couple lines we find the smallest out of cur, left child and right child
	// note that in binary min heap left child node is always smaller than right child node
	// index of the smallest element, we first assume it to be the target node
	int highestPrioityIndex = curIndex;

	// compare left child with cur, also check if right child exist
	if (leftIndex < READY_QUEUE_SIZE
			&& hasGreaterPrioity(readyQueue[leftIndex], readyQueue[highestPrioityIndex])) {
		highestPrioityIndex = leftIndex;
	}

	// compare right child with highestPrioity{leftChild, cur}, also check if right child exist
	if (rightIndex < READY_QUEUE_SIZE
			&& hasGreaterPrioity(readyQueue[rightIndex], readyQueue[highestPrioityIndex]))
		highestPrioityIndex = rightIndex;

	// if curIndex is not the highestPrioityIndex, swap them
	// also do this recursively, with target node being the smallest
	if (highestPrioityIndex != curIndex) {
		TCB *temp = readyQueue[curIndex];
		readyQueue[curIndex] = readyQueue[highestPrioityIndex];
		readyQueue[curIndex] -> indexInReadyQueue = curIndex;

		readyQueue[highestPrioityIndex] = temp;
		readyQueue[highestPrioityIndex] -> indexInReadyQueue = highestPrioityIndex;

		// note that here we call heapify with highestPrioityIndex because we never updated it,
		// it's still either left or right node
		return heapify(highestPrioityIndex);
	}

	return RTX_OK;
}

/*	below helper function are associated with insertion order
 * 	in order to make our binary min heap stable
 * */
task_t nextInsertionOrder() {
	task_t nextVal = nextAvailableOrder;
	//  purposely let it overflow
	nextAvailableOrder = nextAvailableOrder + 1;
	return nextVal;
}

void updateMinInsertionOrder(task_t order) {
	// this function should be called whenever an task is exited
	// and the insertion order is returned to be available for someone else
	curMinInsertionOrder = order == curMinInsertionOrder ? curMinInsertionOrder + 1 : curMinInsertionOrder;
}

int isEarlier(task_t a, task_t b) {
	// the smaller the number the earlier it's inserted
	// so this function also means "is a smaller than b"
	if ( curMinInsertionOrder > a && curMinInsertionOrder <= b) {
		//real of a > real of b
		return 0;
	} else if ( curMinInsertionOrder > b && curMinInsertionOrder <= a ) {
		// real of b > real of a
		return 1;
	} else {
		return a < b;
	}
}

/* The below helper functions are preemption related
 * */
int isQHigherPrioThanP(U8 Q, U8 P)
{
	// Note that in the context of priority, the lower value has greater priority
	return Q<P;
}

int switchToTask(TCB* newTask) {
	// essentially this is the same as k_tsk_run_new()
	// but we can specify a task instead of poping from readyQueue
	// USE WITH CAUTION AND UPDATE READYQUEUE ACCORDINGLY

	if (gp_current_task == newTask || newTask == NULL) {
		return RTX_ERR;
	}

    TCB *p_tcb_old = NULL;

    p_tcb_old = gp_current_task;
    gp_current_task = newTask;

	gp_current_task->state = RUNNING;   // change state of the to-be-switched-in  tcb
	p_tcb_old->state = READY;           // change state of the to-be-switched-out tcb
	k_tsk_switch(p_tcb_old);            // switch stacks

	return RTX_OK;
}

/*
 *===========================================================================
 *                             TO BE IMPLEMETED IN LAB4
 *===========================================================================
 */

int k_tsk_create_rt(task_t *tid, TASK_RT *task)
{
    return 0;
}

void k_tsk_done_rt(void) {
#ifdef DEBUG_0
    printf("k_tsk_done: Entering\r\n");
#endif /* DEBUG_0 */
    return;
}

void k_tsk_suspend(TIMEVAL *tv)
{
#ifdef DEBUG_0
    printf("k_tsk_suspend: Entering\r\n");
#endif /* DEBUG_0 */
    return;
}

/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
