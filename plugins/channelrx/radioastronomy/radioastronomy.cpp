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

#include "radioastronomy.h"

#include <QTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>
#include <QRegExp>
#include <QProcess>

#include <stdio.h>
#include <complex.h>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGChannelReport.h"
#include "SWGChannelActions.h"
#include "SWGRadioAstronomyActions.h"

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "channel/channelwebapiutils.h"
#include "feature/featureset.h"
#include "util/astronomy.h"
#include "util/db.h"
#include "maincore.h"

#include "radioastronomyworker.h"

MESSAGE_CLASS_DEFINITION(RadioAstronomy::MsgConfigureRadioAstronomy, Message)
MESSAGE_CLASS_DEFINITION(RadioAstronomy::MsgStartMeasurements, Message)
MESSAGE_CLASS_DEFINITION(RadioAstronomy::MsgStopMeasurements, Message)
MESSAGE_CLASS_DEFINITION(RadioAstronomy::MsgMeasurementProgress, Message)
MESSAGE_CLASS_DEFINITION(RadioAstronomy::MsgStartCal, Message)
MESSAGE_CLASS_DEFINITION(RadioAstronomy::MsgCalComplete, Message)
MESSAGE_CLASS_DEFINITION(RadioAstronomy::MsgFFTMeasurement, Message)
MESSAGE_CLASS_DEFINITION(RadioAstronomy::MsgSensorMeasurement, Message)
MESSAGE_CLASS_DEFINITION(RadioAstronomy::MsgStartSweep, Message)
MESSAGE_CLASS_DEFINITION(RadioAstronomy::MsgStopSweep, Message)
MESSAGE_CLASS_DEFINITION(RadioAstronomy::MsgSweepComplete, Message)
MESSAGE_CLASS_DEFINITION(RadioAstronomy::MsgSweepStatus, Message)
MESSAGE_CLASS_DEFINITION(RadioAstronomy::MsgScanAvailableFeatures, Message)
MESSAGE_CLASS_DEFINITION(RadioAstronomy::MsgReportAvailableFeatures, Message)
MESSAGE_CLASS_DEFINITION(RadioAstronomy::MsgReportAvailableRotators, Message)

const char * const RadioAstronomy::m_channelIdURI = "sdrangel.channel.radioastronomy";
const char * const RadioAstronomy::m_channelId = "RadioAstronomy";

RadioAstronomy::RadioAstronomy(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSampleRate(0),
        m_sweeping(false)
{
    qDebug("RadioAstronomy::RadioAstronomy");
    setObjectName(m_channelId);

    m_basebandSink = new RadioAstronomyBaseband(this);
    m_basebandSink->setMessageQueueToChannel(getInputMessageQueue());
    m_basebandSink->setChannel(this);
    m_basebandSink->moveToThread(&m_thread);

    m_worker = new RadioAstronomyWorker(this);
    m_worker->setMessageQueueToChannel(getInputMessageQueue());
    m_worker->moveToThread(&m_workerThread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_selectedPipe = nullptr;

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &RadioAstronomy::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &RadioAstronomy::handleIndexInDeviceSetChanged
    );
    QObject::connect(
        MainCore::instance(),
        &MainCore::featureAdded,
        this,
        &RadioAstronomy::handleFeatureAdded
    );
    QObject::connect(
        MainCore::instance(),
        &MainCore::featureRemoved,
        this,
        &RadioAstronomy::handleFeatureRemoved
    );

    m_sweepTimer.setSingleShot(true);
}

RadioAstronomy::~RadioAstronomy()
{
    qDebug("RadioAstronomy::~RadioAstronomy");
    QObject::disconnect(
        MainCore::instance(),
        &MainCore::featureRemoved,
        this,
        &RadioAstronomy::handleFeatureRemoved
    );
    QObject::disconnect(
        MainCore::instance(),
        &MainCore::featureAdded,
        this,
        &RadioAstronomy::handleFeatureAdded
    );
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &RadioAstronomy::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);

    if (m_basebandSink->isRunning()) {
        stop();
    }
    delete m_basebandSink;
    if (m_worker->isRunning()) {
        stop();
    }
    delete m_worker;
}

void RadioAstronomy::setDeviceAPI(DeviceAPI *deviceAPI)
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

uint32_t RadioAstronomy::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void RadioAstronomy::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void RadioAstronomy::start()
{
    qDebug("RadioAstronomy::start");

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    m_worker->reset();
    m_worker->setMessageQueueToGUI(getMessageQueueToGUI());
    m_worker->startWork();
    m_workerThread.start();

    m_basebandSink->getInputMessageQueue()->push(new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency));
    m_basebandSink->getInputMessageQueue()->push(RadioAstronomyBaseband::MsgConfigureRadioAstronomyBaseband::create(m_settings, true));
    m_worker->getInputMessageQueue()->push(RadioAstronomyWorker::MsgConfigureRadioAstronomyWorker::create(m_settings, true));

    scanAvailableFeatures();
}

void RadioAstronomy::stop()
{
    qDebug("RadioAstronomy::stop");
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
    m_worker->stopWork();
    m_workerThread.quit();
    m_workerThread.wait();
}

