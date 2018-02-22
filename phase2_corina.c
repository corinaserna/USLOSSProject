/* ------------------------------------------------------------------------
 phase2.c
 University of Arizona
 Computer Science 452
 ------------------------------------------------------------------------ */

#define _XOPEN_SOURCE 1
#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <stdio.h>
#include <stdlib.h>
#include <message.h>
#include <string.h>

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);

// 1000 == total number of blocked processes allowed
#define MAX_QUEUE   (MAXMBOX*MAXSLOTS+1000)
#define RESERVE_MAXBOX  7       // code seems to want to reserve the first 7

//#define DEBUG1 1

#ifndef DEBUG1
#warning "non-debug mode"
#undef USLOSS_Console
#define USLOSS_Console //
#endif
/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;
/* -----------------------------------------------------------
There's 3 global queues -- one for Mailbox, one for Mailslot, and one to contain queue elements (the linked list elements of a queue).
 
 For the MailBox_Q, the "val" points to an index into MailBoxTable
 For the Slot_Q, the "val" points to an index into MailslotTable
 ** To create the above 2 queues, nodes are popped off of elQ, then pushed to that (MailBox_Q,  Slot_Q) queue
 For the elQ (queue elements), the "val" points to an index into queueElementArray

 Within the MailBox, there's two queues:
    blockedPID_Q -- queue of processes blocked on this mailbox.  "val2" is used for the saved pid
    mailSlot_Q   -- queue of mail slots "val" points to an index into MailslotTable
 
 When a create mailbox happens, a node is pulled from the MailBox_Q
 
 When a mailbox is released, the node is returned to MailBox_Q
 
 When a message is sent to a MailBox, a entry is pulled (popped) from the Slot_Q and pushed into the MailBox's slot queue (mailSlot_Q).
 
 When a message is received, the mailSlot_Q is popped, and after the data is used, that entry is returned to Slot_Q
 
 If a process needs to be blocked  during a send or recieve, an element from the elQ
 is pulled (popped) off, initialized and added (pushed) to the blockedPID_Q queue ("val2" contains the "pid")
 
 When a process is unblocked, the node is popped from the blockedPID_Q, pid extracted, then
the node is returned (pushed) back to the global elements queue (elQ)

----------------------------------------------------------- */
// the mail boxes
Mailbox MailBoxTable[MAXMBOX];

// also need array of mail slots, array of function ptrs to system call
// handlers, ...
Mailslot MailslotTable[MAXSLOTS];

void initMail(void);

QueueEl queueElementArray[MAX_QUEUE];
Queue   elQ;
Queue   MailBox_Q;
Queue   Slot_Q;

#define MAX_RELEASE_ARRAY 20
static int sReleasedArray[MAX_RELEASE_ARRAY] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};   // could use stack, but too small to make a differnce??
/* -------------------------- Helper Functions ----------------------------------- */

// mboxID is always > -1
void addReleaseArray(int mboxID, int count)
{
    int j = 0;
    for (int i = 0; i < MAX_RELEASE_ARRAY && j < count; i++)
        if (sReleasedArray[i] == -1) {
            sReleasedArray[i] = mboxID;
            j++;
        }
}

int isReleased(int mboxID)  // only can check once
{
    for (int i = 0; i < MAX_RELEASE_ARRAY; i++)
        if (sReleasedArray[i] == mboxID) {
            sReleasedArray[i] = -1;
            return 1;
        }
    return 0;
}

void push_elQ(QueueElPtr el)
{
    pushQueue(&elQ, el);
}

QueueElPtr pop_elQ()
{
    return (QueueElPtr)popQueue(&elQ);
}

void release_Q(QueuePtr thisQueue)
{
    while (thisQueue->count > 0)
    {
        push_elQ(popQueue(thisQueue));        // put back in main queue
    }
}

void init_elQ()
{
    elQ.first = elQ.last = NULL;
    elQ.count = 0;
    
    for (int i = 0; i < MAX_QUEUE; i++) {
        push_elQ(&queueElementArray[i]);
    }
}

