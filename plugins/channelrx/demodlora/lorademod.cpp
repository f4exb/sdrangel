///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// (c) 2015 John Greb                                                            //
// (c) 2020 Edouard Griffiths, F4EXB                                             //
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

#include <stdio.h>

#include <QTime>
#include <QDebug>
#include <QThread>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>

#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "util/db.h"

#include "SWGChannelSettings.h"
#include "SWGChannelReport.h"
#include "SWGLoRaDemodReport.h"

#include "lorademodmsg.h"
#include "lorademod.h"

MESSAGE_CLASS_DEFINITION(LoRaDemod::MsgConfigureLoRaDemod, Message)
MESSAGE_CLASS_DEFINITION(LoRaDemod::MsgReportDecodeBytes, Message)
MESSAGE_CLASS_DEFINITION(LoRaDemod::MsgReportDecodeString, Message)

const QString LoRaDemod::m_channelIdURI = "sdrangel.channel.lorademod";
const QString LoRaDemod::m_channelId = "LoRaDemod";

LoRaDemod::LoRaDemod(DeviceAPI* deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSampleRate(0),
        m_lastMsgSignalDb(0.0),
        m_lastMsgNoiseDb(0.0),
        m_lastMsgSyncWord(0),
        m_lastMsgPacketLength(0),
        m_lastMsgNbParityBits(0),
        m_lastMsgHasCRC(false),
        m_lastMsgNbSymbols(0),
        m_lastMsgNbCodewords(0),
        m_lastMsgEarlyEOM(false),
        m_lastMsgHeaderCRC(false),
        m_lastMsgHeaderParityStatus(0),
        m_lastMsgPayloadCRC(false),
        m_lastMsgPayloadParityStatus(0)
{
	setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSink = new LoRaDemodBaseband();
    m_basebandSink->setDecoderMessageQueue(getInputMessageQueue()); // Decoder held on the main thread
    m_basebandSink->moveToThread(m_thread);

	applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);
}

LoRaDemod::~LoRaDemod()
{
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);
    delete m_basebandSink;
    delete m_thread;
}


void LoRaDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool pO)
{
    (void) pO;
	m_basebandSink->feed(begin, end);
}

void LoRaDemod::start()
{
    qDebug() << "LoRaDemod::start";

    if (m_basebandSampleRate != 0) {
        m_basebandSink->setBasebandSampleRate(m_basebandSampleRate);
    }

    m_basebandSink->reset();
    m_thread->start();
}

void LoRaDemod::stop()
{
    qDebug() << "LoRaDemod::stop";
	m_thread->exit();
	m_thread->wait();
}

