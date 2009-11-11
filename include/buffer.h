/**
 * CS 452
 * becmacdo
 * dgoc
 */

#ifndef __BUFFER_H__
#define __BUFFER_H__

typedef int Element;
typedef struct ringBuffer RB;	// Ring buffer

void rb_init ( RB *rb ) ;
void rb_push( RB *rb, Element el ) ;
Element rb_pop( RB *rb ) ;


#endif
