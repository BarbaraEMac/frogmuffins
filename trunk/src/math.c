int ctz( int x ) {
	if ( x == 0 ) return 32;
	int n = 32;
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
