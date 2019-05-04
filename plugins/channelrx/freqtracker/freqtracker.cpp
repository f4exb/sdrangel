///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
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

#include "freqtracker.h"

#include <QTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>

#include <stdio.h>
#include <complex.h>

#include "SWGChannelSettings.h"
#include "SWGFreqTrackerSettings.h"
#include "SWGChannelReport.h"
#include "SWGFreqTrackerReport.h"

#include "dsp/downchannelizer.h"
#include "audio/audiooutput.h"
#include "dsp/dspengine.h"
#include "dsp/threadedbasebandsamplesink.h"
#include "dsp/dspcommands.h"
#include "dsp/fftfilt.h"
#include "device/devicesourceapi.h"
#include "util/db.h"
#include "util/stepfunctions.h"

MESSAGE_CLASS_DEFINITION(FreqTracker::MsgConfigureFreqTracker, Message)
MESSAGE_CLASS_DEFINITION(FreqTracker::MsgSampleRateNotification, Message)
MESSAGE_CLASS_DEFINITION(FreqTracker::MsgConfigureChannelizer, Message)

const QString FreqTracker::m_channelIdURI = "sdrangel.channel.freqtracker";
const QString FreqTracker::m_channelId = "FreqTracker";
const int FreqTracker::m_udpBlockSize = 512;

FreqTracker::FreqTracker(DeviceSourceAPI *deviceAPI) :
        ChannelSinkAPI(m_channelIdURI),
        m_deviceAPI(deviceAPI),
        m_deviceSampleRate(48000),
        m_inputSampleRate(48000),
        m_inputFrequencyOffset(0),
        m_channelSampleRate(48000),
        m_running(false),
        m_squelchOpen(false),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_settingsMutex(QMutex::Recursive)
{
    setObjectName(m_channelId);

	m_magsq = 0.0;

    m_channelizer = new DownChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
    m_deviceAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);

    m_pll.computeCoefficients(0.05, 0.707, 1000);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));

    applyChannelSettings(m_inputSampleRate, m_inputFrequencyOffset, true);
}

FreqTracker::~FreqTracker()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
    m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
}

void FreqTracker::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
	Complex ci;

	if (!m_running) {
        return;
    }

	m_settingsMutex.lock();

	for (SampleVector::const_iterator it = begin; it != end; ++it)
	{
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

		if (m_interpolatorDistance < 1.0f) // interpolate
		{
            processOneSample(ci);

		    while (m_interpolator.interpolate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci);
            }

            m_interpolatorDistanceRemain += m_interpolatorDistance;
		}
		else // decimate
		{
	        if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
	        {
	            processOneSample(ci);
	            m_interpolatorDistanceRemain += m_interpolatorDistance;
	        }
		}
	}

	m_settingsMutex.unlock();
}

void FreqTracker::processOneSample(Complex &ci)
{
    Real re = ci.real() / SDR_RX_SCALEF;
    Real im = ci.imag() / SDR_RX_SCALEF;
    Real magsq = re*re + im*im;
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();
    m_magsqSum += magsq;

    if (magsq > m_magsqPeak)
    {
        m_magsqPeak = magsq;
    }

    m_magsqCount++;

    if (m_magsq < m_squelchLevel)
    {
        if (m_squelchCount > 0) {
            m_squelchCount--;
        }
    }
    else
    {
        if (m_squelchCount < m_channelSampleRate / 10) {
            m_squelchCount++;
        }
    }

    m_squelchOpen = (m_squelchCount >= m_channelSampleRate / 20);

    if (m_squelchOpen)
    {
        if (m_settings.m_trackerType == FreqTrackerSettings::TrackerFLL)
        {
            m_fll.feed(re, im);
        }
        else if (m_settings.m_trackerType == FreqTrackerSettings::TrackerPLL)
        {
            m_pll.feed(re, im);
        }
    }
}

