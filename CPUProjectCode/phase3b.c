/*
 * phase3b.c
 *
 * 	Author: Saul Sonido Perez
 *
 */

#include <assert.h>
#include <phase1.h>
#include <phase2.h>
#include <usloss.h>
#include <string.h>
#include <libuser.h>

#include "phase3.h"
#include "phase3Int.h"

#ifdef DEBUG
int debugging3 = 1;
#else
int debugging3 = 0;
#endif

#define CHECKKERNEL() \
    if ((USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE) == 0) USLOSS_IllegalInstruction()

// Keeps track of the frames's info in an array based on number of frames
struct FrameArray {
    int pid;
    int page;
    int free;
}FrameArray;

struct FrameArray *frameArray;

static int numFrames;
static int numPages;
static int p3FrameInitialized;
void debug3(char *fmt, ...)
{
    USLOSS_Console("1\n");
    va_list ap;

    if (debugging3) {
        va_start(ap, fmt);
        USLOSS_VConsole(fmt, ap);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * P3FrameInit --
 *
 *  Initializes the frame data structures.
 *
 * Results:
 *   P3_ALREADY_INITIALIZED:    this function has already been called
 *   P1_SUCCESS:                success
 *
 *----------------------------------------------------------------------
 */
int
P3FrameInit(int pages, int frames)
{
    int result = P1_SUCCESS;

    CHECKKERNEL();
    if(p3FrameInitialized){
	return P3_ALREADY_INITIALIZED;
    }

    p3FrameInitialized = 1;

    // initialize the frame data structures, e.g. the pool of free frames
    frameArray = malloc(sizeof(struct FrameArray) * frames);
    for(int i = 0; i < frames; i++){
	frameArray[i].pid = -1; // There is not PID assigned
	frameArray[i].page = 0; // Page is set to 0
	frameArray[i].free = 1;	// Frame is Free
    }

    // Record down the number of pages and frames
    numPages = pages;
    numFrames = frames;

    // set P3_vmStats.freeFrames
    P3_vmStats.freeFrames = frames;

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * P3FrameFreeAll --
 *
 *  Frees all frames used by a process
 *
 * Results:
 *   P3_NOT_INITIALIZED:    P3FrameInit has not been called
 *   P1_INVALID_PID:        pid is invalid
 *   P1_SUCCESS:            success
 *
 *----------------------------------------------------------------------
 */

int
P3FrameFreeAll(int pid)
{
    CHECKKERNEL();
    int result = P1_SUCCESS;

    // Check for any invalid values
    if(!p3FrameInitialized){
	return P3_NOT_INITIALIZED;
    }

    if(!(0 <= pid && pid < P1_MAXPROC)){
	return P1_INVALID_PID;
    }


    // Get table from phase 2a
    USLOSS_PTE* pteTable;
    int rc = P3PageTableGet(pid, &pteTable);

    // Check if the pid is not in the table
    if(rc == P1_INVALID_PID){
	return rc;
    }

    for(int i = 0; i < numFrames; i++){
	if(pteTable[i].incore == 1){
		pteTable[i].incore = 0;
		frameArray[pteTable[i].frame].free = 1;
		P3_vmStats.freeFrames += 1;
		P3_vmStats.newPages -= 1;
	}
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * P3PageFaultResolve --
 *
 *  Frees all frames used by a process
 *
 * Results:
 *   P3_NOT_INITIALIZED:    P3FrameInit has not been 
 *   P3_NOT_IMPLEMENTED:    this function has not been implemented
 *   P1_INVALID_PID:        pid is invalid
 *   P1_INVALID_PAGE:       page is invalid
 *   P3_OUT_OF_SWAP:        there is no more swap space
 *   P1_SUCCESS:            success
 *
 *----------------------------------------------------------------------
 */
int
P3PageFaultResolve(int pid, int page, int *frame)
{
    CHECKKERNEL();

    int rc;
    int free;
    int pageSize;
    void* pmAddr;

    // Check for any invalid inputs
    if(!p3FrameInitialized)
	return P3_NOT_INITIALIZED;;

    if(!(0 <= pid && pid < P1_MAXPROC)){
        return P1_INVALID_PID;
    }

    if(!(0 <= page && page < numPages))
	return P3_INVALID_PAGE;

    P3_vmStats.newPages += 1;

    // Put in the values for the first free frame index
    for(int i = 0; i < numFrames; i++){
	if(frameArray[i].free == 1){
		free = 1;
		frameArray[i].pid = pid;
		frameArray[i].page = page;
		frameArray[i].free = 0;
		P3_vmStats.freeFrames -= 1;

		*frame = i;

		rc = USLOSS_MmuGetConfig(NULL, &pmAddr, &pageSize, NULL, NULL, NULL);		//assert(rc == 0);
		memset(pmAddr + pageSize * *frame, 0, pageSize);
		break;
	}
    }

    // Swap out an ocuppied frame
    if(!free){
	rc = P3SwapOut(frame);
	if(rc == P3_OUT_OF_SWAP)
		return rc;
    }

    // Swap in a new page
    rc = P3SwapIn(pid, page, *frame);

    // Reset values to zero, but frame is still going to be occupied
    if(rc == P3_PAGE_NOT_FOUND){
	frameArray[*frame].pid = 0;
	frameArray[*frame].page = 0;
    }

    return P1_SUCCESS;
}

