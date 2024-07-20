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
 * @file        ae_mem.c
 * @brief       memory lab auto-tester
 *
 * @version     V1.2021.01
 * @authors     Yiqing Huang
 * @date        2021 JAN
 *
 *****************************************************************************/

#include "rtx.h"
#include "Serial.h"
#include "printf.h"
#include "ae.h"

#if TEST == -1

int test_mem(void) {
	unsigned int start = timer_get_current_val(2);
	printf("NOTHING TO TEST.\r\n");
	unsigned int end = timer_get_current_val(2);

	// Clock counts down
	printf("This took %u us\r\n", start - end);
	return TRUE;
}

#endif
#if TEST == 1
BOOL test_coalescing_free_regions_using_count_extfrag() {
	void * p1 = k_mem_alloc(32);
	void * p2 = k_mem_alloc(32);

	unsigned int header_size = (unsigned int)p2 - (unsigned int)p1 - 32;

	void * p3 = k_mem_alloc(32);
	void * p4 = k_mem_alloc(32);
	void * p5 = k_mem_alloc(32);
	void * p6 = k_mem_alloc(32);
	void * p7 = k_mem_alloc(32);
	void * p8 = k_mem_alloc(32);
	void * p9 = k_mem_alloc(32);

	int size_33_plus_one_header = k_mem_count_extfrag(32 + header_size + 1);
	int size_97_plus_three_header = k_mem_count_extfrag(96 + 3*header_size + 1);

	if((size_33_plus_one_header!=0) || (size_97_plus_three_header!=0)) {
		printf("test_coalescing_free_regions_using_count_extfrag: 1. Either mem_alloc or mem_count_extfrag has failed.\r\n");
		k_mem_dealloc(p1);
		k_mem_dealloc(p2);
		k_mem_dealloc(p3);
		k_mem_dealloc(p4);
		k_mem_dealloc(p5);
		k_mem_dealloc(p6);
		k_mem_dealloc(p7);
		k_mem_dealloc(p8);
		k_mem_dealloc(p9);
		return FALSE;
	}

	k_mem_dealloc(p2);
	k_mem_dealloc(p4);
	k_mem_dealloc(p6);
	k_mem_dealloc(p8);

	size_33_plus_one_header = k_mem_count_extfrag(32 + header_size + 1);
	size_97_plus_three_header = k_mem_count_extfrag(96 + 3*header_size + 1);

	if((size_33_plus_one_header!=4) || (size_97_plus_three_header!=4)) {
		printf("test_coalescing_free_regions_using_count_extfrag: 2. Either mem_dealloc or coalescing has failed.\r\n");
		k_mem_dealloc(p1);
		k_mem_dealloc(p3);
		k_mem_dealloc(p5);
		k_mem_dealloc(p7);
		k_mem_dealloc(p9);
		return FALSE;
	}

	k_mem_dealloc(p3);
	k_mem_dealloc(p7);

	size_33_plus_one_header = k_mem_count_extfrag(32 + header_size + 1);
	size_97_plus_three_header = k_mem_count_extfrag(96 + 3*header_size + 1);

	if((size_33_plus_one_header!=0) || (size_97_plus_three_header!=2)) {
		printf("test_coalescing_free_regions_using_count_extfrag: 3. Either mem_dealloc or coalescing has failed.\r\n");
		k_mem_dealloc(p1);
		k_mem_dealloc(p5);
		k_mem_dealloc(p9);
		return FALSE;
	}

	k_mem_dealloc(p1);
	k_mem_dealloc(p5);
	k_mem_dealloc(p9);

	int size_289_plus_nine_header = k_mem_count_extfrag(288 + 9*header_size + 1);

	if(size_289_plus_nine_header!=0) {
		printf("test_coalescing_free_regions_using_count_extfrag: 4. Either mem_dealloc or coalescing has failed.\r\n");
		return FALSE;
	}

	return TRUE;
}


