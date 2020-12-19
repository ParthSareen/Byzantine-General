#include <cmsis_os2.h>
#include "general.h"

// add any #includes here
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// add any #defines here
#define MAX_GENERALS 7
#define QUEUE_SIZE 32
#define MSG_PRIO 0
#define TIMEOUT 0

// add global variables here
osMessageQueueId_t* commandQueue;
uint8_t total_generals;
uint8_t reporterGeneral;
uint8_t numTraitors;
bool *loyalGenerals;
osSemaphoreId_t barrierSem;
osMutexId_t printMutex;
osMutexId_t sendMutex;
osSemaphoreId_t finishedSem;


typedef struct {
	char command;
	char visited[8];
} msg_t;


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
	loyalGenerals = loyal;
	numTraitors = 0;
	barrierSem = osSemaphoreNew(total_generals, 0, NULL);
	finishedSem = osSemaphoreNew(total_generals-1, 0, NULL);
	sendMutex = osMutexNew(NULL);
	printMutex = osMutexNew(NULL);
	
	// naive count
	for (int i = 0; i< total_generals; ++i)
		if (!loyal[i])
			numTraitors++;
	c_assert(total_generals>3*numTraitors);
	if (!(total_generals>3*numTraitors))
		return false;
	
	// TODO: check if size could be 2
	commandQueue = (osMessageQueueId_t*)malloc(MAX_GENERALS*sizeof(uint32_t));
	if (commandQueue == NULL){
		printf("malloc for command queue failed");
		return false;
	}
	// TODO: check if we need to have queue for commander
	for (int i=0; i<nGeneral; ++i){
		commandQueue[i] = osMessageQueueNew(QUEUE_SIZE, sizeof(msg_t), NULL);
		//printf("Q%i\n", i);
	}
	
	return true; 
}


/** Delete any OS resources created by setup() and free any memory
  * dynamically allocated by setup().
  */
void cleanup(void) {
	for (int i =0; i< MAX_GENERALS; i++){
		osMessageQueueDelete(commandQueue[i]);
	}
	osSemaphoreDelete(barrierSem);
	osSemaphoreDelete(finishedSem);
	osMutexDelete(printMutex);
	osMutexDelete(sendMutex);
	free(commandQueue);
}


void checkStatus(osStatus_t status, int numGeneral){
	if (status == osOK);
	else if (status == osErrorResource)
		printf("OVF %i\n", numGeneral);
	else
		printf("US %i\n", numGeneral);
}

void traitorAlter(char* msg, uint8_t id){
	if(id % 2 == 0)
		msg[strlen(msg)-1] = 'R';
	else
		msg[strlen(msg)-1] = 'A';
}


void sendMessage(char *msg, uint8_t targetId){
	char *newMsg = malloc(strlen(msg));
	strcpy(newMsg, msg);
	osStatus_t status = osMessageQueuePut(commandQueue[targetId], &msg, MSG_PRIO, TIMEOUT);
	checkStatus(status, targetId);
	free(newMsg);
}


void createMessage(char* msg[8], int curGeneral){
	char strGeneral[1];
	sprintf(strGeneral, "%d", curGeneral);
	// 3 - 1 general 1 colon 1 null
	char* newMsg = malloc(strlen(*msg)+3);
	strcpy(newMsg, strGeneral);
	strcat(newMsg, ":");
	strcat(newMsg, *msg);
	strcpy(*msg, newMsg);
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
	msg_t newMsg = {command, *msg};
	bool loyal = loyalGenerals[sender];
	createMessage(&msg, sender);
	if (loyal)
		strncpy(newMsg.visited, msg, 7);
	
	printf("broadcast msg: %s, command: %c, sender: %i\n", msg, newMsg.command, sender);
	for (uint8_t numGeneral = 0; numGeneral<total_generals; numGeneral++){
		if (numGeneral != sender){
			if(!loyal){
				traitorAlter(msg, numGeneral);
				strncpy(newMsg.visited, msg, 7);
			}
			osStatus_t status = osMessageQueuePut(commandQueue[numGeneral], &newMsg, MSG_PRIO, TIMEOUT);
			if (status != osOK)
				printf("not ok\n");
			//sendMessage(msg, numGeneral);
			//printf("sent to %i, msg: %s\n", numGeneral, msg);
		}
	}
	//printf("hello\n");
	for (int i = 0; i<total_generals; i++){
		osSemaphoreRelease(barrierSem);
	}
	// TODO: add something to wait for last to finish
	for (int i = 0; i < total_generals-1; i++){
		osSemaphoreAcquire(finishedSem, osWaitForever);
	}
}

