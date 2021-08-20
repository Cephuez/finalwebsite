/*
*
*	Author: Saul Sonido Perez
*	Assignment: Phase 2D
*
*/
#include <string.h>
#include <stdlib.h>
#include <usloss.h>
#include <phase1.h>
#include <assert.h>
#include <libuser.h>
#include <libdisk.h>

#include "phase2Int.h"

static void     LockCreateStub(USLOSS_Sysargs *sysargs);
static void     LockAcquireStub(USLOSS_Sysargs *sysargs);
static void     LockReleaseStub(USLOSS_Sysargs *sysargs);
static void     LockFreeStub(USLOSS_Sysargs *sysargs);
static void     LockNameStub(USLOSS_Sysargs *sysargs);
static void     CondCreateStub(USLOSS_Sysargs *sysargs);
static void     CondWaitStub(USLOSS_Sysargs *sysargs);
static void     CondSignalStub(USLOSS_Sysargs *sysargs);
static void     CondBroadcastStub(USLOSS_Sysargs *sysargs);
static void     CondFreeStub(USLOSS_Sysargs *sysargs);
static void     CondNameStub(USLOSS_Sysargs *sysargs);

int UserLockLID[P1_MAXLOCKS];		// Holds which LIDs are hold by the user
int UserCondVID[P1_MAXCONDS];		// Holds which VIDs are hold by the user

// Puts in all the stubs to be called by the Syscall Handler
void SetUpSyscallHandlers(){
    int rc;

    rc = P2_SetSyscallHandler(SYS_LOCKCREATE, LockCreateStub);
    assert(rc == P1_SUCCESS);
    rc = P2_SetSyscallHandler(SYS_LOCKACQUIRE, LockAcquireStub);
    assert(rc == P1_SUCCESS);
    rc = P2_SetSyscallHandler(SYS_LOCKRELEASE, LockReleaseStub);
    assert(rc == P1_SUCCESS);
    rc = P2_SetSyscallHandler(SYS_LOCKFREE, LockFreeStub);
    assert(rc == P1_SUCCESS);
    rc = P2_SetSyscallHandler(SYS_LOCKNAME, LockNameStub);
    assert(rc == P1_SUCCESS);
    rc = P2_SetSyscallHandler(SYS_CONDCREATE, CondCreateStub);
    assert(rc == P1_SUCCESS);
    rc = P2_SetSyscallHandler(SYS_CONDWAIT, CondWaitStub);
    assert(rc == P1_SUCCESS);
    rc = P2_SetSyscallHandler(SYS_CONDSIGNAL, CondSignalStub);
    assert(rc == P1_SUCCESS);
    rc = P2_SetSyscallHandler(SYS_CONDBROADCAST, CondBroadcastStub);
    assert(rc == P1_SUCCESS);
    rc = P2_SetSyscallHandler(SYS_CONDFREE, CondFreeStub);
    assert(rc == P1_SUCCESS);
    rc = P2_SetSyscallHandler(SYS_CONDNAME, CondNameStub);
    assert(rc == P1_SUCCESS);
}

int P2_Startup(void *arg)
{
    int rc, pid;
    int status, pid2;
    P2ClockInit();
    P2DiskInit();

    // install system call handlers
    SetUpSyscallHandlers();

    rc = P2_Spawn("P3_Startup", P3_Startup, NULL, 4*USLOSS_MIN_STACK, 2, &pid);
    assert(rc == P1_SUCCESS);

    // wait for P3_Startup to terminate
    rc = P2_Wait(&pid2, &status);
    assert(rc == P1_SUCCESS);
    P2DiskShutdown();
    P2ClockShutdown();

    return 0;
}

// It will create a Lock
static void
LockCreateStub(USLOSS_Sysargs *sysargs){
    int rc;
    int lid;

    rc = P1_LockCreate((char *)sysargs -> arg1, &lid);
    assert(rc == P1_SUCCESS);

    // It will mark the lid as a user level lock
    UserLockLID[lid] = 1;

    sysargs -> arg1 = (void *)lid;
    sysargs -> arg4 = (void *)rc;
}

// It will Lock the lock if it's a user level lock
static void
LockAcquireStub(USLOSS_Sysargs *sysargs){
    int rc = P1_INVALID_LOCK;

    if(UserLockLID[(int)sysargs -> arg1] == 1){
    	rc = P1_Lock((int)sysargs -> arg1);
    	assert(rc == P1_SUCCESS);

    }

    sysargs -> arg4 = (void *)rc;
}

