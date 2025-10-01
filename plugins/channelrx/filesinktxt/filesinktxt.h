#ifndef INCLUDE_FILESINKTXT_H
#define INCLUDE_FILESINKTXT_H

#include <QFile>
#include <QTextStream>
#include <QNetworkRequest>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"
#include "filesinktxtsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;
class SpectrumVis;

class FileSinkTxt : public BasebandSampleSink, public ChannelAPI
{

public:
    FileSinkTxt(DeviceAPI *deviceAPI);
    virtual ~FileSinkTxt();
    virtual void destroy() { delete this; }
    virtual void setDeviceAPI(DeviceAPI *deviceAPI);
    virtual DeviceAPI *getDeviceAPI() { return m_deviceAPI; }


    void setSpectrumSink(SpectrumVis* spectrumSink) { m_spectrumSink = spectrumSink; }

    using BasebandSampleSink::feed;
    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
    virtual void start();
    virtual void stop();
    virtual void pushMessage(Message *msg) { m_inputMessageQueue.push(msg); }
    virtual QString getSinkName() { return objectName(); }

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual void getTitle(QString& title) { title = "File Sink Txt"; }
    virtual qint64 getCenterFrequency() const { return 0; }
    virtual void setCenterFrequency(qint64) {}

    virtual QByteArray serialize() const { return QByteArray(); }
    virtual bool deserialize(const QByteArray& data) { (void) data; return false; }

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }
    virtual int getStreamIndex() const { return -1; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return 0;
    }

    void setFileName(const QString& fileName) { m_fileName = fileName; }
    QString getFileName() const { return m_fileName; }
    void startRecording();
    void stopRecording();
    void saveToFile();
    void loadFromFile();
    bool isRecording() const { return m_running; }

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    static void webapiFormatChannelSettings(
            SWGSDRangel::SWGChannelSettings& response,
            const FileSinkTxtSettings& settings);

    static void webapiUpdateChannelSettings(
            FileSinkTxtSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

private:
    DeviceAPI *m_deviceAPI;
    QString m_fileName;
    bool m_running;
    QFile *m_file;
    QTextStream *m_stream;
    FileSinkTxtSettings m_settings;
    SpectrumVis* m_spectrumSink;

    QVector<float> m_data;  // Persistent data buffer for detection
    int m_packetSize;       // Persistent packet size counter

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void applySettings(bool force = false);

    virtual bool handleMessage(const Message& cmd);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleInputMessages();
};

#endif // INCLUDE_FILESINKTXT_H
