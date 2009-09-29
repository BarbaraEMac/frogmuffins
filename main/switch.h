/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

// Forward declare these. We don't actually use them,
// so we don't need to include them.
struct taskdesc;
struct request;

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
void kerExit(struct taskdesc *active, struct request *req);
