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
#include <string.h>

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);


/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;

// the mail boxes
Mailbox MailBoxTable[MAXMBOX];

// also need array of mail slots, array of function ptrs to system call
// handlers, ...
Mailslot MailslotTable[MAXSLOTS];
int totalActiveMailSlots;

void initMail();
int findFirstAvailableMailboxAndInit(int numSlots, int slot_size);

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


// Helper Function
void initMail(){
    memset(MailBoxTable, -1, sizeof(Mailbox));
    int i;
    for(i = 0; i < MAXMBOX; i++){
        MailBoxTable[i].mboxID = -1;
        //MailBoxTable[i].mboxTableIndex = -1;
        MailBoxTable[i].totalMailSlots  = -1;
        MailBoxTable[i].activeMailSlots = -1;
        MailBoxTable[i].maxMessageSize  = -1;
        MailBoxTable[i].blockedPID      = -1;
        //MailBoxTable[i].mailSlotTable
    }
    
    memset(MailslotTable, -1, sizeof(Mailslot));
    /*
     memset(MailSlotTable, -1, sizeof(Mailslot));
     int j;
     for(j = 0; j < MAXMSLOT; j++){
     MailSlotTable[j].mboxID = -1;
     MailSlotTable[j].mSlotTableIndex = -1;
     MailSlotTable[j].status = -1;
     MailSlotTable[j].msgSize = -1;
     //MailslotTable[j].msg
     }*/
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
    int mailBoxID = findFirstAvailableMailboxAndInit(slots, slot_size);
    
    return mailBoxID;
} /* MboxCreate */