int test_mem(void) {
	unsigned int start = timer_get_current_val(2);
	int test_coalescing_free_regions_result = test_coalescing_free_regions_using_count_extfrag();
	unsigned int end = timer_get_current_val(2);
	printf("This took %u us\r\n", start - end);


	return test_coalescing_free_regions_result;
}
#endif

#if TEST == 2

#define N 1000

#define CODE_MEM_INIT -1
#define CODE_MEM_ALLOC -2
#define CODE_MEM_DEALLOC -3
#define CODE_HEAP_LEAKAGE_1 -4
#define CODE_HEAP_LEAKAGE_2 -5
#define CODE_SUCCESS 0

int heap_leakage_test() {

	unsigned int start = timer_get_current_val(2);

	static char *p_old[N], *p_new[N];

	// Step 1: Allocate memory
	for (int i = 0; i < N; i++) {
		p_old[i] = (char*) k_mem_alloc(i * 256 + 255);

		// pointer to allocated memory should not be null
		// starting address of allocated memory should be four-byte aligned
		if (p_old[i] == NULL || ((unsigned int) p_old[0] & 3)) {
			printf("Err: p_old[i] == NULL or the returned address is not 4-byte addressable.\r\n");
			return CODE_MEM_ALLOC;
		}

		if (i > 0) {
			// adjacent allocated memory should not conflict
			if (p_old[i - 1] + 256 * i >= p_old[i]) {
				printf("Err: adjacent allocated memory conflict.\r\n");
				printf("Err: p_old[i-1] + 256*i: %x >= p_old[i]: %x.\r\n", p_old[i-1] + 256*i, p_old[i]);
				return CODE_MEM_ALLOC;
			}
		}
	}

	// Step 2: De-allocate memory
	for (int i = 0; i < N; i++) {
		if (k_mem_dealloc(p_old[i]) == -1) {
			printf("Err: k_mem_dealloc returned RTX_ERR.\r\n");
			return CODE_MEM_DEALLOC;
		}
	}

	// Step 3: Memory Leakage
	for (int i = 0; i < N; i++) {
		p_new[i] = (char*) k_mem_alloc((N - i) * 256 - 1);

		// pointer to allocated memory should not be null
		// starting address of allocated memory should be four-byte aligned
		if (p_new[i] == NULL || ((unsigned int) p_new[0] & 3)) {
			printf("Err: k_mem_alloc should return 4-byte aligned address\r\n");
			return CODE_HEAP_LEAKAGE_1;
		}

		if (i > 0) {
			// adjacent allocated memory should not conflict
			if (p_new[i - 1] + 256 * (N - i + 1) >= p_new[i]) {
				printf("Err: k_mem_alloc should not conflict with adjacent memory.\r\n");
				printf("Err: k_mem_alloc info: p_new[i-1]+256*(N-i+1) %d, p_new[i] %d\r\n", p_new[i-1]+256*(N-i+1), p_new[i]);
				return CODE_HEAP_LEAKAGE_1;
			}
		}
	}

	// the total occupied area in the re-allocation
	// should be the same as in the initial allocation
	if (p_old[0] != p_new[0]) {
		printf("Err: k_mem_alloc new is not equal to old at start. old: %d, new %d\r\n", p_old[0], p_new[0]);
		return CODE_HEAP_LEAKAGE_2;
	}
	if (p_old[N - 1] + N * 256 != p_new[N - 1] + 256) {
		printf("Err: k_mem_alloc new is not equal to old at N-1. old: %d, new %d\r\n", p_old[N - 1] + N * 256, p_new[N - 1] + 256);
		return CODE_HEAP_LEAKAGE_2;
	}

	for (int i = 0; i < N; i++) {
		k_mem_dealloc(p_new[i]);
	}
	unsigned int end = timer_get_current_val(2);

		// Clock counts down
		printf("This took %u us\r\n", start - end);

	return CODE_SUCCESS;
}

