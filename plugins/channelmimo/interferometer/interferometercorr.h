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

    void setCorrType(InterferometerSettings::CorrelationType corrType) { m_corrType = corrType; }
    InterferometerSettings::CorrelationType getCorrType() const { return m_corrType; }
    bool performCorr( //!< Returns true if results were produced
        const SampleVector& data0,
        int size0,
        const SampleVector& data1,
        int size1
    );
    int getFullFFTSize() const { return 2*m_fftSize; }

    SampleVector m_scorr; //!< raw correlation result (spectrum) - Sample vector expected
    SampleVector m_tcorr; //!< correlation result (time or spectrum inverse FFT) - Sample vector expected
    int m_processed;      //!< number of samples processed at the end of correlation
    int m_remaining[2];   //!< number of samples remaining per member at the end of correlation

signals:
    void dataReady(int start, int stop);

private:
    bool performOpCorr( //!< Returns true if results were produced
        const SampleVector& data0,
        int size0,
        const SampleVector& data1,
        int size1,
        Sample sampleOp(const Sample& a, const Sample& b)
    );
    bool performFFTCorr( //!< Returns true if results were produced
        const SampleVector& data0,
        int size0,
        const SampleVector& data1,
        int size1
    );
    void adjustSCorrSize(int size);
    void adjustTCorrSize(int size);

    InterferometerSettings::CorrelationType m_corrType;
    int m_fftSize;                   //!< FFT length
    FFTEngine *m_fft[2];             //!< FFT engines
    FFTEngine *m_invFFT;             //!< Inverse FFT engine
    FFTWindow m_window;              //!< FFT window
    std::complex<float> *m_dataj;    //!< conjuate of FFT transform
    int m_scorrSize;                 //!< spectrum correlations vector size
    int m_tcorrSize;                 //!< time correlations vector size
};

#endif // INCLUDE_INTERFEROMETERCORR_H
