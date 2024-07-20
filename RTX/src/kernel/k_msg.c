/**
 * @file:   k_msg.c
 * @brief:  kernel message passing routines
 * @author: Yiqing Huang
 * @date:   2020/10/09
 */

#include "k_msg.h"

#ifdef DEBUG_0
#include "printf.h"
#endif /* ! DEBUG_0 */


/*
 *==========================================================================
 *                            GLOBAL VARIABLES
 *==========================================================================
 */
#define PAD4(x) ((x+3) & ~(3))
 U8 SIZE_OF_TID_PADDING_4_BYTES = PAD4(sizeof(task_t)) - sizeof(task_t);;

int k_mbx_create(size_t size) {
#ifdef DEBUG_0
    printf("k_mbx_create: size = %d\r\n", size);
#endif /* DEBUG_0 */

    // Mailbox initialization
    // set the tail to max value of U32, underflow on purpose
    gp_current_task->mbTail = -1;
    gp_current_task->mbHead = 0;
    gp_current_task->mbSize = 0;

    // EDGE CASES
    if(gp_current_task->mbCapacity != 0 || size < MIN_MBX_SIZE)
    {
    	// capacity is 0 by default, mbCapacity != 0 meaning already have a mailbox
    	return RTX_ERR;
    }
    gp_current_task->mbCapacity = size;

    // Allocate (with kernel ownership) space for the mailbox
    gp_current_task->mailbox = (U8*)k_alloc_p_stack(size);
    if (gp_current_task->mailbox == NULL)
    {
    	// Not enough memory to allocate for mailbox
    	return RTX_ERR;
    }

    // NOTE: When a task exits, mailbox data is deallocated
    return RTX_OK;
}

int IRQ_send_msg(task_t receiver_tid, const void *buf) {
		int returnVal = RTX_OK;
		task_t curTaskTid = gp_current_task->tid;
		gp_current_task->tid = TID_UART_IRQ;
		returnVal = k_send_msg(receiver_tid, buf);
		gp_current_task->tid = curTaskTid;
		return returnVal;
}

int k_send_msg(task_t receiver_tid, const void *buf) {
#ifdef DEBUG_0
    printf("k_send_msg: receiver_tid = %d, buf=0x%x\r\n", receiver_tid, buf);
#endif /* DEBUG_0 */

    TCB *receiver = &g_tcbs[receiver_tid];
    RTX_MSG_HDR *header = (RTX_MSG_HDR*)buf;

    // the cpyMsg condition will execute last after all other ones are checked, do not change the order
    if(receiver->state == DORMANT
    || receiver->mbCapacity == 0
	|| buf == NULL
	|| header->length < (MIN_MSG_SIZE + sizeof(RTX_MSG_HDR))
	|| enqueueMsg(receiver, header) != RTX_OK
    )
    {
    	return RTX_ERR;
    }

    U8 prevState = receiver->state;
	// Unblock blocked receivers
	if (receiver->state == BLK_MSG) {
		receiver->state = READY;
	}

    if (prevState == BLK_MSG) {
		// PREEMPTION CONDITIONS
		TCB *A = gp_current_task;	// Currently running task
		TCB *B = receiver;			// Unblocked task
		U8 P = A->prio;
		U8 Q = B->prio;
		if (isQHigherPrioThanP(Q, P)) {
			/*  if the priority of the unblocked task (Q) is higher than that
			 of the currently running task (P), then the unblocked task (B) preempts the currently
			 running task (A), and the preempted task (A) is added to the back of the ready queue. */

			// DO NOT switch the order, we need readyQueue overwrite to execute first
			if (replaceTopNode(B) != RTX_OK)
				return RTX_ERR;
			assignInsertionOrderToNode(B);

			// note that here A should get a new insertionOrder, latest among all other tasks with same priority
			updateMinInsertionOrder(A->insertionOrder);
			insertNode(A);

			// start executing immediately
			if (switchToTask(B) != RTX_OK)
				return RTX_ERR;
		} else {
			/* If the priority of the unblocked task (Q) is not higher than that of the currently
			 running task (P), then the unblocked task (B) is added to the back of the ready queue. */

			insertNode(B);
		}
	}

    return RTX_OK;
}

