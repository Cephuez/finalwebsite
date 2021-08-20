/*
 * Name(NetID): Saul Sonido Perez(cephuez)
 * Phase: 1a
 */

#include "phase1Int.h"
#include "usloss.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

extern  USLOSS_PTE  *P3_AllocatePageTable(int cid);
extern  void        P3_FreePageTable(int cid);

typedef struct Context {
    void            (*startFunc)(void *);//function called by the context
    void            *startArg;//arguments to the context's function
    USLOSS_Context  context;
    void	    *stack;//pointer to the stack
    int		    stackSize;//max number of elements the stack can hold
    int             active;//tells us whether this is an active context
} Context;

static Context   contexts[P1_MAXPROC];

static int currentCid = -1;//holds the index in contexts of the context currently in use

/*-----------------------------------------------------------------------------
|  Method launch
|
|  Purpose: Helper function to call func passed to P1ContextCreate with its arg.
|
|  Pre-condition: There must be a context in use
|
|  Post-condition: The function func will be called with the arguments arg.
|
|  Parameters:
|	None
|
|  Returns: None
*----------------------------------------------------------------------------*/
static void launch(void)
{
    assert(contexts[currentCid].startFunc != NULL);
    contexts[currentCid].startFunc(contexts[currentCid].startArg);
}

/*-----------------------------------------------------------------------------
|  Method checkForKernelMode
|
|  Purpose: Helper function that makes sure that function is being called in 
|	kernel mode. If kernel mode is not enabled then call 
|	USLOSS_IllegalInstruction and USLOSS_Halt to end the program.
|
|  Pre-condition: None
|
|  Post-condition: The program will continue running only if we are in kernel mode
|
|  Parameters:
|	None
|
|  Returns: None
*----------------------------------------------------------------------------*/
static void checkForKernelMode(void){
    unsigned int currPSR = USLOSS_PsrGet();//get the current PSR
    if((currPSR & 0x1) == 0){//if current mode is unpriviliged throw error and exit
	USLOSS_IllegalInstruction();
	USLOSS_Halt(0);
    }
}

/*-----------------------------------------------------------------------------
|  Method P1ContextInit
|
|  Purpose: This function is always called first and initializes the value of every
|	active field of the context structs stored in the contexts arrays. This
|	indicates that all the processes are currently free.
|
|  Pre-condition: The CPU must be in kernel mode.
|
|  Post-condition: All Context structs will be initialized
|
|  Parameters:
|	None
|
|  Returns: None
*----------------------------------------------------------------------------*/
void P1ContextInit(void)
{
    checkForKernelMode();//this function can only be called in kernel mode
    int i;
    for(i = 0; i < P1_MAXPROC; i++){//initialize active variable of each Context in contexts
	contexts[i].active = 0;//all contexts not in use
    }
}

/*-----------------------------------------------------------------------------
|  Method P1ContextCreate
|
|  Purpose: Creates a new context and adds it to the array of contexts based on
|	the arguments that are passed int.
|
|  Pre-condition: The function P1ContextInit must already have been called and
|	the CPU must be in kernel mode.
|
|  Post-condition: A new Context will be created based on the parameters passed.
|
|  Parameters:
|	func		-- a function pointer that will be run when we switch to this
|		new context.
|	arg		-- void pointer to the arguments that func takes
|	stacksize	-- the size of the stack in bytes
|	cid		-- pointer to place the cid(or index in the array contexts)
|		of the new context
|
|  Returns: This function returns P1_TOO_MANY_CONTEXTS if there are no free
|	processes, P1_INVALID_STACK if the stack size is too small, and
|	P1_SUCCESS if a new context was successfully initialized.
*----------------------------------------------------------------------------*/
int P1ContextCreate(void (*func)(void *), void *arg, int stacksize, int *cid) {
    checkForKernelMode();//this function can only be called in kernel mode

    int result = P1_SUCCESS;
    
    int index = 0;//index in the contexts array
    int curr = contexts[index].active;//the active value of the current Context
    while(curr != 0 && index < P1_MAXPROC){//search for !active Context in contexts
        index++;
        curr = contexts[index].active;
    }

    if(index == P1_MAXPROC){//there are no free contexts
        result = P1_TOO_MANY_CONTEXTS;
    }else if(stacksize < USLOSS_MIN_STACK){//the stacksize is too small
        result = P1_INVALID_STACK;
    }else{//this is a valid context, initialize it
        *cid = index;//the cid is the location of the new context in contexts
        USLOSS_Context newContext;
        void *newStack = malloc(stacksize);//the stack for the new context
	USLOSS_PTE *newPageTable = P3_AllocatePageTable(index);//get page table we get from Phase 3

	USLOSS_ContextInit(&newContext, newStack, stacksize, newPageTable, launch);

	//initialize the Context struct so we can access this context again
    	Context newStruct;
    	newStruct.startFunc = func;
    	newStruct.startArg = arg;
    	newStruct.context = newContext;
        newStruct.stack = newStack;
        newStruct.stackSize = stacksize;
	newStruct.active = 1;

	contexts[index] = newStruct;//add to our array of contexts
    }

    return result;
}