void FreqTracker::start()
{
	qDebug("FreqTracker::start");
	m_squelchCount = 0;
    applyChannelSettings(m_inputSampleRate, m_inputFrequencyOffset, true);
    m_running = true;
}

void FreqTracker::stop()
{
    qDebug("FreqTracker::stop");
    m_running = false;
}

Real FreqTracker::getFrequency() const
{
    if (!m_settings.m_tracking) {
        return 0;
    } else if (m_settings.m_trackerType == FreqTrackerSettings::TrackerPLL) {
        return (m_pll.getFreq() * m_channelSampleRate) / (2.0 * M_PI);
    } else if (m_settings.m_trackerType == FreqTrackerSettings::TrackerFLL) {
        return (m_fll.getFreq() * m_channelSampleRate) / (2.0 * M_PI);
    } else {
        return 0;
    }
}

bool FreqTracker::handleMessage(const Message& cmd)
{
    if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_deviceSampleRate = notif.getSampleRate();

        qDebug() << "FreqTracker::handleMessage: DSPSignalNotification:"
                << " m_deviceSampleRate: " << m_deviceSampleRate
                << " centerFrequency: " << notif.getCenterFrequency();

        configureChannelizer();

        return true;
    }
    else if (DownChannelizer::MsgChannelizerNotification::match(cmd))
	{
		DownChannelizer::MsgChannelizerNotification& notif = (DownChannelizer::MsgChannelizerNotification&) cmd;

        qDebug() << "FreqTracker::handleMessage: MsgChannelizerNotification:"
                << " inputSampleRate: " << notif.getSampleRate()
                << " inputFrequencyOffset: " << notif.getFrequencyOffset();

        applyChannelSettings(notif.getSampleRate(), notif.getFrequencyOffset());
        setInterpolator();

		return true;
	}
	else if (MsgConfigureFreqTracker::match(cmd))
	{
        MsgConfigureFreqTracker& cfg = (MsgConfigureFreqTracker&) cmd;
        qDebug() << "FreqTracker::handleMessage: MsgConfigureFreqTracker";
        applySettings(cfg.getSettings(), cfg.getForce());

		return true;
	}
	else
	{
		return false;
	}
}

void FreqTracker::applyChannelSettings(int inputSampleRate, int inputFrequencyOffset, bool force)
{
    qDebug() << "AMDemod::applyChannelSettings:"
            << " inputSampleRate: " << inputSampleRate
            << " inputFrequencyOffset: " << inputFrequencyOffset;

    if ((m_inputFrequencyOffset != inputFrequencyOffset) ||
        (m_inputSampleRate != inputSampleRate) || force)
    {
        m_nco.setFreq(-inputFrequencyOffset, inputSampleRate);
    }

    if ((m_inputSampleRate != inputSampleRate) || force) {
        setInterpolator();
    }

    m_inputSampleRate = inputSampleRate;
    m_inputFrequencyOffset = inputFrequencyOffset;
}

