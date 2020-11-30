#include <cmsis_os2.h>
#include "general.h"

// add any #includes here
#include <stdlib.h>
#include <stdio.h>

// add any #defines here
#define MAX_GENERALS 7
#define QUEUE_SIZE 10
#define MSG_PRIO 0
#define TIMEOUT 0

// add global variables here
// osMessageQueueId_t commandQueue[MAX_GENERALS];
osMessageQueueId_t* commandQueue;
int total_generals;
osMutexId_t printMutex;
osSemaphoreId_t sem;

/** Record parameters and set up any OS and other resources
  * needed by your general() and broadcast() functions.
  * nGeneral: number of generals
  * loyal: array representing loyalty of corresponding generals
  * reporter: general that will generate output
  * return true if setup successful and n > 3*m, false otherwise
  */
bool setup(uint8_t nGeneral, bool loyal[], uint8_t reporter) {
	total_generals = nGeneral;
	// size could be 2 i think
	commandQueue = (osMessageQueueId_t*)malloc(MAX_GENERALS*sizeof(uint8_t));
	if (commandQueue == NULL){
		printf("malloc for command queue failed");
		return false;
	}
	// TODO: check if we need to have queue for commander
	for (int i; i<nGeneral; ++i){
		commandQueue[i] = osMessageQueueNew(QUEUE_SIZE, sizeof(uint32_t), NULL);
		printf("Q%i\n", i);
	}
	// osMessageQueuePut(commandQueue[0], &msg, MSG_PRIO, TIMEOUT);
	// osMessageQueueGet(commandQueue[0], rcvd_msg, NULL, osWaitForever);
	// printf("%s\n", rcvd_msg);
	return true; 
}


/** Delete any OS resources created by setup() and free any memory
  * dynamically allocated by setup().
  */
void cleanup(void) {
	free(commandQueue);
}


void checkStatus(osStatus_t status, int numGeneral){
	osMutexAcquire(printMutex, osWaitForever);
	if (status == osOK);
	else if (status == osErrorResource)
		printf("Overflow occured for queue %i\n", numGeneral);
	else
		printf("Unkown status for queue %i\n", numGeneral);
	osMutexRelease(printMutex);
}
/** This function performs the initial broadcast to n-1 generals.
  * It should wait for the generals to finish before returning.
  * Note that the general sending the command does not participate
  * in the OM algorithm.
  * command: either 'A' or 'R'
  * sender: general sending the command to other n-1 generals
  */
void broadcast(char command, uint8_t sender) {
	for (int numGeneral; numGeneral<total_generals; numGeneral++){
		osStatus_t status = osMessageQueuePut(commandQueue[0], &command, MSG_PRIO, TIMEOUT);
		checkStatus(status, numGeneral);
		osMutexAcquire(printMutex, osWaitForever);
		printf("done puts for general %i, msg: %s\n", numGeneral, &command);
		//osDelay(5);
		osMutexRelease(printMutex);
	}
	osMutexAcquire(printMutex, osWaitForever);
	uint16_t count = osMessageQueueGetCount(commandQueue[0]);
	printf("queue size, %i", count);
	osMutexRelease(printMutex);
}


/** Generals are created before each test and deleted after each
  * test.  The function should wait for a value from broadcast()
  * and then use the OM algorithm to solve the Byzantine General's
  * Problem.  The general designated as reporter in setup()
  * should output the messages received in OM(0).
  * idPtr: pointer to general's id number which is in [0,n-1]
  */
void general(void *idPtr) {
	uint8_t id = *(uint8_t *)idPtr;
	char* rcvd_msg;
	while(1){
		osStatus_t status = osMessageQueueGet(commandQueue[id], rcvd_msg, NULL, osWaitForever);
		osSemaphoreAcquire(sem, osWaitForever);
		uint16_t count = osMessageQueueGetCount(commandQueue[0]);
		printf("queue size for %d, %i\n", id, count);
		
		printf("msg: %s\n", rcvd_msg);
		osSemaphoreRelease(sem);
		/*
		// checkStatus(status, id);
		osMutexAcquire(printMutex, osWaitForever);
		uint16_t count = osMessageQueueGetCount(commandQueue[0]);
		printf("queue size, %i", count);
		osMutexRelease(printMutex);
		osMutexAcquire(printMutex, osWaitForever);
		if (status == osOK)
			printf("General %i received %s\n", id, rcvd_msg);
		else if(status == osErrorResource)
			printf("resource error\n");
		//else if(status == osErrorParameter)
			//printf("param error\n");
		osDelay(100);
		osMutexRelease(printMutex);
		*/
	}
}
