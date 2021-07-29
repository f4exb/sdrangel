///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef PLUGINS_CHANNELTX_MODDATV_DATVMODREPORT_H_
#define PLUGINS_CHANNELTX_MODDATV_DATVMODREPORT_H_

#include <vector>
#include <QObject>
#include "util/message.h"

class DATVModReport : public QObject
{
    Q_OBJECT
public:
    class MsgReportTsFileSourceStreamTiming : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        int getFrameCount() const { return m_frameCount; }

        static MsgReportTsFileSourceStreamTiming* create(int frameCount)
        {
            return new MsgReportTsFileSourceStreamTiming(frameCount);
        }

    protected:
        int m_frameCount;

        MsgReportTsFileSourceStreamTiming(int frameCount) :
            Message(),
            m_frameCount(frameCount)
        { }
    };

    class MsgReportTsFileSourceStreamData : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getBitrate() const { return m_bitrate; }
        quint64 getStreamLength() const { return m_streamLength; }

        static MsgReportTsFileSourceStreamData* create(int bitrate,
                quint64 streamLength)
        {
            return new MsgReportTsFileSourceStreamData(bitrate, streamLength);
        }

    protected:
        int m_bitrate;
        int m_streamLength; //!< TS length in bytes

        MsgReportTsFileSourceStreamData(int bitrate,
                int streamLength) :
            Message(),
            m_bitrate(bitrate),
            m_streamLength(streamLength)
        { }
    };

    class MsgReportRates : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        int getChannelSampleRate() const { return m_channelSampleRate; }
        int getSampleRate() const { return m_sampleRate; }
        int getDataRate() const { return m_dataRate; }

        static MsgReportRates* create(int channelSampleRate, int sampleRate, int dataRate)
        {
            return new MsgReportRates(channelSampleRate, sampleRate, dataRate);
        }

    protected:
        int m_channelSampleRate;
        int m_sampleRate;
        int m_dataRate;

        MsgReportRates(
                int channelSampleRate,
                int sampleRate,
                int dataRate) :
            Message(),
            m_channelSampleRate(channelSampleRate),
            m_sampleRate(sampleRate),
            m_dataRate(dataRate)
        { }
    };

    class MsgReportUDPBitrate : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        int getBitrate() const { return m_bitrate; }

        static MsgReportUDPBitrate* create(int bitrate)
        {
            return new MsgReportUDPBitrate(bitrate);
        }

    protected:
        int m_bitrate;

        MsgReportUDPBitrate(
                int bitrate) :
            Message(),
            m_bitrate(bitrate)
        { }
    };

    class MsgReportUDPBufferUtilization : public Message
    {
        MESSAGE_CLASS_DECLARATION

    public:
        float getUtilization() const { return m_utilization; }

        static MsgReportUDPBufferUtilization* create(int utilization)
        {
            return new MsgReportUDPBufferUtilization(utilization);
        }

    protected:
        float m_utilization;

        MsgReportUDPBufferUtilization(int utilization) :
            Message(),
            m_utilization(utilization)
        { }
    };

public:
    DATVModReport();
    ~DATVModReport();
};

#endif // PLUGINS_CHANNELTX_MODDATV_DATVMODREPORT_H_