int test_mem(void) {

	int result = heap_leakage_test();
	switch (result) {
	case CODE_MEM_INIT:
	case CODE_MEM_ALLOC:
		printf("Err: Basic allocation fails.\r\n");
		break;
	case CODE_MEM_DEALLOC:
		printf("Err: Basic deallocation fails.\r\n");
		break;
	case CODE_HEAP_LEAKAGE_1:
		printf("Err: Reallocation fails.\r\n");
		break;
	case CODE_HEAP_LEAKAGE_2:
		printf("Err: Heap memory is leaked.\r\n");
		break;
	case CODE_SUCCESS:
		printf("No heap leakage.\r\n");
		break;
	default:
	}

	return result == CODE_SUCCESS;
}
#endif

/*
 *===========================================================================
 *                             UNIT TESTING
 *===========================================================================
 */
#if TEST == 100
#define MANUAL_UNIT_TEST_OK 1
#define MANUAL_UNIT_TEST_FAIL 0
#define BUFFER_SIZE 8
int unit_test() {
	// FORMAT: Scenario title will describe the behaviour we are testing for
	// **IMPORTANT**: Assumption for test case is that all allocated chunks will be deallocated after each respective test case.
	// This will allow future unit tests to be a lot more easier to write up (don't need to constantly create new pointers and remember how much data has been allocated for every new unit test)

	// Address pointer declarations. Feel free to add more if needed
	void *p0, *p1, *p2, *p3, *p4, *p5;

	// SCENARIO: Calling allocate before init should give us an error
	p0 = k_mem_alloc(12);
	if (!p0)
	{
		printf("UNIT TEST Err: Unsafe Allocation.\r\n");
		return MANUAL_UNIT_TEST_FAIL;
	}


	// SCENARIO: Test case to confirm proper unallocation of fragment size
	k_mem_init();
	p1 = k_mem_alloc(12);
	p2 = k_mem_alloc(12);
	p3 = k_mem_alloc(12);
	k_mem_dealloc(p2);
	if(k_mem_count_extfrag(12) != 0 || k_mem_count_extfrag(13) != 1)
	{
		printf("UNIT TEST Err: Improper fragment size deallocated.\r\n");
		return MANUAL_UNIT_TEST_FAIL;
	}
	// CLEANUP
	k_mem_dealloc(p1);
	k_mem_dealloc(p3);
	printf("Test 1 done\r\n");


	// SCENARIO: Deallocation test case for when previous chunk is manually unallocated and next chunk was never allocated in the first place.
	// Desired behaviour is for the current, previous, and next chunk to become a single chunk
	p1 = k_mem_alloc(12);
	p2 = k_mem_alloc(12);
	p3 = k_mem_alloc(12);
	k_mem_dealloc(p2);
	k_mem_dealloc(p3);
	if(k_mem_count_extfrag(24) != 0)
	{
		printf("UNIT TEST Err: Improper fragment size deallocated. Issue with previous and next chunk V1. \r\n");
		return MANUAL_UNIT_TEST_FAIL;
	}
	// CLEANUP
	k_mem_dealloc(p1);
	printf("Test 2 done\r\n");

	// SCENARIO: Deallocation test case for when both previous and next chunk was manually deallocated.
	// Desired behaviour is for the current, previous, and next chunk to become a single chunk
	p1 = k_mem_alloc(12);
	p2 = k_mem_alloc(12);
	p3 = k_mem_alloc(12);
	p4 = k_mem_alloc(12);
	k_mem_dealloc(p2);
	k_mem_dealloc(p3);
	printf("look here: %d\n", k_mem_count_extfrag(25));

	/* strictly less than 36, not <= 36 */
	if(k_mem_count_extfrag(24 + BUFFER_SIZE) != 0 || k_mem_count_extfrag(24 + BUFFER_SIZE +1) != 1)
	{
		printf("UNIT TEST Err: Improper fragment size deallocated. Issue with previous and next chunk V2. \r\n");
		return MANUAL_UNIT_TEST_FAIL;
	}
	// CLEANUP
	k_mem_dealloc(p1);
	k_mem_dealloc(p4);

	printf("Test 3 done\r\n");

	// SCENARIO: Deallocation same pointer twice
	// Desired behaviour: first deallocate success, second rejected, 3rd rejected (was never allocated either)
	p1 = k_mem_alloc(12);
	if (k_mem_dealloc(p1) != 0) {
		printf("UNIT TEST Err: deallocate error \r\n");
		return MANUAL_UNIT_TEST_FAIL;
	}
//	if (k_mem_dealloc(p2) != -1) {
//		printf("UNIT TEST Err(1): deallocate should fail but did not \r\n");
//		return MANUAL_UNIT_TEST_FAIL;
//	}
//	if (k_mem_dealloc(p5) != -1) {
//			printf("UNIT TEST Err(2): deallocate should fail but did not \r\n");
//			return MANUAL_UNIT_TEST_FAIL;
//	}

	printf("Test 4 done\r\n");

	return MANUAL_UNIT_TEST_OK;

}

