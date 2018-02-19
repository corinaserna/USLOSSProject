/* ------------------------------------------------------------------------
   phase2.c

   University of Arizona
   Computer Science 452

   ------------------------------------------------------------------------ */

#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <stdio.h>
#include <stdlib.h>

#include <message.h>

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);


/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;

// the mail boxes 
mailbox MailBoxTable[MAXMBOX];

MailSlots gMailSlots;
// also need array of mail slots, array of function ptrs to system call 
// handlers, ...




/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
 Name - setupMailSlot
 Purpose - Returns the next available mailSlot, if one is avaialble
  Returns - 0 = success
            -2 = no more mail slots available
 Side Effects - slotPtr is NULL if not found, or contains mailSlot with mailBoxID filled in
 ----------------------------------------------------------------------- */
int setupMailSlot(MailboxPtr mailPtr, void *message, int messageSize)
{
    if (gMailSlots.numAtiveMailSlots == MAXSLOTS)   // no more global slots available
        return -2;
    
    if (mailPtr->numActiveSlots == 0)   // no slots available to use in mailbox
         return -2;
         
    int slotID;
    mailSlot theMailSlotPtr;
    theMailSlotPtr = gMailSlots.mailSlots[0];
    for (slotID = 0; slotID < gMailSlots.numAtiveMailSlots; slotID++)
    {
        if (-1 == theMailSlotPtr.mboxID)  // found empty
            break;
        theMailSlotPtr = gMailSlots.mailSlots[slotID];
    }
    theMailSlotPtr.mboxID    = mailPtr->mboxID;
    theMailSlotPtr.msgSize   = messageSize;
    theMailSlotPtr.msg       = message;
    ++gMailSlots.numAtiveMailSlots;
    
    // now add to mailbox
    // the line directly under this message is throwing an compiler error that states:
    // "error: incompatible types when assigning to type ‘slotPtr’ from type ‘mailSlot’"
    // I think it would be the easier to have an array of mailslots within a mailbox
    // instead of a pointer, because if we do a pointer, then we also need to add a pointer
    // that points to the next slot so that we can iterate through the slots
    mailPtr->slotsForThisMailBox[mailPtr->numActiveSlots] = theMailSlotPtr;
    mailPtr->numActiveSlots++;
    
    return 0;
}

// Initialze mailID's to -1 and 0 out everything else
void initMail()
{
    memset(*gMailSlots, -1, sizeof(gMailSlots));
    gMailSlots.numAtiveMailSlots = 0;

    memset(MailBoxTable, -1, sizeof(MailBoxTable));
}

void releaseMailSlot(slotPtr theMailSlot)
{
    free(mailSlot->msg);
    memset(mailSlot, -1, sizeof(MailSlot));
    --gMailSlots.numAtiveMailSlots;
}

/* -----------------------------
 Conditionally send a message to a mailbox. Do not block the invoking process.
 If there is no empty slot in the mailbox in which to place the message, the value -2 is returned. Also return -2 in the case that all the mailbox slots in the system are used and none are available to allocate for this message.
 Return values:
 -3: process has been zap’d.
 -2: mailbox full, message not sent; or no slots available in the system. 
 -1: illegal values given as arguments.
 0: message sent successfully.
*/
int MboxCondSend(int mailboxID, void *message, int messageSize)
{
    MailboxPtr mailBoxPtr = &MailBoxTable[mailboxID];
    if (mbox_id < 0 || mbox_id >= MAXMBOX ||    // check for illegal indexes
        -1 == mailBoxPtr->mboxID                // check for mailbox ID wasn't in use
        
        )
        return -1;
    
    if ( isZapped() )
        return -3;
         
    // Add message to mailSlot
    int result = setupMailSlot(mailBoxPtr);
    if (results != 0)   // should only be -2
         return result;

    // Now send?
    return 0;
}

/* -----------------------------
 Conditionally receive a message from a mailbox. Do not block the invoking process. If there is no message in the mailbox, the value -2 is returned.
 Return values:
 -3: process has been zap’d.
 -2: no message available to receive.
 -1: illegal values given as arguments; or, message sent is too large for receiver’s buffer (no data copied in this case).
 >= 0: the size of the message received.
 
*/
int MboxCondReceive(int mailboxID, void *message, int maxMessageSize)
{
    MailboxPtr mailBoxPtr = &MailBoxTable[mailboxID];
    if (mbox_id < 0 || mbox_id >= MAXMBOX ||    // check for illegal indexes
        -1 == mailBoxPtr->mboxID)     // check for mailbox ID wasn't in use
        return -1;
    
    if ( isZapped() )
        return -3;
    
    if (0 == mailBoxPtr->numActiveSlots)  // no messages available to receive
        return -2;
    
    slotPtr msg = mailBoxPtr->slotsForThisMailBox[0];
    if (msg->msgSize > maxMessageSize)
         return -1;
         
    doReceiveMessage(mailBoxPtr, message, maxMessageSize);
    return 0;
}
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

