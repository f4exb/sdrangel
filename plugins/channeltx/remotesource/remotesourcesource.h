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

#ifndef PLUGINS_CHANNELTX_REMOTESOURCE_REMOTESOURCESOURCE_H_
#define PLUGINS_CHANNELTX_REMOTESOURCE_REMOTESOURCESOURCE_H_

#include <QObject>
#include <QThread>

#include "cm256cc/cm256.h"

#include "dsp/channelsamplesource.h"
#include "channel/remotedatablock.h"
#include "channel/remotedataqueue.h"
#include "channel/remotedatareadqueue.h"

#include "remotesourcesettings.h"

class RemoteSourceWorker;

class RemoteSourceSource : public QObject, public ChannelSampleSource {
    Q_OBJECT
public:
    RemoteSourceSource();
    ~RemoteSourceSource();

    virtual void pull(SampleVector::iterator begin, unsigned int nbSamples);
    virtual void pullOne(Sample& sample);
    virtual void prefetch(unsigned int nbSamples) { (void) nbSamples; }

    void start();
    void stop();
    bool isRunning() const { return m_running; }
    RemoteDataReadQueue& getDataQueue() { return m_dataReadQueue; }
    void dataBind(const QString& m_dataAddress, uint16_t dataPort);
    uint32_t getNbCorrectableErrors() const { return m_nbCorrectableErrors; }
    uint32_t getNbUncorrectableErrors() const { return m_nbUncorrectableErrors; }
    const RemoteMetaDataFEC& getRemoteMetaDataFEC() const { return m_currentMeta; }

    void applyChannelSettings(int channelSampleRate, bool force = false);

signals:
    void newRemoteSampleRate(unsigned int sampleRate);

private:
    bool m_running;
    RemoteSourceWorker *m_sourceWorker;
    QThread m_sourceWorkerThread;
    RemoteDataQueue m_dataQueue;
    RemoteDataReadQueue m_dataReadQueue;
    CM256 m_cm256;
    CM256 *m_cm256p;
    RemoteSourceSettings m_settings;
    CM256::cm256_block   m_cm256DescriptorBlocks[2*RemoteNbOrginalBlocks]; //!< CM256 decoder descriptors (block addresses and block indexes)
    RemoteMetaDataFEC m_currentMeta;
    uint32_t m_nbCorrectableErrors;   //!< count of correctable errors in number of blocks
    uint32_t m_nbUncorrectableErrors; //!< count of uncorrectable errors in number of blocks

    int m_channelSampleRate;

    void startWorker();
    void stopWorker();
    void handleDataFrame(RemoteDataFrame *dataFrame);
    void printMeta(const QString& header, RemoteMetaDataFEC *metaData);

private slots:
    void handleData();
};

#endif // PLUGINS_CHANNELTX_REMOTESOURCE_REMOTESOURCESOURCE_H_
