/**
*
*	Name: Saul Sonido Perez ... No partner
*	Assignment: Phase1d
*/
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <usloss.h>
#include <phase1.h>
#include <phase1Int.h>

#define CHECKKERNEL() \
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0) USLOSS_IllegalInstruction()

static int interruptState, newStatus;

int lid1,lid2,vid1, vid2;

int clockLid, alarmLid, diskLid, termLid;
int clockVid, alarmVid, diskVid, termVid;

int clockSignal, clockDispatch;

static void DeviceHandler(int type, void *arg);
static void SyscallHandler(int type, void *arg);
static void IllegalInstructionHandler(int type, void *arg);

static int Sentinel(void *arg);

// The following structs will be used to handle different devices based on their unit, interrupts, and abort
// A little redundent, but it's way easier to have it like this instead of having one array specifying
// the type of device and if it has units bigger than 1
typedef struct Clock{
        int interrupt;
        int abort;
} Clock;

typedef struct Alarm{
        int interrupt;
        int abort;
} Alarm;

typedef struct Disk{
        int interrupt;
        int abort;
} Disk;

typedef struct Term{
        int interrupt;
        int abort;
} Term;

struct Clock *clockStr;
struct Alarm *alarmStr;
struct Disk *diskStr;
struct Term *termStr;

// Create the devices for the structs that will be used
void createDevices(){
        clockStr = malloc(sizeof(struct Clock));
        alarmStr = malloc(sizeof(struct Alarm));
        diskStr = malloc(2 * sizeof(struct Disk));	// Size 2 of units
        termStr = malloc(4 * sizeof(struct Term));	// Size 4 of units
}

// Create the  locks and conditions for the 4 devices to be used
void createLockAndCond(){
    int lockResult,condResult;

    lockResult = P1_LockCreate("lock1",&lid1);
    condResult = P1_CondCreate("cond1", lid1, &vid1);

    lockResult = P1_LockCreate("lock2",&lid2);
    condResult = P1_CondCreate("cond2", lid2, &vid2);

    lockResult = P1_LockCreate("clockLid",&clockLid);
    condResult = P1_CondCreate("clockVid", clockLid, &clockVid);

    lockResult = P1_LockCreate("alarmLid", &alarmLid);
    condResult = P1_CondCreate("alarmVid", alarmLid, &alarmVid);

    lockResult = P1_LockCreate("diskLid", &diskLid);
    condResult = P1_CondCreate("diskVid", diskLid, &diskVid);

    lockResult = P1_LockCreate("termLid", &termLid);
    condResult = P1_CondCreate("termVid", termLid, &termVid);
}

// Initialize the locks, conditions, and structs for the devices along with starting the sentinel by forking it
void
startup(int argc, char **argv)
{
    int pid;
    newStatus = -1;
    clockSignal = 0;
    clockDispatch = 0;
    interruptState = -1;
    P1ProcInit();
    P1LockInit();
    P1CondInit();
    createDevices();
    createLockAndCond();

    // initialize device data structures
    USLOSS_IntVec[USLOSS_CLOCK_INT] = DeviceHandler;

    // put DeviceHandler into interrupt vector for the remaining devices
    USLOSS_IntVec[USLOSS_ALARM_INT] = DeviceHandler;
    USLOSS_IntVec[USLOSS_DISK_INT] = DeviceHandler;
    USLOSS_IntVec[USLOSS_TERM_INT] = DeviceHandler;
    USLOSS_IntVec[USLOSS_MMU_INT] = DeviceHandler;
    USLOSS_IntVec[USLOSS_SYSCALL_INT] = DeviceHandler;
    USLOSS_IntVec[USLOSS_ILLEGAL_INT] = DeviceHandler;

    USLOSS_IntVec[USLOSS_SYSCALL_INT] = SyscallHandler;

    /* create the sentinel process */
    int rc = P1_Fork("Sentinel", Sentinel, NULL, USLOSS_MIN_STACK, 6 , &pid);
    assert(rc == P1_SUCCESS);
    // should not return
    assert(0);
    return;

} /* End of startup */

// Small method to figure out if an itnerrupt has occured or an abort on one of the devices
void Wait_For_Interrupt_Or_Abort(int type, int unit){
    if(type == USLOSS_CLOCK_DEV){
        while(clockStr -> interrupt != 1 && clockStr -> interrupt != 1){
                USLOSS_WaitInt();
        }
    }else if(type == USLOSS_ALARM_DEV){
        while(alarmStr -> interrupt != 1 && alarmStr -> interrupt != 1){
                USLOSS_WaitInt();
        }
    }else if(type == USLOSS_DISK_DEV){
        while(diskStr[unit].interrupt != 1 && diskStr[unit].interrupt != 1){
                USLOSS_WaitInt();
        }
    }else if(type == USLOSS_TERM_DEV){
        while(termStr -> interrupt != 1 && termStr -> interrupt != 1){
                USLOSS_WaitInt();
        }
    }

}