bool LoRaDemod::handleMessage(const Message& cmd)
{
	if (MsgConfigureLoRaDemod::match(cmd))
	{
		qDebug() << "LoRaDemod::handleMessage: MsgConfigureLoRaDemod";
		MsgConfigureLoRaDemod& cfg = (MsgConfigureLoRaDemod&) cmd;
		LoRaDemodSettings settings = cfg.getSettings();
		applySettings(settings, cfg.getForce());

		return true;
	}
    else if (LoRaDemodMsg::MsgDecodeSymbols::match(cmd))
    {
        qDebug() << "LoRaDemod::handleMessage: MsgDecodeSymbols";
        LoRaDemodMsg::MsgDecodeSymbols& msg = (LoRaDemodMsg::MsgDecodeSymbols&) cmd;
        m_lastMsgSignalDb = msg.getSingalDb();
        m_lastMsgNoiseDb = msg.getNoiseDb();
        m_lastMsgSyncWord = msg.getSyncWord();

        if (m_settings.m_codingScheme == LoRaDemodSettings::CodingLoRa)
        {
            m_decoder.decodeSymbols(msg.getSymbols(), m_lastMsgBytes);
            QDateTime dt = QDateTime::currentDateTime();
            m_lastMsgTimestamp = dt.toString(Qt::ISODateWithMs);
            m_lastMsgPacketLength = m_decoder.getPacketLength();
            m_lastMsgNbParityBits = m_decoder.getNbParityBits();
            m_lastMsgHasCRC = m_decoder.getHasCRC();
            m_lastMsgNbSymbols = m_decoder.getNbSymbols();
            m_lastMsgNbCodewords = m_decoder.getNbCodewords();
            m_lastMsgEarlyEOM = m_decoder.getEarlyEOM();
            m_lastMsgHeaderCRC = m_decoder.getHeaderCRCStatus();
            m_lastMsgHeaderParityStatus = m_decoder.getHeaderParityStatus();
            m_lastMsgPayloadCRC = m_decoder.getPayloadCRCStatus();
            m_lastMsgPayloadParityStatus = m_decoder.getPayloadParityStatus();

            QByteArray bytesCopy(m_lastMsgBytes);
            bytesCopy.truncate(m_lastMsgPacketLength);
            bytesCopy.replace('\0', " ");
            m_lastMsgString = QString(bytesCopy.toStdString().c_str());

            if (getMessageQueueToGUI())
            {
                MsgReportDecodeBytes *msgToGUI = MsgReportDecodeBytes::create(m_lastMsgBytes);
                msgToGUI->setSyncWord(m_lastMsgSyncWord);
                msgToGUI->setSignalDb(m_lastMsgSignalDb);
                msgToGUI->setNoiseDb(m_lastMsgNoiseDb);
                msgToGUI->setPacketSize(m_lastMsgPacketLength);
                msgToGUI->setNbParityBits(m_lastMsgNbParityBits);
                msgToGUI->setHasCRC(m_lastMsgHasCRC);
                msgToGUI->setNbSymbols(m_lastMsgNbSymbols);
                msgToGUI->setNbCodewords(m_lastMsgNbCodewords);
                msgToGUI->setEarlyEOM(m_lastMsgEarlyEOM);
                msgToGUI->setHeaderParityStatus(m_lastMsgHeaderParityStatus);
                msgToGUI->setHeaderCRCStatus(m_lastMsgHeaderCRC);
                msgToGUI->setPayloadParityStatus(m_lastMsgPayloadParityStatus);
                msgToGUI->setPayloadCRCStatus(m_lastMsgPayloadCRC);
                getMessageQueueToGUI()->push(msgToGUI);
            }
        }
        else
        {
            m_decoder.decodeSymbols(msg.getSymbols(), m_lastMsgString);
            QDateTime dt = QDateTime::currentDateTime();
            m_lastMsgTimestamp = dt.toString(Qt::ISODateWithMs);

            if (getMessageQueueToGUI())
            {
                MsgReportDecodeString *msgToGUI = MsgReportDecodeString::create(m_lastMsgString);
                msgToGUI->setSyncWord(m_lastMsgSyncWord);
                msgToGUI->setSignalDb(m_lastMsgSignalDb);
                msgToGUI->setNoiseDb(m_lastMsgNoiseDb);
                getMessageQueueToGUI()->push(msgToGUI);
            }
        }

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        // Forward to the sink
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "LoRaDemod::handleMessage: DSPSignalNotification: m_basebandSampleRate: " << m_basebandSampleRate;
        m_basebandSink->getInputMessageQueue()->push(rep);

        if (getMessageQueueToGUI())
        {
            DSPSignalNotification* repToGUI = new DSPSignalNotification(notif); // make a copy
            getMessageQueueToGUI()->push(repToGUI);
        }

        return true;
    }
	else
	{
		return false;
	}
}

QByteArray LoRaDemod::serialize() const
{
    return m_settings.serialize();
}

