//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Edouard Griffiths, F4EXB                                                              //
//                                                                                                          //
// This program is free software; you can redistribute it and/or modify                                     //
// it under the terms of the GNU General Public License as published by                                     //
// the Free Software Foundation as version 3 of the License, or                                             //
// (at your option) any later version.                                                                      //
//                                                                                                          //
// This program is distributed in the hope that it will be useful,                                          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                                           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                                             //
// GNU General Public License V3 for more details.                                                          //
//                                                                                                          //
// You should have received a copy of the GNU General Public License                                        //
// along with this program. If not, see <http://www.gnu.org/licenses/>.                                     //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_DSP_DCSDETECTOR_H_
#define INCLUDE_DSP_DCSDETECTOR_H_

#include <QMutex>

#include "dsp/dsptypes.h"
#include "util/golay2312.h"

class DCSDetector {
public:
    DCSDetector();
    ~DCSDetector();

    bool analyze(Real *sample, unsigned int& dcsCode); //!< input signal sample
    void setBitrate(float bitrate);
    void setSampleRate(int sampleRate);
    void setEqWindow(int nbBits);

private:
    float m_bitsPerSample;
    float m_samplesPerBit;
    float m_bitIndex;
    float m_bitrate;
    float m_sampleRate;
    float *m_eqSamples;
    int m_eqBits;
    int m_eqSize;
    int m_eqIndex;
    float m_high;
    float m_low;
    float m_mid;
    float m_prevSample;
    unsigned int m_dcsWord; //!< 23 bit DCS code word
    Golay2312 m_golay2312;
    QMutex m_mutex;
};

#endif // INCLUDE_DSP_DCSDETECTOR_H_