void RadioAstronomy::setCenterFrequency(qint64 frequency)
{
    RadioAstronomySettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureRadioAstronomy *msgToGUI = MsgConfigureRadioAstronomy::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

bool RadioAstronomy::handleMessage(const Message& cmd)
{
    if (MsgConfigureRadioAstronomy::match(cmd))
    {
        MsgConfigureRadioAstronomy& cfg = (MsgConfigureRadioAstronomy&) cmd;
        qDebug() << "RadioAstronomy::handleMessage: MsgConfigureRadioAstronomy";
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
        qDebug() << "RadioAstronomy::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);
        // Forward to GUI
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MainCore::MsgStarTrackerTarget::match(cmd))
    {
        MainCore::MsgStarTrackerTarget& msg = (MainCore::MsgStarTrackerTarget&)cmd;
        if (msg.getPipeSource() == m_selectedPipe)
        {
            // Forward to GUI
            if (getMessageQueueToGUI()) {
                getMessageQueueToGUI()->push(new MainCore::MsgStarTrackerTarget(msg));
            }
        }
        return true;
    }
    else if (MsgMeasurementProgress::match(cmd))
    {
        // Forward to GUI
        MsgMeasurementProgress& report = (MsgMeasurementProgress&)cmd;
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new MsgMeasurementProgress(report));
        }

        return true;
    }
    else if (MsgStartCal::match(cmd))
    {
        // Forward to the sink
        MsgStartCal& calCmd = (MsgStartCal&)cmd;

        startCal(calCmd.getHot());

        return true;
    }
    else if (MsgCalComplete::match(cmd))
    {
        // Take copy to forward to GUI
        MsgCalComplete& report = (MsgCalComplete&)cmd;
        MsgCalComplete* copy = nullptr;
        if (getMessageQueueToGUI()) {
            copy = new MsgCalComplete(report);
        }
        calComplete(copy);

        return true;
    }
    else if (MsgFFTMeasurement::match(cmd))
    {
        // Forward to GUI
        MsgFFTMeasurement& report = (MsgFFTMeasurement&)cmd;
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new MsgFFTMeasurement(report));
        }

        if (m_sweeping) {
            m_sweeping = false;
            sweepNext();
        }

        return true;
    }
    else if (MsgStartSweep::match(cmd))
    {
        if (m_settings.m_runMode == RadioAstronomySettings::SWEEP) {
            sweepStart();
        } else {
            callOnStartTime(&RadioAstronomy::startMeasurement);
        }
        return true;
    }
    else if (MsgStopSweep::match(cmd))
    {
        if (m_settings.m_runMode == RadioAstronomySettings::SWEEP)
        {
            m_sweepStop = true;
            m_sweepTimer.setInterval(0);
        }
        else
        {
            m_basebandSink->getInputMessageQueue()->push(MsgStopMeasurements::create());
        }
        return true;
    }
    else if (MsgScanAvailableFeatures::match(cmd))
    {
        scanAvailableFeatures();
        return true;
    }
    else
    {
        return false;
    }
}

void RadioAstronomy::startCal(bool hot)
{
    // Set GPIO pin in SDR to enable calibration
    if (m_settings.m_gpioEnabled)
    {
        int gpioPins;
        int gpioDir;
        if (ChannelWebAPIUtils::getDeviceSetting(getDeviceSetIndex(), "gpioDir", gpioDir))
        {
            // Set pin as output
            gpioDir |= 1 << m_settings.m_gpioPin;
            ChannelWebAPIUtils::patchDeviceSetting(getDeviceSetIndex(), "gpioDir", gpioDir);
            if (ChannelWebAPIUtils::getDeviceSetting(getDeviceSetIndex(), "gpioPins", gpioPins))
            {
                // Set state of pin
                if (m_settings.m_gpioSense) {
                    gpioPins |= 1 << m_settings.m_gpioPin;
                } else {
                    gpioPins &= ~(1 << m_settings.m_gpioPin);
                }
                ChannelWebAPIUtils::patchDeviceSetting(getDeviceSetIndex(), "gpioPins", gpioPins);
            }
            else
            {
                qDebug() << "RadioAstronomy::startCal - Failed to read gpioPins setting. Does this SDR support it?";
            }
        }
        else
        {
            qDebug() << "RadioAstronomy::startCal - Failed to read gpioDir setting. Does this SDR support it?";
        }
    }

    // Execute command to enable calibration
    if (!m_settings.m_startCalCommand.isEmpty())
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QStringList allArgs = m_settings.m_startCalCommand.split(" ", Qt::SkipEmptyParts);
#else
        QStringList allArgs = m_settings.m_startCalCommand.split(" ", QString::SkipEmptyParts);
#endif
        QString program = allArgs[0];
        allArgs.pop_front();
        QProcess::startDetached(program, allArgs);
    }

    // Start calibration after requested delay
    MsgStartCal* startCal = MsgStartCal::create(hot);
    QTimer::singleShot(m_settings.m_calCommandDelay * 1000, [this, startCal] {
        m_basebandSink->getInputMessageQueue()->push(startCal);
    });
}

