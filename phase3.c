#include <usloss.h>
#include <usyscall.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>

Semaphore SemTable[MAXSEMS];

int start2(char *arg)
{
    int pid;
    int status;
    /*
     * Check kernel mode here.
     */

    /*
     * Data structure initialization as needed...
     */
    

    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * Assumes kernel-mode versions of the system calls
     * with lower-case names.  I.e., Spawn is the user-mode function
     * called by the test cases; spawn is the kernel-mode function that
     * is called by the syscallHandler (via the systemCallVec array);s
     * spawnReal is the function that contains the implementation and is
     * called by spawn.
     *
     * Spawn() is in libuser.c.  It invokes USLOSS_Syscall()
     * The system call handler calls a function named spawn() -- note lower
     * case -- that extracts the arguments from the sysargs pointer, and
     * checks them for possible errors.  This function then calls spawnReal().
     *
     * Here, start2 calls spawnReal(), since start2 is in kernel mode.
     *
     * spawnReal() will create the process by using a call to fork1 to
     * create a process executing the code in spawnLaunch().  spawnReal()
     * and spawnLaunch() then coordinate the completion of the phase 3
     * process table entries needed for the new process.  spawnReal() will
     * return to the original caller of Spawn, while spawnLaunch() will
     * begin executing the function passed to Spawn. spawnLaunch() will
     * need to switch to user-mode before allowing user code to execute.
     * spawnReal() will return to spawn(), which will put the return
     * values back into the sysargs pointer, switch to user-mode, and 
     * return to the user code that called Spawn.
     */
    pid = spawnReal("start3", start3, NULL, USLOSS_MIN_STACK, 3);

    /* Call the waitReal version of your wait code here.
     * You call waitReal (rather than Wait) because start2 is running
     * in kernel (not user) mode.
     */
    pid = waitReal(&status);

} /* start2 */

int spawn(USLOSS_Sysargs *sysArg){
    
}

int spawnReal(){
    
}

int spawnLaunch(){
    
}

int wait(){
    
}

int waitReal(){
    
}

void terminate(){
    
}

void terminateReal(){
    
}

void semcreate(USLOSS_Sysargs *sysArg){
    int initValue;
    // Checking values of sysArg
    
    int semID;
    semID = semcreateReal(initValue);
    
    if(isZapped()){
        quit();
    }
    
    sysArg.arg1 = semID;
    
    // set to user mode
    return;
}

int semcreateReal(int initValue){
    int semID;    
    
    // initialize semaphore
    semaphore *newSemaphore = malloc(sizeof(semaphore));
    newSemphore.count = initValue;
    
    // insert into array
    int availableSlot = getNextSemSlot();
    SemTable[availableSlot] = newSemaphore;
    semID = availableSlot;    
    
    // use mailboxes for mutex
    
    return semID;
}

void semp(USLOSS_Sysargs *sysArg){
    int sem = (int) sysArg.arg1;
    // Checking values of sysArg 
    
    
    sempReal(sem);
    
    
    sysArg.arg4 = 0;
    
    // set to user mode
    return;
}

void sempReal(int sem){
    while(1){
        //interruptsDisable();
        if(SemTable[sem].count > 0){
            SemTable[sem].count--;
            break;
        }
        
        //interruptsEnable();
    }
    //interruptsEnable();
}

int semv(USLOSS_Sysargs *sysArg){
    int sem = (int) sysArg.arg1;
    // Checking values of sysArg 
    
    
    semvReal(sem);
    
    
    sysArg.arg4 = 0;
    
    // set to user mode
    return;
}

int semvReal(int sem){
    //interruptsDisable();
    SemTable[sem].count++;
    //interruptsEnable();
}

int semfree(USLOSS_Sysargs *sysArg){
    int sem = (int) sysArg.arg1;
    // Checking values of sysArg 
    
    semfreeReal(sem);
    
    sysArg.arg4 = 0;
    
    // set to user mode
    return;
}

int semfreeReal(int sem){
    
}

void gettimeofday(USLOSS_Sysargs *sysArg){
    sysArg.arg1 = 0;
    
    // set to user mode
    return;
}

void cputime(USLOSS_Sysargs *sysArg){
    sysArg.arg1 = 0;
    
    // set to user mode
    return;
}

void getpid(USLOSS_Sysargs *sysArg){
    sysArg.arg1 = getpid();
    
    // set to user mode
    return;
}