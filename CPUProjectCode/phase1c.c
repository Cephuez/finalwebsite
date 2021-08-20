
/*
*
*	Name: 	Saul Sonido Perez		Partner: No partner for this part
*	ID:	Cephuez
*	Phase: 	Phase 1.c
*/
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <usloss.h>
#include <phase1Int.h>

#define CHECKKERNEL() \
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0) USLOSS_IllegalInstruction()

#define FREE 0
#define BUSY 1

typedef struct Lock
{
    int         inuse;
    char        name[P1_MAXNAME];
    int         state; 			// BUSY or FREE
    int         pid; 			// Process ID that is currently using it

    // more fields here
    int		pidQueue[10];		// Keep track of locks queued for this
    int		queueIndex;

    // Lock id
    int 	lid;

    int		vid;
} Lock;

static Lock locks[P1_MAXLOCKS];

void 
P1LockInit(void) 
{
    CHECKKERNEL();
    P1ProcInit();
    for (int i = 0; i < P1_MAXLOCKS; i++) {
        locks[i].inuse = FALSE;
	locks[i].lid = i;			// Give a unique id to each lock
	locks[i].state = FREE;
    }
}


/*
*
*	Locks are created with a unique name and lid. It looks through the list of locks to find a free lock
*	It has to make sure that the name is not already used and that there is room for another new lock
*
*/
int P1_LockCreate(char *name, int *lid)
{
    int result = P1_SUCCESS;
    int length = 0;
    int freeIndex = 0;
    CHECKKERNEL();

    // disable interrupts
    int interrupt =  P1DisableInterrupts();

    // check parameters
    if(name[0] == 0)
	return P1_NAME_IS_NULL;

    while(1){
	if(length > P1_MAXNAME)
		return P1_NAME_TOO_LONG;
	if(name[length] == 10)
		break;
	length += 1;
    }

    // Also finds for a free lock while checking for free ones
    for(int i = 0; i < P1_MAXLOCKS; i++){
    	if(locks[i].inuse == FALSE)
		break;
	if(i == P1_MAXLOCKS - 1)
		return P1_TOO_MANY_LOCKS;
	freeIndex += 1;
    }

    for(int i = 0; i < P1_MAXLOCKS; i++){
	if(!strcmp(locks[i].name,name))
		return P1_DUPLICATE_NAME;
    }

    // find an unused Lock and initialize it
    locks[freeIndex].inuse = TRUE;
    strcpy(locks[freeIndex].name,name);
    *lid = locks[freeIndex].lid;

    // restore interrupts
    if(interrupt == TRUE)
    	P1EnableInterrupts();
    return result;
}

/*
*
*	A lock is freed only if there are not more queued pids to use that lock
*	Otherwise, it will cleare the lock and make it free to be used
*
*/
int P1_LockFree(int lid) 
{
    int result = P1_SUCCESS;
    int interrupt;
    int lockIndex;
    CHECKKERNEL();
    // disable interrupts
    interrupt = P1DisableInterrupts();

    // check if any processes are waiting on lock
    for(int i = 0 ; i < P1_MAXLOCKS; i++){
	if(locks[i].lid == lid){
		lockIndex = i;
		break;
	}
	if(i == P1_MAXLOCKS - 1)
		return P1_INVALID_LOCK;
    }

    if(locks[lockIndex].queueIndex > 0)
	return P1_BLOCKED_PROCESSES;

    // mark lock as unused and clean up any state
    locks[lockIndex].inuse = FALSE;
    locks[lockIndex].state = FREE;
    locks[lockIndex].pid = 0;
    locks[lockIndex].queueIndex = 0;
    strcpy(locks[lockIndex].name,"");


    // restore interrupts
    if(interrupt == TRUE)
	P1EnableInterrupts();

    return result;
}

/*
*
*	It will look for a specific lid in order to use that lock. IF the lock is currently busy, then the current
*	process will be blocked from use until that lock is freed
*
*/
int P1_Lock(int lid) 
{
    int result = P1_SUCCESS;
    int index = 0;
    int interrupt;

    CHECKKERNEL();

    for(int i = 0; i < P1_MAXLOCKS; i++){
        if(locks[i].lid == lid)
                break;
        if(i == P1_MAXLOCKS - 1)
                return P1_INVALID_LOCK;
        index += 1;
    }

    while(1){
	interrupt = P1DisableInterrupts();
	if(locks[index].state == FREE){
		locks[index].state = BUSY;
		locks[index].pid = P1_GetPid();
		break;
	}
	// Mark process blocked
	int stateVal = P1SetState(P1_GetPid(),P1_STATE_BLOCKED, lid, locks[index].vid);

	// Add to queue
	int queueIndex = locks[index].queueIndex;
        locks[index].pidQueue[queueIndex] = P1_GetPid();
        locks[index].queueIndex += 1;

	if(stateVal){}

	// Enable interrupts again
	if(interrupt == TRUE)
        	P1EnableInterrupts();
	P1Dispatch(FALSE);
    }

    // Enable Interrupts
    if(interrupt == TRUE)
    	P1EnableInterrupts();

    return result;
}