int k_recv_msg(task_t *sender_tid, void *buf, size_t len) {
#ifdef DEBUG_0
    printf("k_recv_msg: sender_tid  = 0x%x, buf=0x%x, len=%d\r\n", sender_tid, buf, len);
#endif /* DEBUG_0 */
    if (buf == NULL || gp_current_task -> mailbox == NULL) return RTX_ERR;

    if (isMailBoxEmpty(gp_current_task)) {
    	gp_current_task -> state = BLK_MSG;
    	popMinNode();
    	// don't insertNode() cuz lab manual says BLK_MSG task don't return to readyqueue
    	k_tsk_run_new();
    }

    if (dequeueMsg(sender_tid, buf, gp_current_task, len) != RTX_OK) {
    	return RTX_ERR;
    }
    return RTX_OK;
}

//int k_recv_msg_nb(task_t *sender_tid, void *buf, size_t len) {
//#ifdef DEBUG_0
//    printf("k_recv_msg_nb: sender_tid  = 0x%x, buf=0x%x, len=%d\r\n", sender_tid, buf, len);
//#endif /* DEBUG_0 */
//    return 0;
//}
//
//int k_mbx_ls(task_t *buf, int count) {
//#ifdef DEBUG_0
//    printf("k_mbx_ls: buf=0x%x, count=%d\r\n", buf, count);
//#endif /* DEBUG_0 */
//    return 0;
//}

int isMailBoxFull(TCB* tcb) {
	return tcb -> mbSize == tcb->mbCapacity;
}

int isMailBoxEmpty(TCB* tcb) {
	return tcb->mbSize == 0;
}

int dequeueMsg(task_t* senderTid, void *dest, TCB* mbOwnerTcb, size_t destLen) {
	int returnFlag;
	if (isMailBoxEmpty(mbOwnerTcb) || destLen < sizeof(RTX_MSG_HDR)) {
		return RTX_ERR;
	}

	// strip out the senderTid first
	U8* senderTidInBytes = (U8*)senderTid;
	for(int i = 0; i < sizeof(task_t) ; i++) {
		senderTidInBytes[i] = mbOwnerTcb->mailbox[(mbOwnerTcb->mbHead + i) % mbOwnerTcb->mbCapacity];
	}
	mbOwnerTcb->mbSize -= sizeof(task_t);
	mbOwnerTcb->mbHead = (mbOwnerTcb->mbHead + sizeof(task_t)) % mbOwnerTcb->mbCapacity;

	// skip the tid padding
	mbOwnerTcb->mbSize -= SIZE_OF_TID_PADDING_4_BYTES;
	mbOwnerTcb->mbHead = (mbOwnerTcb->mbHead + SIZE_OF_TID_PADDING_4_BYTES) % mbOwnerTcb->mbCapacity;

	U8 *destInBytes = (U8*) dest;
	RTX_MSG_HDR *destHdr = (RTX_MSG_HDR*) dest;

	// copy the msg header first, to handle the case where the header is on the edge of the circular queue
	// statically allocate this dumHeader to check legnth before copying
	// at this point the buffer(void *dest) user supplies is guaranteed to have big enough size for the header
	// that's why we load the header part directly into the header
	for (int i = 0; i < sizeof(RTX_MSG_HDR); i++) {
		destInBytes[i] = mbOwnerTcb->mailbox[(mbOwnerTcb->mbHead + i)
				% mbOwnerTcb->mbCapacity];
	}

	mbOwnerTcb->mbSize -= sizeof(RTX_MSG_HDR);
	mbOwnerTcb->mbHead = (mbOwnerTcb->mbHead + sizeof(RTX_MSG_HDR)) % mbOwnerTcb->mbCapacity;	// Head is now pointing to the raw data

	// calculate how much we padded the message by
	int messagePadding = PAD4(destHdr -> length) - destHdr -> length;

	U32 msgSizeWithoutHdr = destHdr->length - sizeof(RTX_MSG_HDR);
	if (destHdr -> length <= destLen) {
		for (int i = 0 ; i < msgSizeWithoutHdr ; i++) {
			destInBytes[i+sizeof(RTX_MSG_HDR)] = mbOwnerTcb->mailbox[(mbOwnerTcb->mbHead + i)
					% mbOwnerTcb->mbCapacity];
		}
		returnFlag = RTX_OK;
	} else {
		// If buffer is too small, need to set head back to where it was before we read the size and tid
		returnFlag = RTX_ERR;
	}

	mbOwnerTcb->mbSize -= (msgSizeWithoutHdr + messagePadding);
	mbOwnerTcb->mbHead = (mbOwnerTcb->mbHead + msgSizeWithoutHdr + messagePadding) % mbOwnerTcb->mbCapacity;

	return returnFlag;
}

