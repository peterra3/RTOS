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
 * @file        k_mem.c
 * @brief       Kernel Memory Management API C Code
 *
 * @version     V1.2021.01.lab2
 * @authors     Yiqing Huang
 * @date        2021 JAN
 *
 * @note        skeleton code
 *
 *****************************************************************************/
/** 
 * @brief:  k_mem.c kernel API implementations, this is only a skeleton.
 * @author: Yiqing Huang
 */
#include "k_mem.h"
#include "Serial.h"
#ifdef DEBUG_0
#include "assert.h"
#include "printf.h"
#endif  /* DEBUG_0 */



typedef struct _buffer { /* struct size: 8 */
	struct _buffer* next; /* next free chunk */
	U32 size;
	task_t tid;
	U32 filler; //this is for 8 byte alignment
} Buffer;


/*
 *==========================================================================
 *                            GLOBAL VARIABLES
 *==========================================================================
 */
// kernel stack size, referred by startup_a9.s
const U32 g_k_stack_size = K_STACK_SIZE;
// task proc space stack size in bytes, referred by system_a9.cs
const U32 g_p_stack_size = U_STACK_SIZE;

// task kernel stacks
// K_STACK_SIZE >> 2 because K_STACK_SIZE is in bytes, 32/4 = 8. shift right by 2 is divide by 4
U32 g_k_stacks[MAX_TASKS][K_STACK_SIZE >> 2] __attribute__((aligned(8)));

//process stack for tasks in SYS mode
//U32 g_p_stacks[MAX_TASKS][U_STACK_SIZE >> 2] __attribute__((aligned(8)));

/*
 *===========================================================================
 *                            FUNCTIONS
 *===========================================================================
 */

U32* k_alloc_k_stack(task_t tid)
{
    return g_k_stacks[tid];
}

U32* k_alloc_p_stack(U16 stack_size)
{
	task_t curTaskTid = gp_current_task->tid;
	gp_current_task->tid = 0;
    void* startOfAvailableMemory = k_mem_alloc((size_t) stack_size);
    gp_current_task->tid = curTaskTid;
    return startOfAvailableMemory;
}

int k_dealloc_p_stack(void *ptr)
{
	task_t curTaskTid = gp_current_task->tid;
	gp_current_task->tid = 0;
	int result = k_mem_dealloc(ptr);
    gp_current_task->tid = curTaskTid;
    return result;
}

/*
 *===========================================================================
 *                 MARK THREE: 8 byte struct, ~1900 us N=100 Test 2
 *===========================================================================
 */

Buffer* head = NULL;
Buffer* first_buf = NULL;

int k_mem_init(void) {
    unsigned int img_end_addr = (unsigned int) &Image$$ZI_DATA$$ZI$$Limit;
    if (img_end_addr >= RAM_END + 1) { /* no memory space */
    	return RTX_ERR;
    }

    first_buf = (Buffer*)img_end_addr;
    head = (Buffer*)img_end_addr;
    head->next = NULL;
    head->size = RAM_END-img_end_addr - sizeof(Buffer);
    /* size used to be +1
     * ex. if end_index: 20, start_index: 3, size: 18 - sizeof(initial_buffer) = 6
     * but we don't anymore because it didn't work? overhead + 1 in worst case so its fine.*/

    return RTX_OK;
}

inline void* k_mem_alloc(size_t size) {
	/*
	 *
	 * The logic is simple:
	 * After a few sanity checks, we iterate through our linked list
	 * of free chunks until we find one of the same size or larger.
	 *
	 * IF IT IS LARGER, we need to split the current chunk into an occupied chunk (current)
	 * and free chunk (new).
	 *
	 * Then, if the chunk we were considering was the head, set head the new chunk.
	 * Otherwise, make the previous point to the new chunk.
	 *
	 * IF IT IS THE SAME, then we just remove this chunk from the linked list. If it is the head,
	 * set head to next. Otherwise, make previous point to next.
	 *
	 */

    /* return NULL if given size is 0 or mem_init has not been run*/
	if ((size == 0) || (head == NULL)) {
		return NULL;
	}


	size = PAD(size); /* bit magic goes fast with malloc inline. dont inline dealloc */

	Buffer* curr = head;
	Buffer* prev = NULL;

	/* loop through free structs */
	while (curr != NULL) {
		if (curr->size >= size) {

			if (curr->size > (size + sizeof(Buffer))) { /* enough room to break into 2 chunks, create new Buffer at end of region*/
				Buffer* new_buffer = (Buffer*)((U32)curr + size + sizeof(Buffer));
				new_buffer->next = curr->next;
				new_buffer->size = curr->size - size - sizeof(Buffer);
				if (prev == NULL) { /* at head */
					head = new_buffer;
				} else {
					prev->next = new_buffer;
				}

				/* no need to set next_ptr of a allocated block to anything*/
				curr->size = size;
			} else { /* if exact size, remove curr from the linked list */
				if (prev == NULL) { /* at head */
					head = curr->next;
				} else {
					prev->next = curr->next;
				}
			}
			curr->tid = gp_current_task->tid;
			return (void*)((U32)curr + sizeof(Buffer)); /* pointer to allocated memory */
		}

		prev = curr;
		curr = curr->next;
	}
    return NULL;	/* failed to allocate memory */
}

