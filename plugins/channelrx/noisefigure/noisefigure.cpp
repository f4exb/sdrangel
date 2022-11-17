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

#include <boost/version.hpp>
#include <boost/math/interpolators/barycentric_rational.hpp>

#include "noisefigure.h"

#include <QTimer>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QProcess>

#include <complex.h>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGChannelReport.h"

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "util/db.h"
#include "util/interpolation.h"
#include "channel/channelwebapiutils.h"
#include "maincore.h"

#if BOOST_VERSION < 107700
using namespace boost::math;
#else
using namespace boost::math::interpolators;
#endif

MESSAGE_CLASS_DEFINITION(NoiseFigure::MsgConfigureNoiseFigure, Message)
MESSAGE_CLASS_DEFINITION(NoiseFigure::MsgPowerMeasurement, Message)
MESSAGE_CLASS_DEFINITION(NoiseFigure::MsgNFMeasurement, Message)
MESSAGE_CLASS_DEFINITION(NoiseFigure::MsgStartStop, Message)
MESSAGE_CLASS_DEFINITION(NoiseFigure::MsgFinished, Message)

const char * const NoiseFigure::m_channelIdURI = "sdrangel.channel.noisefigure";
const char * const NoiseFigure::m_channelId = "NoiseFigure";

NoiseFigure::NoiseFigure(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSampleRate(0),
        m_state(IDLE)
{
    setObjectName(m_channelId);

    m_basebandSink = new NoiseFigureBaseband(this);
    m_basebandSink->setMessageQueueToChannel(getInputMessageQueue());
    m_basebandSink->setChannel(this);
    m_basebandSink->moveToThread(&m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &NoiseFigure::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &NoiseFigure::handleIndexInDeviceSetChanged
    );
}

NoiseFigure::~NoiseFigure()
{
    qDebug("NoiseFigure::~NoiseFigure");
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &NoiseFigure::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    delete m_basebandSink;
}

void NoiseFigure::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSinkAPI(this);
        m_deviceAPI->removeChannelSink(this);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addChannelSink(this);
        m_deviceAPI->addChannelSinkAPI(this);
    }
}

