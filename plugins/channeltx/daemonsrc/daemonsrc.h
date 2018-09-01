///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_DAEMONSRC_DAEMONSRC_H_
#define PLUGINS_CHANNELTX_DAEMONSRC_DAEMONSRC_H_

#include "cm256.h"

#include "dsp/basebandsamplesource.h"
#include "channel/channelsourceapi.h"
#include "util/message.h"

#include "daemonsrcsettings.h"
#include "channel/sdrdaemondataqueue.h"
#include "channel/sdrdaemondatablock.h"
#include "channel/sdrdaemondatareadqueue.h"

class ThreadedBasebandSampleSource;
class UpChannelizer;
class DeviceSinkAPI;
class DaemonSrcThread;
class SDRDaemonDataBlock;

class DaemonSrc : public BasebandSampleSource, public ChannelSourceAPI {
    Q_OBJECT

public:
    class MsgConfigureDaemonSrc : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const DaemonSrcSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureDaemonSrc* create(const DaemonSrcSettings& settings, bool force)
        {
            return new MsgConfigureDaemonSrc(settings, force);
        }

    private:
        DaemonSrcSettings m_settings;
        bool m_force;

        MsgConfigureDaemonSrc(const DaemonSrcSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgSampleRateNotification : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgSampleRateNotification* create(int sampleRate) {
            return new MsgSampleRateNotification(sampleRate);
        }

        int getSampleRate() const { return m_sampleRate; }

    private:

        MsgSampleRateNotification(int sampleRate) :
            Message(),
            m_sampleRate(sampleRate)
        { }

        int m_sampleRate;
    };

    DaemonSrc(DeviceSinkAPI *deviceAPI);
    ~DaemonSrc();

    virtual void destroy() { delete this; }

    virtual void pull(Sample& sample);
    virtual void pullAudio(int nbSamples);
    virtual void start();
    virtual void stop();
    virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }
    virtual qint64 getCenterFrequency() const { return 0; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiReportGet(
            SWGSDRangel::SWGChannelReport& response,
            QString& errorMessage);

    void setDataLink(const QString& dataAddress, uint16_t dataPort);

    static const QString m_channelIdURI;
    static const QString m_channelId;

private:
    DeviceSinkAPI* m_deviceAPI;
    ThreadedBasebandSampleSource* m_threadedChannelizer;
    UpChannelizer* m_channelizer;
    SDRDaemonDataQueue m_dataQueue;
    DaemonSrcThread *m_sourceThread;
    CM256 m_cm256;
    CM256 *m_cm256p;
    bool m_running;

    DaemonSrcSettings m_settings;

    CM256::cm256_block   m_cm256DescriptorBlocks[2*SDRDaemonNbOrginalBlocks]; //!< CM256 decoder descriptors (block addresses and block indexes)
    SDRDaemonMetaDataFEC m_currentMeta;

    SDRDaemonDataReadQueue m_dataReadQueue;

    uint32_t m_nbCorrectableErrors;   //!< count of correctable errors in number of blocks
    uint32_t m_nbUncorrectableErrors; //!< count of uncorrectable errors in number of blocks

    void applySettings(const DaemonSrcSettings& settings, bool force = false);
    void handleDataBlock(SDRDaemonDataBlock *dataBlock);
    void printMeta(const QString& header, SDRDaemonMetaDataFEC *metaData);
    uint32_t calculateDataReadQueueSize(int sampleRate);
    void webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const DaemonSrcSettings& settings);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);

private slots:
    void handleData();
};

#endif // PLUGINS_CHANNELTX_DAEMONSRC_DAEMONSRC_H_
