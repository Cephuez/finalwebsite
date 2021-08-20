/*
 *
 * Phase 1b
 * Name: Saul Sonido Perez (No partner this time)
*/

#include "phase1Int.h"
#include "usloss.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

typedef struct PCB {
    int             cid;                // context's ID
    int             cpuTime;            // process's running time
    char            name[P1_MAXNAME+1]; // process's name
    int             priority;           // process's priority
    P1_State        state;              // state of the PCB
    int 	    status;
    // more fields here
    int		    pid;

    // Indirect linked list connected the parent and their childs
    int		    ptIndex;		// parent table index
    int		    pqIndex;		// parent queue index
    int		    ctIndex;		// child table index
    int		    cqIndex;		// child queue index
    int		    tableIndex;		// process table index
    int		    queueIndex;		// process queue index

} PCB;

static PCB processTable[P1_MAXPROC];    // the process table
static int queueIndex = 0;		// Index of the queueTable while moving through
static PCB queueTable[P1_MAXPROC];	// Creating the queueTable
static PCB currentProcess;		// Keep track of the current process
static PCB clearProcess;		// Reference to an empty process to clear it from the table and queue

void P1ProcInit(void)
{
    P1ContextInit();

    for (int i = 0; i < P1_MAXPROC; i++) {
	processTable[i].cid = 0;
	processTable[i].cpuTime = 0;
 	processTable[i].priority = 0;
        processTable[i].state = P1_STATE_FREE;
	processTable[i].ptIndex = -1;
	processTable[i].pqIndex = -1;
	processTable[i].ctIndex = -1;
	processTable[i].cqIndex = -1;
    }

    // initialize everything else
    clearProcess.state = P1_STATE_FREE;
    clearProcess.ptIndex = -1;
    clearProcess.pqIndex = -1;
    clearProcess.ctIndex = -1;
    clearProcess.cqIndex = -1;
    currentProcess.state = P1_STATE_FREE; 
}

/**
*	Check if the program is in kernel mode before proceeding on
*/
void checkForKernelMode(void){
	unsigned int currPSR = USLOSS_PsrGet();
	if((currPSR & 0x1) == 0){
		USLOSS_IllegalInstruction();
		USLOSS_Halt(0);
	}
}

int P1_GetPid(void) 
{
    return currentProcess.pid;
}

// pid = Process ID
int P1_Fork(char *name, int (*func)(void*), void *arg, int stacksize, int priority, int *pid)
{
    int result = P1_SUCCESS;
    int cid;
    int index = 0;
    checkForKernelMode();
    // Check if interrupts have already been disabled
    P1DisableInterrupts();

    // The following ifs are to check if the parameters are correct or not
    if(priority < 1 || 6 < priority)
	return P1_INVALID_PRIORITY;

    if(stacksize < USLOSS_MIN_STACK)
	return P1_INVALID_STACK;

    if(strcmp(processTable[*pid].name,name) == 0)
	return P1_DUPLICATE_NAME;

    if(name[0] == 0){
	return P1_NAME_IS_NULL;
    }

    if(strlen(name) > P1_MAXNAME){
	return P1_NAME_TOO_LONG;
    }

    for(int i = 0; i < P1_MAXPROC; i++){
	if(processTable[i].state == P1_STATE_FREE){
		index = i;
		break;
	}
	if(i == P1_MAXPROC - 1)
		return P1_TOO_MANY_PROCESSES;
    }


    // Create P1ContextCreate - X
    P1ContextCreate(((void (*)())func),arg,stacksize,&cid);

    // Allocate and Initialize PCB - In the process
    processTable[index].tableIndex = index;
    processTable[index].cid = cid;
    processTable[index].cpuTime = 0;
    strcpy(processTable[index].name, name);
    processTable[index].priority = priority;
    processTable[index].state = P1_STATE_READY;
    processTable[index].pid = *pid;

    // Queue up the process
    queueTable[queueIndex] = processTable[index];
    queueTable[queueIndex].queueIndex = queueIndex;
    queueIndex += 1;
    // Change process if priority higher
    P1Dispatch(FALSE);
    // re-enable interrupts if they were disabled
    P1EnableInterrupts();
    return result;
}

void
P1_Quit(int status)
{
	USLOSS_Console("Check\n");
    // check for kernel mode
    checkForKernelMode();

    // disable interrupts
    P1DisableInterrupts();

    // remove from ready queue, set status to P1_STATE_QUIT
    int qIndex = currentProcess.queueIndex;
    //int tIndex = currentProcess.tableIndex;
    PCB parentProcess = currentProcess;
    queueTable[qIndex] = clearProcess;
    status = P1_STATE_QUIT;

    // If it's not a parent and it's set to quit, then the childs are adopted by parent
    if(currentProcess.ptIndex != -1 && currentProcess.ctIndex != -1){
	PCB parentProcess = processTable[currentProcess.ptIndex];
	PCB childProcess = processTable[currentProcess.ctIndex];
	parentProcess.ctIndex = childProcess.tableIndex;
	parentProcess.cqIndex = childProcess.queueIndex;
	childProcess.ptIndex = parentProcess.tableIndex;
	childProcess.pqIndex = parentProcess.queueIndex;
    }

    // if parent is called to quit and it still have childs
    if(currentProcess.ptIndex == -1 && currentProcess.ctIndex != -1){
	USLOSS_Console("First process quitting with children, halting.\n");
	USLOSS_Halt(1);
    }

    while(1){
	if(parentProcess.ptIndex == -1)
		break;
	parentProcess = processTable[parentProcess.ptIndex];
    }
    // if parent is in state P1_STATE_JOINING set its state to P1_STATE_READY
    P1Dispatch(FALSE);
    // should never get here
    assert(0);
}


// return the child status
int
P1GetChildStatus(int *cpid, int *status) 
{
    int result = P1_SUCCESS;

    for(int i = 0; i < P1_MAXPROC; i ++){
	//PCB child = processTable[i];
    }
    USLOSS_Console("cpid: %d\n",cpid);
    // do stuff here
    return result;
}

int
P1SetState(int pid, P1_State state, int lid, int vid) 
{
    int result = P1_SUCCESS;
    // do stuff here
    return result;
}


// It will change the process if it's necessary to do so
void
P1Dispatch(int rotate)
{
    // select the highest-priority runnable process
    // call P1ContextSwitch to switch to that process
    if(currentProcess.state == P1_STATE_FREE){
	currentProcess = queueTable[0];
	P1ContextSwitch(currentProcess.cid);
    }else{
	for(int i = 1; i < P1_MAXPROC; i++){
		PCB nextProcess = queueTable[i];
		if(nextProcess.priority == 0)
			break;
		// For loop will be inserted below here in the future maybe
		if(nextProcess.priority < currentProcess.priority && rotate == TRUE){
			P1ContextSwitch(currentProcess.cid);
		}
	}
    }
    // It changes from child to parent so it can wait
    //P1ContextSwitch(currentProcess.cid);
}

int
P1_GetProcInfo(int pid, P1_ProcInfo *info)
{
    int result = P1_SUCCESS;
    // fill in info here
    return result;
}







