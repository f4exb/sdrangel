///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2018 Edouard Griffiths, F4EXB.                             //
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

#ifndef INCLUDE_APTDEMOD_H
#define INCLUDE_APTDEMOD_H

#include <vector>

#include <QNetworkRequest>
#include <QThread>
#include <QImage>

#include <apt.h>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "aptdemodbaseband.h"
#include "aptdemodimageworker.h"
#include "aptdemodsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class DeviceAPI;
class APTDemodImageWorker;

class APTDemod : public BasebandSampleSink, public ChannelAPI {
public:
    class MsgConfigureAPTDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const APTDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureAPTDemod* create(const APTDemodSettings& settings, bool force)
        {
            return new MsgConfigureAPTDemod(settings, force);
        }

    private:
        APTDemodSettings m_settings;
        bool m_force;

        MsgConfigureAPTDemod(const APTDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    // One row of pixels from sink
    class MsgPixels : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const float *getPixels() const { return m_pixels; }
        int getZenith() const { return m_zenith; }

        static MsgPixels* create(const float *pixels, int zenith)
        {
            return new MsgPixels(pixels, zenith);
        }

    private:
        const float *m_pixels;
        int m_zenith;

        MsgPixels(const float *pixels, int zenith) :
            Message(),
            m_pixels(pixels),
            m_zenith(zenith)
        {
        }
    };

    // Processed image to be sent to GUI
    class MsgImage : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QImage getImage() const { return m_image; }
        const QStringList getImageTypes() const { return m_imageTypes; }
        const QString getSatelliteName() const { return m_satelliteName; }

        static MsgImage* create(const QImage image, const QStringList imageTypes, const QString satelliteName)
        {
            return new MsgImage(image, imageTypes, satelliteName);
        }

    private:
        QImage m_image;
        QStringList m_imageTypes;
        QString m_satelliteName;

        MsgImage(const QImage image, const QStringList imageTypes, const QString satelliteName) :
            Message(),
            m_image(image),
            m_imageTypes(imageTypes),
            m_satelliteName(satelliteName)
        {
        }
    };

    // Unprocessed line to be sent to GUI
    class MsgLine : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const uchar* getLine() const { return m_line; }
        int getSize() const { return m_size; }
        void setSize(int size) { m_size = size;}

        static MsgLine* create(uchar **line)
        {
            MsgLine *msg =  new MsgLine();
            *line = msg->m_line;
            return msg;
        }

    private:
        uchar m_line[APT_PROW_WIDTH];
        int m_size;

        MsgLine() :
            Message(),
            m_size(APT_PROW_WIDTH)
        {}
    };

    // Sent from worker to GUI to indicate name of image on Map
    class MsgMapImageName : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QString getName() const { return m_name; }

        static MsgMapImageName* create(const QString &name)
        {
            return new MsgMapImageName(name);
        }

    private:
        QString m_name;

        MsgMapImageName(const QString &name) :
            Message(),
            m_name(name)
        {
        }
    };

    // Sent from GUI to reset decoder
    class MsgResetDecoder : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgResetDecoder* create()
        {
            return new MsgResetDecoder();
        }

    private:

        MsgResetDecoder() :
            Message()
        {
        }
    };

    APTDemod(DeviceAPI *deviceAPI);
    virtual ~APTDemod();
    virtual void destroy() { delete this; }
    virtual void setDeviceAPI(DeviceAPI *deviceAPI);
    virtual DeviceAPI *getDeviceAPI() { return m_deviceAPI; }

    using BasebandSampleSink::feed;
    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
    virtual void start();
    virtual void stop();
    virtual void pushMessage(Message *msg) { m_inputMessageQueue.push(msg); }
    virtual QString getSinkName() { return objectName(); }
    void startBasebandSink();
    void stopBasebandSink();
    void startImageWorker();
    void stopImageWorker();

    void setMessageQueueToGUI(MessageQueue* queue) override
    {
        ChannelAPI::setMessageQueueToGUI(queue);
        m_imageWorker->setMessageQueueToGUI(queue);
    }

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual const QString& getURI() const { return getName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }
    virtual qint64 getCenterFrequency() const { return m_settings.m_inputFrequencyOffset; }
    virtual void setCenterFrequency(qint64 frequency);

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return 0;
    }

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiWorkspaceGet(
            SWGSDRangel::SWGWorkspaceInfo& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiActionsPost(
            const QStringList& channelActionsKeys,
            SWGSDRangel::SWGChannelActions& query,
            QString& errorMessage);

    static void webapiFormatChannelSettings(
            SWGSDRangel::SWGChannelSettings& response,
            const APTDemodSettings& settings);

    static void webapiUpdateChannelSettings(
            APTDemodSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    double getMagSq() const { return m_basebandSink->getMagSq(); }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples) {
        m_basebandSink->getMagSqLevels(avg, peak, nbSamples);
    }

    uint32_t getNumberOfDeviceStreams() const;

    static const char * const m_channelIdURI;
    static const char * const m_channelId;

private:
    DeviceAPI *m_deviceAPI;
    QThread m_thread;
    QThread m_imageThread;
    APTDemodBaseband* m_basebandSink;
    APTDemodImageWorker *m_imageWorker;
    APTDemodSettings m_settings;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink
    qint64 m_centerFrequency;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const APTDemodSettings& settings, bool force = false);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const APTDemodSettings& settings, bool force);
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const APTDemodSettings& settings,
        bool force
    );

    bool matchSatellite(const QString satelliteName);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);
};

#endif // INCLUDE_APTDEMOD_H
