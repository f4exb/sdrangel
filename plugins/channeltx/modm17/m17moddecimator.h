///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>                   //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
// Polyphase decimation filter.
//
// Convert an oversampled audio stream to non-oversampled.  Uses a
// windowed sinc FIR filter w/ Blackman window to control aliasing.
// Christian Floisand's 'blog explains it very well.
//
// This version has a very simple main processing loop (the decimate
// method) which vectorizes easily.
//
// Refs:
//   https://christianfloisand.wordpress.com/2012/12/05/audio-resampling-part-1/
//   https://christianfloisand.wordpress.com/2013/01/28/audio-resampling-part-2/
//   http://www.dspguide.com/ch16.htm
//   http://en.wikipedia.org/wiki/Window_function#Blackman_windows

#ifndef INCLUDE_M17MODDECIMATOR_H
#define INCLUDE_M17MODDECIMATOR_H

#include <cstdint>

class M17ModDecimator {

public:
    M17ModDecimator();
    ~M17ModDecimator();

    void initialize(
        double   decimatedSampleRate,
        double   passFrequency,
        unsigned oversampleRatio
    );

    double oversampleRate()  const { return mOversampleRate; }
    int oversampleRatio() const { return mRatio; }

    void decimate(int16_t *in, int16_t *out, unsigned int outCount);
    // N.B., input must have (ratio * outCount) samples.

private:
    double mDecimatedSampleRate;
    double mOversampleRate;
    int mRatio;                 // oversample ratio
    float *mKernel;
    unsigned int mKernelSize;
    float *mShift;              // shift register
    unsigned int mCursor;
};

#endif // INCLUDE_M17MODDECIMATOR_H