// Doubly-linked list queue
QueueElPtr popQueue(QueuePtr thisQueue)
{
    if (thisQueue->count == 0)
        return NULL;
    
    QueueElPtr el = thisQueue->first;
    thisQueue->first = el->next;
    if (thisQueue->first != NULL)
        thisQueue->first->prev = NULL;
    else    // should only happen when count --> 0
    {
        thisQueue->last = NULL;
    }
    thisQueue->count--;
    return el;
}

void pushQueue(QueuePtr thisQueue, QueueElPtr newEl)
{
    newEl->next = newEl->prev = NULL;

    if (thisQueue->first == NULL && thisQueue->last == NULL) {
        thisQueue->first = thisQueue->last = newEl;
    }
    else {
        thisQueue->last->next   = newEl;
        newEl->prev             = thisQueue->last;
        thisQueue->last         = newEl;
    }
    
    thisQueue->count++;
}

// ---------------------------------
// -------- MailBox queue functions
// ---------------------------------
void push_MailBox_Q(QueueElPtr el)
{
    pushQueue(&MailBox_Q, el);
}

MailboxPtr pop_MailBox_Q()
{
    QueueElPtr nextMailBox = (QueueElPtr)popQueue(&MailBox_Q);
    if (nextMailBox == NULL)
        return NULL;
    
    ((MailboxPtr)nextMailBox->val)->mailBoxQ_El = nextMailBox;
    return (MailboxPtr)nextMailBox->val;
}


void init_MailBox_Q()
{
    MailBox_Q.first = MailBox_Q.last = NULL;
    MailBox_Q.count = 0;
    
    QueueElPtr mailBoxEl;
    for (int i = RESERVE_MAXBOX; i < MAXMBOX; i++) {
        mailBoxEl               = pop_elQ();
        mailBoxEl->val          = &MailBoxTable[i];
        MailBoxTable[i].mboxID  = -1;
        MailBoxTable[i].index =  i;
        push_MailBox_Q(mailBoxEl);
    }
}

int popMailBoxAndInit(int numSlots, int slot_size){
    
    MailboxPtr mbox = pop_MailBox_Q();
    if (mbox == NULL)   // no more mail boxes
    {
        USLOSS_Console("popMailBoxAndInit no more availabe mailboxes\n");
        return -1;
    }
    
    if (mbox->mboxID != -1) // initializatoin problem
    {
        USLOSS_Console("popMailBoxAndInit found in use mailbox: %d\n", mbox->mboxID);
        return -1;
    }
    mbox->totalMailSlots = numSlots;
    mbox->maxMessageSize = slot_size;
    mbox->mboxID         = mbox->index;     // ** for some reason, start at index 7???
    
    return mbox->mboxID;
}

// Initialize the members of the Mailbox, then return to the global Slot_Q queue
void releaseMailBoxAndPush(MailboxPtr mailBox)
{
    mailBox->mboxID             = -1;
    mailBox->totalMailSlots     = 0;
    mailBox->zeroSlotMsgSize    = 0;
    memset(mailBox->zeroSlotMsg, 0, sizeof(mailBox->zeroSlotMsg));
    
    // Make sure that any queues in the Mailbox are released
// *** Does that leave any blocking processes deadlocked???
    release_Q(&mailBox->blockedPID_Q);
    release_Q(&mailBox->mailSlot_Q);
    push_MailBox_Q(mailBox->mailBoxQ_El);
}

// ---------------------------------
// -------- MailSlot queue functions
// ---------------------------------
MailslotPtr pop_Slot_Q()
{
    // Grab a slotQ from the master slotQ
    QueueElPtr nextSlot = (QueueElPtr)popQueue(&Slot_Q);
    if (nextSlot == NULL)
        return NULL;
    
    // "val" is of type MailslotPtr.  Initialize the slot reference to this one
    ((MailslotPtr)nextSlot->val)->mailSlotQ_El = nextSlot;
    return (MailslotPtr)nextSlot->val;
}

void push_Slot_Q(QueueElPtr el)
{
    pushQueue(&Slot_Q, el);
}

// Initialize the members of the Mailslot, then return to the global MailBox_Q queue
void releaseMailSlotPush(MailslotPtr mailSlot)
{
    mailSlot->mboxID             = -1;
    
    // We keep around the malloc'd msg so aren't doing free & mallocs all over;
    // if it has been malloced, just clear it.  The assumption is that
    // we have enough memory to keep around MAXSLOTS*MAX_MESSAGE memory pieces
    if (mailSlot->msgSize != 0 && mailSlot->msg != NULL)
        memset(mailSlot->msg, 0, mailSlot->maxMsgSize);
    
    // Give back to the master slot Q
    push_Slot_Q(mailSlot->mailSlotQ_El);
}

