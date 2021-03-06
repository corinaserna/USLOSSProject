/* ------------------------------------------------------------------------
 phase1.c
 
 University of Arizona
 Computer Science 452
 
 ------------------------------------------------------------------------ */

#include "phase1.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "kernel.h"

/* ------------------------- Prototypes ----------------------------------- */
void illegalInstructionHandler(int dev, void *arg);

int sentinel (char *);
void dispatcher(void);
void launch();
static void checkDeadlock();
int isZapped();
void cleanupChild(int);

/* -------------------------- Globals ------------------------------------- */

// Patrick's debugging global variable...
int debugflag = 1;

// Number of entries in the process table
static int numProcEntries;

// the process table
procStruct ProcTable[MAXPROC];

// union reference
// static psrValues un;

// Process lists
//static procPtr ReadyList;

// current process ID
procPtr Current;

// the next pid to be assigned
unsigned int nextPid = SENTINELPID;

// empty struct to use for cleanup
static const struct procStruct EmptyStruct;

static procPtr ReadyList[1000];
int ReadyListStackLoc = 0;
/* -------------------------- Functions ----------------------------------- */
// puts the highest priority at the top
static int priorityCompare(const void * a, const void * b)
{
    
    const procPtr a1 = *(const procPtr *)a;
    const procPtr b1 = *(const procPtr *)b;
    
    return  (a1->priority - b1->priority);
}

void pushReadyList(procPtr newItem)
{
    ReadyList[ReadyListStackLoc++] = newItem;
    
    // now sort based on prioirty (higher priority higher)
    if (ReadyListStackLoc > 1 )
    {
        qsort(ReadyList, ReadyListStackLoc, sizeof (procPtr), priorityCompare);
    }
}

procPtr popReadyList()
{
    if (ReadyListStackLoc > 0)
        ReadyListStackLoc--; // always leave sentienel on stack (ready list)
    
    return ReadyList[0];
}
/* ------------------------------------------------------------------------
 Name - startup
 Purpose - Initializes process lists and clock interrupt vector.
 Start up sentinel process and the test process.
 Parameters - argc and argv passed in by USLOSS
 Returns - nothing
 Side Effects - lots, starts the whole thing
 ----------------------------------------------------------------------- */
void startup(int argc, char *argv[])
{
    int result; /* value returned by call to fork1() */
    
    /* initialize the process table */
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing process table, ProcTable[]\n");
    memset(ProcTable, 0, sizeof(ProcTable));
    
    //Initialize the current variable
    Current = NULL;
    
    // Initialize the Ready list, etc.
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): initializing the Ready list\n");
    //    ReadyList = NULL;
    
    // Initialize the illegalInstruction interrupt handler
    USLOSS_IntVec[USLOSS_ILLEGAL_INT] = illegalInstructionHandler;
    
    // Initialize the clock interrupt handler
    //USLOSS_IntVec[USLOSS_CLOCK_DEV] = clockHandler;
    
    // startup a sentinel process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for sentinel\n");
    result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK,
                   SENTINELPRIORITY);
    if (result < 0) {
        if (DEBUG && debugflag) {
            USLOSS_Console("startup(): fork1 of sentinel returned error, ");
            USLOSS_Console("halting...\n");
        }
        USLOSS_Halt(1);
    }
    
    // start the test process
    if (DEBUG && debugflag)
        USLOSS_Console("startup(): calling fork1() for sentinel %d\n", result);
    
    result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
    if (result < 0) {
        USLOSS_Console("startup(): fork1 for start1 returned an error, ");
        USLOSS_Console("halting...\n");
        USLOSS_Halt(1);
    }
    USLOSS_Console("startup() done fork1 for start1 %d\n", result);
    
    ProcTable[0].nextSiblingPtr = &ProcTable[1];    // setup sentinel's sibling
    dispatcher();
    
    USLOSS_Console("startup(): Should not see this message! ");
    USLOSS_Console("Returned from fork1 call that created start1\n");
    
    return;
} /* startup */

/* ------------------------------------------------------------------------
 Name - finish
 Purpose - Required by USLOSS
 Parameters - none
 Returns - nothing
 Side Effects - none
 ----------------------------------------------------------------------- */
