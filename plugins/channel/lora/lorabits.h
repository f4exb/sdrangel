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
		in[i] = inout[i];
	// top bits are swapped ?
	for (i = 0; i < index; i++) {
		inout[i] = (32 & in[1 + i]) | (16 & in[0 + i]) | (8 & in[2 + i])
				| (4 & in[3 + i]) | (2 & in[4 + i]) | (1 & in[5 + i]);
		in[i + index] = in[i];
	}
}

short LoRaDemod::toGray(short num)
{
        return (num >> 1) ^ num;
}