void RadioAstronomy::calComplete(MsgCalComplete* report)
{
    // Set GPIO pin in SDR to disable calibration
    if (m_settings.m_gpioEnabled)
    {
        int gpioPins;
        if (ChannelWebAPIUtils::getDeviceSetting(getDeviceSetIndex(), "gpioPins", gpioPins))
        {
            if (m_settings.m_gpioSense) {
                gpioPins &= ~(1 << m_settings.m_gpioPin);
            } else {
                gpioPins |= 1 << m_settings.m_gpioPin;
            }
            ChannelWebAPIUtils::patchDeviceSetting(getDeviceSetIndex(), "gpioPins", gpioPins);
        }
        else
        {
            qDebug() << "RadioAstronomy::calComplete - Failed to read gpioPins setting. Does this SDR support it?";
        }
    }

    // Execute command to disable calibration
    if (!m_settings.m_stopCalCommand.isEmpty())
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QStringList allArgs = m_settings.m_stopCalCommand.split(" ", Qt::SkipEmptyParts);
#else
        QStringList allArgs = m_settings.m_stopCalCommand.split(" ", QString::SkipEmptyParts);
#endif
        QString program = allArgs[0];
        allArgs.pop_front();
        QProcess::startDetached(program, allArgs);
    }

    // Send calibration result to GUI
    if (getMessageQueueToGUI()) {
        getMessageQueueToGUI()->push(report);
    }
}

void RadioAstronomy::callOnStartTime(void (RadioAstronomy::*f)())
{
    qint64 delayMSecs = 0;

    if (m_settings.m_sweepStartAtTime) {
        delayMSecs = QDateTime::currentDateTime().msecsTo(m_settings.m_sweepStartDateTime);
    }

    if (delayMSecs > 0)
    {
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(MsgSweepStatus::create(QString("Waiting: %1").arg(m_settings.m_sweepStartDateTime.toString())));
        }
        qDebug() << "RadioAstronomy::callOnStartTime - Wait until " << m_settings.m_sweepStartDateTime.toString();
        // Wait before calling
        QObject::disconnect(m_sweepTimerConnection);
        m_sweepTimerConnection = m_sweepTimer.callOnTimeout(this, f);
        m_sweepTimer.start(delayMSecs);
    }
    else
    {
        // Call immediately
        (this->*f)();
    }
}

void RadioAstronomy::startMeasurement()
{
    m_basebandSink->getInputMessageQueue()->push(MsgStartMeasurements::create());
}

void RadioAstronomy::sweepStart()
{
    m_sweepStop = false;
    m_sweep1Start = m_settings.m_sweep1Start;
    m_sweep1Stop = m_settings.m_sweep1Stop;

    // Handle azimuth/l sweep through 0. E.g. 340deg -> 20deg with +vs step, or 20deg -> 340deg with -ve step
    if ((m_settings.m_sweep1Stop < m_settings.m_sweep1Start) && (m_settings.m_sweep1Step > 0)) {
        m_sweep1Stop = m_settings.m_sweep1Stop + 360.0;
    } else if ((m_settings.m_sweep1Stop > m_settings.m_sweep1Start) && (m_settings.m_sweep1Step < 0)) {
        m_sweep1Start += 360.0;
    }
    m_sweep1 = m_sweep1Start;
    m_sweep2 = m_settings.m_sweep2Start;

    const QRegExp re("F([0-9]+):([0-9]+)");
    if (re.indexIn(m_settings.m_starTracker) >= 0)
    {
        m_starTrackerFeatureSetIndex = re.capturedTexts()[1].toInt();
        m_starTrackerFeatureIndex = re.capturedTexts()[2].toInt();

        if (m_settings.m_sweepType == RadioAstronomySettings::SWP_AZEL) {
            ChannelWebAPIUtils::patchFeatureSetting(m_starTrackerFeatureSetIndex, m_starTrackerFeatureIndex, "target", "Custom Az/El");
        } else if (m_settings.m_sweepType == RadioAstronomySettings::SWP_LB) {
            ChannelWebAPIUtils::patchFeatureSetting(m_starTrackerFeatureSetIndex, m_starTrackerFeatureIndex, "target", "Custom l/b");
        }

        if (m_settings.m_rotator == "None")
        {
            m_rotatorFeatureSetIndex = -1;
            m_rotatorFeatureIndex = -1;

            sweep2();
            callOnStartTime(&RadioAstronomy::sweep1);
        }
        else if (re.indexIn(m_settings.m_rotator) >= 0)
        {
            m_rotatorFeatureSetIndex = re.capturedTexts()[1].toInt();
            m_rotatorFeatureIndex = re.capturedTexts()[2].toInt();

            sweep2();
            callOnStartTime(&RadioAstronomy::sweep1);
        }
        else
        {
            if (getMessageQueueToGUI()) {
                getMessageQueueToGUI()->push(MsgSweepStatus::create("Invalid rotator"));
            }
            qDebug() << "RadioAstronomy::sweepStart: No valid rotator feature is set";
        }
    }
    else
    {
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(MsgSweepStatus::create("Invalid Star Tracker"));
        }
        qDebug() << "RadioAstronomy::sweepStart: No valid StarTracker feature is set";
    }
}

void RadioAstronomy::sweep1()
{
    if (m_sweepStop)
    {
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(MsgSweepStatus::create("Stopped"));
        }
        sweepComplete();
    }
    else
    {
        if (m_settings.m_sweepType == RadioAstronomySettings::SWP_AZEL) {
            ChannelWebAPIUtils::patchFeatureSetting(m_starTrackerFeatureSetIndex, m_starTrackerFeatureIndex, "azimuth", Astronomy::modulo(m_sweep1, 360.0));
        } else if (m_settings.m_sweepType == RadioAstronomySettings::SWP_LB) {
            ChannelWebAPIUtils::patchFeatureSetting(m_starTrackerFeatureSetIndex, m_starTrackerFeatureIndex, "l", Astronomy::modulo(m_sweep1, 360.0));
        } else if (m_settings.m_sweepType == RadioAstronomySettings::SWP_OFFSET) {
            ChannelWebAPIUtils::patchFeatureSetting(m_starTrackerFeatureSetIndex, m_starTrackerFeatureIndex, "azimuthOffset", m_sweep1);
        }
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(MsgSweepStatus::create(QString("Rotating: %1,%2").arg(m_sweep1).arg(m_sweep2)));
        }
        qDebug() << "RadioAstronomy::sweep1 - Sweeping " << m_sweep1 << m_sweep2;
        QObject::disconnect(m_sweepTimerConnection);
        m_sweepTimerConnection = m_sweepTimer.callOnTimeout(this, &RadioAstronomy::waitUntilOnTarget);
        m_sweepTimer.start(100);
    }
}