void finish(int argc, char *argv[])
{
    if (DEBUG && debugflag)
        USLOSS_Console("in finish...\n");
} /* finish */

/* ------------------------------------------------------------------------
 Name       - firstAvailableProcSlot
 Purpose    - Returns the first available entry in ProcTable
 Returns    - -1 if full (nothing available)
 >= 0   entry inProcTable to use
 
 ------------------------------------------------------------------------ */
int firstAvailableProcSlot()
{
    if (numProcEntries >= MAXPROC-1) {
        return -1;
    }
    int firstAvaialble = 0;
    for (firstAvaialble = 0; firstAvaialble < MAXPROC; firstAvaialble++)
    {
        if (ProcTable[firstAvaialble].stackSize == 0) { // assume stacksize is 0 if unused
            ++numProcEntries;
            return firstAvaialble;
        }
    }
    return -1;
}

void dumpSlot(int tableSlot)
{
    USLOSS_Console("fork1(): -- (%d) Table for PID = %d\n", tableSlot, ProcTable[tableSlot].pid);
    USLOSS_Console("            name      [%s]\n", ProcTable[tableSlot].name);
    USLOSS_Console("            startArg  [%s]\n", ProcTable[tableSlot].startArg);
    USLOSS_Console("            startFunc [%x]\n", ProcTable[tableSlot].startFunc);
    USLOSS_Console("            stackSize [%d]\n", ProcTable[tableSlot].stackSize);
    USLOSS_Console("            stack     [%x]\n", ProcTable[tableSlot].stack);
    USLOSS_Console("            priority  [%d]\n", ProcTable[tableSlot].priority);
    USLOSS_Console("            status    [%d]\n", ProcTable[tableSlot].status);
    USLOSS_Console("            nextSiblingPtr [%x]\n", ProcTable[tableSlot].nextSiblingPtr);
    USLOSS_Console("            nextProcPtr    [%x]\n", ProcTable[tableSlot].nextProcPtr);
}

/* ------------------------------------------------------------------------
 Name - fork1
 Purpose - Gets a new process from the process table and initializes
 information of the process.  Updates information in the
 parent process to reflect this child process creation.
 Parameters - the process procedure address, the size of the stack and
 the priority to be assigned to the child process.
 Returns - the process id of the created child or -1 if no child could
 be created or if priority is not between max and min priority.
 Side Effects - ReadyList is changed, ProcTable is changed, Current
 process information changed
 ------------------------------------------------------------------------ */