// It will release the lock if it's a user level lock
static void
LockReleaseStub(USLOSS_Sysargs *sysargs){
    int rc = P1_INVALID_LOCK;

    if(UserLockLID[(int)sysargs -> arg1] == 1){
    	rc = P1_Unlock((int)sysargs -> arg1);
	assert(rc == P1_SUCCESS);
    }
    sysargs -> arg4 = (void *)rc;
}

// It will free the lock if it's a user level lock
static void
LockFreeStub(USLOSS_Sysargs *sysargs){
    int rc = P1_INVALID_LOCK;

    if(UserLockLID[(int)sysargs -> arg1] == 1){
    	rc = P1_LockFree((int) sysargs -> arg1);
	assert(rc == P1_SUCCESS);

	// The value will be resetted to 0 once freed
	UserLockLID[(int)sysargs -> arg1] = 0;
    }

    sysargs -> arg4 = (void *)rc;
}

// It will set the name of the lock if it's a user levle lock
static void
LockNameStub(USLOSS_Sysargs *sysargs){
    int rc = P1_INVALID_LOCK;

    if(UserLockLID[(int)sysargs -> arg1] == 1){
    	rc = P1_LockName((int) sysargs -> arg1, (char *)sysargs ->arg2, (int)sysargs -> arg3);
	assert(rc == P1_SUCCESS);
    }
    sysargs -> arg4 = (void *)rc;
}

// It will create a condition variable with a user level lock
static void
CondCreateStub(USLOSS_Sysargs *sysargs){
    int vid;
    int rc = P1_INVALID_LOCK;

    if(UserLockLID[(int)sysargs -> arg2] == 1){
    	rc = P1_CondCreate((char *)sysargs -> arg1, (int)sysargs -> arg2, &vid);
	assert(rc == P1_SUCCESS);
	UserCondVID[vid] = 1;
	sysargs -> arg1 = (void *)vid;
    }
    sysargs -> arg4 = (void *)rc;
}

// It will wait on the condition variable if vid was created in user level
static void
CondWaitStub(USLOSS_Sysargs *sysargs){
    int rc = P1_INVALID_COND;
    if(UserCondVID[(int)sysargs -> arg1] == 1){
    	rc = P1_Wait((int)sysargs -> arg1);
	assert(rc == P1_SUCCESS);
    }
    sysargs -> arg4 = (void *)rc;
}

// It will signal the condition variable if vid was created in user level
static void
CondSignalStub(USLOSS_Sysargs *sysargs){
    int rc = P1_INVALID_COND;
    if(UserCondVID[(int)sysargs -> arg1] == 1){
    	rc = P1_Signal((int)sysargs -> arg1);
	assert(rc == P1_SUCCESS);
    }
    sysargs -> arg4 = (void *)rc;
}

// It will broadcast all the waiting locks if vid was created in user level
static void
CondBroadcastStub(USLOSS_Sysargs *sysargs){
    int rc = P1_INVALID_COND;

    if(UserCondVID[(int)sysargs -> arg1] == 1){
    	rc = P1_Broadcast((int)sysargs -> arg1);
	assert(rc == P1_SUCCESS);
    }

    sysargs -> arg4 = (void *)rc;
}

// It will free the condition variable if vid was created in user level
static void
CondFreeStub(USLOSS_Sysargs *sysargs){
    int rc = P1_INVALID_COND;

    if(UserCondVID[(int)sysargs -> arg1] == 1){
	rc = P1_CondFree((int)sysargs -> arg1);
	assert(rc == P1_SUCCESS);

	// Reset the value of the VID to 0
	UserCondVID[(int)sysargs -> arg1] = 0;
    }

    sysargs -> arg4 = (void *)rc;
}

// It will set the name of the condition varaible if lid was created in user level
static void
CondNameStub(USLOSS_Sysargs *sysargs){
    int rc = P1_INVALID_COND;

    if(UserCondVID[(int)sysargs -> arg1] == 1){
	rc = P1_CondName((int)sysargs -> arg1, (char *)sysargs -> arg2, (int)sysargs -> arg3);
	assert(rc == P1_SUCCESS);
    }
    sysargs -> arg4 = (void *) rc;
}

