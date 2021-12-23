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
#include <QThread>

#include "dsp/channelsamplesink.h"
#include "channel/remotedatablock.h"


#include "remotesinksettings.h"

class DeviceSampleSource;
class RemoteSinkSender;

class RemoteSinkSink : public QObject, public ChannelSampleSink {
    Q_OBJECT
public:
    RemoteSinkSink();
	~RemoteSinkSink();

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end);

    void start();
    void stop();
    void init();

    void setNbTxBytes(uint32_t nbTxBytes) { m_nbTxBytes = nbTxBytes; }
    void applySettings(const RemoteSinkSettings& settings, bool force = false);
    void applyBasebandSampleRate(uint32_t sampleRate);
    void setDeviceCenterFrequency(uint64_t frequency) { m_deviceCenterFrequency = frequency; }
    void setDeviceIndex(uint32_t deviceIndex) { m_deviceIndex = deviceIndex; }
    void setChannelIndex(uint32_t channelIndex) { m_channelIndex = channelIndex; }

private:
    RemoteSinkSettings m_settings;
    bool m_running;
    QThread m_senderThread;
    RemoteSinkSender *m_remoteSinkSender;

    int m_txBlockIndex;                  //!< Current index in blocks to transmit in the Tx row
    uint16_t m_frameCount;               //!< transmission frame count
    int m_sampleIndex;                   //!< Current sample index in protected block data
    RemoteSuperBlock m_superBlock;
    RemoteMetaDataFEC m_currentMetaFEC;
    RemoteDataFrame *m_dataFrame;

    uint64_t m_deviceCenterFrequency;
    int64_t m_frequencyOffset;
    uint32_t m_basebandSampleRate;
    uint32_t m_deviceIndex;
    uint32_t m_channelIndex;
    int m_nbBlocksFEC;
    uint32_t m_nbTxBytes;
    QString m_dataAddress;
    uint16_t m_dataPort;

    void startSender();
    void stopSender();
    void setNbBlocksFEC(int nbBlocksFEC);
    uint32_t getNbSampleBits();

    inline void convertSampleToData(const SampleVector::const_iterator& begin, int nbSamples, bool isTx)
    {
        if (sizeof(Sample) == m_nbTxBytes * 2) // 16 -> 16 or 24 ->24: direct copy
        {
            memcpy((void *) &m_superBlock.m_protectedBlock.buf[m_sampleIndex*m_nbTxBytes*2],
                    (const void *) &(*(begin)),
                    nbSamples * sizeof(Sample));
        }
        else if (isTx)
        {
            if (m_nbTxBytes == 4) // just convert type int16_t -> int32_t (always 16 bit wide)
            {
                for (int i = 0; i < nbSamples; i++)
                {
                    *((int32_t*) &m_superBlock.m_protectedBlock.buf[(m_sampleIndex+ i)*m_nbTxBytes*2]) = (begin+i)->m_real;
                    *((int32_t*) &m_superBlock.m_protectedBlock.buf[(m_sampleIndex+ i)*m_nbTxBytes*2 + m_nbTxBytes]) = (begin+i)->m_imag;
                }
            }
            else if (m_nbTxBytes == 2) //just convert type int32_t -> int16_t (always 16 bit wide)
            {
                for (int i = 0; i < nbSamples; i++)
                {
                    *((int16_t*) &m_superBlock.m_protectedBlock.buf[(m_sampleIndex+ i)*m_nbTxBytes*2]) = (begin+i)->m_real;
                    *((int16_t*) &m_superBlock.m_protectedBlock.buf[(m_sampleIndex+ i)*m_nbTxBytes*2 + m_nbTxBytes]) = (begin+i)->m_imag;
                }
            }
            else if (m_nbTxBytes == 1) // 16 or 24 -> 8
            {
                for (int i = 0; i < nbSamples; i++)
                {
                    *((int8_t*) &m_superBlock.m_protectedBlock.buf[(m_sampleIndex+ i)*m_nbTxBytes*2]) = (int8_t) ((begin+i)->m_real / (1<<8));
                    *((int8_t*) &m_superBlock.m_protectedBlock.buf[(m_sampleIndex+ i)*m_nbTxBytes*2 + m_nbTxBytes]) = (int8_t) ((begin+i)->m_imag / (1<<8));
                }
            }
        }
        else
        {
            if (m_nbTxBytes == 4) // 16 -> 24
            {
                for (int i = 0; i < nbSamples; i++)
                {
                    *((int32_t*) &m_superBlock.m_protectedBlock.buf[(m_sampleIndex+ i)*m_nbTxBytes*2]) = (begin+i)->m_real * (1<<8);
                    *((int32_t*) &m_superBlock.m_protectedBlock.buf[(m_sampleIndex+ i)*m_nbTxBytes*2 + m_nbTxBytes]) = (begin+i)->m_imag * (1<<8);
                }
            }
            else if (m_nbTxBytes == 2) // 24 -> 16
            {
                for (int i = 0; i < nbSamples; i++)
                {
                    *((int16_t*) &m_superBlock.m_protectedBlock.buf[(m_sampleIndex+ i)*m_nbTxBytes*2]) = (begin+i)->m_real / (1<<8);
                    *((int16_t*) &m_superBlock.m_protectedBlock.buf[(m_sampleIndex+ i)*m_nbTxBytes*2 + m_nbTxBytes]) = (begin+i)->m_imag / (1<<8);
                }
            }
            else if ((m_nbTxBytes == 1) && (sizeof(Sample) == 4)) // 16 -> 8
            {
                for (int i = 0; i < nbSamples; i++)
                {
                    *((int8_t*) &m_superBlock.m_protectedBlock.buf[(m_sampleIndex+ i)*m_nbTxBytes*2]) = (int8_t) ((begin+i)->m_real / (1<<8));
                    *((int8_t*) &m_superBlock.m_protectedBlock.buf[(m_sampleIndex+ i)*m_nbTxBytes*2 + m_nbTxBytes]) = (int8_t) ((begin+i)->m_imag / (1<<8));
                }
            }
            else if ((m_nbTxBytes == 1) && (sizeof(Sample) == 8)) // 24 -> 8
            {
                for (int i = 0; i < nbSamples; i++)
                {
                    *((int8_t*) &m_superBlock.m_protectedBlock.buf[(m_sampleIndex+ i)*m_nbTxBytes*2]) = (int8_t) ((begin+i)->m_real / (1<<16));
                    *((int8_t*) &m_superBlock.m_protectedBlock.buf[(m_sampleIndex+ i)*m_nbTxBytes*2 + m_nbTxBytes]) = (int8_t) ((begin+i)->m_imag / (1<<16));
                }
            }
        }
    }
};

#endif // INCLUDE_REMOTESINKSINK_H_