int enqueueMsg(TCB* destMBTcb, RTX_MSG_HDR *src)
{
	// calculate padding for message
	int messagePadding = PAD4(src -> length) - src -> length;
	// DO NOT update the destMBTcb -> mailBox.length else when we're receving the message we won't know how much we padded it by

	if (isMailBoxFull(destMBTcb) || (destMBTcb->mbCapacity-destMBTcb->mbSize) < (src->length + sizeof(task_t) + SIZE_OF_TID_PADDING_4_BYTES + messagePadding)) {
			return RTX_ERR;
		}

		U8* dest = (U8*)destMBTcb->mailbox;
		task_t senderTid = gp_current_task -> tid;
		U8* senderTidInBytes = (U8*)&senderTid;
		U8* srcInBytes = (U8*) src;

		// + 1 because mbTail is the last occupied slot in the array
		// put the sender tid in the beginning
		for(int i = 1; i < sizeof(task_t) + 1 ; i++) {
			dest[(destMBTcb->mbTail + i) % destMBTcb->mbCapacity] = senderTidInBytes[i-1];
		}

		destMBTcb->mbSize += sizeof(task_t);
		destMBTcb->mbTail = (destMBTcb->mbTail + sizeof(task_t)) % destMBTcb->mbCapacity;

		// add tid padding
		if(SIZE_OF_TID_PADDING_4_BYTES > 0) {
		for(int i = 1; i < SIZE_OF_TID_PADDING_4_BYTES + 1 ; i++) {
			dest[(destMBTcb->mbTail + i) % destMBTcb->mbCapacity] = 0;
		}

		destMBTcb->mbSize += SIZE_OF_TID_PADDING_4_BYTES;
		destMBTcb->mbTail = (destMBTcb->mbTail + SIZE_OF_TID_PADDING_4_BYTES) % destMBTcb->mbCapacity;
		}

		// copy the entire message
		for(int i = 1 ; i < src -> length + 1; i++) {
			// can optimize here, only check of we go circular once
			dest[(destMBTcb->mbTail + i) % destMBTcb->mbCapacity] = srcInBytes[i-1];
		}

		destMBTcb->mbSize += src -> length;
		destMBTcb->mbTail = (destMBTcb->mbTail + src -> length) % destMBTcb->mbCapacity;

		// pad message to 4 byte aligned
		if(messagePadding > 0) {
			for(int i = 1 ; i < messagePadding + 1; i++) {
						// can optimize here, only check of we go circular once
						dest[(destMBTcb->mbTail + i) % destMBTcb->mbCapacity] = 0;
					}

					destMBTcb->mbSize += messagePadding;
					destMBTcb->mbTail = (destMBTcb->mbTail + messagePadding) % destMBTcb->mbCapacity;
		}

		return RTX_OK;
}
