
#define DEBUG2 1

typedef struct mailSlot 	*slotPtr;
typedef struct mailSlot 	mailSlot;
typedef struct mailbox   	mailbox;
typedef struct mailbox   	*MailboxPtr;
typedef struct mboxProc 	*mboxProcPtr;
typedef struct MailSlots   MailSlots;

struct mailbox {
    int       	mboxID;
    int			numMailSlots;
    int			messageSize;
    int			numActiveSlots;
    int         maxMessageSize;
    slotPtr		*slotsForThisMailBox;		// contains pointers to slots in use
};

struct mailSlot {
    int       mboxID;
    int       status;
    // other items as needed...
    int		  msgSize;
    void		*msg;
};

struct MailSlots {
	int 		numAtiveMailSlots;
	mailSlot	mailSlots[MAXSLOTS];		// array of mail slots
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
