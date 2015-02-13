/*
 Interleaving is "easiest" if the same number of bits is used per symbol as for FEC
 Chosen mode "spreading 8, low rate" has 6 bits per symbol, so use 4:6 FEC

 More spreading needs higher frequency resolution and longer time on air, increasing drift errors.
 Want higher bandwidth when using more spreading, which needs more CPU and a better FFT.

 Six bit Hamming can only correct drift errors. Need 7 or 8 bit FEC for QRM
 Easier to use 5:4 FEC with Reed-Soloman messages.

 Implicit Mode: explicit starts with a 4:8 block and has different whitening
*/

// Six bits per symbol, six chars per block
void LoRaDemod::interleave6(char* inout, int size)
{
	int i, j;
	char in[6 * 2];
	short s;

	for (j = 0; j < size; j+=6) {
		for (i = 0; i < 6; i++)
			in[i] = in[i + 6] = inout[i + j];
		// top bits are swapped
		for (i = 0; i < 6; i++) {
			s = (32 & in[2 + i]) | (16 & in[1 + i]) | (8 & in[3 + i])
				| (4 & in[4 + i]) | (2 & in[5 + i]) | (1 & in[6 + i]);
			// bits are also  rotated
			s = (s << 3) | (s >> 3);
			s &= 63;
			s = (s >> i) | (s << (6 - i));
			inout[i + j] = s & 63;
		}
	}
}

short LoRaDemod::toGray(short num)
{
        return (num >> 1) ^ num;
}

// ignore FEC, try to extract raw bits
void LoRaDemod::hamming6(char* c, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		c[i] = ((c[i] & 1)<<3) | ((c[i] & 2)<<0) | ((c[i] & 4)>>0) | ((c[i] & 8)>>3);
		i++;
		c[i] = ((c[i] & 1)<<2) | ((c[i] & 2)<<2) | ((c[i] & 4)>>1) | ((c[i] & 8)>>3);
		i++;
		c[i] = ((c[i] &32)>>2) | ((c[i] & 2)<<1) | ((c[i] & 4)>>1) | ((c[i] & 8)>>3);
		i++;
		c[i] = ((c[i] & 1)<<3) | ((c[i] & 2)<<1) | ((c[i] & 4)>>1) | ((c[i] & 8)>>3);
		i++;
		c[i] = ((c[i] & 1)<<3) | ((c[i] & 2)<<1) | ((c[i] & 4)>>1) | ((c[i] &16)>>4);
		i++;
		c[i] = ((c[i] & 1)<<3) | ((c[i] & 2)<<1) | ((c[i] & 4)>>2) | ((c[i] & 8)>>2);
	}
	c[i] = 0;
}

// data whitening (6 bit)
void LoRaDemod::prng6(char* inout, int size)
{
	const char otp[] = {
	//explicit mode
	"cOGGg7CM2=b5a?<`i;T2of5jDAB=2DoQ9ko?h_RLQR4@Z\\`9jY\\PX89lHX8h_R]c_^@OB<0`W08ik?Mg>dQZf3kn5Je5R=R4h[<Ph90HHh9j;h:mS^?f:lQ:GG;nU:b?WFU20Lf4@A?`hYJMnW\\QZ\\AMIZ<h:jQk[PP<`6[Z"
#if 0
	// implicit mode (offset 2 symbols)
	"5^ZSm0=cOGMgUB=bNcb<@a^T;_f=6DEB]2ImPIKg:j]RlYT4YZ<`9hZ\\PPb;@8X8i]Zmc_6B52\\8oUPHIcBOc>dY?d9[n5Lg]b]R8hR<0`T008h9c9QJm[c?a:lQEGa;nU=b_WfUV2?V4@c=8h9B9njlQZDC@9Z<Q8\\iiX\\Rb6k:iY"
#endif
	};
	int i, maxchars;

	maxchars = sizeof( otp );
	if (size < maxchars)
		maxchars = size;
	for (i = 0; i < maxchars; i++)
		inout[i] ^= (otp[i] - 48);
}