int k_mem_dealloc(void *ptr) {
	/*
	 * Whenever we deallocate, we are adding nodes to the linked list before convolascing.
	 *
	 * We iterate through the free chunks untli we pass the address, then go back
	 * to the previous free chunk and use chunk size to interate through allocated chunks.
	 *
	 * Once we reach the address we are good! Otherwise, if we pass the free chunk we
	 * stopped at we are in trouble and exit.
	 *
	 * Now, we simply remove it from the linked list. If we are at the head, then set head to this.
	 */

	if (ptr == NULL) {
	    	return RTX_OK;
	    }

		Buffer* curr = head;
		Buffer* prev = NULL;
		while (curr != NULL && (U32)curr < (U32)ptr) {
			prev = curr;
			curr = curr->next;
		}

		/* curr has past ptr, perform linear search from prev to curr */

		Buffer* target = first_buf;
		if (prev != NULL) {
			target = (Buffer*)((U32)prev + prev->size + sizeof(Buffer));
		}

		while ((U32)target + sizeof(Buffer) != (U32)ptr) {
			if ((U32)target >= (U32)curr || (U32)target >= (U32)RAM_END) {
				/* did not find ptr */
				return RTX_ERR;
			}
			target = (Buffer*)((U32)target + target->size + sizeof(Buffer));
		}


		if(target->tid != gp_current_task->tid)
		{
			return RTX_ERR;
		}

		/* deallocate target, insert into linked list */
		target->next = curr;

		if (prev != NULL) { /* idk why but this is faster than putting this in the next if lol */
			prev->next = target;
		}

		/* merge with next if next chunk is free (in linked list)*/
		if ((Buffer*)((U32)target + target->size + sizeof(Buffer)) == curr) {
			target->size = target->size + curr->size + sizeof(Buffer); /* add set next */
			target->next = curr->next;
		}

		if (prev != NULL) {
			/* merge with previous if previous chunk is free (in linked list) */
			if ((Buffer*)((U32)prev + prev->size + sizeof(Buffer)) == target ) {
				prev->size = prev->size + target->size + sizeof(Buffer);
				prev->next = target->next;
				target = prev;
			}
		}

		/* check if head needs to be updated. */
		if ((U32)target < (U32)head) {
			head = target;
		}

	return RTX_OK;
}

int k_mem_count_extfrag(size_t size) {
    Buffer* curr = head;
    int count = 0;

    while (curr != NULL) {
    	if (curr->size + sizeof(Buffer) < (U32)size) {
    		count += 1;
    	}
    	curr = curr->next;
    }
    return count;
}

#ifdef DEBUG_0
void display_all_mem() {
	printf("\r\n---------------start--------------------\r\n");

	printf("first_buf address 0x%d\r\n", first_buf);
	printf("head address 0x%d\r\n", head);
	printf("RAM_END address 0x%d\r\n", RAM_END);

	Buffer* temp = first_buf;

	while((U32)temp != RAM_END) {
		printf("cur address: 0d%d | ", temp);
		printf("next: 0d%d | ", temp -> next);
		printf("size: %d | ", temp -> size);
		printf("size + header struct: %d |\r\n", temp -> size + sizeof(Buffer));

		temp = (Buffer*)((U32)temp + temp ->size + sizeof(Buffer));
	}
	printf("---------------end--------------------\r\n\r\n");
}
#endif /* DEBUG_0 */
