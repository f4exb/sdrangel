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

#ifndef INCLUDE_INTERFEROMETERCORR_H
#define INCLUDE_INTERFEROMETERCORR_H

#include <complex>
#include <QObject>

#include "dsp/dsptypes.h"
#include "dsp/fftwindow.h"
#include "util/message.h"

#include "interferometersettings.h"

class FFTEngine;

class InterferometerCorrelator : public QObject {
  	Q_OBJECT
public:
    InterferometerCorrelator(int fftSize);
    ~InterferometerCorrelator();

    void setCorrIndex(unsigned int corrIndex) { m_corrIndex = corrIndex; }
    void setCorrType(InterferometerSettings::CorrelationType corrType) { m_corrType = corrType; }
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, unsigned int argIndex);

    SampleVector m_scorr; //!< raw correlation result (spectrum) - Sample vector expected
    SampleVector m_tcorr; //!< correlation result (time or spectrum inverse FFT) - Sample vector expected

signals:
    void dataReady(int start, int stop);

private:
    void feedOp(
        const SampleVector::const_iterator& begin,
        const SampleVector::const_iterator& end,
        unsigned int argIndex,
        Sample complexOp(std::complex<float>& a, const std::complex<float>& b)
    );
    void feedCorr(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, unsigned int argIndex);
    void processFFTBlocks(unsigned int argIndex, unsigned int dataIndex, int length);
    void processFFT(unsigned int argIndex, int blockIndex);

    InterferometerSettings::CorrelationType m_corrType;
    int m_fftSize;                   //!< FFT length
    FFTEngine *m_fft[2];             //!< FFT engines
    FFTEngine *m_invFFT;             //!< Inverse FFT engine
    FFTWindow m_window;              //!< FFT window
    std::complex<float> *m_data[2];  //!< from input
    std::complex<float> *m_dataj;    //!< conjuate of FFT transform
    unsigned int m_dataIndex[2];     //!< Current sample index in A
    unsigned int m_corrIndex;        //!< Input index on which correlation is actioned
    static const unsigned int m_nbFFTBlocks; //!< number of buffered FFT blocks
};

#endif // INCLUDE_INTERFEROMETERCORR_H