bool LoRaDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureLoRaDemod *msg = MsgConfigureLoRaDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureLoRaDemod *msg = MsgConfigureLoRaDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void LoRaDemod::applySettings(const LoRaDemodSettings& settings, bool force)
{
    qDebug() << "LoRaDemod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_bandwidthIndex: " << settings.m_bandwidthIndex
            << " m_spreadFactor: " << settings.m_spreadFactor
            << " m_deBits: " << settings.m_deBits
            << " m_codingScheme: " << settings.m_codingScheme
            << " m_hasHeader: " << settings.m_hasHeader
            << " m_hasCRC: " << settings.m_hasCRC
            << " m_nbParityBits: " << settings.m_nbParityBits
            << " m_packetLength: " << settings.m_packetLength
            << " m_rgbColor: " << settings.m_rgbColor
            << " m_title: " << settings.m_title
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if ((settings.m_bandwidthIndex != m_settings.m_bandwidthIndex) || force) {
        reverseAPIKeys.append("bandwidthIndex");
    }
    if ((settings.m_spreadFactor != m_settings.m_spreadFactor) || force) {
        reverseAPIKeys.append("spreadFactor");
    }
    if ((settings.m_deBits != m_settings.m_deBits) || force) {
        reverseAPIKeys.append("deBits");
    }

    if ((settings.m_spreadFactor != m_settings.m_spreadFactor)
     || (settings.m_deBits != m_settings.m_deBits) || force) {
         m_decoder.setNbSymbolBits(settings.m_spreadFactor, settings.m_deBits);
    }

    if ((settings.m_codingScheme != m_settings.m_codingScheme) || force)
    {
        reverseAPIKeys.append("codingScheme");
        m_decoder.setCodingScheme(settings.m_codingScheme);
    }

    if ((settings.m_hasHeader != m_settings.m_hasHeader) || force)
    {
        reverseAPIKeys.append("hasHeader");
        m_decoder.setLoRaHasHeader(settings.m_hasHeader);
    }

    if ((settings.m_hasCRC != m_settings.m_hasCRC) || force)
    {
        reverseAPIKeys.append("hasCRC");
        m_decoder.setLoRaHasCRC(settings.m_hasCRC);
    }

    if ((settings.m_nbParityBits != m_settings.m_nbParityBits) || force)
    {
        reverseAPIKeys.append("nbParityBits");
        m_decoder.setLoRaParityBits(settings.m_nbParityBits);
    }

    if ((settings.m_packetLength != m_settings.m_packetLength) || force)
    {
        reverseAPIKeys.append("packetLength");
        m_decoder.setLoRaPacketLength(settings.m_packetLength);
    }

    if ((settings.m_decodeActive != m_settings.m_decodeActive) || force) {
        reverseAPIKeys.append("decodeActive");
    }
    if ((settings.m_eomSquelchTenths != m_settings.m_eomSquelchTenths) || force) {
        reverseAPIKeys.append("eomSquelchTenths");
    }
    if ((settings.m_nbSymbolsMax != m_settings.m_nbSymbolsMax) || force) {
        reverseAPIKeys.append("nbSymbolsMax");
    }
    if ((settings.m_preambleChirps != m_settings.m_preambleChirps) || force) {
        reverseAPIKeys.append("preambleChirps");
    }
    if ((settings.m_rgbColor != m_settings.m_rgbColor) || force) {
        reverseAPIKeys.append("rgbColor");
    }
    if ((settings.m_title != m_settings.m_title) || force) {
        reverseAPIKeys.append("title");
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

    LoRaDemodBaseband::MsgConfigureLoRaDemodBaseband *msg = LoRaDemodBaseband::MsgConfigureLoRaDemodBaseband::create(settings, force);
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

int LoRaDemod::webapiSettingsGet(
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setLoRaDemodSettings(new SWGSDRangel::SWGLoRaDemodSettings());
    response.getLoRaDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);

    return 200;
}

int LoRaDemod::webapiSettingsPutPatch(
    bool force,
    const QStringList& channelSettingsKeys,
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    LoRaDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureLoRaDemod *msg = MsgConfigureLoRaDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureLoRaDemod *msgToGUI = MsgConfigureLoRaDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void LoRaDemod::webapiUpdateChannelSettings(
        LoRaDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getLoRaDemodSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("bandwidthIndex")) {
        settings.m_bandwidthIndex = response.getLoRaDemodSettings()->getBandwidthIndex();
    }
    if (channelSettingsKeys.contains("spreadFactor")) {
        settings.m_spreadFactor = response.getLoRaDemodSettings()->getSpreadFactor();
    }
    if (channelSettingsKeys.contains("deBits")) {
        settings.m_deBits = response.getLoRaDemodSettings()->getDeBits();
    }
    if (channelSettingsKeys.contains("codingScheme")) {
        settings.m_codingScheme = (LoRaDemodSettings::CodingScheme) response.getLoRaDemodSettings()->getCodingScheme();
    }
    if (channelSettingsKeys.contains("decodeActive")) {
        settings.m_decodeActive = response.getLoRaDemodSettings()->getDecodeActive() != 0;
    }
    if (channelSettingsKeys.contains("eomSquelchTenths")) {
        settings.m_eomSquelchTenths = response.getLoRaDemodSettings()->getEomSquelchTenths();
    }
    if (channelSettingsKeys.contains("nbSymbolsMax")) {
        settings.m_nbSymbolsMax = response.getLoRaDemodSettings()->getNbSymbolsMax();
    }
    if (channelSettingsKeys.contains("preambleChirps")) {
        settings.m_preambleChirps = response.getLoRaDemodSettings()->getPreambleChirps();
    }
    if (channelSettingsKeys.contains("nbParityBits")) {
        settings.m_nbParityBits = response.getLoRaDemodSettings()->getNbParityBits();
    }
    if (channelSettingsKeys.contains("packetLength")) {
        settings.m_packetLength = response.getLoRaDemodSettings()->getPacketLength();
    }
    if (channelSettingsKeys.contains("hasCRC")) {
        settings.m_hasCRC = response.getLoRaDemodSettings()->getHasCrc() != 0;
    }
    if (channelSettingsKeys.contains("hasHeader")) {
        settings.m_hasHeader = response.getLoRaDemodSettings()->getHasHeader() != 0;
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getLoRaDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getLoRaDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getLoRaDemodSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getLoRaDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getLoRaDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getLoRaDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getLoRaDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getLoRaDemodSettings()->getReverseApiChannelIndex();
    }
}

int LoRaDemod::webapiReportGet(
    SWGSDRangel::SWGChannelReport& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setLoRaDemodReport(new SWGSDRangel::SWGLoRaDemodReport());
    response.getLoRaDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void LoRaDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const LoRaDemodSettings& settings)
{
    response.getLoRaDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getLoRaDemodSettings()->setBandwidthIndex(settings.m_bandwidthIndex);
    response.getLoRaDemodSettings()->setSpreadFactor(settings.m_spreadFactor);
    response.getLoRaDemodSettings()->setDeBits(settings.m_deBits);
    response.getLoRaDemodSettings()->setCodingScheme((int) settings.m_codingScheme);
    response.getLoRaDemodSettings()->setDecodeActive(settings.m_decodeActive ? 1 : 0);
    response.getLoRaDemodSettings()->setEomSquelchTenths(settings.m_eomSquelchTenths);
    response.getLoRaDemodSettings()->setNbSymbolsMax(settings.m_nbSymbolsMax);
    response.getLoRaDemodSettings()->setPreambleChirps(settings.m_preambleChirps);
    response.getLoRaDemodSettings()->setNbParityBits(settings.m_nbParityBits);
    response.getLoRaDemodSettings()->setHasCrc(settings.m_hasCRC ? 1 : 0);
    response.getLoRaDemodSettings()->setHasHeader(settings.m_hasHeader ? 1 : 0);

    response.getLoRaDemodSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getLoRaDemodSettings()->getTitle()) {
        *response.getLoRaDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getLoRaDemodSettings()->setTitle(new QString(settings.m_title));
    }

    response.getLoRaDemodSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getLoRaDemodSettings()->getReverseApiAddress()) {
        *response.getLoRaDemodSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getLoRaDemodSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getLoRaDemodSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getLoRaDemodSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getLoRaDemodSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
}

void LoRaDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getLoRaDemodReport()->setChannelPowerDb(CalcDb::dbPower(getTotalPower()));
    response.getLoRaDemodReport()->setSignalPowerDb(m_lastMsgSignalDb);
    response.getLoRaDemodReport()->setNoisePowerDb(CalcDb::dbPower(getCurrentNoiseLevel()));
    response.getLoRaDemodReport()->setSnrPowerDb(m_lastMsgSignalDb - m_lastMsgNoiseDb);
    response.getLoRaDemodReport()->setChannelSampleRate(m_basebandSink->getChannelSampleRate());
    response.getLoRaDemodReport()->setHasCrc(m_lastMsgHasCRC);
    response.getLoRaDemodReport()->setNbParityBits(m_lastMsgNbParityBits);
    response.getLoRaDemodReport()->setPacketLength(m_lastMsgPacketLength);
    response.getLoRaDemodReport()->setNbSymbols(m_lastMsgNbSymbols);
    response.getLoRaDemodReport()->setNbCodewords(m_lastMsgNbCodewords);
    response.getLoRaDemodReport()->setHeaderParityStatus(m_lastMsgHeaderParityStatus);
    response.getLoRaDemodReport()->setHeaderCrcStatus(m_lastMsgHeaderCRC);
    response.getLoRaDemodReport()->setPayloadParityStatus(m_lastMsgPayloadParityStatus);
    response.getLoRaDemodReport()->setPayloadCrcStatus(m_lastMsgPayloadCRC);
    response.getLoRaDemodReport()->setMessageTimestamp(new QString(m_lastMsgTimestamp));
    response.getLoRaDemodReport()->setMessageString(new QString(m_lastMsgString));

    response.getLoRaDemodReport()->setMessageBytes(new QList<QString *>);
    QList<QString *> *bytesStr = response.getLoRaDemodReport()->getMessageBytes();

    for (QByteArray::const_iterator it = m_lastMsgBytes.begin(); it != m_lastMsgBytes.end(); ++it)
    {
        unsigned char b = *it;
        bytesStr->push_back(new QString(tr("%1").arg(b, 2, 16, QChar('0'))));
    }
}