// Helper Function
int findFirstAvailableMailboxAndInit(int numSlots, int slot_size){
    
    int index;
    
    // starting at 7 in the mean time for testcases 1 and 2
    for(index = 7; index < MAXMBOX; index++){
        //printf("We are in the for loop.");
        //printf("%d", MailBoxTable[index].mboxID);
        if(MailBoxTable[index].mboxID == -1){
            MailBoxTable[index].mboxID = index;
            MailBoxTable[index].totalMailSlots = numSlots;
            MailBoxTable[index].activeMailSlots = 0;
            MailBoxTable[index].maxMessageSize = slot_size;
            if(numSlots > 0){
                MailBoxTable[index].mailSlotTable = malloc(sizeof(Mailslot)*numSlots);
                memset(MailBoxTable[index].mailSlotTable, -1, sizeof(Mailslot)*numSlots);
            }
            else{
                MailBoxTable[index].mailSlotTable = malloc(sizeof(Mailslot)*numSlots);
            }
            return index;
        }
    }
    
    return -1; // no available mailbox found
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
    // Checking for possible errors: inactive mailbox id, message size too large, etc.
    if(mbox_id < 0 || mbox_id >= MAXMBOX){
        USLOSS_Console("MboxSend(): bad message #: %d\n", mbox_id);

        return -1;
    }
    if(MailBoxTable[mbox_id].mboxID == -1){
        USLOSS_Console("MboxSend(): message mailbox (%d) isn't valid\n", mbox_id);
      return -1;
    }
    if(msg_size > MAX_MESSAGE){
        USLOSS_Console("MboxSend(): message size (%d) too large (%d)\n", msg_size, MAX_MESSAGE);
       return -1;
    }
    
    // A mailbox does exist, now check if there are mailslots
    if(MailBoxTable[mbox_id].totalMailSlots > 0){
        // Check if there is an available mail slot
        if((MailBoxTable[mbox_id].totalMailSlots - MailBoxTable[mbox_id].activeMailSlots) > 0){
            // Check that there are enough spaces in the overall mailslots table
            if(totalActiveMailSlots < MAXSLOTS){
//                USLOSS_Console("MboxSend(%d): finding slots\n", mbox_id);
              // Find slot available in the mailslots table
                int slotsTableIndex;
                for(int i = 0; i < MAXSLOTS; i++){
                    if(MailslotTable[i].mboxID == -1){
                        slotsTableIndex = i;
                        break;
                    }
                }
                // Find slot available in the mailbox's own slots table
                int mailboxSlotIndex;
                for(int i = 0; i < MailBoxTable[mbox_id].totalMailSlots; i++){
                    if(MailBoxTable[mbox_id].mailSlotTable[i].mboxID == -1){
                        mailboxSlotIndex = i;
                        break;
                    }
                }
//                USLOSS_Console("MboxSend(%d): slotsTableIndex:%d slotsTableIndex:%d\n", mbox_id, slotsTableIndex, slotsTableIndex);
              // Check that the message size would fit into this mailbox slot
                if(msg_size < MailBoxTable[mbox_id].maxMessageSize){
                    // Create new mailslot
                    Mailslot *newSlot           = (Mailslot *)&MailBoxTable[mbox_id].mailSlotTable[mailboxSlotIndex];
                    newSlot->mboxID             = mbox_id;
                    newSlot->totalSlotTableIndex = slotsTableIndex;
                    newSlot->mailSlotTableIndex  = mailboxSlotIndex;
                    newSlot->msgSize             = msg_size;
/*  free this */    newSlot->msg                 = malloc(msg_size);
                    memcpy(newSlot->msg, msg_ptr, msg_size);
                    
                    // Copy to other table
                    memcpy(&MailslotTable[slotsTableIndex], newSlot, sizeof(Mailslot));
                    MailBoxTable[mbox_id].activeMailSlots++;
                    totalActiveMailSlots++;
                    
                    // now that there's a message in the mailbox, check to see if there's any processs blocked on it
                    if (MailBoxTable[mbox_id].blockedPID != -1)
                    {
                        unblockProc(MailBoxTable[mbox_id].blockedPID);
                    }
                    return 0;
                }
                
            }
        }
    }
    
    return -1;
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
    // Checking for possible errors: inactive mailbox id, message size too large, etc.
    if(mbox_id < 0 || mbox_id >= MAXMBOX){
        USLOSS_Console("MboxReceive(): mbox id (%d) out of bounds\n", mbox_id);
        return -1;
    }
    if(MailBoxTable[mbox_id].mboxID == -1){
        USLOSS_Console("MboxReceive(): mboxid (%d) not valid\n", mbox_id);
        return -1;
    }
    
    // Check that there is a message in the mailbox
    if(MailBoxTable[mbox_id].mailSlotTable[0].mboxID == - 1){
        USLOSS_Console("MboxReceive(%d): no message in mailslot\n", mbox_id);
        MailBoxTable[mbox_id].blockedPID = getpid();
        USLOSS_Console("MboxReceive(%d): blocking on pid %d\n", mbox_id, MailBoxTable[mbox_id].blockedPID);
        blockMe(11);
        
        // done blocking, reset blocked id
 //** Should allow more than one process to block on a mailbox? If so
//** blockedPID should be a stack that is push and popped
       MailBoxTable[mbox_id].blockedPID = -1;
       //** Block process
    }
    
//** Need to check that we were unblocked because the mailbox was released

    // if here, we have a message in the mailbox slot
    // Check that the message is not larger than max parameter
    if(MailBoxTable[mbox_id].mailSlotTable[0].msgSize < max_msg_size){
        // Copy into message pointer parameter
        memcpy(msg_ptr, MailBoxTable[mbox_id].mailSlotTable[0].msg, MailBoxTable[mbox_id].mailSlotTable[0].msgSize);
        
        // Free the mail slot in both tables
        int slotTableIndex;
        int mailboxSlotIndex;
        
        slotTableIndex = MailBoxTable[mbox_id].mailSlotTable[0].totalSlotTableIndex;
        mailboxSlotIndex = MailBoxTable[mbox_id].mailSlotTable[0].mailSlotTableIndex;
        
        // Adjust the mailboxSlotTable
        int index1 = 0;
        for(int i = 1; i < MailBoxTable[mbox_id].activeMailSlots; i++){
            MailBoxTable[mbox_id].mailSlotTable[index1] = MailBoxTable[mbox_id].mailSlotTable[i];
            index1++;
        }
        MailBoxTable[mbox_id].mailSlotTable[index1].mboxID = -1;
        //memset(MailBoxTable[mbox_id].mailSlotTable[index1], -1, sizeof(Mailslot));
        MailBoxTable[mbox_id].activeMailSlots--;
        
        // Adjust the main SlotTable
        int index2 = slotTableIndex;
        for(int j = index2 + 1; j < totalActiveMailSlots; j++){
            MailslotTable[index2] = MailslotTable[j];
            index2++;
        }
        MailslotTable[index2].mboxID = -1;
        //memset(MailslotTable[index2], -1, sizeof(Mailslot));
        totalActiveMailSlots--;
        
        // Return size of message
        return MailBoxTable[mbox_id].mailSlotTable[0].msgSize;
    }
    else {  // return -1 for message too large
        USLOSS_Console("MboxReceive(%d): message too large: %d > %d\n", mbox_id, MailBoxTable[mbox_id].mailSlotTable[0].msgSize, max_msg_size);
        return -1;
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
