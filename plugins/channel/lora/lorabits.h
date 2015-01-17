/*
 Interleaving is "easiest" if the same number of bits is used per symbol as for FEC
 Chosen mode "spreading 8, low rate" has 6 bits per symbol, so use 4:6 FEC
*/

// Needs adjusting for different sizes
void interleave(short* inout)
{
	int i, index = 6;
	short in[index * 2];
	for (i = 0; i < index; i++)
		in[i] = inout[i];
	for (i = 0; i < index; i++) {
		inout[i] = (1 & in[0 + i]) | (2 & in[1 + i]) | (4 & in[2 + i])
			| (8 & in[3 + i]) | (16 & in[4 + i]) | (32 & in[5 + i]); 
		in[i + index] = in[i];
	}
}

// Same sequence for any size
void make_gray(void)
{
	short gray[1<<8];
//	short ungray[1<<8];
	short k = 0;
	for (short i = 0; i < 1<<8; i++) {
		gray[i] = k;
//		ungray[k] = i;
		short r = (i+1) & ~i;
		k ^= r;
	}
//	for (short i = 0; i < 1<<5; i++)
//		printf("%x:%x:%x.\n", i, gray[i], ungray[gray[i]]);
}