void init_Slot_Q()
{
    Slot_Q.first = Slot_Q.last = NULL;
    Slot_Q.count = 0;
    
    QueueElPtr slotEl;
    for (int i = 0; i < MAXSLOTS; i++) {
        slotEl                  = pop_elQ();
        slotEl->val             = &MailslotTable[i];
        MailslotTable[i].mboxID  = -1;
       push_Slot_Q(slotEl);
    }
}

// ---------------------------------
// -------- blocked pid queue functions
// ---------------------------------
int pop_blockedPid_Q(MailboxPtr thisMailBox) {
    if (thisMailBox->blockedPID_Q.count == 0)  {
        USLOSS_Console("pop_blockedPid_Q(): mailBox: %d/%d popping exmpty blockPD\n", thisMailBox->mboxID, getpid());
       return -1;
    }
    
    QueueElPtr getQEl = popQueue(&thisMailBox->blockedPID_Q);
    
    int pid = getQEl->val2;
    push_elQ(getQEl);    // return to the master queue
    return pid;
}

void push_blockedPid_Q(MailboxPtr thisMailBox, int pid) {
    QueueElPtr getQEl = pop_elQ();
    if (getQEl == 0)  {
        USLOSS_Console("pop_blockedPid_Q(): mailBox: %d/%d no more elements to push \n", thisMailBox->mboxID, getpid());
        return ;
    }

    getQEl->val2 = pid;
    pushQueue(&thisMailBox->blockedPID_Q, getQEl);
}

void initMail(){
    
    init_elQ();

    memset(MailBoxTable, 0, sizeof(MailBoxTable));
    memset(MailslotTable, 0, sizeof(MailslotTable));
    
    init_MailBox_Q();
    init_Slot_Q();
#if 0
    for(i = 0; i < 10; i++)
        USLOSS_Console("initMail %d\n", MailBoxTable[i].mboxID);
#endif
}
/* -------------------------- Functions ----------------------------------- */

/* ------------------------------------------------------------------------
 Name - start1
 Purpose - Initializes mailboxes and interrupt vector.
 Start the phase2 test process.
 Parameters - one, default arg passed by fork1, not used here.
 Returns - one to indicate normal quit.
 Side Effects - lots since it initializes the phase2 data structures.
 ----------------------------------------------------------------------- */
int start1(char *arg)
{
    int kidPid;
    int status;
    
    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): at beginning\n");
    
    // Initialize the mail box table, slots, & other data structures.
    initMail();
    
    // Initialize USLOSS_IntVec and system call handlers,
    
    // allocate mailboxes for interrupt handlers.  Etc...
    
    // Create a process for start2, then block on a join until start2 quits
    if (DEBUG2 && debugflag2)
        USLOSS_Console("start1(): fork'ing start2 process\n");
    kidPid = fork1("start2", start2, NULL, 4 * USLOSS_MIN_STACK, 1);
    if ( join(&status) != kidPid ) {
        USLOSS_Console("start2(): join returned something other than ");
        USLOSS_Console("start2's pid\n");
    }
    
    return 0;
} /* start1 */

int invalidMessageBox(const char *funcStr, int mbox_id, MailboxPtr mBox)
{
    // Checking for possible errors: inactive mailbox id, message size too large, etc.
    if(mbox_id < RESERVE_MAXBOX || mbox_id >= MAXMBOX){
        USLOSS_Console("%s(%d, %d): bad message #\n", funcStr, mbox_id, getpid());
        
        return -1;
    }
    
    if(mBox->mboxID == -1){
        USLOSS_Console("%s(%d, %d): message mailbox isn't valid\n", funcStr, mbox_id, getpid());
        return -1;
    }
    
    if (isZapped())
    {
        USLOSS_Console("%s(%d, %d): Zapped; errpr\n", funcStr, mbox_id, getpid());
        return -3;
    }
    return 0;
}

#define activeSlots mBox->mailSlot_Q.count