int test_mem(void) {
//	int result = k_mem_init_test();
//	if (result == 1) {
//		printf("k_mem_init() successfully run.\r\n");
//	}
	return unit_test();
}

#endif

#if TEST == 101
int andysTest() {
	void *p1 = k_mem_alloc(12);
	void *p2 = k_mem_alloc(12);
	void *p3 = k_mem_alloc(12);
	void *p4 = k_mem_alloc(12);
	void *p5 = k_mem_alloc(12);
	k_mem_dealloc(p3);
/*	display_all_mem(); */

	k_mem_dealloc(p2);
/*	display_all_mem(); */

	k_mem_dealloc(p4);
/*	display_all_mem(); */

	printf("Success: %d\n", k_mem_count_extfrag(12));

	printf("should fail: %d\n", k_mem_dealloc(p4));
	printf("should fail: %d\n", k_mem_dealloc((void*)((U32)p4+1)));

	return 1;
}

int test_mem(void) {

	return andysTest();
}

#endif

#if TEST == 102

#define N 5000
#define S 3
#define MANUAL_UNIT_TEST_OK 1
#define MANUAL_UNIT_TEST_FAIL 0

int unit_test2(){ //rename to appropriate test name later
	unsigned int start;
	unsigned int end;
	unsigned int avg = 0;
	// Address pointer declarations. Feel free to add more if needed
	void *p0, *p1, *p2, *p3, *p4, *p5, *p6, *p7, *p8, *p9, *p10, *p11, *p12, *p13, *p14, *p15;

	for (int j = 0; j<S; j++){
		start = timer_get_current_val(2);
		//Scenario: Throughput Test using different sized chunks (4.5.1 Performance Metric)
		for (int i = 0; i<N; i++){ //total of 100000 requests (50000 alloc, 50000 dealloc)
			p0 = k_mem_alloc(4);
			p1 = k_mem_alloc(8);
			p2 = k_mem_alloc(12);
			p3 = k_mem_alloc(16);
			p4 = k_mem_alloc(24);
			p5 = k_mem_alloc(32);
			p6 = k_mem_alloc(64);
			p7 = k_mem_alloc(4); //****** Run this whole test min 3 times and take avg of these times.
			p8 = k_mem_alloc(12);
			p9 = k_mem_alloc(8);
			k_mem_dealloc(p0);
			k_mem_dealloc(p1);
			k_mem_dealloc(p2);
			k_mem_dealloc(p3);
			k_mem_dealloc(p4);
			k_mem_dealloc(p5);
			k_mem_dealloc(p6);
			k_mem_dealloc(p7);
			k_mem_dealloc(p8);
			k_mem_dealloc(p9);
		}
		end = timer_get_current_val(2);
		printf("This took %u us\r\n", start - end);
		avg = avg + (start - end);
	}
	printf("Avg time over 3 iterations of 100000 requests: %u us\r\n", avg/3);
	printf("To calc throughput, convert avg from us to sec. Then 100,000/avg(in sec) = throughput.\r\n");


	//Scenario: Heap Utilization Ratio (4.5.1 Performance Metric)
	//Scenario: Allocate to an already allocated pointer


	//Scenario: Fill entire memory space
	printf("Starting alloc\r\n");

	unsigned int img_end_addr = (unsigned int) &Image$$ZI_DATA$$ZI$$Limit;
	unsigned int dynamic_total_mem = (unsigned int)RAM_END - img_end_addr;
	int remainder = dynamic_total_mem % 4;
	dynamic_total_mem = dynamic_total_mem - (unsigned int)remainder - (unsigned int)8;
    printf("ae_mem: image ends at 0x%x\r\n", img_end_addr);
	printf("ae_mem: RAM ends at 0x%x\r\n", RAM_END); // if we can get the addr of the end of the img we can know the exact size of memory to allocate
	printf("ae_mem: Total memory space (after being 4-byte aligned) is 0x%x\r\n", dynamic_total_mem);
	printf("Filling entire memory-space \r\n");
	p0 = k_mem_alloc(dynamic_total_mem);

	k_mem_dealloc(p0);


	printf("End of allocs\r\n");
	return MANUAL_UNIT_TEST_OK;
}

