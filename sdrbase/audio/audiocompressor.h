///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef SDRBASE_AUDIO_AUDIOCOMPRESSOR_H_
#define SDRBASE_AUDIO_AUDIOCOMPRESSOR_H_

#include <stdint.h>
#include "export.h"

class SDRBASE_API AudioCompressor
{
public:
    AudioCompressor();
    ~AudioCompressor();
    void fillLUT();   //!< 4 bands
    void fillLUT2();  //!< 8 bands (default)
    void fillALaw();  //!< A-law compression to 8 bits
    void fillULaw();  //!< u-law compression to 8 bits
    int16_t compress(int16_t sample);
    int8_t compress8(int16_t sample);

private:
    int8_t ALaw_Encode(int16_t number);
    int8_t MuLaw_Encode(int16_t number);

    int16_t m_lut[32768];
    static const uint16_t ALAW_MAX;
    static const uint16_t MULAW_MAX;
    static const uint16_t MULAW_BIAS;

};



#endif /* SDRBASE_AUDIO_AUDIOCOMPRESSOR_H_ */