// Returns -1 if not available mailbox found; else the index
int findFirstAvailableMailboxAndInit(int slots, int slot_size)
{
    MailboxPtr mboxPtr = NULL;
    int index = 0;
    
    do {
        mboxPtr = &MailBoxTable[index];
        if (-1 == mboxPtr->mboxID)
        {
            mboxPtr->mboxID                 = index;
            mboxPtr->numMailSlots           = slots;
            mboxPtr->maxMessageSize         = slot_size;
            mboxPtr->numActiveSlots         = 0;
            if (slots)  // create array of slotPtr, init to NULL
            {
                mboxPtr->slotsForThisMailBox = maloc(sizeof(slotPtr)*slots);
                memset(mboxPtr->slotsForThisMailBox, 0, sizeof(slotPtr)*slots);
            }
            else
                mboxPtr->slotsForThisMailBox = NULL;
               
            return index;
        }
        ++index;
    } while (index < MAXMBOX);
 
    // if here, mailbox is full
    return -1;
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
    if (0 == slots)
    {
        //** do blocking
    }
    // if here, unblocked??
    int mailBoxID = findFirstAvailableMailboxAndInit(slots, slot_size);
    
    return mailBoxID;
} /* MboxCreate */


// Marks all mailslots as released and then marks this mailbox as released
void doReleaseMailbox(MailboxPtr mBoxPtr)
{
    // release the slots in teh mailbox
    int i;
    for (i = 0; i < mBoxPtr->numMailSlots; i++)
        releaseMailSlot(mBoxPtr[i]);
    
    // release the mailbox
    memset(&MailBoxTable[mbox_id], -1, sizeof(mailbox));
}

/*  ------------------------------------------------------------------------
 Name - MboxRelease
 Releases a previously created mailbox. Any process can release any mailbox.
 The code for MboxRelease will need to devise a means of handling processes that are blocked on a mailbox being released. Essentially, each blocked process should return -3 from the send or receive that caused it to block. The process that called MboxRelease needs to unblock all the blocked processes. When each of these processes awake from the blockMe call inside send or receive, it will need to “notice” that the mailbox has been released...
 Return values:
 -3: process has been zap’d.
 -1: the mailboxID is not a mailbox that is in use. 0: successful completion.
 
  ------------------------------------------------------------------------*/
int MboxRelease(int mbox_id)
{
    if (mbox_id < 0 || mbox_id >= MAXMBOX ||    // check for illegal indexes
        -1 == MailBoxTable[mbox_id].mboxID)     // check for mailbox ID wasn't in use
        return -1;
    
    MailboxPtr mBoxPtr = &MailBoxTable[mbox_id];
     // release the mailbox
    mBoxPtr->mboxID = -2;      // mark mailbox as about to be released
    
    //** Handle any blocked on on this mailbox
    
    
    // Mark the mailbox as released
    doReleaseMailbox(mBoxPtr);
   
    if ( isZapped() )
        return -3;

    // if here, everything completed correctly
    return 0;
} /* MboxRelease */

/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, 
            -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size)
{
    if (mbox_id < 0 || mbox_id >= MAXMBOX ||    // check for illegal indexes
        -1 == mailBoxPtr->mboxID                // check for mailbox ID wasn't in use
        
        )
        return -1;
        
    MailboxPtr mailBoxPtr = &MailBoxTable[mbox_id];

    return 0;
} /* MboxSend */

// Releases the mail slot and does the accounting for it
// Treats the slots array as stack, so needs to move it "up"
// No error checking done here -- if called, all assumed good
void popMsgStackAndRelease(MailboxPtr mailPtr)
{
    slotPtr msgPtr = mailPtr->slotsForThisMailBox[0];
    
    // release slot
    releaseMailSlot(msgPtr);
    --mailPtr->numActiveSlots;
    
    int i = 0;
    if (mailPtr->numActiveSlots > 0) // need to adjust "stack" up
    {
        for (i = 0; i < mailPtr->numActiveSlots; i++)
            mailPtr->slotsForThisMailBox[i] = mailPtr->slotsForThisMailBox[i+1];
    }
    mailPtr->slotsForThisMailBox[i] = NULL;
   
}

// Does the actual message copy and releases the mail slot
// Always assumes that the top of the stack has the message (slot) to copy
// No error checking done here -- if called, all assumed good
void doReceiveMessage(MailboxPtr mailPtr, void *msg_ptr, int maxMessageSize)
{
    slotPtr msgPtr = mailPtr->slotsForThisMailBox[0];
    int sizeOfMessage = (msgPtr->msgSize > maxMessageSize ? maxMessageSize : mailPtr->msgSize);
    memcpy(msg_ptr, msgPtr->msg, sizeOfMessage);
    
    popMsgStackAndRelease(mailPtr);
    
}
/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int maxMessageSize)
{
    if (mbox_id < 0 || mbox_id >= MAXMBOX ||    // check for illegal indexes
        -1 == mailBoxPtr->mboxID                // check for mailbox ID wasn't in use
        
        )
        return -1;
        
    MailboxPtr mailBoxPtr = &MailBoxTable[mbox_id];
    //** Handle process order, etc
    
    // if here, ready to do the actual receive (copy) of the message
    doReceiveMessage(mailPtr, msg_ptr, maxMessageSize);
    return 0;
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
