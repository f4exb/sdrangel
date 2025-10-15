#include "tcpiqplot.h"
#include "device/deviceapi.h"
#include "maincore.h"
#include "dsp/dspcommands.h"
#include "dsp/spectrumvis.h"
#include "dsp/dspengine.h"
#include "dsp/fftfactory.h"
#include <QDebug>
#include <cstring>
#include <iostream>

MESSAGE_CLASS_DEFINITION(TcpIqPlot::MsgNewData, Message)
MESSAGE_CLASS_DEFINITION(TcpIqPlot::MsgConfigure, Message)
MESSAGE_CLASS_DEFINITION(TcpIqPlot::MsgStatus, Message)

const char* const TcpIqPlot::m_channelIdURI = "sdrangel.channel.tcpiqplot";
const char* const TcpIqPlot::m_channelId = "TcpIqPlot";
const int TcpIqPlot::maxPlotSamples;

TcpIqPlot::TcpIqPlot(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
    BasebandSampleSink(),
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_tcpServer(),
    m_clientSocket(),
    m_statusText("Server stopped"),
    m_iSamples(),
    m_qSamples(),
    m_spectrumSink(nullptr),
    m_fftSequence(-1),
    m_fft(nullptr),
    m_fftWindow(),
    m_fftSize(1024),
    m_fftBuffer(nullptr),
    m_fftBufferFill(0)
{
    setObjectName(m_channelId);
    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);
}

TcpIqPlot::~TcpIqPlot()
{
    if (!m_clientSocket.isNull()) {
        m_clientSocket->close();
        delete m_clientSocket.data();
    }
    if (!m_tcpServer.isNull()) {
        m_tcpServer->close();
        delete m_tcpServer.data();
    }
}

void TcpIqPlot::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSinkAPI(this);
        m_deviceAPI->removeChannelSink(this, false);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addChannelSink(this);
        m_deviceAPI->addChannelSinkAPI(this);
    }
}

void TcpIqPlot::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool)
{
    static int callCount = 0;
    callCount++;
    
    // Receive I/Q data from USRP (or any SDR device) and plot it
    int numSamples = end - begin;
    
    if (numSamples <= 0) {
     
        return;
    }
    
    // Limit to reasonable number of samples for plotting (e.g., 2048)
    
    int samplesToPlot = std::min(numSamples, maxPlotSamples);
    
    // Clear previous data
    m_iSamples.clear();
    m_qSamples.clear();
    m_iSamples.reserve(samplesToPlot);
    m_qSamples.reserve(samplesToPlot);
    
    // Convert samples to float arrays
    // SDRangel samples are typically 16-bit integers scaled
    for (int i = 0; i < samplesToPlot; i++) {
        const Sample& sample = *(begin + i);
        // Convert from SDRangel's integer format to float (-1.0 to 1.0)
        float iValue = sample.real() / 32768.0f;
        float qValue = sample.imag() / 32768.0f;
        m_iSamples.append(iValue);
        m_qSamples.append(qValue);
    }
    

    // Send data to GUI for plotting via Qt signal (thread-safe)
    emit newDataAvailable(m_iSamples, m_qSamples);
}

void TcpIqPlot::start() {}
void TcpIqPlot::stop() {}

bool TcpIqPlot::handleMessage(const Message& cmd)
{
    if (MsgConfigure::match(cmd))
    {
        MsgConfigure& cfg = (MsgConfigure&) cmd;
        applySettings(cfg.getSettings(), cfg.getForce());
        return true;
    }
    return false;
}

void TcpIqPlot::applySettings(const TcpIqPlotSettings& settings, bool force)
{
    if (m_settings.m_port != settings.m_port || force) {
        stopServer();
        startServer(settings.m_port);
    }
    m_settings = settings;
}

