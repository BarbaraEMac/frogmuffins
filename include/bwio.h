/*
 * bwio.h
 */

#define COM1	0
#define COM2	1

#define ON	1
#define	OFF	0

int bwsendc( int channel, char c, int timeout );

int bwputc( int channel, char c );

int bwgetc( int channel );

int bwreadc( int channel, char *c, int timeout );

void bwclear( int channel );

int bwputx( int channel, char c );

int bwputstr( int channel, char *str );

int bwputr( int channel, unsigned int reg );

void bwputw( int channel, int n, char fc, char *bf );

void bwprintf( int channel, const char *format, ... );
