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

#ifndef INCLUDE_REMOTESINKSINK_H_
#define INCLUDE_REMOTESINKSINK_H_

#include <QObject>

#include "dsp/channelsamplesink.h"
#include "channel/remotedatablock.h"


#include "remotesinksettings.h"

class DeviceSampleSource;
class RemoteSinkSender;
class QThread;

class RemoteSinkSink : public QObject, public ChannelSampleSink {
    Q_OBJECT
public:
    RemoteSinkSink();
	~RemoteSinkSink();

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void startSender();
    void stopSender();

    void applySettings(const RemoteSinkSettings& settings, bool force = false);
    void applyBasebandSampleRate(uint32_t sampleRate);
    void setDeviceCenterFrequency(uint64_t frequency) { m_deviceCenterFrequency = frequency; }

private:
    RemoteSinkSettings m_settings;
    QThread *m_senderThread;
    RemoteSinkSender *m_remoteSinkSender;

    int m_txBlockIndex;                  //!< Current index in blocks to transmit in the Tx row
    uint16_t m_frameCount;               //!< transmission frame count
    int m_sampleIndex;                   //!< Current sample index in protected block data
    RemoteSuperBlock m_superBlock;
    RemoteMetaDataFEC m_currentMetaFEC;
    RemoteDataBlock *m_dataBlock;

    uint64_t m_deviceCenterFrequency;
    int64_t m_frequencyOffset;
    uint32_t m_basebandSampleRate;
    int m_nbBlocksFEC;
    int m_txDelay;
    QString m_dataAddress;
    uint16_t m_dataPort;

    void setNbBlocksFEC(int nbBlocksFEC);
    void setTxDelay(int txDelay, int nbBlocksFEC, int log2Decim);
};

#endif // INCLUDE_REMOTESINKSINK_H_