void unBlockIfShould(const char *funcDesc, MailboxPtr mBox)
{
    // now that that we've removed a message, check that a send wasn't blocked on a full mailbox
    if (mBox->blockedPID_Q.count > 0)
    {
        QueueElPtr firstSlot;
        do {
            firstSlot   = popQueue(&mBox->blockedPID_Q);
            USLOSS_Console("%s(%d, %d): unblocking  %d\n", funcDesc, mBox->mboxID, getpid(), firstSlot->val2);
            unblockProc(firstSlot->val2);
            push_elQ(firstSlot);        // return to el Q
        } while (mBox->blockedPID_Q.count > 0);
     }
}

int recieveMsg(const char *funcDesc, MailboxPtr mBox, void *msg_ptr, int max_msg_size) {
    QueueElPtr firstSlot    = popQueue(&mBox->mailSlot_Q);
    MailslotPtr mSlot       = firstSlot->val;
    int returnVal           = -1;
    // Check that the message is not larger than max parameter
    if(mSlot->msgSize < max_msg_size){
        
        // Copy into message pointer parameter
        memcpy(msg_ptr, mSlot->msg, mSlot->msgSize);
        
        returnVal = mSlot->msgSize;
    }
    else {  // return -1 for message too large
        USLOSS_Console("%s(%d, %d): message too large: %d > %d\n", funcDesc, mBox->mboxID, getpid(), mSlot->msgSize, max_msg_size);
    }
    
    releaseMailSlotPush(mSlot);
    return returnVal;
}

int doSendMessage(const char *funcDesc, MailboxPtr mBox, void *msg_ptr, int msg_size) {
    MailslotPtr mSlot = pop_Slot_Q();
    if (mSlot == NULL) {
        USLOSS_Console("%s(%d, %d): no more total mail slots avaiable\n", funcDesc, mBox->mboxID, getpid());
        return -1;
    }
    
    if(msg_size > mBox->maxMessageSize) {
        USLOSS_Console("%s(%d, %d): msg %d > max %d\n", funcDesc, mBox->mboxID, getpid(), msg_size, mBox->maxMessageSize);
        return -1;
    }
    
    // If here, have valid mail slot, so initialize
    mSlot->mboxID           = mBox->mboxID;
    mSlot->msgSize          = msg_size;
    if (msg_size > mSlot->maxMsgSize)
    {
        mSlot->maxMsgSize   = MAX_MESSAGE;  // go ahead an malloc once --
        // the assumption is that its better to keep around MAXSLOTS*MAX_MESSAGE memory
        // than to do frequent free/malloc combinations
        if (mSlot->msg != NULL)
            free(mSlot->msg);
        mSlot->msg      = malloc(msg_size);
    }
    memcpy(mSlot->msg, msg_ptr, msg_size);
    pushQueue(&mBox->mailSlot_Q, mSlot->mailSlotQ_El);
    USLOSS_Console("%s(%d, %d): message slot count: %d\n", funcDesc, mBox->mboxID, getpid(), mBox->mailSlot_Q.count);
    // now that that we've sent a message, check that a receive wasn't blocked on waiting
    // for a message
    unBlockIfShould(funcDesc, mBox);
    
    return 0;
}

/* ------------------------------------------------------------------------
 Name - MboxRelease
 Releases a previously created mailbox. Any process can release any mailbox.
 The code for MboxRelease will need to devise a means of handling processes that are blocked on a mailbox being released. Essentially, each blocked process should return -3 from the send or receive that caused it to block. The process that called MboxRelease needs to unblock all the blocked processes. When each of these processes awake from the blockMe call inside send or receive, it will need to “notice” that the mailbox has been released...
 Return values:
 -3: process has been zap’d.
 -1: the mailboxID is not a mailbox that is in use.
 0: successful completion.
 ----------------------------------------------------------------------- */

