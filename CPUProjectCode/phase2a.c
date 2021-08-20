/*
*
*	Author: Saul Sonido Perez
*	Assignement: Phase2A
*
*/
#include <stdlib.h>
#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <stdio.h>
#include <assert.h>
#include <libuser.h>
#include <usyscall.h>

#include "phase2Int.h"

#define TAG_KERNEL 0
#define TAG_USER 1


void (*Handler_Array[USLOSS_MAX_SYSCALLS])(USLOSS_Sysargs *sysargs);

static void SpawnStub(USLOSS_Sysargs *sysargs);

int userPID[P1_MAXPROC];
int userIndex = 0;
int userStatus = 1;
int userLID, userVID, clockVal;
int stackSizeRef, priorityRef;
int (*wrapper)(void *) = NULL;
void *argRef = NULL;
char *nameRef = "1";
/*
 * IllegalHandler
 *
 * Handler for illegal instruction interrupts.
 *
 */

static void 
IllegalHandler(int type, void *arg) 
{
    USLOSS_Console("Illegal Handler\n");
    // do something here
}

/*
 * SyscallHandler
 *
 * Handler for system call interrupts.
 *
 */


static void 
SyscallHandler(int type, void *arg) 
{
    int sysNum;
    USLOSS_Sysargs *sysargs = (USLOSS_Sysargs *) arg;
    // call the proper handler for the system call.
    sysNum = sysargs -> number;
    USLOSS_Console("Sys Num: %d\n",sysNum);


    // Set the outputs
    if(sysNum == SYS_SPAWN){
	//int rc = P2_Spawn(refName, wrapper, argRef, stackSizeRef, priorityRef, );
	sysargs -> arg1 = (void *)userPID[0];
	//sysargs -> arg4 = rc;
	//USLOSS_Console("%d\n",userPID[0]);
    }

    if(sysNum == SYS_GETTIMEOFDAY){
	int now;
        int status;
	status = USLOSS_DeviceInput(USLOSS_CLOCK_DEV, 0, &now);
	sysargs -> arg1 = (void *)now;
	//USLOSS_Console("Now: %d\n",  now);
    }

    Handler_Array[sysNum](sysargs);

   // Get the inputs?
   if(sysNum == SYS_SPAWN){
        //clockVal = 3;
	/*
	sysargs -> arg1 = wrapper;
	sysargs -> arg2 = argRef;
	sysargs -> arg3 = (void *) stackSizeRef;
	sysargs -> arg4 = (void *) priorityRef;
	sysargs -> arg5 = (void *) nameRef;
	*/
   }

    //sysargs->arg4 = (void *) P2_INVALID_SYSCALL;

}


/*
 * P2ProcInit
 *
 * Initialize everythi ung.
 *
 */
void
P2ProcInit(void) 
{
    int rc;

    rc = P1_LockCreate("User",&userLID);
    rc = P1_CondCreate("User",userLID,&userVID);
    rc = P1_Lock(userLID);

    USLOSS_IntVec[USLOSS_ILLEGAL_INT] = IllegalHandler;
    USLOSS_IntVec[USLOSS_SYSCALL_INT] = SyscallHandler;

    // call P2_SetSyscallHandler to set handlers for all system calls
    rc = P2_SetSyscallHandler(SYS_SPAWN, SpawnStub);
    rc = P2_SetSyscallHandler(SYS_WAIT, SpawnStub);
    rc = P2_SetSyscallHandler(SYS_TERMINATE, SpawnStub);
    rc = P2_SetSyscallHandler(SYS_GETPROCINFO, SpawnStub);
    rc = P2_SetSyscallHandler(SYS_GETPID, SpawnStub);
    rc = P2_SetSyscallHandler(SYS_GETTIMEOFDAY, SpawnStub);

    assert(rc == P1_SUCCESS);
}

/*
 * P2_SetSyscallHandler
 *
 * Set the system call handler for the specified system call.
 *
 */

int
P2_SetSyscallHandler(unsigned int number, void (*handler)(USLOSS_Sysargs *args))
{
    Handler_Array[number] = *handler;

    return P1_SUCCESS;
}

/*
 * P2_Spawn
 *
 * Spawn a user-level process.
 *
 */

static void launch(void){
    int result;
    unsigned int currPSR = USLOSS_PsrGet();
    currPSR = (currPSR >> 1) << 1;
    result = USLOSS_PsrSet(currPSR);

    assert(wrapper != NULL);
    Sys_Terminate(wrapper(argRef));
}

int
P2_Spawn(char *name, int(*func)(void *arg), void *arg, int stackSize, int priority, int *pid) 
{
    //USLOSS_Console("Will Spawn\n");
    int forkResult;
    wrapper = func;
    argRef = arg;
    stackSizeRef = stackSize;
    priorityRef = priority;
    nameRef = name;
    forkResult = P1_Fork(name, (void *)launch, arg, stackSize, priority, pid);
    userPID[userIndex] = *pid;
    userIndex += 1;

    return P1_SUCCESS;
}

/*
 * P2_Wait
 *
 * Wait for a user-level process.
 *
 */

int P2_Wait(int *pid, int *status) 
{
    // do something here
    int result = 0;

    //USLOSS_Console("Call Join\n");
    result = P1_Join(pid,status);
    USLOSS_Console("Join return %d\n",*status);
    return P1_SUCCESS;
}

/*
 * P2_Terminate
 *
 * Terminate a user-level process.
 *
 */

int 
P2_Terminate(int status) 
{
    // do something here
    USLOSS_Console("Will terminate?\n");
    P1_Quit(status);
    return P1_SUCCESS;
}

/*
 * SpawnStub
 *
 * Stub for Sys_Spawn system call. 
 *
 */

static void 
SpawnStub(USLOSS_Sysargs *sysargs) 
{
    int (*func)(void *) = sysargs->arg1;
    void *arg = sysargs->arg2;
    int stackSize = (int) sysargs->arg3;
    int priority = (int) sysargs->arg4;
    char *name = sysargs->arg5;
    int pid;
    int rc = P2_Spawn(name, func, arg, stackSize, priority, &pid);
    if (rc == P1_SUCCESS) {
        sysargs->arg1 = (void *) pid;
    }
	USLOSS_Console("Pid: %d\n",pid);
    sysargs->arg4 = (void *) rc;
}