void RadioAstronomy::sweep2()
{
    if (m_settings.m_sweepType == RadioAstronomySettings::SWP_AZEL) {
        ChannelWebAPIUtils::patchFeatureSetting(m_starTrackerFeatureSetIndex, m_starTrackerFeatureIndex, "elevation", m_sweep2);
    } else if (m_settings.m_sweepType == RadioAstronomySettings::SWP_LB) {
        ChannelWebAPIUtils::patchFeatureSetting(m_starTrackerFeatureSetIndex, m_starTrackerFeatureIndex, "b", m_sweep2);
    } else if (m_settings.m_sweepType == RadioAstronomySettings::SWP_OFFSET) {
        ChannelWebAPIUtils::patchFeatureSetting(m_starTrackerFeatureSetIndex, m_starTrackerFeatureIndex, "elevationOffset", m_sweep2);
    }
}

// Wait until the antenna is pointing at the target
void RadioAstronomy::waitUntilOnTarget()
{
    int onTarget;

    if (m_sweepStop)
    {
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(MsgSweepStatus::create("Stopped"));
        }
        sweepComplete();
    }
    else
    {
        if (m_settings.m_rotator == "None")
        {
            onTarget = true;
        }
        else if (!ChannelWebAPIUtils::getFeatureReportValue(m_rotatorFeatureSetIndex, m_rotatorFeatureIndex, "onTarget", onTarget))
        {
            sweepComplete();
            return;
        }

        if (onTarget)
        {
            if (getMessageQueueToGUI()) {
                getMessageQueueToGUI()->push(MsgSweepStatus::create("Settle"));
            }
            QObject::disconnect(m_sweepTimerConnection);
            m_sweepTimerConnection = m_sweepTimer.callOnTimeout(this, &RadioAstronomy::sweepStartMeasurement);
            m_sweepTimer.start(m_settings.m_sweep1Delay * 1000);
        }
        else
        {
            // Wait some more and retry
            QObject::disconnect(m_sweepTimerConnection);
            m_sweepTimerConnection = m_sweepTimer.callOnTimeout(this, &RadioAstronomy::waitUntilOnTarget);
            m_sweepTimer.start(100);
        }
    }
}

void RadioAstronomy::sweepStartMeasurement()
{
    if (getMessageQueueToGUI()) {
        getMessageQueueToGUI()->push(MsgSweepStatus::create(QString("Measure: %1,%2").arg(m_sweep1).arg(m_sweep2)));
    }
    // Start measurement
    m_sweeping = true;
    m_basebandSink->getInputMessageQueue()->push(MsgStartMeasurements::create());
}

void RadioAstronomy::sweepNext()
{
    if (m_sweepStop)
    {
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(MsgSweepStatus::create("Stopped"));
        }
        sweepComplete();
    }
    else
    {
        if (   ((m_sweep1 >= m_sweep1Stop) && (m_settings.m_sweep1Step >= 0))
            || ((m_sweep1 <= m_sweep1Stop) && (m_settings.m_sweep1Step < 0))
           )
        {
            if (   ((m_sweep2 >= m_settings.m_sweep2Stop) && (m_settings.m_sweep2Step >= 0))
                || ((m_sweep2 <= m_settings.m_sweep2Stop) && (m_settings.m_sweep2Step < 0))
               )
            {
                if (getMessageQueueToGUI()) {
                    getMessageQueueToGUI()->push(MsgSweepStatus::create("Complete"));
                }
                // Finished
                sweepComplete();
            }
            else
            {
                m_sweep2 += m_settings.m_sweep2Step;
                sweep2();
                m_sweep1 = m_sweep1Start;
                if (getMessageQueueToGUI()) {
                    getMessageQueueToGUI()->push(MsgSweepStatus::create("Delay"));
                }
                QObject::disconnect(m_sweepTimerConnection);
                m_sweepTimerConnection = m_sweepTimer.callOnTimeout(this, &RadioAstronomy::sweep1);
                m_sweepTimer.start(m_settings.m_sweep2Delay * 1000);
            }
        }
        else
        {
            m_sweep1 += m_settings.m_sweep1Step;
            if (getMessageQueueToGUI()) {
                getMessageQueueToGUI()->push(MsgSweepStatus::create("Delay"));
            }
            QObject::disconnect(m_sweepTimerConnection);
            m_sweepTimerConnection = m_sweepTimer.callOnTimeout(this, &RadioAstronomy::sweep1);
            m_sweepTimer.start(m_settings.m_sweep2Delay * 1000);
        }
    }
}

void RadioAstronomy::sweepComplete()
{
    ChannelWebAPIUtils::patchFeatureSetting(m_starTrackerFeatureSetIndex, m_starTrackerFeatureIndex, "elevationOffset", 0);
    ChannelWebAPIUtils::patchFeatureSetting(m_starTrackerFeatureSetIndex, m_starTrackerFeatureIndex, "azimuthOffset", 0);
    if (getMessageQueueToGUI()) {
        getMessageQueueToGUI()->push(MsgSweepComplete::create());
    }
}

