/**
 * CS 452: KERNEL
 * becmacdo
 * dgoc
 */

#include <bwio.h>
#include <ts7200.h>

#include "td.h"
#include "requests.h"
#include "switch.h"

void firstTaskStart () {

    // Create low priority
    bwprintf (COM2, "Created: %d.\n\r", Create (2, &userTaskStart)); 
    bwprintf (COM2, "Created: %d.\n\r", Create (2, &userTaskStart)); 
    // Create high priority
    bwprintf (COM2, "Created: %d.\n\r", Create (0, &userTaskStart)); 
    bwprintf (COM2, "Created: %d.\n\r", Create (0, &userTaskStart)); 

    bwputstr (COM2, "First: exiting.");
 
    Exit();
}


void userTaskStart ( TD *td ) {
    
    bwprintf (COM2, "Tid: %d Parent Tid: %d\n\r", td->id, td->parentId);

    Pass();
    
    bwprintf (COM2, "Tid: %d Parent Tid: %d\n\r", td->id, td->parentId);

    Exit();
}

TD * getUnusedTask ( TDManager *manager ) {
    TD *ret = &manager->tdArray[manager->backPtr];
    manager->backPtr += 1;

    return ret;
}

int getNextId ( TDManager *manager ) {
    int ret = manager->nextId;
    manager->nextId += 1;

    return ret;
}

void initTaskDesc ( TD *td, int priority, void (*s)(), int parentId, TDManager *manager ) {
 	int *pcOnStack;
    
    td->spsr = 0x10;	
	td->sp = 0; //TODO
	pcOnStack = td->sp + PC_OFFSET;

	td->start = s;
	
	td->id = getNextId (manager);
	td->parentId = parentId;
	
	td->returnValue = 0;
	td->priority = priority;
	td->state = READY;

    // Temporary until we insert into PQ
    td->nextPQ = 0;
    td->prevPQ = 0;
}

void insertInPQ (TD *td, TDManager *manager ) {
    int i;
    int priority = td->priority;

    TD *next = manager->pq[priority];
    TD *prev;
    TD *itr;

    // First td for this priority
    if ( next == 0 ) {
        
        // Check all lower priorities for prev value
        for ( i = priority - 1; i >= 0; i -- ) {
            itr = manager->pq[i];
            
            // Found one
            if ( itr != 0 ) {
                next = itr->nextPQ; // Could be 0

                td->prevPQ = itr;
                td->nextPQ = next;

                itr->nextPQ = td;
                
                if ( next != 0 ) {
                    next->prevPQ = td;
                }

                break;
            }
        }

        // This td is highest priority
        if ( i < 0 ) {
            td->prevPQ = 0; // Just to assert (it should already be 0)
    
            // Now check for any lower priorities
            for ( i = priority + 1; i <= MAX_PRIORITY; i ++ ) {
                itr = manager->pq[i];

                // Found one
                if ( itr != 0 ) {
                    td->nextPQ  = itr;
                    itr->prevPQ = td;
                    
                    break;
                }
            }
        }
    }
    // There is another td at this priority
    else {
        prev = next->prevPQ; // Could be 0

        td->nextPQ = next;
        td->prevPQ = prev;

        next->prevPQ = td;

        if ( prev != 0 ) {
            prev->nextPQ = td;
        }
    }

    // Put the new task in the manager's PQ
    manager->pq[priority] = td;
}

TD * kernCreateTask ( int priority, void (*start)(), int parentId, TDManager *manager ) {

	// Grab the new task
	TD *newTask = getUnusedTask (manager);

	// Initialize the values
	initTaskDesc ( newTask, priority, start, parentId, manager );

	// Insert into PQ
    insertInPQ (newTask, manager);
	
    return newTask;
}
