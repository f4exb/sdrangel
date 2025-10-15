#ifndef TCPIQPLOT_H
#define TCPIQPLOT_H

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"
#include "util/messagequeue.h"
#include "dsp/fftengine.h"
#include "dsp/fftwindow.h"
#include "dsp/spectrumsettings.h"
#include "tcpiqplotsettings.h"
#include <QTcpServer>
#include <QTcpSocket>
#include <QVector>
#include <QPointer>

class SpectrumVis;

class TcpIqPlot : public ChannelAPI, public BasebandSampleSink
{
    Q_OBJECT
public:
    explicit TcpIqPlot(DeviceAPI *deviceAPI);
    ~TcpIqPlot();
    
    void setSpectrumSink(SpectrumVis* spectrumSink) { m_spectrumSink = spectrumSink; }
    SpectrumVis* getSpectrumSink() { return m_spectrumSink; }

    // ChannelAPI / BasebandSampleSink
    void destroy() override { delete this; }
    void setDeviceAPI(DeviceAPI *deviceAPI) override;
    DeviceAPI *getDeviceAPI() override { return m_deviceAPI; }
    void getIdentifier(QString& id) override { id = objectName(); }
    QString getIdentifier() const override { return objectName(); }
    void getTitle(QString& title) override { title = m_settings.m_title; }
    qint64 getCenterFrequency() const override { return 0; }
    void setCenterFrequency(qint64) override {}
    
    void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly) override;
    void start() override;
    void stop() override;
    void pushMessage(Message *msg) override { m_inputMessageQueue.push(msg); }
    QString getSinkName() override { return objectName(); }
    
    bool handleMessage(const Message& cmd) override;
    QByteArray serialize() const override;
    bool deserialize(const QByteArray& data) override;
    
    int getNbSinkStreams() const override { return 1; }
    int getNbSourceStreams() const override { return 0; }
    int getStreamIndex() const override { return m_settings.m_streamIndex; }
    qint64 getStreamCenterFrequency(int, bool) const override { return 0; }
    
    QString getStatusText() const;
    static const int maxPlotSamples = 64;
    // Web API
    QVariant getReport() const;
    QVariant getSettings() const;
    void setSettings(const QVariantMap& settings);
    QVariant processCommand(const QString& command, const QVariantMap& args);

    static const char* const m_channelIdURI;
    static const char* const m_channelId;

    class MsgConfigure;
    class MsgStatus;
    class MsgNewData;

signals:
    void newDataAvailable(const QVector<float>& iData, const QVector<float>& qData);

public slots:
    void startServer(int port);
    void stopServer();

private slots:
    void newConnection();
    void readyRead();
    void disconnected();

private:
    void applySettings(const TcpIqPlotSettings& settings, bool force);
    void processFFT(const QVector<float>& iData, const QVector<float>& qData);

    DeviceAPI* m_deviceAPI;
    TcpIqPlotSettings m_settings;
    QPointer<QTcpServer> m_tcpServer;
    QPointer<QTcpSocket> m_clientSocket;
    QString m_statusText;
    QVector<float> m_iSamples;
    QVector<float> m_qSamples;
    // Note: m_inputMessageQueue is inherited from ChannelAPI, don't redeclare it!
    
    // FFT processing (kept for potential future use, but not initialized)
    SpectrumVis* m_spectrumSink;
    int m_fftSequence;
    FFTEngine* m_fft;
    FFTWindow m_fftWindow;
    int m_fftSize;
    Complex* m_fftBuffer;
    int m_fftBufferFill;
};

class TcpIqPlot::MsgNewData : public Message
{
    MESSAGE_CLASS_DECLARATION
public:
    const QVector<float>& getIData() const { return m_iData; }
    const QVector<float>& getQData() const { return m_qData; }
    static MsgNewData* create(const QVector<float>& iData, const QVector<float>& qData) {
        return new MsgNewData(iData, qData);
    }
private:
    QVector<float> m_iData;
    QVector<float> m_qData;
    MsgNewData(const QVector<float>& iData, const QVector<float>& qData) : Message(), m_iData(iData), m_qData(qData) {}
};

class TcpIqPlot::MsgConfigure : public Message
{
    MESSAGE_CLASS_DECLARATION
public:
    const TcpIqPlotSettings& getSettings() const { return m_settings; }
    bool getForce() const { return m_force; }
    static MsgConfigure* create(const TcpIqPlotSettings& settings, bool force) {
        return new MsgConfigure(settings, force);
    }
private:
    TcpIqPlotSettings m_settings;
    bool m_force;
    MsgConfigure(const TcpIqPlotSettings& settings, bool force) : Message(), m_settings(settings), m_force(force) {}
};

class TcpIqPlot::MsgStatus : public Message
{
    MESSAGE_CLASS_DECLARATION
public:
    const QString& getStatus() const { return m_status; }
    static MsgStatus* create(const QString& status) {
        return new MsgStatus(status);
    }
private:
    
    QString m_status;
    MsgStatus(const QString& status) : Message(), m_status(status) {}
};

#endif // TCPIQPLOT_H
