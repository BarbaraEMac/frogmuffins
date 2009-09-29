/**
 * CS 452: KERNEL
 * becmacdo
 * dgoc
 */

#include <bwio.h>
#include <ts7200.h>

#include "td.h"
#include "requests.h"

void firstTask () {

    bwprintf (COM2, "Created: %d.\n\r", Create (0, &userTaskStart)); 
    bwprintf (COM2, "Created: %d.\n\r", Create (0, &userTaskStart)); 
    bwprintf (COM2, "Created: %d.\n\r", Create (2, &userTaskStart)); 
    bwprintf (COM2, "Created: %d.\n\r", Create (2, &userTaskStart)); 

    bwputstr (COM2, "First: exiting.");
 
    Exit();
}


void userTaskStart ( TD *td ) {
    
    bwprintf (COM2, "Tid: %d Parent Tid: %d\n\r", td->id, td->parentId);

    Pass();
    
    bwprintf (COM2, "Tid: %d Parent Tid: %d\n\r", td->id, td->parentId);

    Exit();
}