void FreqTracker::applySettings(const FreqTrackerSettings& settings, bool force)
{
    qDebug() << "FreqTracker::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_log2Decim: " << settings.m_log2Decim
            << " m_squelch: " << settings.m_squelch
            << " m_rgbColor: " << settings.m_rgbColor
            << " m_title: " << settings.m_title
            << " m_tracking: " << settings.m_tracking
            << " m_trackerType: " << settings.m_trackerType
            << " m_pllPskOrder: " << settings.m_pllPskOrder
            << " m_rrc: " << settings.m_rrc
            << " m_rrcRolloff: " << settings.m_rrcRolloff
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIPort: " << settings.m_reverseAPIPort
            << " m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
            << " m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
            << " force: " << force;

    QList<QString> reverseAPIKeys;
    bool updateChannelizer = false;
    bool updateInterpolator = false;

    if ((m_settings.m_inputFrequencyOffset != settings.m_inputFrequencyOffset) || force)
    {
        reverseAPIKeys.append("inputFrequencyOffset");
        updateChannelizer = true;
    }

    if ((m_settings.m_log2Decim != settings.m_log2Decim) || force)
    {
        reverseAPIKeys.append("log2Decim");
        updateChannelizer = true;
    }

    if ((m_settings.m_rfBandwidth != settings.m_rfBandwidth) || force)
    {
        updateInterpolator = true;
        reverseAPIKeys.append("rfBandwidth");
    }

    if ((m_settings.m_squelch != settings.m_squelch) || force)
    {
        m_squelchLevel = CalcDb::powerFromdB(settings.m_squelch);
        reverseAPIKeys.append("squelch");
    }

    if ((m_settings.m_rgbColor != settings.m_rgbColor) || force) {
        reverseAPIKeys.append("rgbColor");
    }
    if ((m_settings.m_title != settings.m_title) || force) {
        reverseAPIKeys.append("title");
    }

    if ((m_settings.m_tracking != settings.m_tracking) || force)
    {
        reverseAPIKeys.append("tracking");
        if (settings.m_tracking)
        {
            m_pll.reset();
            m_fll.reset();
        }
    }

    if ((m_settings.m_trackerType != settings.m_trackerType) || force)
    {
        reverseAPIKeys.append("trackerType");

        if (settings.m_trackerType == FreqTrackerSettings::TrackerFLL) {
            m_fll.reset();
        } else if (settings.m_trackerType == FreqTrackerSettings::TrackerPLL) {
            m_pll.reset();
        }
    }

    if ((m_settings.m_pllPskOrder != settings.m_pllPskOrder) || force)
    {
        reverseAPIKeys.append("pllPskOrder");

        if (settings.m_pllPskOrder < 32) {
            m_pll.setPskOrder(settings.m_pllPskOrder);
        }
    }

    if ((m_settings.m_rrc != settings.m_rrc) || force) {
        reverseAPIKeys.append("rrc");
    }
    if ((m_settings.m_rrcRolloff != settings.m_rrcRolloff) || force) {
        reverseAPIKeys.append("rrcRolloff");
    }

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

    if (updateChannelizer) {
        configureChannelizer();
    } else if (updateInterpolator) {
        setInterpolator();
    }
}

void FreqTracker::setInterpolator()
{
    m_settingsMutex.lock();
    m_interpolator.create(16, m_inputSampleRate, m_settings.m_rfBandwidth / 2.2f);
    m_interpolatorDistanceRemain = 0;
    m_interpolatorDistance = (Real) m_inputSampleRate / (Real) m_channelSampleRate;
    m_settingsMutex.unlock();
}

void FreqTracker::configureChannelizer()
{
    m_channelSampleRate = m_deviceSampleRate / (1<<m_settings.m_log2Decim);

    qDebug() << "FreqTracker::configureChannelizer:"
            << " sampleRate: " << m_channelSampleRate
            << " inputFrequencyOffset: " << m_settings.m_inputFrequencyOffset;

    m_pll.setSampleRate(m_channelSampleRate);
    m_fll.setSampleRate(m_channelSampleRate);

    m_channelizer->configure(m_channelizer->getInputMessageQueue(),
        m_channelSampleRate,
        m_settings.m_inputFrequencyOffset);

    if (m_guiMessageQueue)
    {
        MsgSampleRateNotification *msg = MsgSampleRateNotification::create(m_deviceSampleRate / (1<<m_settings.m_log2Decim));
        m_guiMessageQueue->push(msg);
    }
}

QByteArray FreqTracker::serialize() const
{
    return m_settings.serialize();
}

