#define DEBUG2 1

typedef struct Mailbox      Mailbox;
typedef struct Mailbox      *MailboxPtr;
typedef struct Mailslot     Mailslot;
typedef struct Mailslot    *MailslotPtr;
typedef struct Queue      Queue;
typedef struct Queue      *QueuePtr;
typedef struct QueueEl      QueueEl;
typedef struct QueueEl      *QueueElPtr;

struct Queue {
    QueueElPtr first;
    QueueElPtr last;
    int count;
};

struct QueueEl {
    QueueElPtr prev;
    QueueElPtr next;
    void *val;
    int val2;
};

QueueElPtr popQueue(QueuePtr thisQueue);
void pushQueue(QueuePtr thisQueue, QueueElPtr val);

struct Mailbox {
    int         mboxID;
    int         index;
   int          totalMailSlots;
    int         maxMessageSize;
    QueueElPtr  mailBoxQ_El;           // queue element for this mailbox
    Queue		blockedPID_Q;
    Queue       mailSlot_Q;
};

struct Mailslot {
    int         mboxID;
//    int         mailSlotTableIndex; // I dont think I need this
//    int         totalSlotTableIndex;
    // int         status;
    QueueElPtr  mailSlotQ_El;           // queue element for this mailSlot
    int         maxMsgSize;     // don't free messages unless need to increase size
    int         msgSize;
    void        *msg;
};

struct psrBits {
    unsigned int curMode:1;
    unsigned int curIntEnable:1;
    unsigned int prevMode:1;
    unsigned int prevIntEnable:1;
    unsigned int unused:28;
};

union psrValues {
    struct psrBits bits;
    unsigned int integerPart;
};
