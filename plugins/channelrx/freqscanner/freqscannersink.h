///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_FREQSCANNERSINK_H
#define INCLUDE_FREQSCANNERSINK_H

#include <QDateTime>

#include "dsp/channelsamplesink.h"
#include "dsp/nco.h"
#include "dsp/interpolator.h"
#include "dsp/fftfactory.h"
#include "dsp/fftengine.h"
#include "dsp/fftwindow.h"
#include "util/fixedaverage2d.h"
#include "util/messagequeue.h"

#include "freqscannersettings.h"

class ChannelAPI;
class FreqScanner;

class FreqScannerSink : public ChannelSampleSink {
public:
    FreqScannerSink(FreqScanner *packetDemod);
    ~FreqScannerSink();

    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, int scannerSampleRate, int fftSize, int binsPerChannel, bool force = false);
    void applySettings(const FreqScannerSettings& settings, const QStringList& settingsKeys, bool force = false);
    void setMessageQueueToChannel(MessageQueue *messageQueue) { m_messageQueueToChannel = messageQueue; }
    void setChannel(ChannelAPI *channel) { m_channel = channel; }
    void setCenterFrequency(qint64 centerFrequency) { m_centerFrequency = centerFrequency; }

private:

    FreqScanner *m_freqScanner;
    FreqScannerSettings m_settings;
    ChannelAPI *m_channel;
    int m_channelSampleRate;
    int m_channelFrequencyOffset;
    int m_scannerSampleRate; // Sample rate scanner runs at
    qint64 m_centerFrequency;

    NCO m_nco;
    Interpolator m_interpolator;
    Real m_interpolatorDistance;
    Real m_interpolatorDistanceRemain;

    MessageQueue *m_messageQueueToChannel;

    int m_fftSequence;
    FFTEngine *m_fft;
    int m_fftCounter;
    FFTWindow m_fftWindow;
    int m_fftSize;
    int m_binsPerChannel;
    QDateTime m_fftStartTime;
    FixedAverage2D<Real> m_fftAverage;     // magSq average
    QVector<Real> m_magSq;
    int m_averageCount;

    void processOneSample(Complex &ci);
    MessageQueue *getMessageQueueToChannel() { return m_messageQueueToChannel; }
    Real totalPower(int bin) const;
    Real peakPower(int bin) const;
    Real magSq(int bin) const;
};

#endif // INCLUDE_FREQSCANNERSINK_H