int MboxRelease(int mbox_id)
{
    MailboxPtr mBox = &MailBoxTable[mbox_id]; // account for #7 offset
    
    USLOSS_Console("MboxRelease(%d, %d) start\n", mbox_id, getpid());
    int returnVal = invalidMessageBox("MboxRelease", mbox_id, mBox);
    if (returnVal != 0)
        return returnVal;

    if (mBox->blockedPID_Q.count) {  // need to ublock
        addReleaseArray(mbox_id, mBox->blockedPID_Q.count);
        USLOSS_Console("MboxRelease(%d, %d) # blocked mailbox: %d \n", mbox_id, getpid(), mBox->blockedPID_Q.count);

        // unblock any processes waiting on this mailbox (they should return -3)
        unBlockIfShould("MboxRelease", mBox);
    }

    releaseMailBoxAndPush(mBox);
    return 0;
}

/* ------------------------------------------------------------------------
 Name - MboxCondSend

 Conditionally send a message to a mailbox. Do not block the invoking process.
 If there is no empty slot in the mailbox in which to place the message, the value -2 is returned. Also return -2 in the case that all the mailbox slots in the system are used and none are available to allocate for this message.
 Return values:
 -3: process has been zap’d.
 -2: mailbox full, message not sent; or no slots available in the system.
 -1: illegal values given as arguments.
 0: message sent successfully.
 
 ----------------------------------------------------------------------- */
int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size)
{
    MailboxPtr mBox = &MailBoxTable[mbox_id]; // account for #7 offset
    
    USLOSS_Console("MboxCondSend(%d, %d) start: (%s)\n", mbox_id, getpid(), msg_ptr);
    int returnVal = invalidMessageBox("MboxSend", mbox_id, mBox);
    if (returnVal != 0)
        return returnVal;

    if(msg_size > MAX_MESSAGE){
        USLOSS_Console("MboxSend(%d, %d): message size (%d) too large (%d)\n", mbox_id, getpid(), msg_size, MAX_MESSAGE);
        return -1;
    }
    
    if(mBox->totalMailSlots <= 0){  // should never happen?
        USLOSS_Console("MboxSend(%d, %d): bad totalMailSlots (%d)\n", mbox_id, getpid(), mBox->totalMailSlots);
        return -1;
    }

    // unlike regular send, if the mail box is full, return an error (-2)
    if(mBox->totalMailSlots == activeSlots) {
        USLOSS_Console("MboxCondSend(%d, %d): mailbox slots full; errpr\n", mbox_id, getpid());
        return -2;
    }
    if(Slot_Q.count == 0) {
        USLOSS_Console("MboxCondSend(%d, %d): global mail slots full; errpr\n", mbox_id, getpid());
        return -2;
    }
    returnVal = doSendMessage("MboxSend", mBox, msg_ptr, msg_size);
    return returnVal;
}

/* ------------------------------------------------------------------------
Name - MboxCondReceive

 Conditionally receive a message from a mailbox. Do not block the invoking process. If there is no message in the mailbox, the value -2 is returned.
 Return values:
 -3:    process has been zap’d.
 -2:    no message available to receive.
 -1:    illegal values given as arguments; or, message sent is too large for receiver’s buffer (no data copied in this case).
 >= 0:  size of the message received
 ----------------------------------------------------------------------- */
int MboxCondReceive(int mbox_id, void *msg_ptr, int max_msg_size)
{
    MailboxPtr mBox = &MailBoxTable[mbox_id]; // account for #7 offset
    
    USLOSS_Console("MboxCondReceive(%d, %d) start\n", mbox_id, getpid());
    int returnVal = invalidMessageBox("MboxCondReceive", mbox_id, mBox);
    if (returnVal != 0)
        return returnVal;

    if(mBox->mailSlot_Q.count == 0){
        USLOSS_Console("MboxCondReceive(%d, %d): mailbox empty; errpr\n", mbox_id, getpid());
        return -2;
    }
    
    returnVal = recieveMsg("MboxCondReceive", mBox, msg_ptr, max_msg_size);
    
    return returnVal;
}

/* ------------------------------------------------------------------------
 Name - MboxCreate
 Purpose - gets a free mailbox from the table of mailboxes and initializes it
 Parameters - maximum number of slots in the mailbox and the max size of a msg
 sent to the mailbox.
 Returns - -1 to indicate that no mailbox was created, or a value >= 0 as the
 mailbox id.
 Side Effects - initializes one element of the mail box array.
 ----------------------------------------------------------------------- */
