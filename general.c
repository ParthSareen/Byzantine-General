#include <cmsis_os2.h>
#include "general.h"

// add any #includes here
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// add any #defines here
#define MAX_GENERALS 7
#define QUEUE_SIZE 8
#define MSG_PRIO NULL
#define TIMEOUT 0

// add global variables here
osMessageQueueId_t commandQueue[3][MAX_GENERALS];
uint8_t total_generals;
uint8_t reporterGeneral;
uint8_t numTraitors;
bool loyalGenerals[MAX_GENERALS];

osSemaphoreId_t barrierSem;
osMutexId_t printMutex;
osSemaphoreId_t finishedSem;


/** Record parameters and set up any OS and other resources
  * needed by your general() and broadcast() functions.
  * nGeneral: number of generals
  * loyal: array representing loyalty of corresponding generals
  * reporter: general that will generate output
  * return true if setup successful and n > 3*m, false otherwise
  */
bool setup(uint8_t nGeneral, bool loyal[], uint8_t reporter) {
	total_generals = nGeneral;
	reporterGeneral = reporter;
	numTraitors = 0;
	barrierSem = osSemaphoreNew(total_generals, 0, NULL);
	finishedSem = osSemaphoreNew(total_generals-1, 0, NULL);
	printMutex = osMutexNew(NULL);

	
	for (int i = 0; i< total_generals; i++){
		loyalGenerals[i] = loyal[i];
		if (!loyalGenerals[i]){
			numTraitors++;
		}
	}
		
	c_assert(total_generals>3*numTraitors);
	if (!(total_generals>3*numTraitors))
		return false;
	
	
	for (int i=0; i<3; ++i){
		for (int j =0; j<nGeneral; j++){
			commandQueue[i][j] = osMessageQueueNew(QUEUE_SIZE, sizeof(char)*8, NULL);
		}
	}
	return true; 
}


/** Delete any OS resources created by setup() and free any memory
  * dynamically allocated by setup().
  */
void cleanup(void) {
	for (int i =0; i< 3; i++){
		for(int j=0; j<MAX_GENERALS; j++){
			osMessageQueueDelete(commandQueue[i][j]);
		}
	}
	osSemaphoreDelete(barrierSem);
	osSemaphoreDelete(finishedSem);
	osMutexDelete(printMutex);
	memset(loyalGenerals, NULL, MAX_GENERALS*sizeof(bool));
	
	total_generals = 0;
	numTraitors = 0;
	reporterGeneral = 0;
}


void checkStatus(osStatus_t status, int numGeneral){
	osMutexAcquire(printMutex, osWaitForever);
	if (status == osOK);
	else if (status == osErrorResource)
		printf("OVF %i\n", numGeneral);
	else
		printf("US %i\n", numGeneral);
	osMutexRelease(printMutex);
}


// Saves who not to send to using the message, returns the size
int checkMessage(char* msg, int* doNotSend){
	int lastNum = 0;
	int msgLength = sizeof(msg)/sizeof(msg[0])+1;
	for (int charNum = 0; charNum < msgLength; charNum++){
		if (msg[charNum] >= '0' && msg[charNum] <= '9'){
			doNotSend[lastNum] = msg[charNum] - '0';
			lastNum++;
		}
	}
	return lastNum;
}


/** This function performs the initial broadcast to n-1 generals.
  * It should wait for the generals to finish before returning.
  * Note that the general sending the command does not participate
  * in the OM algorithm.
  * command: either 'A' or 'R'
  * sender: general sending the command to other n-1 generals
  */

void broadcast(char command, uint8_t sender) {
	
	bool loyal = loyalGenerals[sender];
	char msg[4];
	snprintf(msg, 4, "%d:%c", sender, command);
	
	printf("broadcast msg: %s, sender: %i, loyal: %i\n", msg, sender, loyal);
	for (uint8_t numGeneral = 0; numGeneral<total_generals; numGeneral++){
		if (numGeneral != sender){
			if (!loyal){
				if(numGeneral % 2 == 0){
					msg[strlen(msg)-1] = 'R';
				}
				else{
					msg[strlen(msg)-1] = 'A';
				}
			}
			osStatus_t status = osMessageQueuePut(commandQueue[0][numGeneral], &msg, MSG_PRIO, osWaitForever);
			if (status != osOK){
				checkStatus(status, numGeneral);
				osMutexAcquire(printMutex, osWaitForever);
				printf("not ok broadcast id: %i\n", numGeneral);
				osMutexRelease(printMutex);
			}
		}
	}

	for (int i = 0; i<total_generals; i++){
		osSemaphoreRelease(barrierSem);
	}
	// Waits for all generals to finish OM before returning to final.c
	for (int i = 0; i < total_generals-1; i++){
		osSemaphoreAcquire(finishedSem, osWaitForever);
	}
	return;
}