/*
*
*	It will get the lock based on lid and make the state free.
*	This will enable for the next process in queue to use the lock if there is one.
*/
int P1_Unlock(int lid) 
{
    int result = P1_SUCCESS;
    int index = 0;
    int interrupt;
    int newPid;
    int stateVal;
    //Lock lock;
    CHECKKERNEL();

    // Disable Interrupt
    interrupt =  P1DisableInterrupts();

    // Check if lid exists and index location of the lock
    for(int i = 0; i < P1_MAXLOCKS; i++){
        if(locks[i].lid == lid)
                break;

        if(i == P1_MAXLOCKS - 1)
                return P1_INVALID_LOCK;
        index += 1;
    }

    // lock->state = FREE;
    locks[index].state = FREE;

    if(interrupt == TRUE){}

    if(locks[index].queueIndex > 0){
	newPid = locks[index].pidQueue[0];
    	stateVal = P1SetState(newPid,P1_STATE_READY, lid, locks[index].vid);
	P1Dispatch(FALSE);
    }

    if(stateVal){}

    // Enable Interrupts
    if(interrupt == TRUE){}
	P1EnableInterrupts();
    return result;
}

/*
*
*	It will copy the name in baed on the number of characters requested by len
*
*/
int P1_LockName(int lid, char *name, int len) {
    int result = P1_SUCCESS;
    int lockIndex = 0;
    CHECKKERNEL();

    // more code here
    for(int i = 0; i < P1_MAXLOCKS; i++){
	if(locks[i].lid == lid){
		lockIndex = i;
		break;
	}
	if(i == P1_MAXLOCKS - 1)
		return P1_INVALID_LOCK;
    }

    if(name[0] == 0)
	return P1_NAME_IS_NULL;

    strncpy(locks[lockIndex].name, name, len);

    return result;
}

/*
 * Condition variable functions.
 */

typedef struct Condition
{
    int         inuse;
    char        name[P1_MAXNAME];
    int         lock;  			// lock associated with this variable

    // more fields here
    int		vid;			// condition id
    int		lockQueue[P1_MAXLOCKS];
    int		queueIndex;
} Condition;

static Condition conditions[P1_MAXCONDS];


/*
*
*	It will remove the lock that wants to use the condition. This allows access to the lock easier to find
*
*/
void Remove_Process_From_Cond(int condIndex){ 
	conditions[condIndex].lock = conditions[condIndex].lockQueue[0];
	for(int i = 0; i < conditions[condIndex].queueIndex - 1; i++){
		conditions[condIndex].lockQueue[i] = conditions[condIndex].lockQueue[i + 1];
        }
        conditions[condIndex].queueIndex -= 1;
}


void P1CondInit(void) {
    CHECKKERNEL();
    P1LockInit();
    for (int i = 0; i < P1_MAXCONDS; i++) {
        conditions[i].inuse = FALSE;
	conditions[i].vid = i;
    }
}

/*
*
*	It will create a condition with a unique name and a lock ID.
*	It will also return a unique identifier for vid
*
*/
int P1_CondCreate(char *name, int lid, int *vid) {
    int result = P1_SUCCESS;
    int length = 0;
    int condIndex = 0;
    CHECKKERNEL();

    // more code here
    if(name[0] == 0)
	return P1_NAME_IS_NULL;

    while(1){
        if(length > P1_MAXNAME)
                return P1_NAME_TOO_LONG;
        if(name[length] == 10)
                break;
        length += 1;
    }

    for(int i = 0; i < P1_MAXCONDS; i++){
	if(!strcmp(conditions[i].name,name))
		return P1_DUPLICATE_NAME;
    }

    for(int i = 0; i < P1_MAXCONDS; i++){
	if(conditions[i].inuse == FALSE){
		condIndex += 1;
		break;
        }
	if(i == P1_MAXCONDS - 1)
		return P1_TOO_MANY_CONDS;
    }

    for(int i = 0; i < P1_MAXLOCKS; i++){
	if(locks[i].lid == lid){
		locks[i].vid = *vid;
		break;
	}
	if(i == P1_MAXLOCKS - 1)
		return P1_INVALID_LOCK;
    }

    // Not sure about in use status
    conditions[condIndex].inuse = FALSE;
    strcpy(conditions[condIndex].name,name);
    conditions[condIndex].lock = lid;
    *vid = conditions[condIndex].vid;
    conditions[condIndex].queueIndex = 0;

    return result;
}