void TcpIqPlot::startServer(int port)
{
    // Stop existing server if running
    if (!m_tcpServer.isNull()) {
        m_tcpServer->close();
        delete m_tcpServer.data();
    }
    
    // Create new server without parent to avoid Qt object hierarchy issues
    m_tcpServer = new QTcpServer();
    connect(m_tcpServer.data(), SIGNAL(newConnection()), this, SLOT(newConnection()));
    
    if (m_tcpServer->listen(QHostAddress::Any, port)) {
        m_statusText = QString("Listening on port %1").arg(port);
    } else {
        m_statusText = QString("Error: Could not listen on port %1").arg(port);
    }
}
void TcpIqPlot::stopServer()
{
    if (!m_tcpServer.isNull()) {
        m_tcpServer->close();
        delete m_tcpServer.data();
    }
    if (!m_clientSocket.isNull()) {
        m_clientSocket->disconnectFromHost();
        m_clientSocket.clear();
    }
    m_statusText = "Server stopped";
}
void TcpIqPlot::newConnection()
{
    if (!m_clientSocket.isNull()) {
        // Reject new connections if one is already active
        QTcpSocket *socket = m_tcpServer->nextPendingConnection();
        socket->close();
        delete socket;
        return;
    }
    m_clientSocket = m_tcpServer->nextPendingConnection();
    connect(m_clientSocket.data(), SIGNAL(readyRead()), this, SLOT(readyRead()));
    connect(m_clientSocket.data(), SIGNAL(disconnected()), this, SLOT(disconnected()));
    m_statusText = "Client connected";
}

void TcpIqPlot::disconnected()
{
    if (!m_clientSocket.isNull()) {
        m_clientSocket->deleteLater();
        m_clientSocket.clear();
    }
    m_statusText = QString("Listening on port %1").arg(m_settings.m_port);
}

void TcpIqPlot::readyRead()
{
    if (m_clientSocket.isNull()) return;

    QByteArray buffer = m_clientSocket->readAll();
    const int sampleSize = sizeof(float) * 2; // I and Q
    int numSamples = buffer.size() / sampleSize;

    if (numSamples == 0) return;

    m_iSamples.resize(numSamples);
    m_qSamples.resize(numSamples);

    const float* data = reinterpret_cast<const float*>(buffer.constData());

    for (int i = 0; i < numSamples; ++i) {
        m_iSamples[i] = data[i * 2];
        m_qSamples[i] = data[i * 2 + 1];
    }

    // Send data to plot widget via signal
    emit newDataAvailable(m_iSamples, m_qSamples);
}

void TcpIqPlot::processFFT(const QVector<float>& iData, const QVector<float>& qData)
{
    if (!m_spectrumSink || !m_fft || iData.size() == 0) {
        return;
    }

    int numSamples = qMin(iData.size(), qData.size());
    
    for (int i = 0; i < numSamples; i++)
    {
        m_fftBuffer[m_fftBufferFill].real(iData[i]);
        m_fftBuffer[m_fftBufferFill].imag(qData[i]);
        m_fftBufferFill++;
        
        if (m_fftBufferFill >= m_fftSize)
        {
            // Copy to FFT input buffer
            std::memcpy(m_fft->in(), m_fftBuffer, m_fftSize * sizeof(Complex));
            
            // Apply windowing
            m_fftWindow.apply(m_fft->in());
            
            // Perform FFT
            m_fft->transform();
            
            // Get FFT output and convert to Sample format for spectrum display
            Complex* fftOut = m_fft->out();
            SampleVector samples(m_fftSize);
            
            for (int j = 0; j < m_fftSize; j++)
            {
                Real re = fftOut[j].real();
                Real im = fftOut[j].imag();
                // Convert to Sample format (I/Q as 16-bit integers)
                samples[j].setReal(re * 32768.0);
                samples[j].setImag(im * 32768.0);
            }
            
            // Send to spectrum display
            m_spectrumSink->feed(samples.begin(), samples.end(), false);
            
            m_fftBufferFill = 0;
        }
    }
}


QByteArray TcpIqPlot::serialize() const
{
    return m_settings.serialize();
}

bool TcpIqPlot::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigure *msg = MsgConfigure::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    return false;
}

QString TcpIqPlot::getStatusText() const
{
    return m_statusText;
}

QVariant TcpIqPlot::getReport() const { return QVariant(); }
QVariant TcpIqPlot::getSettings() const { return QVariant(); }
void TcpIqPlot::setSettings(const QVariantMap&) {}
QVariant TcpIqPlot::processCommand(const QString&, const QVariantMap&) { return QVariant(); }
