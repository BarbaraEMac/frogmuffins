/**
 * CS 451: kernel
 * becmacdo
 * dgoc
 */

/* This file is here to fool the linker to follow the orex.ld
 * it has to be compiled unoptimized for this to work.
 * since it's unoptimized, it's not recommended to put any useful 
 * code into this file.
 *
 * I seems that passing a function pointer in unoptimized code 
 * forces the rest of the executable to be loaded in a way that 
 * the function pointers work.
 */

void __blank__ ( void ) {
	return;
}

int __do_nothing__ ( void ) {
	return (int) &__blank__;
}


