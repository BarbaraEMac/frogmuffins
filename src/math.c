/**
 * Math Library
 */

int ctz( int x ) {
	if ( x == 0 ) return 32;
	int n = 0;
	int mask = 0xFFFF;
	if ( (x & mask) == 0 ) { n += 16; }
	mask = 0xFF << n;
	if ( (x & mask) == 0 ) { n += 8; }
	mask = 0xF << n;
	if ( (x & mask) == 0 ) { n += 4; }
	mask = 0x3 << n;
	if ( (x & mask) == 0 ) { n += 2; }
	mask = 0x1 << n;
	if ( (x & mask) == 0 ) { n += 1; }
	return n;
}

// The following function was copied from wikipedia
int log_2(unsigned int x) {
  int r = 0;
  while ((x >> r) != 0) {
    r++;
  }
  return r-1; // returns -1 for x==0, floor(log2(x)) otherwise
}


int abs ( int n ) {
	return (n > 0) ? n : -n ;
}