void RadioAstronomy::applySettings(const RadioAstronomySettings& settings, bool force)
{
    qDebug() << "RadioAstronomy::applySettings:"
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
    if ((settings.m_sampleRate != m_settings.m_sampleRate) || force) {
        reverseAPIKeys.append("sampleRate");
    }
    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force) {
        reverseAPIKeys.append("rfBandwidth");
    }
    if ((settings.m_integration != m_settings.m_integration) || force) {
        reverseAPIKeys.append("integration");
    }
    if ((settings.m_fftSize != m_settings.m_fftSize) || force) {
        reverseAPIKeys.append("fftSize");
    }
    if ((settings.m_fftWindow != m_settings.m_fftWindow) || force) {
        reverseAPIKeys.append("fftWindow");
    }
    if ((settings.m_filterFreqs != m_settings.m_filterFreqs) || force) {
        reverseAPIKeys.append("filterFreqs");
    }

    if ((settings.m_starTracker != m_settings.m_starTracker) || force) {
        reverseAPIKeys.append("starTracker");
    }
    if ((settings.m_rotator != m_settings.m_rotator) || force) {
        reverseAPIKeys.append("rotator");
    }

    if ((settings.m_runMode != m_settings.m_runMode) || force) {
        reverseAPIKeys.append("runMode");
    }
    if ((settings.m_sweepStartAtTime != m_settings.m_sweepStartAtTime) || force) {
        reverseAPIKeys.append("sweepStartAtTime");
    }
    if ((settings.m_sweepStartDateTime != m_settings.m_sweepStartDateTime) || force) {
        reverseAPIKeys.append("sweepStartDateTime");
    }
    if ((settings.m_sweepType != m_settings.m_sweepType) || force) {
        reverseAPIKeys.append("sweepType");
    }
    if ((settings.m_sweep1Start != m_settings.m_sweep1Start) || force) {
        reverseAPIKeys.append("sweep1Start");
    }
    if ((settings.m_sweep1Stop != m_settings.m_sweep1Stop) || force) {
        reverseAPIKeys.append("sweep1Stop");
    }
    if ((settings.m_sweep1Step != m_settings.m_sweep1Step) || force) {
        reverseAPIKeys.append("sweep1Step");
    }
    if ((settings.m_sweep1Delay != m_settings.m_sweep1Delay) || force) {
        reverseAPIKeys.append("sweep1Delay");
    }
    if ((settings.m_sweep2Start != m_settings.m_sweep2Start) || force) {
        reverseAPIKeys.append("sweep2Start");
    }
    if ((settings.m_sweep2Stop != m_settings.m_sweep2Stop) || force) {
        reverseAPIKeys.append("sweep2Stop");
    }
    if ((settings.m_sweep2Step != m_settings.m_sweep2Step) || force) {
        reverseAPIKeys.append("sweep2Step");
    }
    if ((settings.m_sweep2Delay != m_settings.m_sweep2Delay) || force) {
        reverseAPIKeys.append("sweep2Delay");
    }

    if ((m_settings.m_starTracker != settings.m_starTracker)
        || (!settings.m_starTracker.isEmpty() && (m_selectedPipe == nullptr)) // Change in available pipes
        || force)
    {
        if (!settings.m_starTracker.isEmpty())
        {
            Feature *feature = nullptr;

            for (const auto& fval : m_availableFeatures)
            {
                QString starTrackerText = tr("F%1:%2 %3").arg(fval.m_featureSetIndex).arg(fval.m_featureIndex).arg(fval.m_type);

                if (settings.m_starTracker == starTrackerText)
                {
                    feature = m_availableFeatures.key(fval);
                    break;
                }
            }

            if (feature) {
                m_selectedPipe = feature;
            } else {
                qDebug() << "RadioAstronomy::applySettings: No plugin corresponding to target " << settings.m_starTracker;
            }
        }

        reverseAPIKeys.append("starTracker");
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

    m_basebandSink->getInputMessageQueue()->push(RadioAstronomyBaseband::MsgConfigureRadioAstronomyBaseband::create(settings, force));

    m_worker->getInputMessageQueue()->push(RadioAstronomyWorker::MsgConfigureRadioAstronomyWorker::create(settings, force));

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

QByteArray RadioAstronomy::serialize() const
{
    return m_settings.serialize();
}

bool RadioAstronomy::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureRadioAstronomy *msg = MsgConfigureRadioAstronomy::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureRadioAstronomy *msg = MsgConfigureRadioAstronomy::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int RadioAstronomy::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setRadioAstronomySettings(new SWGSDRangel::SWGRadioAstronomySettings());
    response.getRadioAstronomySettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int RadioAstronomy::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int RadioAstronomy::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    RadioAstronomySettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureRadioAstronomy *msg = MsgConfigureRadioAstronomy::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("RadioAstronomy::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureRadioAstronomy *msgToGUI = MsgConfigureRadioAstronomy::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

int RadioAstronomy::webapiActionsPost(
        const QStringList& channelActionsKeys,
        SWGSDRangel::SWGChannelActions& query,
        QString& errorMessage)
{
    SWGSDRangel::SWGRadioAstronomyActions *swgRadioAstronomyActions = query.getRadioAstronomyActions();

    if (swgRadioAstronomyActions)
    {
        if (channelActionsKeys.contains("start"))
        {
            getInputMessageQueue()->push(MsgStartSweep::create());
            return 202;
        }
        else
        {
            errorMessage = "Unknown action";
            return 400;
        }
    }
    else
    {
        errorMessage = "Missing RadioAstronomyActions in query";
        return 400;
    }
}

void RadioAstronomy::webapiUpdateChannelSettings(
        RadioAstronomySettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getRadioAstronomySettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("sampleRate")) {
        settings.m_sampleRate = response.getRadioAstronomySettings()->getSampleRate();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getRadioAstronomySettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("integration")) {
        settings.m_integration = response.getRadioAstronomySettings()->getIntegration();
    }
    if (channelSettingsKeys.contains("fftSize")) {
        settings.m_fftSize = response.getRadioAstronomySettings()->getFftSize();
    }
    if (channelSettingsKeys.contains("fftWindow")) {
        settings.m_fftWindow = (RadioAstronomySettings::FFTWindow)response.getRadioAstronomySettings()->getFftWindow();
    }
    if (channelSettingsKeys.contains("filterFreqs")) {
        settings.m_filterFreqs = *response.getRadioAstronomySettings()->getFilterFreqs();
    }

    if (channelSettingsKeys.contains("starTracker")) {
        settings.m_starTracker = *response.getRadioAstronomySettings()->getStarTracker();
    }
    if (channelSettingsKeys.contains("rotator")) {
        settings.m_rotator = *response.getRadioAstronomySettings()->getRotator();
    }

    if (channelSettingsKeys.contains("runMode")) {
        settings.m_runMode = (RadioAstronomySettings::RunMode)response.getRadioAstronomySettings()->getRunMode();
    }
    if (channelSettingsKeys.contains("sweepStartAtTime")) {
        settings.m_sweepStartAtTime = (bool)response.getRadioAstronomySettings()->getSweepStartAtTime();
    }
    if (channelSettingsKeys.contains("sweepStartDateTime")) {
        settings.m_sweepStartDateTime = QDateTime::fromString(*response.getRadioAstronomySettings()->getRotator(), Qt::ISODate);
    }
    if (channelSettingsKeys.contains("sweepType")) {
        settings.m_sweepType = (RadioAstronomySettings::SweepType)response.getRadioAstronomySettings()->getSweepType();
    }
    if (channelSettingsKeys.contains("sweep1Start")) {
        settings.m_sweep1Start = response.getRadioAstronomySettings()->getSweep1Start();
    }
    if (channelSettingsKeys.contains("sweep1Stop")) {
        settings.m_sweep1Stop = response.getRadioAstronomySettings()->getSweep1Stop();
    }
    if (channelSettingsKeys.contains("sweep1Step")) {
        settings.m_sweep1Step = response.getRadioAstronomySettings()->getSweep1Step();
    }
    if (channelSettingsKeys.contains("sweep1Delay")) {
        settings.m_sweep1Delay = response.getRadioAstronomySettings()->getSweep1Delay();
    }
    if (channelSettingsKeys.contains("sweep12Start")) {
        settings.m_sweep2Start = response.getRadioAstronomySettings()->getSweep2Start();
    }
    if (channelSettingsKeys.contains("sweep12Stop")) {
        settings.m_sweep2Stop = response.getRadioAstronomySettings()->getSweep2Stop();
    }
    if (channelSettingsKeys.contains("sweep2Step")) {
        settings.m_sweep2Step = response.getRadioAstronomySettings()->getSweep2Step();
    }
    if (channelSettingsKeys.contains("sweep2Delay")) {
        settings.m_sweep2Delay = response.getRadioAstronomySettings()->getSweep2Delay();
    }

    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getRadioAstronomySettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getRadioAstronomySettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getRadioAstronomySettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getRadioAstronomySettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getRadioAstronomySettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getRadioAstronomySettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getRadioAstronomySettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getRadioAstronomySettings()->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getRadioAstronomySettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getRadioAstronomySettings()->getRollupState());
    }
}

