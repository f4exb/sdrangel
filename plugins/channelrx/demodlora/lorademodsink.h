///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_LORADEMODSINK_H
#define INCLUDE_LORADEMODSINK_H

#include <vector>

#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "util/message.h"
#include "dsp/fftfilt.h"

#include "lorademodsettings.h"

class BasebandSampleSink;

class LoRaDemodSink : public ChannelSampleSink {
public:
    LoRaDemodSink();
	~LoRaDemodSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

	void setSpectrumSink(BasebandSampleSink* spectrumSink) { m_spectrumSink = spectrumSink; }
    void applyChannelSettings(int channelSampleRate, int bandwidth, int channelFrequencyOffset, bool force = false);
    void applySettings(const LoRaDemodSettings& settings, bool force = false);

private:
    LoRaDemodSettings m_settings;
	Real m_Bandwidth;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
	int m_chirp;
	int m_angle;
	int m_bin;
	int m_result;
	int m_count;
	int m_header;
	int m_time;
	short m_tune;

	sfft* loraFilter;
	sfft* negaFilter;
	float* mov;
	short* history;
	short* finetune;

	NCO m_nco;
	Interpolator m_interpolator;
	Real m_sampleDistanceRemain;

	BasebandSampleSink* m_spectrumSink;
	SampleVector m_sampleBuffer;

    static const int DATA_BITS;
    static const int SAMPLEBITS;
    static const int SPREADFACTOR;
    static const int LORA_SFFT_LEN;
    static const int LORA_SQUELCH;

	int  detect(Complex sample, Complex angle);
	void dumpRaw(void);
	short synch (short bin);

    /*
    Interleaving is "easiest" if the same number of bits is used per symbol as for FEC
    Chosen mode "spreading 8, low rate" has 6 bits per symbol, so use 4:6 FEC

    More spreading needs higher frequency resolution and longer time on air, increasing drift errors.
    Want higher bandwidth when using more spreading, which needs more CPU and a better FFT.

    Six bit Hamming can only correct long runs of drift errors when not using interleaving. Interleaving defeats the point of using Gray code and puts multiple bit errors into single FEC blocks. Hardware decoding uses RSSI to detect the symbols most likely to be damaged, so that individual bits can be repaired after de-interleaving.

    Using Implicit Mode: explicit starts with a 4:8 block and seems to have a different whitening sequence.
    */

    // Six bits per symbol, six chars per block
    inline void interleave6(char* inout, int size)
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

    inline short toGray(short num)
    {
            return (num >> 1) ^ num;
    }

    // Ignore the FEC bits, just extract the data bits
    inline void hamming6(char* c, int size)
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
    inline void prng6(char* inout, int size)
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
};

#endif // INCLUDE_LORADEMODSINK_H