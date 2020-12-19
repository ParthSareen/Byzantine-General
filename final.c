#include <cmsis_os2.h>
#include <stdlib.h>
#include "general.h"

typedef struct {
	uint8_t n;
	bool *loyal;
	uint8_t reporter;
	char command;
	uint8_t sender;
} test_t;

bool loyal0[] = { true, true, false };
bool loyal1[] = { true, false, true, true };
bool loyal2[] = { true, false, true, true };
bool loyal3[] = { true, false, true, true, false, true, true };
bool loyal4[] = { true, true, true };//no traitors
bool loyal5[] = { true, true, true, true, true, true, true};//no traitors max size
bool loyal6[] = { true, true, true, false, true, true, true};//traitor is broadcaster 1 traitor total 
bool loyal7[] = { true, true, true, true, true, false, true};//traitor is reciever 1 traitor total
bool loyal8[] = { true, false, true, true, true, true, true};//traitor is reporter 1 traitor total
bool loyal9[] = { false, false, false, true, true, true, true};//3 traitors 7 gen
bool loyal10[] = { true, true, true, true, false, false, true };//2 traitors 1 reciever 1 commander
bool loyal11[] = { false, true, true, false, true, true, true };//2 traitors 1 commander 1 reporter
bool loyal12[] = { true, true, true, true, false};//1 traitor 5 generals, traitor is commander
bool loyal13[] = { false, true, true, true, true}; //1 traitor 5 generals, traitor is reciever

#define N_TEST 14

test_t tests[N_TEST] = {
	{ sizeof(loyal0)/sizeof(loyal0[0]), loyal0, 1, 'R', 0 },
	{ sizeof(loyal1)/sizeof(loyal1[0]), loyal1, 2, 'R', 0 },
	{ sizeof(loyal2)/sizeof(loyal2[0]), loyal2, 2, 'A', 1 },
	{ sizeof(loyal3)/sizeof(loyal3[0]), loyal3, 6, 'R', 0 },
	{ sizeof(loyal4)/sizeof(loyal4[0]), loyal4, 1, 'R', 0 },
	{ sizeof(loyal5)/sizeof(loyal5[0]), loyal5, 6, 'A', 3 },
	{ sizeof(loyal6)/sizeof(loyal6[0]), loyal6, 2, 'R', 3 },
	{ sizeof(loyal7)/sizeof(loyal7[0]), loyal7, 6, 'A', 0 },
	{ sizeof(loyal8)/sizeof(loyal8[0]), loyal8, 1, 'R', 4 },
	{ sizeof(loyal9)/sizeof(loyal9[0]), loyal9, 1, 'R', 0 },
	{ sizeof(loyal10)/sizeof(loyal10[0]), loyal10, 2, 'R', 4 },
	{ sizeof(loyal11)/sizeof(loyal11[0]), loyal11, 3, 'R', 0 },
	{ sizeof(loyal12)/sizeof(loyal12[0]), loyal12, 0, 'A', 4 },
	{ sizeof(loyal13)/sizeof(loyal13[0]), loyal13, 2, 'R', 3 }

};

#define MAX_GENERALS 7
uint8_t ids[MAX_GENERALS] = { 0, 1, 2, 3, 4, 5, 6 };
osThreadId_t generals[MAX_GENERALS];
uint8_t nGeneral;

void startGenerals(uint8_t n) {
	nGeneral = n;
	for(uint8_t i=0; i<nGeneral; i++) {
		generals[i] = osThreadNew(general, ids + i, NULL);
		if(generals[i] == NULL) {
			printf("failed to create general[%d]\n", i);
		}
	}
}

void stopGenerals(void) {
	for(uint8_t i=0; i<nGeneral; i++) {
		osThreadTerminate(generals[i]);
	}
}

void testCases(void *arguments) {
	// change back to 0
	for(int i=0; i<N_TEST; i++) {
		printf("\ntest case %d\n", i);
		if(setup(tests[i].n, tests[i].loyal, tests[i].reporter)) {
			startGenerals(tests[i].n);
			broadcast(tests[i].command, tests[i].sender);
			cleanup();
			stopGenerals();
			
		} else {
			printf(" setup failed\n");
		}
	}
	printf("\ndone\n");
}

/* main */
int main(void) {
	osKernelInitialize();
  osThreadNew(testCases, NULL, NULL);
	osKernelStart();
	
	for( ; ; ) ;
}