/*-----------------------------------------------------------------------------
|  Method P1ContextSwitch
|
|  Purpose: Switches from the current context to a new valid context.
|
|  Pre-condition: The function P1ContextInit must already have been called, CPU 
|	must be in kernel mode, and a valid cid must be passed in.
|
|  Post-condition: We will switch to a new context.
|
|  Parameters:
|	cid -- the index in the array contexts of the context we wish to switch to.
|
|  Returns: Returns P1_INVALID_ID the cid is out of range(less than zero or
|	greater than P1_MAXPROC) or is an uninitialized process and returns
|	P1_SUCCESS if the cid is valid and the context has been switched.
*----------------------------------------------------------------------------*/
int P1ContextSwitch(int cid) {
    checkForKernelMode();//this function can only be called in kernel mode

    int result = P1_SUCCESS;

    USLOSS_Context oldContext;//there is no context to switch from
    if(currentCid != -1){//if there is a context to switch from
	oldContext = contexts[currentCid].context;	
    }
    USLOSS_Context newContext = contexts[cid].context;
    
    if(cid < 0 || cid > P1_MAXPROC || contexts[cid].active == 0){//make sure cid is valid
	result = P1_INVALID_CID;//not a valid cid, do not switch
    }else{//switch to new context
	currentCid = cid;
	USLOSS_ContextSwitch(&oldContext, &newContext);
    }

    return result;
}

/*-----------------------------------------------------------------------------
|  Method P1ContextFree
|
|  Purpose: Frees the context that corresponds to the cid if the context is not
|	the current context and is already initialized.
|
|  Pre-condition: The function P1ContextInit must already have been called, CPU 
|	must be in kernel mode, and a valid cid must be passed in.
|
|  Post-condition: We will free the context indicated by the cid.
|
|  Parameters:
|	cid -- the index in the array contexts of the context we wish to free.
|
|  Returns: Returns P1_INVALID_ID the cid is out of range(less than zero or
|	greater than P1_MAXPROC) or is an uninitialized process, returns
|	P1_CONTEXT_IN_USE if the conext we are trying to free is the currently
|	running process, and returns P1_SUCCESS if the cid is valid and the
|	context has been freed.
*----------------------------------------------------------------------------*/
int P1ContextFree(int cid) {
    checkForKernelMode();//this function can only be called in kernel mode

    int result = P1_SUCCESS;

    // free the stack and mark the context as unused
    if(contexts[cid].active == 0 || cid < 0 || cid > P1_MAXPROC){//invalid cid
	result = P1_INVALID_CID;
    }else if(cid == currentCid){//the context we are trying to free is the current one
	result = P1_CONTEXT_IN_USE;
    }else{//the cid is valid and we can free the context
    	P3_FreePageTable(cid);//free the page table used by the context
        free(contexts[cid].stack);//free the memory the stack was using
    	contexts[cid].active = 0;//mark the context as unused
    }

    return result;
}

/*-----------------------------------------------------------------------------
|  Method P1EnableInterrupts
|
|  Purpose: Modifies the PSR value to enable interrupts.
|
|  Pre-condition: The function P1ContextInit must already have been called and
|	the CPU must be in kernel mode.
|
|  Post-condition: The interrupts bit of the PSR will be set to one.
|
|  Parameters:
|	None
|
|  Returns: None
*----------------------------------------------------------------------------*/
void 
P1EnableInterrupts(void) 
{
    checkForKernelMode();//this function can only be called in kernel mode

    unsigned int currPSR = USLOSS_PsrGet();//get the current PSR
    currPSR = currPSR | 0x2;//set 2nd bit to one in order to enable interrupts
    int result = USLOSS_PsrSet(currPSR);//update the PSR
    if(result == USLOSS_ERR_INVALID_PSR){
	USLOSS_Console("ERROR: INVALID PSR, COULD NOT ENABLE INTERRUPTS.");
	USLOSS_Halt(0);
    }
}

/*-----------------------------------------------------------------------------
|  Method P1DisableInterrupts
|
|  Purpose: Modifies the PSR value to disable interrupts.
|
|  Pre-condition: The function P1ContextInit must already have been called and
|	the CPU must be in kernel mode.
|
|  Post-condition: The interrupts bit of the PSR will be set to zero.
|
|  Parameters:
|	None
|
|  Returns: Returns TRUE if interrupts were previously enabled and FALSE if
|	they were not.
*----------------------------------------------------------------------------*/
int 
P1DisableInterrupts(void) 
{
    checkForKernelMode();//this function can only be called in kernel mode

    int enabled = TRUE;
    unsigned int currPSR = USLOSS_PsrGet();//get the current PSR
    unsigned int interrupt = (currPSR >> 1) & 0x1;//just look at the 2nd bit of the PSR
    if(interrupt == 1){//interrupts were enabled we need to modify the psr
        unsigned int psrMask = ~(0x1 << 1);
        currPSR = currPSR & psrMask;//set 2nd bit to zero in order to disable interrupts
    	int result = USLOSS_PsrSet(currPSR);//update the PSR
	if(result == USLOSS_ERR_INVALID_PSR){
	    USLOSS_Console("ERROR: INVALID PSR, COULD NOT DISABLE INTERRUPTS.");
	    USLOSS_Halt(0);
	}
    }else{//interrupts were already disabled we don't need to change PSR
        enabled = FALSE;
    }
    return enabled;
}
