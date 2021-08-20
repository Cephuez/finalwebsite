/*
 * phase3a.c
 *
 *		Author: Saul Sonido Perez
 *		Assignment: Phase3a
 */

#include <assert.h>
#include <phase1.h>
#include <phase2.h>
#include <usloss.h>
#include <string.h>
#include <libuser.h>

#include "phase3Int.h"

#define CHECKKERNEL() \
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0) USLOSS_IllegalInstruction();
P3_VmStats  P3_vmStats;
struct USLOSS_PTE *pteTable[P1_MAXPROC];
int pidTableCheck[10];
int faultLID, faultVID;
int pagerLID, pagerVID;
int faultQueue[200];
int faultQueuePID[200];
int faultIndex;
int terminationQueue[200];
int terminationIndex;
int pagerPID;
int NotShutDown = 1;
int terminalNotCalled = 1;
int faultNotHandled = 1;
int vmInitCalled = 0;
int faultInfo;
static void
FaultHandler(int type, void *arg)
{
    USLOSS_Console("1\n");
    CHECKKERNEL();
    //USLOSS_Console("Type %d\n", type);
    int cause = USLOSS_MmuGetCause();
    //int status = -1;
    int rc = -1;
    int page = 0;
    //rc = P1_Lock(pagerLID);						assert(rc == 0);
    if(cause == USLOSS_MMU_ACCESS){
	rc = P2_Terminate(P3_ACCESS_VIOLATION);
	assert(rc == 0);
    }else{
	rc = USLOSS_MmuGetConfig(NULL, NULL, &page, NULL, NULL, NULL);	assert(rc == USLOSS_MMU_OK); 
	faultInfo = ((int) arg) / page;

	rc = P1_Lock(faultLID);						assert(rc == 0);
	faultQueue[faultIndex] = faultInfo;
	faultQueuePID[faultIndex] = P1_GetPid();
	faultIndex += 1;
	terminalNotCalled = 0;
	rc = P1_Signal(faultVID); 					assert(rc == 0);
	while(faultNotHandled){
		rc = P1_Wait(pagerVID);					assert(rc == 0);
	}
	rc = P1_Unlock(faultLID);					assert(rc == 0);
	faultNotHandled = 1;
	if(terminationQueue[0]){
		P3_FreePageTable(faultQueuePID[0]);
		rc = P2_Terminate(P3_OUT_OF_SWAP);			assert(rc == 0);
	}
    }
    /*******************

    if it's an access fault (USLOSS_MmuGetCause)
        terminate faulting process w/ P3_ACCESS_VIOLATION
    else
        add fault information to a queue of pending faults
        let the pager know that there is a pending fault
        wait until the fault has been handled by the pager
        terminate the process if necessary

    *********************/

}
void popQueue(){
	for(int i = 1; i < faultIndex; i++){
		faultQueuePID[i - 1] = faultQueuePID[i];
		faultQueue[i - 1] = faultQueue[i];
	}
}

static int
Pager(void *arg)
{
    int rc = 1;
    int frame;
    CHECKKERNEL();
//    rc = P1_LockCreate("PagerLock", &pagerLID);				assert(rc == 0);
//    rc = P1_CondCreate("PagerCond", pagerLID, &pagerVID);		assert(rc == 0);
    rc = P1_Lock(pagerLID);						assert(rc == 0);
    USLOSS_Console("2\n");
    while(NotShutDown){
	frame = 0;
	rc = P1_Lock(faultLID);						assert(rc == 0);
	while(terminalNotCalled){
		rc = P1_Wait(faultVID);					assert(rc == 0);
	}
	rc = P1_Unlock(faultLID);					assert(rc == 0);
	if(pidTableCheck[P1_GetPid()] == 0)
		USLOSS_Abort("Table Not here\n");

	P3_vmStats.faults += 1;
	P3_vmStats.newPages += 1;

	int pid = faultQueuePID[0];
	int page = faultQueue[0];
	popQueue();
	USLOSS_PTE *currTable;
	rc = P3PageTableGet(pid, &currTable);				assert(rc == 0);

	rc = P3PageFaultResolve(pid, page, &frame);
	//USLOSS_Console("RC: %d\n",rc);
	if(rc == P3_OUT_OF_SWAP){
		terminationQueue[faultIndex - 1] = 1;
	}else{
		if(rc == P3_NOT_IMPLEMENTED){
			frame = page;
		}
		pteTable[pid][page].frame = frame;
		pteTable[pid][page].incore = 1;
	}
	rc = P1_Lock(faultLID);						assert(rc == 0);
	faultNotHandled = 0;
	rc = P1_Signal(pagerVID);					assert(rc == 0);
	terminalNotCalled = 1;
	rc = P1_Unlock(faultLID);					assert(rc == 0);
    }
    /*******************

    loop until P3_VmShutdown is called
        wait for a fault
        if the process does not have a page table
            call USLOSS_Abort with an error message
        rc = P3PageFaultResolve(pid, page, &frame)
        if rc == P3_OUT_OF_SWAP
            mark the faulting process for termination
        else
            if rc == P3_NOT_IMPLEMENTED
                frame = page
            update PTE in page table to map page to frame
       unblock faulting process

    *********************/
    return 0;
}

