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

typedef struct taskdesc {
    int spsr;           // Saved Processor State Register
	int sp;             // Stack Pointer
	void (* start )();  // The first function this runs

    int id;             // A unique identifying id
    int parentId;       // The unique id of the parent

    int returnValue;    // TODO: What the heck is this?

    int priority;       // A priority value (ranges from 0->4)
                        
    enum TASK_STATE state; // State of the task

    struct taskdes *nextPQ; // Link to the next TD in the PQ
    struct taskdes *prevPQ; // Link to the prev TD in the PQ

} TD;

void userTaskStart (TD *t);

void firstTask ();
