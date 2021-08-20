/*
*
*	Author: Saul Sonido Perez
*	Assignment: Phase2c 03/29/2021
*
*/

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <usloss.h>
#include <phase1.h>

#include "phase2Int.h"


static int      DiskDriver(void *);
static void     ReadStub(USLOSS_Sysargs *sysargs);
static void     WriteStub(USLOSS_Sysargs *sysargs);
static void     SizeStub(USLOSS_Sysargs *sysargs);

static char *
MakeName(char *prefix, int suffix)
{
    static char name[P1_MAXNAME];
    snprintf(name, sizeof(name), "%s%d", prefix, suffix);
    return name;
}

int lid, vid;
int track = 0;
int NotShutDown;
int NoRequests = 1;
int head1;
int head2;
int RequestsNotFilled = 1;
int RequestNotTaken = 1;
int listSize = 0;
struct USLOSS_DeviceRequest reqSeek, reqReadWrite;
struct USLOSS_DeviceRequest requestList[P1_MAXPROC];
int requestPIDList[P1_MAXPROC];
int requestPIDSect[P1_MAXPROC];

//struct USLOSS_DeviceRequest request;
/*
 * P2DiskInit
 *
 * Initialize the disk data structures and fork the disk drivers.
v */
void 
P2DiskInit(void) 
{
    int rc;
    NotShutDown = 1;
    rc = P1_LockCreate("Lock", &lid);
    rc = P1_CondCreate("Cond", lid, &vid);
    // initialize data structures here including lock and condition variables
    rc = P2_SetSyscallHandler(SYS_DISKREAD, ReadStub);
    assert(rc == P1_SUCCESS);

    rc = P2_SetSyscallHandler(SYS_DISKWRITE, WriteStub);
    assert(rc == P1_SUCCESS);

    rc = P2_SetSyscallHandler(SYS_DISKSIZE, SizeStub);
    assert(rc == P1_SUCCESS);

    for (int unit = 0; unit < USLOSS_DISK_UNITS; unit++) {
        int pid;
        rc = P1_Fork(MakeName("Disk Driver ", unit), DiskDriver, (void *) unit, USLOSS_MIN_STACK*4, 
                     1, &pid);
        assert(rc == P1_SUCCESS);
    }
}

/*
 * P2DiskShutdown
 *
 * Stop the disk drivers.
 */

void 
P2DiskShutdown(void) 
{
    int rc; 
    // stop clock driver
    rc = P1_DeviceAbort(USLOSS_DISK_DEV, 0);
    rc = P1_DeviceAbort(USLOSS_DISK_DEV, 1);
    NotShutDown = 0;
}

/*
 * DiskDriver
 *
 * Kernel process that manages a disk device and services disk I/O requests from other processes.
 * Note that it may require several disk operations to service a single I/O request. A disk
 * operation is performed by sending a request of type USLOSS_DeviceRequest to the disk via
 * USLOSS_DeviceOutput, then waiting for the operation to finish via P1_WaitDevice. The status
 * returned by P1_WaitDevice will tell you if the operation was successful or not.
 */
static int 
DiskDriver(void *arg) 
{
    //printf("DiskDriver\n");
    int unit = (int) arg;
    int rc;
    int status = 1;
    rc = P1_Lock(lid);		assert(rc == 0);
    //printf("Unit: %d\n",unit);
    // Have a lock for this part to prevent corruptions xd
    while(NotShutDown){
	while(listSize < 1){
		rc = P1_Wait(vid);			assert(rc == 0);
	}

	int pid = requestPIDList[0];
	int first = (int)requestList[pid].reg1;
	int sectors = requestPIDSect[pid];
	for(int i = first; i < sectors; i++){
		requestList[pid].reg1 = (void *)i;
		//USLOSS_Console("Opr: %d, Reg1: %d, Reg2: %s\n",(int)requestList[pid].opr, (int)requestList[pid].reg1, (char *)requestList[pid].reg2);
	        rc = USLOSS_DeviceOutput(USLOSS_DISK_DEV, unit, &requestList[pid]);          assert(rc == 0);
	}
	rc = P1_DeviceWait(USLOSS_DISK_DEV, unit, &status);				assert(rc == 0);
	//USLOSS_Console("Status: %d\n",status);
	RequestNotTaken = 0;	// Let the device know the request has been taken
	//USLOSS_Console("Driver VID: %d\n", vid);
	rc = P1_Signal(vid);								assert(rc == 0);
    }
    /****
    repeat
        choose request with shortest seek from current track // This be protected with a lock. The rest shouldn't
        seek to proper track if necessary
        while request isn't complete
             for all sectors to be read/written in current track
                read/write sector
             seek to next track
        wake the waiting process
    until P2DiskShutdown has been called
    ****/

    USLOSS_Console("DiskDriver PID %d unit %d exiting.\n", P1_GetPid(), unit);
    return 0;
}