void RadioAstronomy::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const RadioAstronomySettings& settings)
{
    response.getRadioAstronomySettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getRadioAstronomySettings()->setSampleRate(settings.m_sampleRate);
    response.getRadioAstronomySettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getRadioAstronomySettings()->setIntegration(settings.m_integration);
    response.getRadioAstronomySettings()->setFftSize(settings.m_fftSize);
    response.getRadioAstronomySettings()->setFftWindow((int)settings.m_fftWindow);
    response.getRadioAstronomySettings()->setFilterFreqs(new QString(settings.m_filterFreqs));

    response.getRadioAstronomySettings()->setStarTracker(new QString(settings.m_starTracker));
    response.getRadioAstronomySettings()->setRotator(new QString(settings.m_rotator));

    response.getRadioAstronomySettings()->setRunMode((int)settings.m_runMode);
    response.getRadioAstronomySettings()->setSweepStartAtTime((int)settings.m_sweepStartAtTime);
    response.getRadioAstronomySettings()->setSweepStartDateTime(new QString(settings.m_sweepStartDateTime.toString(Qt::ISODate)));
    response.getRadioAstronomySettings()->setSweepType((int)settings.m_sweepType);
    response.getRadioAstronomySettings()->setSweep1Start(settings.m_sweep1Start);
    response.getRadioAstronomySettings()->setSweep1Stop(settings.m_sweep1Stop);
    response.getRadioAstronomySettings()->setSweep1Step(settings.m_sweep1Step);
    response.getRadioAstronomySettings()->setSweep1Delay(settings.m_sweep1Delay);
    response.getRadioAstronomySettings()->setSweep2Start(settings.m_sweep2Start);
    response.getRadioAstronomySettings()->setSweep2Stop(settings.m_sweep2Stop);
    response.getRadioAstronomySettings()->setSweep2Step(settings.m_sweep2Step);
    response.getRadioAstronomySettings()->setSweep2Delay(settings.m_sweep2Delay);

    response.getRadioAstronomySettings()->setRgbColor(settings.m_rgbColor);
    if (response.getRadioAstronomySettings()->getTitle()) {
        *response.getRadioAstronomySettings()->getTitle() = settings.m_title;
    } else {
        response.getRadioAstronomySettings()->setTitle(new QString(settings.m_title));
    }

    response.getRadioAstronomySettings()->setStreamIndex(settings.m_streamIndex);
    response.getRadioAstronomySettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getRadioAstronomySettings()->getReverseApiAddress()) {
        *response.getRadioAstronomySettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getRadioAstronomySettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getRadioAstronomySettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getRadioAstronomySettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getRadioAstronomySettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getRadioAstronomySettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getRadioAstronomySettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getRadioAstronomySettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getRadioAstronomySettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getRadioAstronomySettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getRadioAstronomySettings()->setRollupState(swgRollupState);
        }
    }
}