int test_mem(void) {

	return unit_test2();
}
#endif

#if TEST == 103
#define MANUAL_UNIT_TEST_OK 1
#define MANUAL_UNIT_TEST_FAIL 0
#define BufferSize 8

int heapTest() {
	unsigned int img_end_addr = (unsigned int) &Image$$ZI_DATA$$ZI$$Limit;
	unsigned int dynamic_total_mem = (unsigned int)RAM_END - img_end_addr;
	int remainder = dynamic_total_mem % 4;
	dynamic_total_mem = dynamic_total_mem - (unsigned int)remainder - (unsigned int)BufferSize;
	int P = 0;
	int TotalSize = 0;
	int ByteAligned = 0;
	int counter = 1;

	printf("Heap Utilization Test Started. Make sure to check/change buffer size macro\n");
	for(int i = 1; i<20000; ++i)
	{
		if (TotalSize + i + BufferSize >= dynamic_total_mem)
		{
			break;
		}

		if (k_mem_alloc(i) == NULL)
		{
			printf("HEAP UTILIZATION TEST FAILED 1: Issue with allocating %d bytes\n", i);
			return MANUAL_UNIT_TEST_FAIL;
		}
		ByteAligned = ((i % 4) > 0 ? i + (4 - (i % 4)) : i);
		P += ByteAligned;
		TotalSize += (ByteAligned + BufferSize);
	}
	while(TotalSize + counter + BufferSize < dynamic_total_mem )
	{
		if (k_mem_alloc(counter) == NULL)
		{
			printf("HEAP UTILIZATION TEST FAILED 2: Issue with allocating %d bytes\n", counter);
			return MANUAL_UNIT_TEST_FAIL;
		}
		ByteAligned = ((counter % 4) > 0 ? counter + (4 - (counter % 4)) : counter);
		P += ByteAligned;
		TotalSize += (ByteAligned + BufferSize);
		counter *= 2;
	}

	printf("HEAP UTILIZATION: P = %d, H = %d\n", P, TotalSize);


	return MANUAL_UNIT_TEST_OK;
}

int test_mem(void) {

	return heapTest();
}

#endif

/* please turn DEBUG 0 on when running this test*/
#if TEST == 104
int firstWithLast() {
	void *p1 = k_mem_alloc(12);
	void *p2 = k_mem_alloc(12);
	void *p3 = k_mem_alloc(12);
	void *p4 = k_mem_alloc(12);
	k_mem_dealloc(p3);
	/*	display_all_mem(); */

	k_mem_dealloc(p2);
	/*	display_all_mem(); */

	k_mem_dealloc(p1);
	/*	display_all_mem(); */

	k_mem_dealloc(p4);
	/*	display_all_mem(); */

	printf("Success: %d\n", k_mem_count_extfrag(12));
	return 1;
}

int test_mem(void) {

	return firstWithLast();
}

#endif
/*
 *===========================================================================
 *                             END OF FILE
 *===========================================================================
 */