// Update the status of the status if the device has not aborted and also clean up some things if necessary
void Update_Status(int type, int unit, int *status){
	if(type == USLOSS_CLOCK_DEV){
		if(clockStr -> abort != 1)
			*status = newStatus;
		clockStr -> interrupt = 0;
		clockStr -> abort = 0;
        }else if(type == USLOSS_ALARM_DEV){
                if(alarmStr -> abort != 1)
                        *status = newStatus;
		alarmStr -> interrupt = 0;
		alarmStr -> abort = 0;
	}else if(type == USLOSS_DISK_DEV){
                if(diskStr[unit].abort != 1)
                        *status = newStatus;
		diskStr[unit].interrupt = 0;
		diskStr[unit].abort = 0;
	}else if(type == USLOSS_TERM_DEV){
                if(diskStr[unit].abort != 1)
			*status = newStatus;
		termStr[unit].interrupt = 0;
		diskStr[unit].abort = 0;
	}
}

// The device will wait until a certain interrupt has occured. It will also check if the type and unit are correct
// It will allow the device to wait before updating the status and returning it to the program
int
P1_DeviceWait(int type, int unit, int *status) 
{
    int result = P1_SUCCESS;
    int interrupt;

    int lockResult = -1; 
    int condResult = -1;

    //int devResult = 0;

    // Check if it's valid
    if(!(type == USLOSS_CLOCK_DEV || type == USLOSS_ALARM_DEV || type == USLOSS_DISK_DEV || type == USLOSS_TERM_DEV)){
	return P1_INVALID_TYPE;
    }

    // Check if the units are valid for the type
    if(type == USLOSS_CLOCK_DEV && unit != 0){
	return P1_INVALID_UNIT;
    }else if(type == USLOSS_ALARM_DEV && unit != 0){
	return P1_INVALID_UNIT;
    }else if(type == USLOSS_DISK_DEV && !(unit == 0 || unit == 1)){
	return P1_INVALID_UNIT;
    }else if(type == USLOSS_TERM_UNITS && !(0 <= unit && unit <= USLOSS_TERM_UNITS - 1)){
	return P1_INVALID_UNIT;
    }

    // Check if the device was aborted or not
    if(type == USLOSS_CLOCK_DEV && clockStr -> abort == 1){
	return P1_WAIT_ABORTED;
    }else if(type == USLOSS_ALARM_DEV && alarmStr -> abort == 1){
        return P1_WAIT_ABORTED;
    }else if(type == USLOSS_DISK_DEV && diskStr[unit].abort == 1){
	return P1_WAIT_ABORTED;
    }else if(type == USLOSS_TERM_DEV && termStr[unit].abort == 1){
        return P1_WAIT_ABORTED;
    }

    // disable interrupts
    interrupt = P1DisableInterrupts();

    // check kernel mode
    CHECKKERNEL();

    // acquire device's lock
    if(type == USLOSS_CLOCK_DEV){
	condResult = P1_Lock(clockLid);
    }else if(type == USLOSS_ALARM_DEV){
        condResult = P1_Lock(alarmLid);
    }else if(type == USLOSS_DISK_DEV){
        condResult = P1_Lock(diskLid);
    }else if(type == USLOSS_TERM_DEV){
        condResult = P1_Lock(termLid);
    }

    // while interrupt has not occurred and not aborted
    //      wait for interrupt or abort on type:unit
    interrupt = USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);
    Wait_For_Interrupt_Or_Abort(type, unit);

    // if not aborted
    //      set *status to device's status
    Update_Status(type, unit, status);


    // release lock
    if(type == USLOSS_CLOCK_DEV){
        lockResult = P1_Unlock(clockLid);
    }else if(type == USLOSS_ALARM_DEV){
        lockResult = P1_Unlock(alarmLid);
    }else if(type == USLOSS_DISK_DEV){
        lockResult = P1_Unlock(diskLid);
    }else if(type == USLOSS_TERM_DEV){
        lockResult = P1_Unlock(termLid);
    }

    // restore interrupts
    if(interrupt == TRUE)P1EnableInterrupts();
    return result;
}