/*
*
*	It will free the condition if there are no more processes in queue to use the condition
*	It will cleare the whole condition before it's set up again
*
*/
int P1_CondFree(int vid) {
    int result = P1_SUCCESS;
    int condIndex = 0;
    CHECKKERNEL();

    // more code here
    for(int i = 0; i < P1_MAXCONDS; i++){
	if(conditions[i].vid == vid){
		condIndex = i;
		break;
	}
	if(i == P1_MAXCONDS - 1)
		return P1_INVALID_COND;
    }

    if(conditions[condIndex].queueIndex > 0)
	return P1_BLOCKED_PROCESSES;

    conditions[condIndex].inuse = FALSE;
    strcpy(conditions[condIndex].name,"");
    conditions[condIndex].lock = 0;
    conditions[condIndex].queueIndex = 0;
    return result;
}

/*
*
*	It will make the current process wait for the condition by blocking the process and queue up the
*	process that will later use the condition.
*
*/
int P1_Wait(int vid) {
    int result = P1_SUCCESS;
    int interrupt;
    int state;
    int condIndex = 0;
    int lockIndex = 0;
    CHECKKERNEL();

    // DisableInterrupts(); - X
    interrupt = P1DisableInterrupts();

    // Confirm lock is held - X
    for(int i = 0; i < P1_MAXCONDS; i++){
	if(conditions[i].vid == vid){
		condIndex = i;
		break;
	}
	if(i == P1_MAXCONDS - 1)
		return P1_INVALID_COND;
    }

    for(int i = 0; i < P1_MAXLOCKS; i++){
	if(locks[i].pid == P1_GetPid()){
		lockIndex = i;
		break;
	}
	if(i == P1_MAXLOCKS - 1)
		return P1_INVALID_LOCK;
    }

    if(locks[lockIndex].pid != locks[conditions[condIndex].lock].pid)
    	return P1_LOCK_NOT_HELD;

    // cv->waiting++; - X
    conditions[condIndex].queueIndex += 1;

    // Release(cv->lock); - X
    locks[conditions[condIndex].lock].state = FREE;

    // Make process BLOCKED, add to cv->q - X
    state = P1SetState(P1_GetPid(),P1_STATE_BLOCKED,locks[lockIndex].lid,vid);

    // Add to cv->q - X
    int queueIndex = conditions[condIndex].queueIndex - 1;
    conditions[condIndex].lockQueue[queueIndex] = P1_GetPid();

    // Add to lock queue too
    int index = locks[lockIndex].queueIndex;
    locks[lockIndex].pidQueue[index] = P1_GetPid();
    locks[lockIndex].queueIndex += 1;

    // Dispatcher(); - X
    if(state == P1_STATE_BLOCKED){}
    P1Dispatch(FALSE);

    // Acquire(cv->lock); - X
    locks[conditions[condIndex].lock].state = BUSY;

    if(interrupt == TRUE)
	P1EnableInterrupts();

    return result;
}

/*
*
*	It will signal the next process waiting for the condition to start up again.
*	It will also remove that process' queue in order to avoid repetition unless specified again
*
*/
int P1_Signal(int vid) {
    int result = P1_SUCCESS;
    int interrupt;
    int condIndex = 0;
    int lockIndex = 0;
    int stateVal;

    CHECKKERNEL();

    // Disable Interrupts
    interrupt = P1DisableInterrupts();

    // Confirm lock is held - X
    for(int i = 0; i < P1_MAXCONDS; i++){
        if(conditions[i].vid == vid){
		condIndex = i;
                break;
	}
        if(i == P1_MAXCONDS - 1)
                return P1_INVALID_COND;
    }

    for(int i = 0; i < P1_MAXLOCKS; i ++){
        if(locks[i].pid == P1_GetPid()){
		lockIndex = i;
		break;
	}
        if(i == P1_MAXLOCKS - 1)
                return P1_INVALID_LOCK;
    }

    if(locks[lockIndex].pid != locks[conditions[condIndex].lock].pid)
        return P1_LOCK_NOT_HELD;

    // if (cv->waiting > 0) {
    if(conditions[condIndex].queueIndex > 0){
	Lock nextLock;
        for(int i = 0 ; i < P1_MAXCONDS; i++){
		if(conditions[condIndex].lockQueue[0] == locks[i].lid){
			nextLock = locks[i];
			break;
		}
	}
	// Remove process from cv->q, make READY && cv -> waiting --
	Remove_Process_From_Cond(condIndex);

        //USLOSS_Console("Signal --- Pid: %d, Lock: %d, Vid: %d\n", -1, conditions[0].lockQueue[0], conditions[condIndex].vid);
	//USLOSS_Console("Signal --- Pid: %d, Lock: %d, Vid: %d\n", nextLock.pid, nextLock.lid, conditions[condIndex].vid);
        //USLOSS_Console("Correct Signal --- Pid: 1, Lock: 0, Vid: 1\n");

        stateVal = P1SetState(nextLock.pid, P1_STATE_READY, nextLock.lid, conditions[condIndex].vid);

	if(stateVal == P1_SUCCESS){}
	P1Dispatch(FALSE);
    }

    if(stateVal){}

    // Restor Interrupts
    if(interrupt == TRUE)
    	P1EnableInterrupts();

    return result;
}