int fork1(char *name, int (*startFunc)(char *), char *arg,
          int stackSize, int priority)
{
    if (DEBUG && debugflag)
        USLOSS_Console("fork1(): creating process %s\n", name);
    
    //test if in kernel mode; halt if in user mode
    if(!(USLOSS_PSR_CURRENT_MODE & USLOSS_PsrGet()))
    {
        //throw some sort of error because it was not in kernel mode
        USLOSS_Console("fork1(): Is in user mode.  Halting...\n");
      	 USLOSS_Halt(1);
    }
    
    // Return if stack size is too small
    if (stackSize < USLOSS_MIN_STACK) {
        USLOSS_Console("fork1(): stacksize < min %d\n", stackSize);
        return -1;
    }
    
    // check for min/max priroity
    if(strcmp(name, "sentinel") == 0){
        if(priority != 6){
            USLOSS_Console("fork1(): sentinel priority = %d and it must be 6\n", priority);
            return -1;
        }
    }
    else if (priority > MINPRIORITY) {
        USLOSS_Console("fork1(): priority %d < min %d\n", priority, MINPRIORITY);
        return -1;
    }
    else if (priority < MAXPRIORITY) {
        USLOSS_Console("fork1(): priority %d > max %d\n", priority, MAXPRIORITY);
        return -1;
    }
    
    // Is there room in the process table? What is the next PID?
    int procSlot = firstAvailableProcSlot();
    
    if (procSlot == -1) {
        USLOSS_Console("fork1(): ProcTable full\n");
        return -1;
    }
    procPtr thisProc = &ProcTable[procSlot];
   
    // fill-in entry in process table */
    if ( strlen(name) >= (MAXNAME - 1) ) {
        USLOSS_Console("fork1(): Process name is too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    strcpy(thisProc->name, name);
    thisProc->startFunc = startFunc;
    if ( arg == NULL )
        thisProc->startArg[0] = '\0';
    else if ( strlen(arg) >= (MAXARG - 1) ) {
        USLOSS_Console("fork1(): argument too long.  Halting...\n");
        USLOSS_Halt(1);
    }
    else
        strcpy(thisProc->startArg, arg);
    
    thisProc->stackSize   = stackSize;
    thisProc->stack       = calloc(stackSize, 1);   // create stack and init to 0
    if (thisProc->stack == NULL)  { // memory issue
        USLOSS_Console("fork1(): Couldn't get memory for stack (size = %d).  Halting...\n", stackSize);
        USLOSS_Halt(1);
    }
    
    thisProc->priority    = priority;
    thisProc->pid         = nextPid;
    nextPid++;
    
    // Initialize context for this process, but use launch function pointer for
    // the initial value of the process's program counter (PC)
    
    USLOSS_ContextInit(&(thisProc->state),
                       thisProc->stack,
                       thisProc->stackSize,
                       NULL,
                       launch);
    
    // for future phase(s)
    p1_fork(thisProc->pid);
    
    // More stuff to do here...call dispatcher
    
    dumpSlot(procSlot);
    
    // setup current process
    
    thisProc->status          = ePROC_READY;
    //** figure out nextSiblingPtr, nextProcPtr
    
    if(Current == NULL){
        thisProc->parentProcPtr = NULL;
    }
    else{
        thisProc->parentProcPtr = Current;
        if(Current->childProcPtr == NULL){
            Current->childProcPtr = thisProc;
        }
        else{
            thisProc->nextSiblingPtr    = Current->childProcPtr;
            Current->childProcPtr       = thisProc;
            // finally setting that to address of process at procSlot
        }
    }
    if (Current) {
        if (Current->childProcPtr) {    // if already has a child, link it in
            thisProc->nextSiblingPtr = Current->childProcPtr;
        }
        Current->childProcPtr = thisProc;
    }
    //  ReadyList;
    pushReadyList(thisProc);
    
       return thisProc->pid;
} /* fork1 */

/* ------------------------------------------------------------------------
 Name - launch
 Purpose - Dummy function to enable interrupts and launch a given process
 upon startup.
 Parameters - none
 Returns - nothing
 Side Effects - enable interrupts
 ------------------------------------------------------------------------ */
void launch()
{
    int result;
    
    if (DEBUG && debugflag)
        USLOSS_Console("launch(): started\n");
    
    // Enable interrupts
    
    // Call the function passed to fork1, and capture its return value
    Current->status = ePROC_RUNNING;
    result = Current->startFunc(Current->startArg);
    
    if (DEBUG && debugflag)
        USLOSS_Console("Process %d returned to launch with result: %d\n", Current->pid ,result);
    
    quit(result);
    
} /* launch */

// Compact readyList, decrement
void removeFromReadyList(procPtr processToBeRemoved, int startIndex)
{
    int i;
    for (i = startIndex; i < ReadyListStackLoc-1; i++) {
        ReadyList[i] = ReadyList[i+1];
    }
    ReadyList[i] = NULL;
    ReadyListStackLoc--;
}

//
// Find this process in the readyList, remove it and mark it as blocked
//
void blockReadyProcess(procPtr processToBeBlocked)
{
    for (int i = 0; i < ReadyListStackLoc; i++)
        if (ReadyList[i] == processToBeBlocked)
        {
            processToBeBlocked->status = ePROC_BLOCKED;
            removeFromReadyList(processToBeBlocked, i);
            break;
        }
}

/* ------------------------------------------------------------------------
 Name - join
 Purpose - Wait for a child process (if one has been forked) to quit.  If
 one has already quit, don't wait.
 Parameters - a pointer to an int where the termination code of the
 quitting process is to be stored.
 Returns - the process id of the quitting child joined on.
 -1 if the process was zapped in the join
 -2 if the process has no children
 Side Effects - If no child process has quit before join is called, the
 parent is removed from the ready list and blocked.
 ------------------------------------------------------------------------ */
int join(int *status)
{
    USLOSS_Console("join(): join %d\n", Current->pid);
    if(Current->childProcPtr != NULL)
    {
        if(Current->childProcPtr->status != ePROC_QUIT){
            USLOSS_Console("join(): waiting for child  %d\n", Current->childProcPtr->pid);
            // If no child process has quit before join is called, the
            // parent is removed from the ready list and blocked.
            blockReadyProcess(Current);
        
        // ** Current has been popped off the ready list, so no further action needs to happen here?
        dumpProcesses();
        dispatcher();
        dumpProcesses();
        // clean up child
        
        Current->status = Current->childQuitStatus;
        short returnPID = Current->childQuitPID;
        cleanupChild(Current->childQuitPID);
        return returnPID;
       }
       else if(Current->childHasQuit == 1){
           Current->status = Current->childQuitStatus;
           short returnPID = Current->childQuitPID;
           cleanupChild(Current->childQuitPID);
           return returnPID;
       }
    }
    else if(isZapped()){
        return -1;
    }
    else{
        return -2;
    }
} /* join */

void cleanupChild(int pid){
    int i;
    for (i = 0; i < MAXPROC; i++) {
        if(ProcTable[i].pid == pid){
            break;
        }
    }
    ProcTable[i] = EmptyStruct;
}


/* ------------------------------------------------------------------------
 Name - quit
 Purpose - Stops the child process and notifies the parent of the death by
 putting child quit info on the parents child completion code
 list.
 Parameters - the code to return to the grieving parent
 Returns - nothing
 Side Effects - changes the parent of pid child completion status list.
 ------------------------------------------------------------------------ */
void quit(int status)
{
    if(Current->childProcPtr != NULL){
        // has no children, print error message, call halt(1)
        USLOSS_Console("quit(): process has no child in quit/n");
        USLOSS_Halt(1);
    }
    
    Current->status = ePROC_QUIT;
    Current->parentProcPtr->childHasQuit = 1;
    procPtr parent = Current->parentProcPtr;
    parent->childQuitStatus = status; // return pid of this process AND the status argument from called
    parent->childQuitPID = Current->pid;
    
    if(parent != NULL) // could be NULL if sentinel or "start1" processes
    {
        // removing myself from my parent's children list
        procPtr firstChild = parent->childProcPtr;
        procPtr prevChild = NULL;
        if(firstChild->pid == Current->pid){
                procPtr newNextSibling = firstChild->nextSiblingPtr;
                parent->childProcPtr = firstChild->nextSiblingPtr;
                parent->nextSiblingPtr = newNextSibling->nextSiblingPtr;
        }
        else{
            while(1){
                if(firstChild->pid == Current->pid){
                    prevChild->nextSiblingPtr = firstChild->nextSiblingPtr;
                    break;
                }
                prevChild = firstChild;
                firstChild = firstChild->nextSiblingPtr;
            }
        }
        
        //
        
        
        if(parent->status == ePROC_BLOCKED){
            parent->status = ePROC_READY;
            /*if(parent->quitChild == NULL){ // put child on parent's quit children list
                parent->quitChild = (Current);
                parent->quitSibling = NULL;
            }
            else{
                Current->quitSibling = parent->quitChild;
                parent->quitChild = (Current);
            }*/
            pushReadyList(parent);
        }
        else{ // parent has not yet done a join
            /*if(parent->quitChild == NULL){ // put child on parent's quit children list
                parent->quitChild = (Current);
                parent->quitSibling = NULL;
            }
            else{
                Current->quitSibling = parent->quitChild;
                parent->quitChild = (Current);
            }*/
            dispatcher();
        }
    }
    else{
        if(ProcTable[0].status != ePROC_QUIT){ // if sentinel process is in quit
            // cleaning up process table
            for(int i; i < MAXPROC; i++){
                ProcTable[i] = EmptyStruct;
            }
        }
        else{
            USLOSS_Console("quit(): sentinel is not quitting");
            USLOSS_Console("halting...\n");
            USLOSS_Halt(1);
        }
    }
    p1_quit(Current->pid);
} /* quit */

int isZapped(){
    if(Current->zapped == 1){
        return 1;
    }
    else{
        return 0;
    }
}

int zap (int pid){
    if(Current->pid == pid){
        USLOSS_Console("zap(): trying to zap itself");
        USLOSS_Halt(1);
    }
    
    for(int i = 0; i < MAXPROC; i++){
        if(ProcTable[i].pid == pid){
            ProcTable[i].zapped == 1;
            if(Current->zapped == 1){
                return -1; // process was zapped while in zap
            }
            dispatcher();
            return 0;
        }
    }
    
    //process did not exist
    //print error message and halt
    USLOSS_Console("zap(): process does not exit");
    USLOSS_Halt(1);
    
}

void illegalInstructionHandler(int dev, void *arg)
{
    if (DEBUG && debugflag)
        USLOSS_Console("illegalInstructionHandler() called\n");
} /* illegalInstructionHandler */

void clockHandler(int dev, void *arg){
    
}
/* ------------------------------------------------------------------------
 Name - dispatcher
 Purpose - dispatches ready processes.  The process with the highest
 priority (the first on the ready list) is scheduled to
 run.  The old process is swapped out and the new process
 swapped in.
 Parameters - none
 Returns - nothing
 Side Effects - the context of the machine is changed
 ----------------------------------------------------------------------- */
void dispatcher(void)
{
    procPtr nextProcess             = popReadyList();
    USLOSS_Context *curProcessState = Current ? &Current->state : NULL;
    
    USLOSS_Console("dispatcher() switching from %x to %x\n", Current, nextProcess);
    USLOSS_Console("dispatcher() switching from pid=%d to %d\n", Current ? Current->pid : -1, nextProcess->pid);
    
    Current = nextProcess;
    USLOSS_ContextSwitch(curProcessState, &nextProcess->state);
    
    /* int zapi = 0;
     procPtr currProc = Current->zapList[zapi];
     while (currProc != NULL) {
     currProc->status = READY;
     add(currProc);
     
     zapi++;
     currProc = Current->zapList[zapi];
     }
     Current->zapped = 0;*/
    
    //   p1_switch(Current->pid, nextProcess->pid);
} /* dispatcher */


/* ------------------------------------------------------------------------
 Name - sentinel
 Purpose - The purpose of the sentinel routine is two-fold.  One
 responsibility is to keep the system going when all other
 processes are blocked.  The other is to detect and report
 simple deadlock states.
 Parameters - none
 Returns - nothing
 Side Effects -  if system is in deadlock, print appropriate error
 and halt.
 ----------------------------------------------------------------------- */
int sentinel (char *dummy)
{
    if (DEBUG && debugflag)
        USLOSS_Console("sentinel(): called\n");
    while (1)
    {
        checkDeadlock();
        //        USLOSS_WaitInt();
    }
} /* sentinel */


/* check to determine if deadlock has occurred... */
static void checkDeadlock()
{
    
} /* checkDeadlock */


/*
 * Disables the interrupts.
 */
void disableInterrupts()
{
    // turn the interrupts OFF iff we are in kernel mode
    // if not in kernel mode, print an error message and
    // halt USLOSS
    
} /* disableInterrupts */



/*
 * Print table of data structure
 */
void dumpProcesses(){
    for(int tableSlot = 0; tableSlot < MAXPROC; tableSlot++){
        USLOSS_Console("fork1(): -- (%d) Table for PID = %d\n", tableSlot, ProcTable[tableSlot].pid);
        USLOSS_Console("            name      [%s]\n", ProcTable[tableSlot].name);
        USLOSS_Console("            startArg  [%s]\n", ProcTable[tableSlot].startArg);
        USLOSS_Console("            startFunc [%x]\n", ProcTable[tableSlot].startFunc);
        USLOSS_Console("            stackSize [%d]\n", ProcTable[tableSlot].stackSize);
        USLOSS_Console("            stack     [%x]\n", ProcTable[tableSlot].stack);
        USLOSS_Console("            priority  [%d]\n", ProcTable[tableSlot].priority);
        USLOSS_Console("            status    [%d]\n", ProcTable[tableSlot].status);
        USLOSS_Console("            nextSiblingPtr [%x]\n", ProcTable[tableSlot].nextSiblingPtr);
        USLOSS_Console("            nextProcPtr    [%x]\n", ProcTable[tableSlot].nextProcPtr);
    }
}