uint32_t NoiseFigure::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void NoiseFigure::setCenterFrequency(qint64 frequency)
{
    NoiseFigureSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureNoiseFigure *msgToGUI = MsgConfigureNoiseFigure::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void NoiseFigure::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void NoiseFigure::start()
{
    qDebug("NoiseFigure::start");

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    NoiseFigureBaseband::MsgConfigureNoiseFigureBaseband *msg = NoiseFigureBaseband::MsgConfigureNoiseFigureBaseband::create(m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void NoiseFigure::stop()
{
    qDebug("NoiseFigure::stop");
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
}

bool NoiseFigure::openVISADevice()
{
    m_visa.openDefault();
    m_session = m_visa.open(m_settings.m_visaDevice);
    return m_session != 0;
}

void NoiseFigure::closeVISADevice()
{
    m_visa.close(m_session);
    m_visa.closeDefault();
}

void NoiseFigure::processVISA(QStringList commands)
{
    if (m_session > 0)
    {
        for (int i = 0; i < commands.size(); i++)
        {
            QString command = commands[i].trimmed();
            if (!command.isEmpty() && !command.startsWith("#"))   // Allow # to comment out lines
            {
                qDebug() << "VISA ->: " << command;
                QByteArray bytes = QString("%1\n").arg(command).toLatin1();
                char *cmd = bytes.data();
                m_visa.viPrintf(m_session, cmd);
                if (command.endsWith("?"))
                {
                    char buf[1024] = "";
                    char format[] = "%t";
                    m_visa.viScanf(m_session, format, buf);
                    qDebug() << "VISA <-: " << buf;
                }
            }
        }
    }
}

// Calculate ENR at specified frequency
double NoiseFigure::calcENR(double frequency)
{
    double enr;
    int size = m_settings.m_enr.size();
    if (size >= 2)
    {
        std::vector<double> x(size);
        std::vector<double> y(size);
        for (int i = 0; i < size; i++)
        {
            x[i] = m_settings.m_enr[i]->m_frequency;
            y[i] = m_settings.m_enr[i]->m_enr;
        }
        if (m_settings.m_interpolation == NoiseFigureSettings::LINEAR)
        {
            enr = Interpolation::linear(x.begin(), x.end(), y.begin(), frequency);
        }
        else
        {
            int order = size - 1;
            barycentric_rational<double> interpolant(std::move(x), std::move(y), order);
            enr = interpolant(frequency);
        }
    }
    else if (size == 1)
    {
        enr = m_settings.m_enr[0]->m_enr;
    }
    else
    {
        // Shouldn't get here
        enr = 0.0;
    }
    qDebug() << "ENR at " << frequency << " interpolated to " << enr;
    return enr;
}

// FSM for running measurements over multiple frequencies
void NoiseFigure::nextState()
{
    double scaleFactor = m_settings.m_setting == "centerFrequency" ? 1e6 : 1.0;
    switch (m_state)
    {
    case IDLE:
        if (m_settings.m_enr.size() < 1)
        {
            if (getMessageQueueToGUI()) {
                getMessageQueueToGUI()->push(MsgFinished::create("ENRs has not been specified"));
            }
            return;
        }
        m_step = 0;
        if (m_settings.m_sweepSpec == NoiseFigureSettings::LIST)
        {
            // Create list of sweep values from string
            QRegularExpression separator("[( |,|\t|)]");
            QStringList valueStrings = m_settings.m_sweepList.trimmed().split(separator);
            m_values.clear();
            for (int i = 0; i < valueStrings.size(); i++)
            {
                QString valueString = valueStrings[i].trimmed();
                if (!valueString.isEmpty())
                {
                    bool ok;
                    double value = valueString.toDouble(&ok);
                    if (ok) {
                        m_values.append(value * scaleFactor);
                    } else {
                        qDebug() << "NoiseFigure::nextState: Invalid value: " << valueString;
                    }
                }
            }
            if (m_values.size() == 0)
            {
                qDebug() << "NoiseFigure::nextState: Sweep list is empty";
                if (getMessageQueueToGUI()) {
                    getMessageQueueToGUI()->push(MsgFinished::create("Sweep list is empty"));
                }
                return;
            }
            // Set start value and number of values to step through
            m_sweepValue = m_values[0];
            m_steps = m_values.size();
        }
        else
        {
            if (m_settings.m_stopValue < m_settings.m_startValue)
            {
                if (getMessageQueueToGUI()) {
                    getMessageQueueToGUI()->push(MsgFinished::create("Stop value must be greater or equal to start value"));
                }
                return;
            }
            // Set start value and number of values to step through
            m_sweepValue = m_settings.m_startValue * scaleFactor;
            if (m_settings.m_sweepSpec == NoiseFigureSettings::RANGE) {
                m_steps = m_settings.m_steps;
            } else {
                m_steps = (m_settings.m_stopValue - m_settings.m_startValue) / m_settings.m_step + 1;
            }
        }
        m_state = SET_SWEEP_VALUE;
        QTimer::singleShot(0, this, &NoiseFigure::nextState);
        break;

    case SET_SWEEP_VALUE:
        // Set device setting that is being swept
        if (ChannelWebAPIUtils::patchDeviceSetting(getDeviceSetIndex(), m_settings.m_setting, m_sweepValue))
        {
            qDebug() << "NoiseFigure::nextState: Set " << m_settings.m_setting << " to " << m_sweepValue;
            m_state = POWER_ON;
            QTimer::singleShot(100, this, &NoiseFigure::nextState);
        } else
        {
            qDebug() << "NoiseFigure::nextState: Unable to set " << m_settings.m_setting << " to " << m_sweepValue;
        }
        break;

    case POWER_ON:
        // Power on noise source
        powerOn();
        QTimer::singleShot(m_settings.m_powerDelay * 1000.0, this, &NoiseFigure::nextState);
        m_state = MEASURE_ON;
        break;

    case MEASURE_ON:
        // Start measurement of power when noise source is on
        qDebug() << "NoiseFigure::nextState: Starting on measurement";
        m_basebandSink->startMeasurement();
        break;

    case POWER_OFF:
        // Power off noise source
        powerOff();
        QTimer::singleShot(m_settings.m_powerDelay * 1000.0, this, &NoiseFigure::nextState);
        m_state = MEASURE_OFF;
        break;

    case MEASURE_OFF:
        // Start measurement of power when noise source is off
        qDebug() << "NoiseFigure::nextState: Starting off measurement";
        m_basebandSink->startMeasurement();
        break;

    case COMPLETE:
        // Calculate noise figure and temperature using Y-factor method
        double t = 290.0;
        double k = 1.38064852e-23;
        double bw = 1;
        double y = m_onPower - m_offPower;
        double enr = calcENR(m_centerFrequency/1e6);
        double nf = 10.0*log10(pow(10.0, enr/10.0)/(pow(10.0, y/10.0)-1.0));
        double temp = t*(pow(10.0, nf/10.0)-1.0);
        double floor = 10.0*log10(1000.0*k*t) + nf + 10*log10(bw);

        // Send result to GUI
        if (getMessageQueueToGUI())
        {
            MsgNFMeasurement *msg = MsgNFMeasurement::create(m_sweepValue/scaleFactor, nf, temp, y, enr, floor);
            getMessageQueueToGUI()->push(msg);
        }

        m_step++;
        if (m_step >= m_steps)
        {
            // All values swept
            closeVISADevice();
            m_state = IDLE;
            if (getMessageQueueToGUI()) {
                getMessageQueueToGUI()->push(MsgFinished::create());
            }
        }
        else
        {
            // Move to next value in sweep
            if (m_settings.m_sweepSpec == NoiseFigureSettings::LIST) {
                m_sweepValue = m_values[m_step];
            } else if (m_settings.m_sweepSpec == NoiseFigureSettings::RANGE) {
                m_sweepValue += scaleFactor * (m_settings.m_stopValue - m_settings.m_startValue) / (m_settings.m_steps - 1);
            } else {
                m_sweepValue += m_settings.m_step * scaleFactor;
            }
            m_state = SET_SWEEP_VALUE;
            QTimer::singleShot(0, this, &NoiseFigure::nextState);
        }
        break;
    }
}

void NoiseFigure::powerOn()
{
    QString command = m_settings.m_powerOnCommand.trimmed();

    if (!command.isEmpty())
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QStringList allArgs = command.split(" ", Qt::SkipEmptyParts);
#else
        QStringList allArgs = command.split(" ", QString::SkipEmptyParts);
#endif
        QString program = allArgs[0];
        allArgs.pop_front();
        QProcess::execute(program, allArgs);
    }

    QStringList commands = m_settings.m_powerOnSCPI.split("\n");
    processVISA(commands);
}

void NoiseFigure::powerOff()
{
    QStringList commands = m_settings.m_powerOffSCPI.split("\n");
    processVISA(commands);

    QString command = m_settings.m_powerOffCommand.trimmed();

    if (!command.isEmpty())
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QStringList allArgs = command.split(" ", Qt::SkipEmptyParts);
#else
        QStringList allArgs = command.split(" ", QString::SkipEmptyParts);
#endif
        QString program = allArgs[0];
        allArgs.pop_front();
        QProcess::execute(program, allArgs);
    }
}

bool NoiseFigure::handleMessage(const Message& cmd)
{
    if (MsgConfigureNoiseFigure::match(cmd))
    {
        MsgConfigureNoiseFigure& cfg = (MsgConfigureNoiseFigure&) cmd;
        qDebug() << "NoiseFigure::handleMessage: MsgConfigureNoiseFigure";
        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();
        // Forward to the sink
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "NoiseFigure::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (m_guiMessageQueue) {
            m_guiMessageQueue->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MsgPowerMeasurement::match(cmd))
    {
        MsgPowerMeasurement& report = (MsgPowerMeasurement&)cmd;

        if (m_state == MEASURE_ON)
        {
            m_onPower = report.getPower();
            m_state = POWER_OFF;
            nextState();
        }
        else if (m_state == MEASURE_OFF)
        {
            m_offPower = report.getPower();
            m_state = COMPLETE;
            nextState();
        }

        return true;
    }
    else if (MsgStartStop::match(cmd))
    {
        if (m_state == IDLE)
        {
            if (!m_settings.m_visaDevice.isEmpty())
            {
                if (openVISADevice()) {
                    QTimer::singleShot(0, this, &NoiseFigure::nextState);
                } else if (getMessageQueueToGUI()) {
                    getMessageQueueToGUI()->push(MsgFinished::create(QString("Failed to open VISA device %1").arg(m_settings.m_visaDevice)));
                }
            }
            else
            {
                QTimer::singleShot(0, this, &NoiseFigure::nextState);
            }
        }
        else
        {
            // Set maximum step so test stops after current measurement
            m_step = m_steps;
        }

        return true;
    }
    else
    {
        return false;
    }
}

void NoiseFigure::applySettings(const NoiseFigureSettings& settings, bool force)
{
    qDebug() << "NoiseFigure::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_fftSize: " << settings.m_fftSize
            << " m_fftCount: " << settings.m_fftCount
            << " m_sweepSpec: " << settings.m_sweepSpec
            << " m_startValue: " << settings.m_startValue
            << " m_stopValue: " << settings.m_stopValue
            << " m_steps: " << settings.m_steps
            << " m_step: " << settings.m_step
            << " m_sweepList: " << settings.m_sweepList
            << " m_setting: " << settings.m_setting
            << " m_visaDevice: " << settings.m_visaDevice
            << " m_powerOnSCPI: " << settings.m_powerOnSCPI
            << " m_powerOffSCPI: " << settings.m_powerOffSCPI
            << " m_powerOnCommand: " << settings.m_powerOnCommand
            << " m_powerOffCommand: " << settings.m_powerOffCommand
            << " m_powerDelay: " << settings.m_powerDelay
            << " m_enr: " << settings.m_enr
            << " m_streamIndex: " << settings.m_streamIndex
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIPort: " << settings.m_reverseAPIPort
            << " m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
            << " m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if (m_settings.m_streamIndex != settings.m_streamIndex)
    {
        if (m_deviceAPI->getSampleMIMO()) // change of stream is possible for MIMO devices only
        {
            m_deviceAPI->removeChannelSinkAPI(this);
            m_deviceAPI->removeChannelSink(this, m_settings.m_streamIndex);
            m_deviceAPI->addChannelSink(this, settings.m_streamIndex);
            m_deviceAPI->addChannelSinkAPI(this);
        }

        reverseAPIKeys.append("streamIndex");
    }

    NoiseFigureBaseband::MsgConfigureNoiseFigureBaseband *msg = NoiseFigureBaseband::MsgConfigureNoiseFigureBaseband::create(settings, force);
    m_basebandSink->getInputMessageQueue()->push(msg);

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex) ||
                (m_settings.m_reverseAPIChannelIndex != settings.m_reverseAPIChannelIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    m_settings = settings;
}

QByteArray NoiseFigure::serialize() const
{
    return m_settings.serialize();
}

bool NoiseFigure::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureNoiseFigure *msg = MsgConfigureNoiseFigure::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureNoiseFigure *msg = MsgConfigureNoiseFigure::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int NoiseFigure::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setNoiseFigureSettings(new SWGSDRangel::SWGNoiseFigureSettings());
    response.getNoiseFigureSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int NoiseFigure::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int NoiseFigure::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    NoiseFigureSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureNoiseFigure *msg = MsgConfigureNoiseFigure::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("NoiseFigure::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureNoiseFigure *msgToGUI = MsgConfigureNoiseFigure::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void NoiseFigure::webapiUpdateChannelSettings(
        NoiseFigureSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getNoiseFigureSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("fftSize")) {
        settings.m_fftSize = response.getNoiseFigureSettings()->getFftSize();
    }
    if (channelSettingsKeys.contains("sweepSpec")) {
        settings.m_sweepSpec = (NoiseFigureSettings::SweepSpec)response.getNoiseFigureSettings()->getSweepSpec();
    }
    if (channelSettingsKeys.contains("startValue")) {
        settings.m_startValue = response.getNoiseFigureSettings()->getStartValue();
    }
    if (channelSettingsKeys.contains("stopValue")) {
        settings.m_stopValue = response.getNoiseFigureSettings()->getStopValue();
    }
    if (channelSettingsKeys.contains("steps")) {
        settings.m_steps = response.getNoiseFigureSettings()->getSteps();
    }
    if (channelSettingsKeys.contains("step")) {
        settings.m_step = response.getNoiseFigureSettings()->getStep();
    }
    if (channelSettingsKeys.contains("list")) {
        settings.m_sweepList = *response.getNoiseFigureSettings()->getList();
    }
    if (channelSettingsKeys.contains("setting")) {
        settings.m_setting = *response.getNoiseFigureSettings()->getSetting();
    }
    if (channelSettingsKeys.contains("visaDevice")) {
        settings.m_visaDevice = *response.getNoiseFigureSettings()->getVisaDevice();
    }
    if (channelSettingsKeys.contains("powerOnSCPI")) {
        settings.m_powerOnSCPI = *response.getNoiseFigureSettings()->getPowerOnScpi();
    }
    if (channelSettingsKeys.contains("powerOffSCPI")) {
        settings.m_powerOffSCPI = *response.getNoiseFigureSettings()->getPowerOffScpi();
    }
    if (channelSettingsKeys.contains("powerOnCommand")) {
        settings.m_powerOnCommand = *response.getNoiseFigureSettings()->getPowerOnCommand();
    }
    if (channelSettingsKeys.contains("powerOffCommand")) {
        settings.m_powerOffCommand = *response.getNoiseFigureSettings()->getPowerOffCommand();
    }
    if (channelSettingsKeys.contains("powerDelay")) {
        settings.m_powerDelay = response.getNoiseFigureSettings()->getPowerDelay();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getNoiseFigureSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getNoiseFigureSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getNoiseFigureSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getNoiseFigureSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getNoiseFigureSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getNoiseFigureSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getNoiseFigureSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getNoiseFigureSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getNoiseFigureSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getNoiseFigureSettings()->getRollupState());
    }
}