// The device will handle interrupts that will occur. It will also update the device based on its unit if
// an interrupt has occured.
static void
DeviceHandler(int type, void *arg) 
{
    int condResult = -1;
    int unit = (int) arg;
    // record that interrupt occurred
    interruptState = type;

    // save device status
    condResult = USLOSS_DeviceInput(type,unit,&newStatus);

    // if clock device
    if(type == 0){
    //  1 tick = 20m
	clockSignal += 1;
	clockDispatch += 1;
        if(clockSignal >= 5){
    		//  Wake it up every 5 tikes. Every hundred of milliseconds
    		//  naked signal clock every 5 interrupts
		condResult = P1_NakedSignal(clockVid);
		clockStr -> interrupt = 1;
		clockSignal = 0;
        }
        if(clockDispatch >= 4){
    		// P1Dispatch(TRUE) every 4 interrupts
    		P1Dispatch(TRUE);
		clockDispatch = 0;
	}
    // else
    }else{
	// naked signal type:unit
	if(type == USLOSS_CLOCK_DEV){
                clockStr -> interrupt = 1;
                condResult = P1_NakedSignal(clockVid);
	}else if(type == USLOSS_ALARM_INT){
		alarmStr -> interrupt = 1;
		condResult = P1_NakedSignal(alarmVid);
		//USLOSS_Console("Alarm Signaled\n");
	}else if(type == USLOSS_DISK_DEV){
		diskStr[unit].interrupt = 1;
		condResult = P1_NakedSignal(diskVid);
        }else if(type == USLOSS_TERM_DEV){
                termStr[unit].interrupt = 1;
                condResult = P1_NakedSignal(termVid);
        }
    }
}

// It will abort the device based on its unit. It will also make sure that the type and unit
// is valid  before updating the abort value of the device
int
P1_DeviceAbort(int type, int unit){
    int result = P1_SUCCESS;
    // Check if it's valid
    if(!(type == USLOSS_CLOCK_DEV || type == USLOSS_ALARM_DEV || type == USLOSS_DISK_DEV || type == USLOSS_TERM_DEV)){
        return P1_INVALID_TYPE;
    }

    if(type == USLOSS_CLOCK_DEV && unit != 0){
        return P1_INVALID_UNIT;
    }else if(type == USLOSS_ALARM_DEV && unit != 0){
        return P1_INVALID_UNIT;
    }else if(type == USLOSS_DISK_DEV && !(unit == 0 || unit == 1)){
        return P1_INVALID_UNIT;
    }else if(type == USLOSS_TERM_UNITS && !(0 <= unit && unit <= USLOSS_TERM_UNITS - 1)){
        return P1_INVALID_UNIT;
    }

    if(type == USLOSS_CLOCK_DEV){
	clockStr -> abort = 1;
    }else if(type == USLOSS_ALARM_DEV){
	alarmStr -> abort = 1;
    }else if(type == USLOSS_DISK_DEV){
	diskStr[unit].abort = 1;
    }else if(type == USLOSS_TERM_UNITS){
	termStr[unit].abort = 1;
    }
    return result;
}

// It will run the sentinel as the lowest priority process because it
// will be the last to shut down.
static int
Sentinel (void *notused)
{
    int     pid;
    int     rc;
    int	    status;
    int     childrenStat = 0;

    //int childStatus;
    /* start the P2_Startup process */
    USLOSS_Console("Sentinel Start\n");
    rc = P1_Fork("P2_Startup", P2_Startup, NULL, 4 * USLOSS_MIN_STACK, 2, &pid);
    assert(rc == P1_SUCCESS);

    // while sentinel has children
        // call P1GetChildStatus to get children that have quit
        // wait for an interrupt via USLOSS_WaitInt

    while(childrenStat != P1_NO_CHILDREN){
	childrenStat = P1GetChildStatus(&pid, &status);
	USLOSS_WaitInt();
    }

    USLOSS_Console("Sentinel End.\n");
    return 0;
} /* End of sentinel */

// The process will join with the other processes if it's possible. The process will have
// to wait if its children are still running.
int 
P1_Join(int *pid, int *status) 
{
    //USLOSS_Console("Join is joining\n");
    int result = P1_SUCCESS;
    int interrupt = -1;
    int rc = -1;
    // disable interrupts
    interrupt = P1DisableInterrupts();

    // check kernel mode
    CHECKKERNEL();

    // repeat
    while(1){
    	// call P1GetChildStatus
	result = P1GetChildStatus(pid,status);
	if(result == 0)
		break;
    	// if there are children but no children have quit
        if(result == -6){
		rc = P1SetState(P1_GetPid(),P1_STATE_JOINING,lid2,vid2);
		P1Dispatch(FALSE);
        }
    	// until either a child quit or there are no children
    }
    // restore interrupts
    if(interrupt)
    	P1EnableInterrupts();
    return result;
}

static void
SyscallHandler(int type, void *arg) 
{
    USLOSS_Sysargs *sysargs = (USLOSS_Sysargs *) arg;
    USLOSS_Console("System call %d not implemented.\n", sysargs->number);
    P1_Quit(1025);
}
