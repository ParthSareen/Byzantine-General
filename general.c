#include <cmsis_os2.h>
#include "general.h"

// add any #includes here
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// add any #defines here
#define MAX_GENERALS 7
#define QUEUE_SIZE 10
#define MSG_PRIO 0
#define TIMEOUT 0

// add global variables here
osMessageQueueId_t* commandQueue;
uint8_t total_generals;
uint8_t reporterGeneral;
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
	reporterGeneral = reporter;
	// TODO: check if size could be 2
	commandQueue = (osMessageQueueId_t*)malloc(MAX_GENERALS*sizeof(uint8_t));
	if (commandQueue == NULL){
		printf("malloc for command queue failed");
		return false;
	}
	// TODO: check if we need to have queue for commander
	for (int i; i<nGeneral; ++i){
		commandQueue[i] = osMessageQueueNew(QUEUE_SIZE, sizeof(uint32_t), NULL);
		//printf("Q%i\n", i);
	}
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

void sendMessage(char *msg, uint8_t targetId){
	osStatus_t status = osMessageQueuePut(commandQueue[targetId], &msg, MSG_PRIO, TIMEOUT);
	checkStatus(status, targetId);
}

void createMessage(char* msg, int curGeneral){
	char strGeneral[1];
	sprintf(strGeneral, "%d", curGeneral);
	// 3 - 1 general 1 colon 1 null
	char* newMsg = malloc(strlen(msg)+3);
	strcpy(newMsg, strGeneral);
	strcat(newMsg, ":");
	strcat(newMsg, msg);
	printf("loop msg: %s\n", newMsg);
	strcpy(msg, newMsg);
	free(newMsg);
}

/** This function performs the initial broadcast to n-1 generals.
  * It should wait for the generals to finish before returning.
  * Note that the general sending the command does not participate
  * in the OM algorithm.
  * command: either 'A' or 'R'
  * sender: general sending the command to other n-1 generals
  */
void broadcast(char command, uint8_t sender) {
	char *msg;
	msg = &command;
	createMessage(msg, sender);
	printf("broadcast msg: %s\n", msg);
	for (int numGeneral; numGeneral<total_generals; numGeneral++){
		if (numGeneral != sender){
			sendMessage(msg, numGeneral);
		}
	}
}

int checkMessage(char* msg, int* doNotSend){
	int lastNum = 0;
	int msgLength = sizeof(msg)/sizeof(msg[0])+1;
	//printf("length: %i\n", msgLength);
	for (int charNum = 0; charNum < msgLength; charNum++){
		if (msg[charNum] >= '0' && msg[charNum] <= '9'){
			//printf("converted %c", msg[charNum]);
			doNotSend[lastNum] = msg[charNum] - '0';
			lastNum++;
		}
	}
	return lastNum;
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
	// superloop for thread
	while(1){
		if (id == 1){ 
			char *rcvd_msg;
			osStatus_t status = osMessageQueueGet(commandQueue[id], &rcvd_msg, NULL, osWaitForever);
			
			if (status == osOK){
				printf("os ok, msg: %s\n", rcvd_msg);
				int* doNotSend = (int*)malloc(MAX_GENERALS*sizeof(uint16_t));
				int doNotSendSize = checkMessage(rcvd_msg, doNotSend);
				
				for (int numGeneral = 0; numGeneral < total_generals; ++numGeneral){
					bool found = false;
					if (numGeneral == id)
						found = true;
					
					uint16_t i = 0;
					while (!found && i < doNotSendSize){
						if (numGeneral == doNotSend[i])
							found = true;
						i++;
					}
					
					if (!found){
						// rcvd_msg = "0:A";
						createMessage(rcvd_msg, numGeneral);
						printf("msg created: %s\n", rcvd_msg);
						sendMessage(rcvd_msg, numGeneral);
						printf("sending msg, id:%i\n", numGeneral);
					}
				}
				
			}
		}
	}
}