void Zero_P3_VmStats(){
    P3_vmStats.pages = 0;
    P3_vmStats.frames = 0;
    P3_vmStats.blocks = 0;
    P3_vmStats.freeFrames = 0;
    P3_vmStats.freeBlocks = 0;
    P3_vmStats.faults = 0;
    P3_vmStats.newPages = 0;
    P3_vmStats.pageIns = 0;
    P3_vmStats.pageOuts = 0;
    P3_vmStats.replaced = 0;
}

int
P3_VmInit(int unused, int pages, int frames, int pagers)
{
    int rc;
    CHECKKERNEL();
    if(pages < 1)
	return P3_INVALID_NUM_PAGES;
    if(frames < 1)
	return P3_INVALID_NUM_FRAMES;
    if(pagers != P3_MAX_PAGERS)
	return P3_INVALID_NUM_PAGERS;

    if(!vmInitCalled){
    	vmInitCalled = 1;
    	USLOSS_Console("3\n");
    	rc = USLOSS_MmuInit(0, pages, frames, USLOSS_MMU_MODE_PAGETABLE);
	for(int i = 0; i < P1_MAXPROC; i++){
		pteTable[i] = NULL;
	}
    	// zero P3_vmStats
    	Zero_P3_VmStats();
    	// initialize fault queue, lock, and condition variable
   	// faultQueue initialized
    	rc = P1_LockCreate("FaultLock", &faultLID);			assert(rc == 0);
    	rc = P1_CondCreate("FaultCond", faultLID, &faultVID);		assert(rc == 0);
    	rc = P1_CondCreate("PagerCond", faultLID, &pagerVID);       	assert(rc == 0);

    	// call P3FrameInit
    	rc = P3FrameInit(pages, frames);				assert(rc == 0);
    	// call P3SwapInit
    	rc = P3SwapInit(pages,frames);				assert(rc == 0);
    	// fork pager
	USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler;
        rc = P1_Fork("Pager", Pager, (void*) P3_MAX_PAGERS, USLOSS_MIN_STACK * 4, P3_PAGER_PRIORITY, &pagerPID);		assert(rc == 0);

	return P1_SUCCESS;
    }
    return P3_ALREADY_INITIALIZED;
}

void
P3_VmShutdown(void)
{
    CHECKKERNEL();
    USLOSS_Console("4\n");
    // cause pager to quit
    NotShutDown = 0;
    P3_PrintStats(&P3_vmStats);
}

USLOSS_PTE *
P3_AllocatePageTable(int pid)
{
    //int rc = -1;
    CHECKKERNEL();
    if(vmInitCalled || (0 <= pid && pid <= P1_MAXPROC)){
    	USLOSS_Console("5\n");
        // create a new page table here
    	USLOSS_PTE  *table = malloc(sizeof(struct USLOSS_PTE) * P3_vmStats.pages);
	for(int i = 0; i < P3_vmStats.pages; i++){
		table[i].read = 1;
		table[i].write = 1;
		table[i].frame = 1;
		table[i].incore = 0;
	}
	pidTableCheck[pid] = 1;
    	pteTable[pid] = table;
    	return table;
    }
    return NULL;
}

void
P3_FreePageTable(int pid)
{
    int rc = -1;
    CHECKKERNEL();
    if(vmInitCalled && pidTableCheck[pid] == 1){
    	USLOSS_Console("6\n");
    	// free the page table here
    	rc = P3FrameFreeAll(pid);
    	rc = P3SwapFreeAll(pid);
    }
}

int
P3PageTableGet(PID pid, USLOSS_PTE **table)
{
    USLOSS_Console("7\n");
    CHECKKERNEL();
    if(!(0 <= pid && pid < P1_MAXPROC)){
	return P1_INVALID_PID;
    }
    if(pidTableCheck[pid] == 1){
	*table = pteTable[pid];
    }else{
    	*table = NULL;
    }
    return P1_SUCCESS;
}

int P3_Startup(void *arg)
{
    int pid;
    int pid4;
    int status;
    int rc;

    //pteTable = malloc(sizeof(struct USLOSS_PTE) * 100);
    rc = Sys_Spawn("P4_Startup", P4_Startup, NULL,  3 * USLOSS_MIN_STACK, 2, &pid4);
    assert(rc == 0);
    assert(pid4 >= 0);
    rc = Sys_Wait(&pid, &status);
    assert(rc == 0);
    assert(pid == pid4);
    Sys_VmShutdown();
    return 0;
}

void
P3_PrintStats(P3_VmStats *stats)
{
    USLOSS_Console("P3_PrintStats:\n");
    USLOSS_Console("\tpages:\t\t%d\n", stats->pages);
    USLOSS_Console("\tframes:\t\t%d\n", stats->frames);
    USLOSS_Console("\tblocks:\t\t%d\n", stats->blocks);
    USLOSS_Console("\tfreeFrames:\t%d\n", stats->freeFrames);
    USLOSS_Console("\tfreeBlocks:\t%d\n", stats->freeBlocks);
    USLOSS_Console("\tfaults:\t\t%d\n", stats->faults);
    USLOSS_Console("\tnewPages:\t%d\n", stats->newPages);
    USLOSS_Console("\tpageIns:\t%d\n", stats->pageIns);
    USLOSS_Console("\tpageOuts:\t%d\n", stats->pageOuts);
    USLOSS_Console("\treplaced:\t%d\n", stats->replaced);
}