void RadioAstronomy::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const RadioAstronomySettings& settings, bool force)
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

void RadioAstronomy::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const RadioAstronomySettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("RadioAstronomy"));
    swgChannelSettings->setRadioAstronomySettings(new SWGSDRangel::SWGRadioAstronomySettings());
    SWGSDRangel::SWGRadioAstronomySettings *swgRadioAstronomySettings = swgChannelSettings->getRadioAstronomySettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgRadioAstronomySettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("sampleRate") || force) {
        swgRadioAstronomySettings->setInputFrequencyOffset(settings.m_sampleRate);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgRadioAstronomySettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("integration") || force) {
        swgRadioAstronomySettings->setRfBandwidth(settings.m_integration);
    }
    if (channelSettingsKeys.contains("fftSize") || force) {
        swgRadioAstronomySettings->setRfBandwidth(settings.m_fftSize);
    }
    if (channelSettingsKeys.contains("fftWindow") || force) {
        swgRadioAstronomySettings->setRfBandwidth((int)settings.m_fftWindow);
    }
    if (channelSettingsKeys.contains("filterFreqs") || force) {
        swgRadioAstronomySettings->setFilterFreqs(new QString(settings.m_filterFreqs));
    }

    if (channelSettingsKeys.contains("starTracker") || force) {
        swgRadioAstronomySettings->setStarTracker(new QString(settings.m_starTracker));
    }
    if (channelSettingsKeys.contains("rotator") || force) {
        swgRadioAstronomySettings->setRotator(new QString(settings.m_rotator));
    }

    if (channelSettingsKeys.contains("runMode") || force) {
        swgRadioAstronomySettings->setRunMode((int)settings.m_runMode);
    }
    if (channelSettingsKeys.contains("sweepStartAtTime") || force) {
        swgRadioAstronomySettings->setSweepStartAtTime((int)settings.m_sweepStartAtTime);
    }
    if (channelSettingsKeys.contains("sweepStartDateTime") || force) {
        swgRadioAstronomySettings->setSweepStartDateTime(new QString(settings.m_sweepStartDateTime.toString(Qt::ISODate)));
    }
    if (channelSettingsKeys.contains("sweepType") || force) {
        swgRadioAstronomySettings->setSweepType(settings.m_sweepType);
    }
    if (channelSettingsKeys.contains("sweep1Start") || force) {
        swgRadioAstronomySettings->setSweep1Start(settings.m_sweep1Start);
    }
    if (channelSettingsKeys.contains("sweep1Stop") || force) {
        swgRadioAstronomySettings->setSweep1Stop(settings.m_sweep1Stop);
    }
    if (channelSettingsKeys.contains("sweep1Step") || force) {
        swgRadioAstronomySettings->setSweep1Step(settings.m_sweep1Step);
    }
    if (channelSettingsKeys.contains("sweep2Delay") || force) {
        swgRadioAstronomySettings->setSweep2Delay(settings.m_sweep2Delay);
    }
    if (channelSettingsKeys.contains("sweep2Start") || force) {
        swgRadioAstronomySettings->setSweep2Start(settings.m_sweep2Start);
    }
    if (channelSettingsKeys.contains("sweep2Stop") || force) {
        swgRadioAstronomySettings->setSweep2Stop(settings.m_sweep2Stop);
    }
    if (channelSettingsKeys.contains("sweep2Step") || force) {
        swgRadioAstronomySettings->setSweep2Step(settings.m_sweep2Step);
    }
    if (channelSettingsKeys.contains("sweep2Delay") || force) {
        swgRadioAstronomySettings->setSweep2Delay(settings.m_sweep2Delay);
    }

    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgRadioAstronomySettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgRadioAstronomySettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgRadioAstronomySettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgRadioAstronomySettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgRadioAstronomySettings->setRollupState(swgRollupState);
    }
}

void RadioAstronomy::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "RadioAstronomy::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("RadioAstronomy::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void RadioAstronomy::handleIndexInDeviceSetChanged(int index)
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

