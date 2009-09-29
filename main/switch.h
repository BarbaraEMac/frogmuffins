/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

enum TASK_STATE {
    ACTIVE = 0,         // Only 1 task will ever be active
    READY,              // Task may be selected to be active
    BLOCKED,            // Task is waiting for something
    DEFUNCT             // Task will never run again :(
} taskState;

typedef struct taskdes {
    int spsr;           // Saved Processor State Register
	int sp;             // Stack Pointer
	void (* start )();  // The first function this runs

    int id;             // A unique identifying id
    int parentId;       // The unique id of the parent

    int returnValue;    // What the heck is this?

    int priority;       // A priority value (ranges from 0->2)
                        
    enum TASK_STATE state; // State of the task

    struct taskdes *nextPQ; // Link to the next TD in the PQ
    struct taskdes *prevPQ; // Link to the prev TD in the PQ

} TD;

typedef struct {
	int arg0;
	int arg1;
	int arg2;
	int arg3;
} Request;

// kerEnt allows a task to return exectution to the kernel, 
// doing the following:
// * acquire the arguments of the request
// * acquire the lr, which is the pc of the active task;
// * change to system mode;
// * overwrite lr
// * push the registers of the active task onto its stack;
// * acquire the sp of the active task;
// * return to svc state;
// * acquire the spsr of the active task;
// * pop the registers of the kernel from its stack;
// * fill in the request with its arguments;
// * put the sp and the spsr into the TD of the active task.
void kerEnt();

// kerExit switches from the kernel to the task described, 
// doing the following:
// * save the kernel context
// * change to system state;
// * install the sp the active task
// * pop the registers of the active task from its stack;
// * put the return value in r0;
// * return to svc state;
// * install the spsr of the active task; and
// * install the pc of the active task.
void kerExit(TD *active, Request *req);