int MboxCreate(int slots, int slot_size)
{
    /*
     A mailbox can be created with zero slots. Zero-slot mailboxes require special handling. Such mailboxes are intended for synchronization between sender and receiver. Two cases:
     a.) The sender will be blocked until a receiver collects the message;
     OR
     b.) the receiver will be blocked until a sender sends the message.     */
    if (slots == 0){
        //** do blocking
    }
    // if here, unblocked??
    int mailBoxID = popMailBoxAndInit(slots, slot_size);
    USLOSS_Console("MboxCreate(%d, %d) mbox #Slots:%d  #max msg size:%d\n", mailBoxID, getpid(), slots, slot_size);

    return mailBoxID;
} /* MboxCreate */

void doBlock(const char *funcDesc, MailboxPtr mBox, int blockStatus)
{
    USLOSS_Console("%s(%d, %d): mailbox slots full; blocking\n", funcDesc, mBox->mboxID, getpid());
    int thisPid = getpid();
    push_blockedPid_Q(mBox, thisPid);
    USLOSS_Console("MboxSend(%d, %d): blocking on pid %d\n", mBox->mboxID, getpid(), thisPid);
    blockMe(blockStatus);
}

/* ------------------------------------------------------------------------
 Name - MboxSend
 Purpose - Put a message into a slot for the indicated mailbox.
 Block the sending process if no slot available.
 Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
 Returns - zero if successful, -1 if invalid args.
 Side Effects - none.
 ----------------------------------------------------------------------- */

int MboxSend(int mbox_id, void *msg_ptr, int msg_size){
    MailboxPtr mBox = &MailBoxTable[mbox_id]; // account for #7 offset
    
    USLOSS_Console("MboxSend(%d, %d) start: (%s)\n", mbox_id, getpid(), msg_ptr);
    int returnVal = invalidMessageBox("MboxSend", mbox_id, mBox);
    if (returnVal != 0)
        return returnVal;
    
    if(msg_size > MAX_MESSAGE){
        USLOSS_Console("MboxSend(%d, %d): message size (%d) too large (%d)\n", mbox_id, getpid(), msg_size, MAX_MESSAGE);
        return -1;
    }
    
    /*
     3.3 Zero-slot Mailboxes
     A mailbox can be created with zero slots. Zero-slot mailboxes require special handling. Such mailboxes are intended for synchronization between sender and receiver. Two cases:
     a.) The sender will be blocked until a receiver collects the message;
        OR
     b.) the receiver will be blocked until a sender sends the message.
     */
    if(mBox->totalMailSlots == 0) {
        USLOSS_Console("MboxSend(%d, %d): zero totalMailSlots (%d)\n", mbox_id, getpid(), mBox->totalMailSlots);
        
        if(msg_size > mBox->maxMessageSize) {
            USLOSS_Console("MboxSend (zero slots)(%d, %d): msg %d > max %d\n", mBox->mboxID, getpid(), msg_size, mBox->maxMessageSize);
            return -1;
        }
        mBox->zeroSlotMsgSize = msg_size;
        memcpy(mBox->zeroSlotMsg, msg_ptr, msg_size);

        if (mBox->blockedPID_Q.count > 0)   // receiver is waiting for this message
            unBlockIfShould("MboxSend (zero)", mBox);
        else    // block
            doBlock("MboxSend (zero)", mBox, 12);

        //Need to check that we were unblocked because the mailbox was released
        // Can't use mBox->isReleased, as the variable setting doesn't propagate here
        if (isReleased(mbox_id))
        {
            unBlockIfShould("MboxSend (zero)", mBox);
            USLOSS_Console("MboxSend (zero) (%d, %d): mailbox has been released? -3\n", mbox_id, getpid());
            return -3;
        }

        return 0;
    }
    else {
       // A mailbox does exist, now check if there are mailslots
        // Check if there is an available mail slot (if not, block)
        if(mBox->totalMailSlots == activeSlots) {
            doBlock("MboxSend", mBox, 12);
        }
        
        //Need to check that we were unblocked because the mailbox was released
        // Can't use mBox->isReleased, as the variable setting doesn't propagate here
        if (isReleased(mbox_id))
        {
            unBlockIfShould("MboxSend", mBox);
            USLOSS_Console("MboxSend(%d, %d): mailbox has been released? -3\n", mbox_id, getpid());
            return -3;
        }

        returnVal = doSendMessage("MboxSend", mBox, msg_ptr, msg_size);
        }
    return returnVal;     // should never get here
} /* MboxSend */


