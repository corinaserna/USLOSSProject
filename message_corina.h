#define DEBUG2 1

typedef struct Mailbox      Mailbox;
typedef struct Mailslot     Mailslot;

struct Mailbox {
    int         mboxID;
    int         totalMailSlots;
    int         activeMailSlots;
    int         maxMessageSize;
    Mailslot    *mailSlotTable;
};

struct Mailslot {
    int         mboxID;
    int         mailSlotTableIndex; // I dont think I need this
    int         totalSlotTableIndex;
    // int         status;
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