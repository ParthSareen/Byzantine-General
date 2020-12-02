#include <cmsis_os2.h>
#include "general.h"

// add any #includes here
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// add any #defines here
#define MAX_GENERALS 7
#define QUEUE_SIZE 20
#define MSG_PRIO 0
#define TIMEOUT 0

// add global variables here
osMessageQueueId_t* commandQueue;
uint8_t total_generals;
uint8_t reporterGeneral;
uint8_t numTraitors;

osSemaphoreId_t barrierSem;
osMutexId_t printMutex;
osMutexId_t sendMutex;
char *msgArray[MAX_GENERALS];

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
	sendMutex = osMutexNew(NULL);
	printMutex = osMutexNew(NULL);
	
	for (int i = 0; i < total_generals; ++i)
		msgArray[i] = "";
	// naive count
	for (int i = 0; i< total_generals; ++i)
		if (!loyal[i])
			numTraitors++;
	//c_assert(total_generals>3*numTraitors);
	//if (!(total_generals>3*numTraitors))
		//return false;
		
	// TODO: check if size could be 2
	commandQueue = (osMessageQueueId_t*)malloc(MAX_GENERALS*sizeof(uint8_t));
	if (commandQueue == NULL){
		printf("malloc for command queue failed");
		return false;
	}
	// TODO: check if we need to have queue for commander
	for (int i; i<nGeneral; ++i){
		commandQueue[i] = osMessageQueueNew(QUEUE_SIZE, 8, NULL);
		// printf("Q%i\n", i);
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
	if (status == osOK);
	else if (status == osErrorResource)
		printf("OVF %i\n", numGeneral);
	else
		printf("US %i\n", numGeneral);
}

void traitorAlter(char* msg, uint8_t target){
	msg[strlen(msg)-1] = 'A';
}


void sendMessage(char *msg, uint8_t targetId){
	char *newMsg = malloc(strlen(msg));
	strcpy(newMsg, msg);
	osStatus_t status = osMessageQueuePut(commandQueue[targetId], &msg, MSG_PRIO, TIMEOUT);
	checkStatus(status, targetId);
	free(newMsg);
}


void createMessage(char* msg, int curGeneral){
	char strGeneral[1];
	sprintf(strGeneral, "%d", curGeneral);
	// 3 - 1 general 1 colon 1 null
	char* newMsg = malloc(strlen(msg)+3);
	strcpy(newMsg, strGeneral);
	strcat(newMsg, ":");
	strcat(newMsg, msg);
	strcpy(msg, newMsg);
	free(newMsg);
}


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
	char *msg;
	msg = &command;
	createMessage(msg, sender);
	printf("broadcast msg: %s, sender: %i\n", msg, sender);
	for (uint8_t numGeneral = 0; numGeneral<total_generals; numGeneral++){
		if (numGeneral != sender){
			sendMessage(msg, numGeneral);
			//printf("sent to %i, msg: %s\n", numGeneral, msg);
		}
	}
	//printf("hello\n");
	for (int i = 0; i<total_generals; i++){
		osSemaphoreRelease(barrierSem);
	}
	// TODO: add something to wait for last to finish
}

void om(){}
	
	
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
	osStatus_t status;
	uint32_t count = 0;
	// superloop for thread
	// TODO add loyal/traitor functionality
	char* send = malloc(strlen(msgArray[id])+4);
	while(1){
		status = osMessageQueueGet(commandQueue[id], &msgArray[id], NULL, osWaitForever);
		if (status == osOK){
			osMutexAcquire(printMutex, osWaitForever);
			printf("id: %i rcvd_msg: %s\n", id,  msgArray[id]);
			osMutexRelease(printMutex);
			
			int* doNotSend = (int*)malloc(MAX_GENERALS*sizeof(uint16_t));
			int doNotSendSize = checkMessage(msgArray[id], doNotSend);
			
			strcpy(send, msgArray[id]);
			if(doNotSendSize-1 < numTraitors){
				createMessage(send, id);
				osMutexAcquire(printMutex, osWaitForever);
				printf("id: %i, dns: %i, msg: %s\n", id, doNotSendSize, send);
				osMutexRelease(printMutex);
				//traitorAlter(send, 1);
				// n-1 loop
				for (int numGeneral = 0; numGeneral < total_generals; numGeneral++){
					bool found = false;
					if (numGeneral == id)
						found = true;
					
					uint16_t i = 0;
					while (!found && i < doNotSendSize){
						if (numGeneral == doNotSend[i]){
							found = true;
						}
						i++;
					}
					if (!found){
						osMutexAcquire(printMutex, osWaitForever);
						//sendMessage(send, numGeneral);
						strcpy(msgArray[id], send);
						free(send);
						osStatus_t status = osMessageQueuePut(commandQueue[numGeneral], &msgArray[id], MSG_PRIO, TIMEOUT);
						if (status != osOK)
							printf("put wrong");
						printf("%i -> %i\n", id,  numGeneral);
						printf("sending %s\n", msgArray[id]);
						osMutexRelease(printMutex);
					}
				}
			}
			
			status = osErrorResource;
		}
		count++;
	}
	free(send);
}