/* ------------------------------------------------------------------------
 Name - MboxReceive
 Purpose - Get a msg from a slot of the indicated mailbox.
 Block the receiving process if no msg available.
 Parameters - mailbox id, pointer to put data of msg, max # of bytes that
 can be received.
 Returns - actual size of msg if successful, -1 if invalid args.
 Side Effects - none.
 ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int max_msg_size){
    MailboxPtr mBox = &MailBoxTable[mbox_id];
    
    USLOSS_Console("MboxReceive(%d, %d) start\n", mbox_id, getpid());
   int returnVal = invalidMessageBox("MboxReceive", mbox_id, mBox);
    if (returnVal != 0)
        return returnVal;
    
    /*
     3.3 Zero-slot Mailboxes
     A mailbox can be created with zero slots. Zero-slot mailboxes require special handling. Such mailboxes are intended for synchronization between sender and receiver. Two cases:
     a.) The sender will be blocked until a receiver collects the message;
     OR
     b.) the receiver will be blocked until a sender sends the message.
     */
    if(mBox->totalMailSlots == 0) {
        USLOSS_Console("MboxReceive(%d, %d): zero totalMailSlots (%d)\n", mbox_id, getpid(), mBox->totalMailSlots);
       
        if (mBox->blockedPID_Q.count > 0)   // sender is waiting for this message
            unBlockIfShould("MboxReceive (zero)", mBox);
        else    // block
            doBlock("MboxReceive (zero)", mBox, 11);

        if (mBox->zeroSlotMsgSize < max_msg_size) {
            USLOSS_Console("MboxReceive (zero)(%d, %d): message size (%d) too large (%d)\n", mbox_id, getpid(), max_msg_size, mBox->zeroSlotMsgSize);
            return -1;
        }
       
        memcpy(msg_ptr, mBox->zeroSlotMsg, mBox->zeroSlotMsgSize);
        
       //Need to check that we were unblocked because the mailbox was released
        // Can't use mBox->isReleased, as the variable setting doesn't propagate here
        if (isReleased(mbox_id))
        {
            unBlockIfShould("MboxReceive (zero)", mBox);
            USLOSS_Console("MboxReceive (zero) (%d, %d): mailbox has been released? -3\n", mbox_id, getpid());
            return -3;
        }
        
        int zeroSlotMsgSize = mBox->zeroSlotMsgSize;
        mBox->zeroSlotMsgSize = 0;      // reset

        return zeroSlotMsgSize;
   }
    else {
        // Check that there is a message in the mailbox
        if(mBox->mailSlot_Q.count == 0){
            doBlock("MboxReceive", mBox, 11);
         }
        
        //Need to check that we were unblocked because the mailbox was released
        if (isReleased(mbox_id))
        {
            unBlockIfShould("MboxReceive", mBox);
            USLOSS_Console("MboxReceive(%d, %d): mailbox has been released? -3\n", mbox_id, getpid());
            return -3;
        }
        returnVal = -1;
        if (mBox->mailSlot_Q.count != 0)  {  // only reason to get here is because this mailbox was released????
            returnVal = recieveMsg("MboxReceive", mBox, msg_ptr, max_msg_size);
        }
        else {  // return -1 for message too large
            USLOSS_Console("MboxReceive(%d, %d): slots empty after block\n", mbox_id, getpid());
        }

        // now that that we've removed a message, check that a send wasn't blocked on a full mailbox
        unBlockIfShould("MboxReceive", mBox);
        // Return size of message
        return returnVal;
    }
 } /* MboxReceive */

/* ------------------------------------------------------------------------
 Name - check_io
 Purpose - Determine if there any processes blocked on any of the
 interrupt mailboxes.
 Returns - 1 if one (or more) processes are blocked; 0 otherwise
 Side Effects - none.
 Note: Do nothing with this function until you have successfully completed
 work on the interrupt handlers and their associated mailboxes.
 ------------------------------------------------------------------------ */
int check_io(void)
{
    if (DEBUG2 && debugflag2)
        USLOSS_Console("check_io(): called\n");
    return 0;
} /* check_io */