void om(char* msg, uint8_t id, uint8_t m){
	if (m == 0){
		if (id == reporterGeneral){
			osMutexAcquire(printMutex, osWaitForever);
			printf("id: %i, visited: %s\n", id, msg);
			osMutexRelease(printMutex);
		}
		else return;
	}
	if (m > 0){
		int* doNotSend = (int*)malloc(MAX_GENERALS*sizeof(uint16_t));
		int doNotSendSize = checkMessage(msg, doNotSend);
		
		// Message creation, and alteration (if traitor)
		char newMsg[strlen(msg)+3];
		snprintf(newMsg, strlen(msg)+3, "%d:%s", id, msg);
		bool loyal = loyalGenerals[id];
		
		if (!loyal){
			if(id % 2 == 0){
				newMsg[strlen(newMsg)-1] = 'R';
			}
			else{
				newMsg[strlen(newMsg)-1] = 'A';
			}
		}
		
		// Send messages loop
		for (int numGeneral = 0; numGeneral < total_generals; numGeneral++){
			bool found = false;
			if (numGeneral == id)
				found = true;
		
			uint16_t i = 0;
			while (!found && i < doNotSendSize){
				int tmp = *(doNotSend+i);
				if (numGeneral == tmp)
					found = true;
				i++;
			}
			
			if (!found){
				osMutexAcquire(printMutex, osWaitForever);
				osStatus_t status = osMessageQueuePut(commandQueue[m][numGeneral], &newMsg, MSG_PRIO, osWaitForever);
				uint32_t count = osMessageQueueGetCount(commandQueue[m][numGeneral]);
				if (status != osOK){
					printf("put wrong, count: %i, msg: %s\n", count, newMsg);
				}
				osMutexRelease(printMutex);
			}
		}
		
		// Get messages loop
		for (int numGeneral = 0; numGeneral < total_generals; numGeneral++){
			bool found = false;
			if (numGeneral == id)
				found = true;
		
			uint16_t i = 0;
			while (!found && i < doNotSendSize){
				int tmp = *(doNotSend+i);
				if (numGeneral == tmp)
					found = true;
				i++;
			}
			char getMsg[8];
			if (!found){
				osMutexAcquire(printMutex, osWaitForever);
				osStatus_t status = osMessageQueueGet(commandQueue[m][numGeneral], &getMsg, MSG_PRIO, osWaitForever);
				uint32_t count = osMessageQueueGetCount(commandQueue[m][numGeneral]);
				if (status != osOK)
					printf("get incorrect, count: %i, msg: %s\n", count, getMsg);
				osMutexRelease(printMutex);
				om(getMsg, numGeneral, m-1);
			}
		}
			free(doNotSend);
		}		
	}
	
	
/** Generals are created before each test and deleted after each
  * test.  The function should wait for a value from broadcast()
  * and then use the OM algorithm to solve the Byzantine General's
  * Problem.  The general designated as reporter in setup()
  * should output the messages received in OM(0).
  * idPtr: pointer to general's id number which is in [0,n-1]
  */
void general(void *idPtr) {
	osSemaphoreAcquire(barrierSem, osWaitForever);
	uint8_t id = *(uint8_t *)idPtr;
	// Superloop
	while(1){
		char msg[4];
		osStatus_t status = osMessageQueueGet(commandQueue[0][id], &msg, NULL, osWaitForever);
		if (status == osOK){
			om(msg, id, numTraitors);
			// Release semaphore to signal being done OM
			osSemaphoreRelease(finishedSem);
		}
		else{
			osMutexAcquire(printMutex, osWaitForever);
			printf("Error on init pull id: %i\n", id);
			osMutexRelease(printMutex);
		}
	}
}
