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

#ifndef INCLUDE_LOCALSINKSINK_H_
#define INCLUDE_LOCALSINKSINK_H_

#include <QObject>
#include <QThread>

#include "dsp/channelsamplesink.h"

#include "localsinksettings.h"

class DeviceSampleSource;
class LocalSinkWorker;
class SpectrumVis;
class fftfilt;

class LocalSinkSink : public QObject, public ChannelSampleSink {
    Q_OBJECT
public:
    LocalSinkSink();
	~LocalSinkSink();

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void setSpectrumSink(SpectrumVis* spectrumSink) { m_spectrumSink = spectrumSink; }
    void applySettings(const LocalSinkSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void start(DeviceSampleSource *deviceSource);
    void stop();
    bool isRunning() const { return m_running; }
    void setSampleRate(int sampleRate);

private:
    DeviceSampleSource *m_deviceSource;
    SampleSinkFifo m_sampleFifo;
    LocalSinkSettings m_settings;
    LocalSinkWorker *m_sinkWorker;
    QThread m_sinkWorkerThread;
    SpectrumVis* m_spectrumSink;
    SampleVector m_spectrumBuffer;
    bool m_running;
    float m_gain; //!< Amplitude gain
    fftfilt* m_fftFilter;

    uint64_t m_centerFrequency;
    int64_t m_frequencyOffset;
    uint32_t m_sampleRate;
    uint32_t m_deviceSampleRate;

	void startWorker();
	void stopWorker();
};

#endif // INCLUDE_LOCALSINKSINK_H_
