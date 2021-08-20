/*
*
*	Author: Saul Sonido Perez
*	Assignment: Phase2B
*/


#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <usloss.h>
#include <phase1.h>

#include "phase2Int.h"


static int      ClockDriver(void *);
static void     SleepStub(USLOSS_Sysargs *sysargs);

static int      now; // current time

int sleeperPID[100];	// Array of PIDs that are set to Sleep
int pidIndex; 
int sleeperPIDSec[100];	// Condition variables for the PIDs waiting on X seconds
int secConv = 1000000;	// Used to convert the milliseconds to seconds

int lid, vid;		// Lock and Condition variable
int forkPID;		// forkPid for reference
int time;		// Time being tracked
/*
 * P2ClockInit
 *
 * Initialize the clock data structures and fork the clock driver.
 */
void 
P2ClockInit(void) 
{
    int rc;

    P2ProcInit();

    // initialize data structures here
    rc = P1_LockCreate("Timer",&lid);
    rc = P1_CondCreate("TimerCond",lid,&vid); 

    rc = P2_SetSyscallHandler(SYS_SLEEP, SleepStub);
    assert(rc == P1_SUCCESS);

    // fork the clock driver here
    rc = P1_Fork("clockdevice",ClockDriver, NULL, USLOSS_MIN_STACK, 1, &forkPID);
}

/*
 * P2ClockShutdown
 *
 * Clean up the clock data structures and stop the clock driver.
 */

void 
P2ClockShutdown(void) 
{
    int rc; 
    // stop clock driver
    rc = P1_DeviceAbort(USLOSS_CLOCK_DEV, 0);
}

/*
 * ClockDriver
 *
 * Kernel process that manages the clock device and wakes sleeping processes.
 */
static int 
ClockDriver(void *arg) 
{
    while(1) {
        int rc;
        // wait for the next interrup
        rc = P1_DeviceWait(USLOSS_CLOCK_DEV, 0, &now);

        if (rc == P1_WAIT_ABORTED) {
            break;
        }
        assert(rc == P1_SUCCESS);

        // Lock the processor before broadcasting all the processors
	rc = P1_Lock(lid);

        // wakeup any sleeping processes whose wakeup time has arrived
        rc = P1_Broadcast(vid);

	// Unlock when it's all done.
        rc = P1_Unlock(lid);

     }
    return P1_SUCCESS;
}

/*
 * P2_Sleep
 *
 * Causes the current process to sleep for the specified number of seconds.
 */
int 
P2_Sleep(int seconds) 
{
    int rc;

    if(seconds < 0)
	return P2_INVALID_SECONDS;
    // Acquire lock before accessing data structure
    rc = P1_Lock(lid);

    // update current time and determine wakeup time
    // add current process to data structure of sleepers
    sleeperPID[P1_GetPid()] = P1_GetPid();
    sleeperPIDSec[P1_GetPid()] = seconds;

    // wait until it's wakeup time or the minimum amount of seconds have passed
    while((now/secConv) < sleeperPIDSec[P1_GetPid()]){
    	rc = P1_Wait(vid);
    }

    sleeperPID[P1_GetPid()] = 0;
    sleeperPIDSec[P1_GetPid()] = 0;
    rc = P1_Unlock(lid);
    // Remove PID if it has reached the seconds
    return P1_SUCCESS;
}

/*
 * SleepStub
 *
 * Stub for the Sys_Sleep system call.
 */
static void 
SleepStub(USLOSS_Sysargs *sysargs) 
{
    int seconds = (int) sysargs->arg1;
    int rc = P2_Sleep(seconds);
    sysargs->arg4 = (void *) rc;
}