void RadioAstronomy::scanAvailableFeatures()
{
    qDebug("RadioAstronomy::scanAvailableFeatures");
    MainCore *mainCore = MainCore::instance();
    MessagePipes& messagePipes = mainCore->getMessagePipes();
    std::vector<FeatureSet*>& featureSets = mainCore->getFeatureeSets();
    m_availableFeatures.clear();
    m_rotators.clear();

    for (const auto& featureSet : featureSets)
    {
        for (int fei = 0; fei < featureSet->getNumberOfFeatures(); fei++)
        {
            Feature *feature = featureSet->getFeatureAt(fei);

            if (RadioAstronomySettings::m_pipeURIs.contains(feature->getURI()))
            {
                if (!m_availableFeatures.contains(feature))
                {
                    qDebug("RadioAstronomy::scanAvailableFeatures: register %d:%d %s (%p)",
                        featureSet->getIndex(), fei, qPrintable(feature->getURI()), feature);
                    ObjectPipe *pipe = messagePipes.registerProducerToConsumer(feature, this, "startracker.target");
                    MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
                    QObject::connect(
                        messageQueue,
                        &MessageQueue::messageEnqueued,
                        this,
                        [=](){ this->handleFeatureMessageQueue(messageQueue); },
                        Qt::QueuedConnection
                    );
                    connect(pipe, SIGNAL(toBeDeleted(int, QObject*)), this, SLOT(handleMessagePipeToBeDeleted(int, QObject*)));
                    RadioAstronomySettings::AvailableFeature availableFeature =
                        RadioAstronomySettings::AvailableFeature{featureSet->getIndex(), fei, feature->getIdentifier()};
                    m_availableFeatures[feature] = availableFeature;
                }
            }
            else if (feature->getURI() == "sdrangel.feature.gs232controller")
            {
                RadioAstronomySettings::AvailableFeature rotator =
                    RadioAstronomySettings::AvailableFeature{featureSet->getIndex(), fei, feature->getIdentifier()};
                m_rotators[feature] = rotator;
            }
        }
    }

    notifyUpdateFeatures();
    notifyUpdateRotators();
}

void RadioAstronomy::notifyUpdateFeatures()
{
    if (getMessageQueueToGUI())
    {
        MsgReportAvailableFeatures *msg = MsgReportAvailableFeatures::create();
        msg->getFeatures() = m_availableFeatures.values();
        getMessageQueueToGUI()->push(msg);
    }
}

void RadioAstronomy::notifyUpdateRotators()
{
    if (getMessageQueueToGUI())
    {
        MsgReportAvailableRotators *msg = MsgReportAvailableRotators::create();
        msg->getFeatures() = m_rotators.values();
        getMessageQueueToGUI()->push(msg);
    }
}

void RadioAstronomy::handleFeatureAdded(int featureSetIndex, Feature *feature)
{
    qDebug("RadioAstronomy::handleFeatureAdded: featureSetIndex: %d:%d feature: %s (%p)",
        featureSetIndex, feature->getIndexInFeatureSet(), qPrintable(feature->getURI()), feature);
    FeatureSet *featureSet = MainCore::instance()->getFeatureeSets()[featureSetIndex];

    if (RadioAstronomySettings::m_pipeURIs.contains(feature->getURI()))
    {
        int fei = feature->getIndexInFeatureSet();

        if (!m_availableFeatures.contains(feature))
        {
            MessagePipes& messagePipes = MainCore::instance()->getMessagePipes();
            ObjectPipe *pipe = messagePipes.registerProducerToConsumer(feature, this, "startracker.target");
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            QObject::connect(
                messageQueue,
                &MessageQueue::messageEnqueued,
                this,
                [=](){ this->handleFeatureMessageQueue(messageQueue); },
                Qt::QueuedConnection
            );
            QObject::connect(
                pipe,
                &ObjectPipe::toBeDeleted,
                this,
                &RadioAstronomy::handleMessagePipeToBeDeleted
            );
        }

        RadioAstronomySettings::AvailableFeature availableFeature =
            RadioAstronomySettings::AvailableFeature{featureSet->getIndex(), fei, feature->getIdentifier()};
        m_availableFeatures[feature] = availableFeature;

        notifyUpdateFeatures();
    }
    else if (feature->getURI() == "sdrangel.feature.gs232controller")
    {
        if (!m_rotators.contains(feature))
        {
            RadioAstronomySettings::AvailableFeature rotator =
                RadioAstronomySettings::AvailableFeature{featureSet->getIndex(), feature->getIndexInFeatureSet(), feature->getIdentifier()};
            m_rotators[feature] = rotator;
        }

        notifyUpdateRotators();
    }
}

void RadioAstronomy::handleFeatureRemoved(int featureSetIndex, Feature *feature)
{
    qDebug("RadioAstronomy::handleFeatureRemoved: featureSetIndex: %d (%p)", featureSetIndex, feature);

    if (m_rotators.contains(feature))
    {
        m_rotators.remove(feature);
        notifyUpdateRotators();
    }
}

void RadioAstronomy::handleMessagePipeToBeDeleted(int reason, QObject* object)
{
    if ((reason == 0) && m_availableFeatures.contains((Feature*) object)) // producer (feature)
    {
        qDebug("RadioAstronomy::handleMessagePipeToBeDeleted: removing feature at (%p)", object);
        m_availableFeatures.remove((Feature*) object);
        notifyUpdateFeatures();
    }
}

void RadioAstronomy::handleFeatureMessageQueue(MessageQueue* messageQueue)
{
    Message* message;

    while ((message = messageQueue->pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}