/*
*
*	It will let know to all processes waiting for the condition to wake up and be runned. This will happen
*	in a while loop until it make sure it lets all the processes queued for the condition to wake up
*
*/
int P1_Broadcast(int vid) {
    int result = P1_SUCCESS;
    int interrupt;
    int condIndex = 0;
    int lockIndex = 0;
    int stateVal;

    CHECKKERNEL();

    // Disable Interrupts
    interrupt = P1DisableInterrupts();

    // Confirm lock is held - X
    for(int i = 0; i < P1_MAXCONDS; i++){
        if(conditions[i].vid == vid){
                condIndex = i;
                break;
        }
        if(i == P1_MAXCONDS - 1)
                return P1_INVALID_COND;
    }

    for(int i = 0; i < P1_MAXLOCKS; i ++){
        if(locks[i].pid == P1_GetPid()){
                lockIndex = i;
                break;
        }
        if(i == P1_MAXLOCKS - 1)
                return P1_INVALID_LOCK;
    }

    if(locks[lockIndex].pid != locks[conditions[condIndex].lock].pid)
        return P1_LOCK_NOT_HELD;

    // if (cv->waiting > 0) {
    while(conditions[condIndex].queueIndex > 0){
        Lock nextLock;
        for(int i = 0 ; i < P1_MAXCONDS; i++){
                if(conditions[condIndex].lockQueue[0] == locks[i].lid){
                        nextLock = locks[i];
                        break;
                }
        }
        // Remove process from cv->q, make READY && cv -> waiting --
        Remove_Process_From_Cond(condIndex);

        stateVal = P1SetState(nextLock.pid, P1_STATE_READY, nextLock.lid, conditions[condIndex].vid);
        if(stateVal == P1_SUCCESS){ }
        P1Dispatch(FALSE);
    }

    if(stateVal){}

    // Restor Interrupts
    if(interrupt == TRUE)
        P1EnableInterrupts();

    return result;
}

/*
*
*	Like Signal, it will wake up the process but it does not need to make sure if the lock is being held by current process
*
*/
int P1_NakedSignal(int vid) {
    int result = P1_SUCCESS;
    int condIndex = 0;
    int stateVal;
    CHECKKERNEL();

    // more code here
    for(int i = 0; i < P1_MAXCONDS; i++){
	if(conditions[i].vid == vid){
		condIndex = i;
		break;
	}
	if(i == P1_MAXCONDS - 1)
		return P1_INVALID_COND;
    }

    if(conditions[condIndex].queueIndex > 0){
        Lock nextLock;
        for(int i = 0 ; i < P1_MAXCONDS; i++){
                if(conditions[condIndex].lockQueue[0] == locks[i].lid){
                        nextLock = locks[i];
                        break;
                }
        }

        // Remove process from cv->q, make READY && cv -> waiting --
        Remove_Process_From_Cond(condIndex);

        stateVal = P1SetState(nextLock.pid, P1_STATE_READY, nextLock.lid, conditions[condIndex].vid);

        if(stateVal == P1_SUCCESS){}
        P1Dispatch(FALSE);
    }

    return result;
}

/*
*
*	It will copy the name into the requested condition varaible with a specific length 
*
*/
int P1_CondName(int vid, char *name, int len) {
    int result = P1_SUCCESS;
    int condIndex = 0;
    CHECKKERNEL();

    // more code here
    for(int i = 0; i < P1_MAXCONDS; i++){
        if(conditions[i].vid == vid){
                condIndex = i;
                break;
        }
        if(i == P1_MAXCONDS - 1)
                return P1_INVALID_COND;
    }

    if(name[0] == 0)
        return P1_NAME_IS_NULL;

    strncpy(conditions[condIndex].name,name,len);

    return result;

}
