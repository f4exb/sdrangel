///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_SAMPLESOURCE_LIMESDROUTPUT_LIMESDROUTPUT_H_
#define PLUGINS_SAMPLESOURCE_LIMESDROUTPUT_LIMESDROUTPUT_H_

#include <QString>
#include <stdint.h>

#include "dsp/devicesamplesink.h"
#include "limesdr/devicelimesdrshared.h"
#include "limesdroutputsettings.h"

class DeviceSinkAPI;
class LimeSDROutputThread;
struct DeviceLimeSDRParams;

class LimeSDROutput : public DeviceSampleSink
{
public:
    class MsgConfigureLimeSDR : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const LimeSDROutputSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureLimeSDR* create(const LimeSDROutputSettings& settings, bool force)
        {
            return new MsgConfigureLimeSDR(settings, force);
        }

    private:
        LimeSDROutputSettings m_settings;
        bool m_force;

        MsgConfigureLimeSDR(const LimeSDROutputSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgStartStop : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getStartStop() const { return m_startStop; }

        static MsgStartStop* create(bool startStop) {
            return new MsgStartStop(startStop);
        }

    protected:
        bool m_startStop;

        MsgStartStop(bool startStop) :
            Message(),
            m_startStop(startStop)
        { }
    };

    class MsgGetStreamInfo : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgGetStreamInfo* create()
        {
            return new MsgGetStreamInfo();
        }

    private:
        MsgGetStreamInfo() :
            Message()
        { }
    };

    class MsgGetDeviceInfo : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgGetDeviceInfo* create()
        {
            return new MsgGetDeviceInfo();
        }

    private:
        MsgGetDeviceInfo() :
            Message()
        { }
    };

    class MsgReportStreamInfo : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool     getSuccess() const { return m_success; }
        bool     getActive() const { return m_active; }
        uint32_t getFifoFilledCount() const { return m_fifoFilledCount; }
        uint32_t getFifoSize() const { return m_fifoSize; }
        uint32_t getUnderrun() const { return m_underrun; }
        uint32_t getOverrun() const { return m_overrun; }
        uint32_t getDroppedPackets() const { return m_droppedPackets; }
        float    getSampleRate() const { return m_sampleRate; }
        float    getLinkRate() const { return m_linkRate; }
        uint64_t getTimestamp() const { return m_timestamp; }

        static MsgReportStreamInfo* create(
                bool     success,
                bool     active,
                uint32_t fifoFilledCount,
                uint32_t fifoSize,
                uint32_t underrun,
                uint32_t overrun,
                uint32_t droppedPackets,
                float    sampleRate,
                float    linkRate,
                uint64_t timestamp
                )
        {
            return new MsgReportStreamInfo(
                    success,
                    active,
                    fifoFilledCount,
                    fifoSize,
                    underrun,
                    overrun,
                    droppedPackets,
                    sampleRate,
                    linkRate,
                    timestamp
                    );
        }

    private:
        bool     m_success;
        // everything from lms_stream_status_t
        bool     m_active; //!< Indicates whether the stream is currently active
        uint32_t m_fifoFilledCount; //!< Number of samples in FIFO buffer
        uint32_t m_fifoSize; //!< Size of FIFO buffer
        uint32_t m_underrun; //!< FIFO underrun count
        uint32_t m_overrun; //!< FIFO overrun count
        uint32_t m_droppedPackets; //!< Number of dropped packets by HW
        float    m_sampleRate; //!< Sampling rate of the stream
        float    m_linkRate; //!< Combined data rate of all stream of the same direction (TX or RX)
        uint64_t m_timestamp; //!< Current HW timestamp

        MsgReportStreamInfo(
                bool     success,
                bool     active,
                uint32_t fifoFilledCount,
                uint32_t fifoSize,
                uint32_t underrun,
                uint32_t overrun,
                uint32_t droppedPackets,
                float    sampleRate,
                float    linkRate,
                uint64_t timestamp
                ) :
            Message(),
            m_success(success),
            m_active(active),
            m_fifoFilledCount(fifoFilledCount),
            m_fifoSize(fifoSize),
            m_underrun(underrun),
            m_overrun(overrun),
            m_droppedPackets(droppedPackets),
            m_sampleRate(sampleRate),
            m_linkRate(linkRate),
            m_timestamp(timestamp)
        { }
    };

    LimeSDROutput(DeviceSinkAPI *deviceAPI);
    virtual ~LimeSDROutput();
    virtual void destroy();

    virtual void init();
    virtual bool start();
    virtual void stop();

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
    virtual const QString& getDeviceDescription() const;
    virtual int getSampleRate() const;
    virtual quint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);

    virtual bool handleMessage(const Message& message);

    virtual int webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage);

    virtual int webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage);

    virtual int webapiRunGet(
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    virtual int webapiRun(
            bool run,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    std::size_t getChannelIndex();
    void getLORange(float& minF, float& maxF, float& stepF) const;
    void getSRRange(float& minF, float& maxF, float& stepF) const;
    void getLPRange(float& minF, float& maxF, float& stepF) const;
    uint32_t getHWLog2Interp() const;

private:
    DeviceSinkAPI *m_deviceAPI;
    QMutex m_mutex;
    LimeSDROutputSettings m_settings;
    LimeSDROutputThread* m_limeSDROutputThread;
    QString m_deviceDescription;
    bool m_running;
    DeviceLimeSDRShared m_deviceShared;
    bool m_channelAcquired;

    lms_stream_t m_streamId;

    bool openDevice();
    void closeDevice();
    bool acquireChannel();
    void releaseChannel();
    void suspendRxBuddies();
    void resumeRxBuddies();
    void suspendTxBuddies();
    void resumeTxBuddies();
    bool applySettings(const LimeSDROutputSettings& settings, bool force = false, bool forceNCOFrequency = false);
    void webapiFormatDeviceSettings(SWGSDRangel::SWGDeviceSettings& response, const LimeSDROutputSettings& settings);
};

#endif /* PLUGINS_SAMPLESOURCE_LIMESDROUTPUT_LIMESDROUTPUT_H_ */