bool FreqTracker::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureFreqTracker *msg = MsgConfigureFreqTracker::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureFreqTracker *msg = MsgConfigureFreqTracker::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int FreqTracker::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setFreqTrackerSettings(new SWGSDRangel::SWGFreqTrackerSettings());
    response.getFreqTrackerSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int FreqTracker::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    FreqTrackerSettings settings = m_settings;

    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getFreqTrackerSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getFreqTrackerSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getFreqTrackerSettings()->getLog2Decim();
    }
    if (channelSettingsKeys.contains("squelch")) {
        settings.m_squelch = response.getFreqTrackerSettings()->getSquelch();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getFreqTrackerSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getFreqTrackerSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("tracking")) {
        settings.m_tracking = response.getFreqTrackerSettings()->getTracking() ? 1 : 0;
    }
    if (channelSettingsKeys.contains("trackerType"))
    {
        int32_t trackerTypeCode = response.getFreqTrackerSettings()->getTrackerType();
        settings.m_trackerType = trackerTypeCode < 0 ?
            FreqTrackerSettings::TrackerFLL : trackerTypeCode > 1 ?
                FreqTrackerSettings::TrackerPLL : (FreqTrackerSettings::TrackerType) trackerTypeCode;
    }
    if (channelSettingsKeys.contains("pllPskOrder")) {
        settings.m_pllPskOrder = response.getFreqTrackerSettings()->getPllPskOrder();
    }
    if (channelSettingsKeys.contains("rrc")) {
        settings.m_rrc = response.getFreqTrackerSettings()->getRrc() ? 1 : 0;
    }
    if (channelSettingsKeys.contains("rrcRolloff")) {
        settings.m_rrcRolloff = response.getFreqTrackerSettings()->getRrcRolloff();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getAmDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getAmDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getAmDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getAmDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getAmDemodSettings()->getReverseApiChannelIndex();
    }

    MsgConfigureFreqTracker *msg = MsgConfigureFreqTracker::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("FreqTracker::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureFreqTracker *msgToGUI = MsgConfigureFreqTracker::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

int FreqTracker::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setFreqTrackerReport(new SWGSDRangel::SWGFreqTrackerReport());
    response.getFreqTrackerReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void FreqTracker::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const FreqTrackerSettings& settings)
{
    response.getFreqTrackerSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getFreqTrackerSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getFreqTrackerSettings()->setRgbColor(settings.m_rgbColor);
    response.getFreqTrackerSettings()->setSquelch(settings.m_squelch);

    if (response.getFreqTrackerSettings()->getTitle()) {
        *response.getFreqTrackerSettings()->getTitle() = settings.m_title;
    } else {
        response.getFreqTrackerSettings()->setTitle(new QString(settings.m_title));
    }

    response.getFreqTrackerSettings()->setTrackerType((int) m_settings.m_trackerType);
    response.getFreqTrackerSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getFreqTrackerSettings()->getReverseApiAddress()) {
        *response.getFreqTrackerSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getFreqTrackerSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getFreqTrackerSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getFreqTrackerSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getFreqTrackerSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
}

void FreqTracker::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);

    response.getFreqTrackerReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    response.getFreqTrackerReport()->setSquelch(m_squelchOpen ? 1 : 0);
    response.getFreqTrackerReport()->setSampleRate(m_channelSampleRate);
    response.getFreqTrackerReport()->setChannelSampleRate(m_inputSampleRate);
}

void FreqTracker::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const FreqTrackerSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setTx(0);
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("FreqTracker"));
    swgChannelSettings->setFreqTrackerSettings(new SWGSDRangel::SWGFreqTrackerSettings());
    SWGSDRangel::SWGFreqTrackerSettings *swgFreqTrackerSettings = swgChannelSettings->getFreqTrackerSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgFreqTrackerSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgFreqTrackerSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgFreqTrackerSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("squelch") || force) {
        swgFreqTrackerSettings->setSquelch(settings.m_squelch);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgFreqTrackerSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("trackerType") || force) {
        swgFreqTrackerSettings->setTrackerType((int) settings.m_trackerType);
    }

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/channel/%4/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex)
            .arg(settings.m_reverseAPIChannelIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer=new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgChannelSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);

    delete swgChannelSettings;
}

void FreqTracker::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "FreqTracker::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
        return;
    }

    QString answer = reply->readAll();
    answer.chop(1); // remove last \n
    qDebug("FreqTracker::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
}