/*
 * P2_DiskRead
 *
 * Reads the specified number of sectors from the disk starting at the first sector.
 */
int 
P2_DiskRead(int unit, int first, int sectors, void *buffer) 
{
    int rc = 1;
    if(!(unit == 0 || unit == 1)){
        return P1_INVALID_UNIT;
    }

    if(!(0 <= sectors && first <= 15)){
        return P2_INVALID_FIRST;
    }

    if(sectors < 0){
        return P2_INVALID_SECTORS;
    }

    if(buffer == NULL){
        return P2_NULL_ADDRESS;
    }

    //printf("Disk Read -> Unit: %d, First: %d, Sectors: %d\n", unit, first, sectors);
    // validate parameters
    // give request to the unit's device driver
    rc = P1_Lock(lid);    		assert(rc == 0);

    requestPIDList[listSize] = P1_GetPid();
    requestList[P1_GetPid()].opr = USLOSS_DISK_READ;
    requestList[P1_GetPid()].reg1 = (void *) 0;
    requestList[P1_GetPid()].reg2 = buffer;
    listSize += 1;

    RequestNotTaken = 1;
    rc = P1_Signal(vid);		assert(rc == 0);
    while(RequestNotTaken){
        //USLOSS_Console("Driver VID: %d\n", vid);
    	rc = P1_Wait(vid);
    }
    rc = P1_Unlock(lid);
    // wait until device driver completes the request
    return P1_SUCCESS;
}

/*
 * P2_DiskWrite
 *
 * Writes the specified number of sectors to the disk starting at the first sector.
 */
int 
P2_DiskWrite(int unit, int first, int sectors, void *buffer) 
{
    int rc = 1;
    if(!(unit == 0 || unit == 1)){
        return P1_INVALID_UNIT;
    }

    if(!(0 <= sectors && first <= 15)){
        return P2_INVALID_FIRST;
    }

    if(sectors < 0){
        return P2_INVALID_SECTORS;
    }

    if(buffer == NULL){
        return P2_NULL_ADDRESS;
    }

    printf("Disk Write -> Unit: %d, First: %d, Sectors: %d\n", unit, first, sectors);
    // validate parameters
    // give request to the unit's device driver
    rc = P1_Lock(lid);   		assert(rc == 0);

    requestPIDList[listSize] = P1_GetPid();
    requestPIDSect[P1_GetPid()] = sectors;
    requestList[P1_GetPid()].opr = USLOSS_DISK_WRITE;
    requestList[P1_GetPid()].reg1 = (void *)first;
    requestList[P1_GetPid()].reg2 = buffer;
    listSize += 1;

    RequestNotTaken = 1;
    rc = P1_Signal(vid);                assert(rc == 0);
    while(RequestNotTaken){
        USLOSS_Console("Disk Write VID: %d\n", vid);
        rc = P1_Wait(vid);		assert(rc == 0);
	USLOSS_Console("Request Status: %d\n",RequestNotTaken);
    }

    requestPIDList[listSize] = 0;
    listSize -= 1;
    rc = P1_Unlock(lid);		assert(rc == 0);
    // wait until device driver completes the request
    USLOSS_Console("Writing End\n");
    return P1_SUCCESS;
}

/*
 * P2_DiskSize
 *
 * Returns the size of the disk.
 */
int 
P2_DiskSize(int unit, int *sector, int *disk) 
{
    printf("Disk Size\n");
    if(unit < 0 || unit < 1)
        return P1_INVALID_UNIT;

    printf("4 -- Unit: %d, Sectors: %d, Disk: %d\n", unit, *sector, *disk);
    // validate parameter
    // give request to the unit's device driver
    // wait until device driver completes the request
    return P1_SUCCESS;
}

static void 
ReadStub(USLOSS_Sysargs *sysargs) 
{
    int     rc;
    rc = P2_DiskRead((int) sysargs->arg4, (int) sysargs->arg3, (int) sysargs->arg2, sysargs->arg1);
    sysargs->arg4 = (void *) rc;
}

static void 
WriteStub(USLOSS_Sysargs *sysargs) 
{
    int     rc;
    rc = P2_DiskWrite((int) sysargs->arg4, (int) sysargs->arg3, (int) sysargs->arg2, sysargs->arg1);
    sysargs->arg4 = (void *) rc;
}


static void 
SizeStub(USLOSS_Sysargs *sysargs) 
{
    int     rc;
    int     sector;
    int     disk;

    rc = P2_DiskSize((int) sysargs->arg1, &sector, &disk);
    sysargs->arg1 = (void *) sector;
    sysargs->arg2 = (void *) disk;
    sysargs->arg4 = (void *) rc;
}


