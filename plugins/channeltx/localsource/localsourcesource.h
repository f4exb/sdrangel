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

#ifndef PLUGINS_CHANNELTX_LOCALSOURCE_LOCALSOURCESOURCE_H_
#define PLUGINS_CHANNELTX_LOCALSOURCE_LOCALSOURCESOURCE_H_

#include <QObject>
#include <QThread>

#include "dsp/channelsamplesource.h"
#include "localsourcesettings.h"

class DeviceSampleSink;
class SampleSourceFifo;
class LocalSourceWorker;

class LocalSourceSource : public QObject, public ChannelSampleSource {
    Q_OBJECT
public:
    LocalSourceSource();
    ~LocalSourceSource();

    virtual void pull(SampleVector::iterator begin, unsigned int nbSamples);
    virtual void pullOne(Sample& sample);
    virtual void prefetch(unsigned int nbSamples) { (void) nbSamples; }

    void start(DeviceSampleSink *deviceSink);
    void stop();
    bool isRunning() const { return m_running; }

signals:
    void pullSamples(unsigned int count);

private:
    bool m_running;
    LocalSourceWorker *m_sinkWorker;
    QThread m_sinkWorkerThread;
    SampleSourceFifo *m_localSampleSourceFifo;
    int m_chunkSize;
    SampleVector m_localSamples;
    int m_localSamplesIndex;
    int m_localSamplesIndexOffset;

    void startWorker();
    void stopWorker();

private slots:
    void processSamples(unsigned int iPart1Begin, unsigned int iPart1End, unsigned int iPart2Begin, unsigned int iPart2End);
};


#endif // PLUGINS_CHANNELTX_LOCALSOURCE_LOCALSOURCESOURCE_H_