void NoiseFigure::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const NoiseFigureSettings& settings)
{
    response.getNoiseFigureSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);

    response.getNoiseFigureSettings()->setRgbColor(settings.m_rgbColor);
    if (response.getNoiseFigureSettings()->getTitle()) {
        *response.getNoiseFigureSettings()->getTitle() = settings.m_title;
    } else {
        response.getNoiseFigureSettings()->setTitle(new QString(settings.m_title));
    }

    response.getNoiseFigureSettings()->setStreamIndex(settings.m_streamIndex);
    response.getNoiseFigureSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getNoiseFigureSettings()->getReverseApiAddress()) {
        *response.getNoiseFigureSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getNoiseFigureSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getNoiseFigureSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getNoiseFigureSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getNoiseFigureSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getNoiseFigureSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getNoiseFigureSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getNoiseFigureSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getNoiseFigureSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getNoiseFigureSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getNoiseFigureSettings()->setRollupState(swgRollupState);
        }
    }
}

void NoiseFigure::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const NoiseFigureSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    webapiFormatChannelSettings(channelSettingsKeys, swgChannelSettings, settings, force);

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/channel/%4/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex)
            .arg(settings.m_reverseAPIChannelIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgChannelSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgChannelSettings;
}

void NoiseFigure::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const NoiseFigureSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("NoiseFigure"));
    swgChannelSettings->setNoiseFigureSettings(new SWGSDRangel::SWGNoiseFigureSettings());
    SWGSDRangel::SWGNoiseFigureSettings *swgNoiseFigureSettings = swgChannelSettings->getNoiseFigureSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgNoiseFigureSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("fftSize") || force) {
        swgNoiseFigureSettings->setFftSize(settings.m_fftSize);
    }
    if (channelSettingsKeys.contains("fftCount") || force) {
        swgNoiseFigureSettings->setFftCount(settings.m_fftCount);
    }
    if (channelSettingsKeys.contains("sweepSpec") || force) {
        swgNoiseFigureSettings->setSweepSpec((int)settings.m_sweepSpec);
    }
    if (channelSettingsKeys.contains("startValue") || force) {
        swgNoiseFigureSettings->setStartValue(settings.m_startValue);
    }
    if (channelSettingsKeys.contains("stopValue") || force) {
        swgNoiseFigureSettings->setStopValue(settings.m_stopValue);
    }
    if (channelSettingsKeys.contains("steps") || force) {
        swgNoiseFigureSettings->setSteps(settings.m_steps);
    }
    if (channelSettingsKeys.contains("step") || force) {
        swgNoiseFigureSettings->setStep(settings.m_step);
    }
    if (channelSettingsKeys.contains("list") || force) {
        swgNoiseFigureSettings->setList(new QString(settings.m_sweepList));
    }
    if (channelSettingsKeys.contains("setting") || force) {
        swgNoiseFigureSettings->setSetting(new QString(settings.m_setting));
    }
    if (channelSettingsKeys.contains("visaDevice") || force) {
        swgNoiseFigureSettings->setVisaDevice(new QString(settings.m_visaDevice));
    }
    if (channelSettingsKeys.contains("powerOnSCPI") || force) {
        swgNoiseFigureSettings->setPowerOnScpi(new QString(settings.m_powerOnSCPI));
    }
    if (channelSettingsKeys.contains("powerOffSCPI") || force) {
        swgNoiseFigureSettings->setPowerOffScpi(new QString(settings.m_powerOffSCPI));
    }
    if (channelSettingsKeys.contains("powerOnCommand") || force) {
        swgNoiseFigureSettings->setPowerOnCommand(new QString(settings.m_powerOnCommand));
    }
    if (channelSettingsKeys.contains("powerOffCommand") || force) {
        swgNoiseFigureSettings->setPowerOffCommand(new QString(settings.m_powerOffCommand));
    }
    if (channelSettingsKeys.contains("powerDelay") || force) {
        swgNoiseFigureSettings->setPowerDelay(settings.m_powerDelay);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgNoiseFigureSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgNoiseFigureSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgNoiseFigureSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgNoiseFigureSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgNoiseFigureSettings->setRollupState(swgRollupState);
    }
}

void NoiseFigure::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "NoiseFigure::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("NoiseFigure::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void NoiseFigure::handleIndexInDeviceSetChanged(int index)
{
    if (index < 0) {
        return;
    }

    QString fifoLabel = QString("%1 [%2:%3]")
        .arg(m_channelId)
        .arg(m_deviceAPI->getDeviceSetIndex())
        .arg(index);
    m_basebandSink->setFifoLabel(fifoLabel);
}