void LoRaDemod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const LoRaDemodSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("LoRaMod"));
    swgChannelSettings->setLoRaModSettings(new SWGSDRangel::SWGLoRaModSettings());
    SWGSDRangel::SWGLoRaDemodSettings *swgLoRaDemodSettings = swgChannelSettings->getLoRaDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgLoRaDemodSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("bandwidthIndex") || force) {
        swgLoRaDemodSettings->setBandwidthIndex(settings.m_bandwidthIndex);
    }
    if (channelSettingsKeys.contains("spreadFactor") || force) {
        swgLoRaDemodSettings->setSpreadFactor(settings.m_spreadFactor);
    }
    if (channelSettingsKeys.contains("deBits") || force) {
        swgLoRaDemodSettings->setDeBits(settings.m_deBits);
    }
    if (channelSettingsKeys.contains("codingScheme") || force) {
        swgLoRaDemodSettings->setCodingScheme((int) settings.m_codingScheme);
    }
    if (channelSettingsKeys.contains("decodeActive") || force) {
        swgLoRaDemodSettings->setDecodeActive(settings.m_decodeActive ? 1 : 0);
    }
    if (channelSettingsKeys.contains("decodeActive") || force) {
        swgLoRaDemodSettings->setEomSquelchTenths(settings.m_eomSquelchTenths ? 1 : 0);
    }
    if (channelSettingsKeys.contains("decodeActive") || force) {
        swgLoRaDemodSettings->setNbSymbolsMax(settings.m_nbSymbolsMax ? 1 : 0);
    }
    if (channelSettingsKeys.contains("preambleChirps") || force) {
        swgLoRaDemodSettings->setPreambleChirps(settings.m_preambleChirps);
    }
    if (channelSettingsKeys.contains("nbParityBits") || force) {
        swgLoRaDemodSettings->setNbParityBits(settings.m_nbParityBits);
    }
    if (channelSettingsKeys.contains("hasCRC") || force) {
        swgLoRaDemodSettings->setHasCrc(settings.m_hasCRC ? 1 : 0);
    }
    if (channelSettingsKeys.contains("hasHeader") || force) {
        swgLoRaDemodSettings->setHasHeader(settings.m_hasHeader ? 1 : 0);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgLoRaDemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgLoRaDemodSettings->setTitle(new QString(settings.m_title));
    }

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

void LoRaDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "LoRaDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("LoRaDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

bool LoRaDemod::getDemodActive() const
{
    return m_basebandSink->getDemodActive();
}

double LoRaDemod::getCurrentNoiseLevel() const
{
    return m_basebandSink->getCurrentNoiseLevel();
}

double LoRaDemod::getTotalPower() const
{
    return m_basebandSink->getTotalPower();
}