void om(msg_t msg, uint8_t id, uint8_t m){
	if (m == 0){
		if (id == reporterGeneral){
			osMutexAcquire(printMutex, osWaitForever);
			printf("id: %i, visited: %s, m: %i\n", id, msg.visited, m);
			osMutexRelease(printMutex);
		}
		else return;
	}
	if (m > 0){
		int* doNotSend = (int*)malloc(MAX_GENERALS*sizeof(uint16_t));
		int doNotSendSize = checkMessage(msg.visited, doNotSend);
		/*if(doNotSendSize - 1 == numTraitors){
			if (id == reporterGeneral){
				osMutexAcquire(printMutex, osWaitForever);
				printf("id: %i, visited: %s, m: %i\n", id, msg.visited, m);
				osMutexRelease(printMutex);
			}
		}*/
		// loop to check if seen
		//if(doNotSendSize-1 < numTraitors){
		char *tmp = msg.visited;
		createMessage(&tmp, id);
		bool loyal = loyalGenerals[id];
		if (!loyal)
			traitorAlter(tmp, id);
		strncpy(msg.visited, tmp, 7);
		
		// send msgs
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
				osStatus_t status = osMessageQueuePut(commandQueue[numGeneral], &msg, MSG_PRIO, TIMEOUT);
				uint32_t count = osMessageQueueGetCount(commandQueue[numGeneral]);
				if (status != osOK){
					printf("put wrong, count: %i, msg: %s\n", count, msg.visited);
					//printf("%i -> %i, %s, m:%i\n", id,  numGeneral, msg.visited, m);
					//printf("sending %s\n", msg.visited);
				}
				osMutexRelease(printMutex);
			}
			//osStatus_t status = osMessageQueuePut(commandQueue[numGeneral], &msg, MSG_PRIO, TIMEOUT);
			//om(msg, numGeneral, m-1);
		}
		
		// Get messages
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
			msg_t newMsg = { .command=0, .visited="" };
			if (!found){
				osMutexAcquire(printMutex, osWaitForever);
				// getting the message here should be the same as what was sent
				osStatus_t status = osMessageQueueGet(commandQueue[numGeneral], &newMsg, MSG_PRIO, TIMEOUT);
				//printf("msg got: %s\n", newMsg.visited);
				uint32_t count = osMessageQueueGetCount(commandQueue[numGeneral]);
				if (status != osOK)
					printf("got wrong, count: %i, msg: %s\n", count, newMsg.visited);
				osMutexRelease(printMutex);
				om(newMsg, numGeneral, m-1);
			}
		}
			//free(doNotSend);
			
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
	
	while(1){
		msg_t msg = { .command=0, .visited="" };
		//osMutexAcquire(printMutex, osWaitForever);
		osStatus_t status = osMessageQueueGet(commandQueue[id], &msg, NULL, osWaitForever);
		if (status == osOK){
			osMutexAcquire(printMutex, osWaitForever);
			printf("init msg got: %s, id: %i\n", msg.visited, id);
			osMutexRelease(printMutex);
			om(msg, id, numTraitors);
			// add semaphore shit
			osDelay(10000);
		}
		else{
			printf("Error on init pull id: %i\n", id);
		}
	}
}
