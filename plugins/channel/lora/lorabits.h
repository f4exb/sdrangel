/*
 Interleaving is "easiest" if the same number of bits is used per symbol as for FEC
 Chosen mode "spreading 8, low rate" has 6 bits per symbol, so use 4:6 FEC
*/

// Needs adjusting for different sizes
void LoRaDemod::interleave(char* inout)
{
	int i, index = 6;
	char in[index * 2];
	for (i = 0; i < index; i++)
		in[i] = in[i + index] = inout[i];
	// top bits are swapped ?
	for (i = 0; i < index; i++) {
		inout[i] = (32 & in[2 + i]) | (16 & in[1 + i]) | (8 & in[3 + i])
				| (4 & in[4 + i]) | (2 & in[5 + i]) | (1 & in[6 + i]);
	}
}

short LoRaDemod::toGray(short num)
{
        return (num >> 1) ^ num;
}

// ignore FEC, data in lsb
void LoRaDemod::hamming(char* inout, int size)
{
	int i;
	char c;
	for (i = 0; i < size; i++) {
		c = inout[i];
		c = ((c&1)<<3) | ((c&2)<<1) | ((c&4)>>1) | ((c&8)>>3);
		inout[i] = c;
	}
	inout[i] = 0;
}

// data whitening
void LoRaDemod::prng(char* inout, int size)
{
	const char otp[] = {
	"                                                                                                                                                                              "
	};
	int i, maxchars;

	maxchars = sizeof( otp );
	if (size < maxchars)
		maxchars = size;
	for (i = 0; i < maxchars; i++)
		inout[i] ^= otp[i] - 32;
}

