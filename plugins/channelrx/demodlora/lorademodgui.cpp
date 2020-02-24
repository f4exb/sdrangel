///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2020 Edouard Griffiths, F4EXB                              //
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

#include "device/deviceuiset.h"
#include <QScrollBar>
#include <QDebug>

#include "ui_lorademodgui.h"
#include "dsp/spectrumvis.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "gui/glspectrum.h"
#include "gui/glspectrumgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "mainwindow.h"

#include "lorademod.h"
#include "lorademodgui.h"

LoRaDemodGUI* LoRaDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	LoRaDemodGUI* gui = new LoRaDemodGUI(pluginAPI, deviceUISet, rxChannel);
	return gui;
}

void LoRaDemodGUI::destroy()
{
	delete this;
}

void LoRaDemodGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString LoRaDemodGUI::getName() const
{
	return objectName();
}

qint64 LoRaDemodGUI::getCenterFrequency() const {
	return m_channelMarker.getCenterFrequency();
}

void LoRaDemodGUI::setCenterFrequency(qint64 centerFrequency)
{
	m_channelMarker.setCenterFrequency(centerFrequency);
	applySettings();
}

void LoRaDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray LoRaDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool LoRaDemodGUI::deserialize(const QByteArray& data)
{
    resetLoRaStatus();

    if (m_settings.deserialize(data))
    {
        displaySettings();
        applySettings(true);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool LoRaDemodGUI::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        int basebandSampleRate = notif.getSampleRate();
        qDebug() << "LoRaDemodGUI::handleMessage: DSPSignalNotification: m_basebandSampleRate: " << basebandSampleRate;

        if (basebandSampleRate != m_basebandSampleRate)
        {
            m_basebandSampleRate = basebandSampleRate;
            setBandwidths();
        }

        return true;
    }
    else if (LoRaDemod::MsgReportDecodeBytes::match(message))
    {
        if (m_settings.m_codingScheme == LoRaDemodSettings::CodingLoRa) {
            showLoRaMessage(message);
        }

        return true;
    }
    else if (LoRaDemod::MsgReportDecodeString::match(message))
    {
        if ((m_settings.m_codingScheme == LoRaDemodSettings::CodingASCII)
         || (m_settings.m_codingScheme == LoRaDemodSettings::CodingTTY)) {
             showTextMessage(message);
        }

        return true;
    }
    else
    {
    	return false;
    }
}

void LoRaDemodGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void LoRaDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
	applySettings();
}

void LoRaDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void LoRaDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void LoRaDemodGUI::on_BW_valueChanged(int value)
{
    if (value < 0) {
        m_settings.m_bandwidthIndex = 0;
    } else if (value < LoRaDemodSettings::nbBandwidths) {
        m_settings.m_bandwidthIndex = value;
    } else {
        m_settings.m_bandwidthIndex = LoRaDemodSettings::nbBandwidths - 1;
    }

	int thisBW = LoRaDemodSettings::bandwidths[value];
	ui->BWText->setText(QString("%1 Hz").arg(thisBW));
	m_channelMarker.setBandwidth(thisBW);
	ui->glSpectrum->setSampleRate(thisBW);
	ui->glSpectrum->setCenterFrequency(thisBW/2);

	applySettings();
}

void LoRaDemodGUI::on_Spread_valueChanged(int value)
{
    m_settings.m_spreadFactor = value;
    ui->SpreadText->setText(tr("%1").arg(value));
    ui->spectrumGUI->setFFTSize(m_settings.m_spreadFactor);

    applySettings();
}

void LoRaDemodGUI::on_deBits_valueChanged(int value)
{
    m_settings.m_deBits = value;
    ui->deBitsText->setText(tr("%1").arg(m_settings.m_deBits));
    applySettings();
}

void LoRaDemodGUI::on_preambleChirps_valueChanged(int value)
{
    m_settings.m_preambleChirps = value;
    ui->preambleChirpsText->setText(tr("%1").arg(m_settings.m_preambleChirps));
    applySettings();
}

void LoRaDemodGUI::on_scheme_currentIndexChanged(int index)
{
    m_settings.m_codingScheme = (LoRaDemodSettings::CodingScheme) index;

    if (m_settings.m_codingScheme != LoRaDemodSettings::CodingLoRa) {
        resetLoRaStatus();
    }

    applySettings();
}

void LoRaDemodGUI::on_mute_toggled(bool checked)
{
    m_settings.m_decodeActive = !checked;
    applySettings();
}

void LoRaDemodGUI::on_clear_clicked(bool checked)
{
    (void) checked;
    ui->messageText->clear();
    ui->hexText->clear();
}

void LoRaDemodGUI::on_eomSquelch_valueChanged(int value)
{
    m_settings.m_eomSquelchTenths = value;
    displaySquelch();
    applySettings();
}

void LoRaDemodGUI::on_messageLength_valueChanged(int value)
{
    m_settings.m_nbSymbolsMax = value;
    ui->messageLengthText->setText(tr("%1").arg(m_settings.m_nbSymbolsMax));
    applySettings();
}

void LoRaDemodGUI::on_header_stateChanged(int state)
{
    m_settings.m_hasHeader = (state == Qt::Checked);

    if (!m_settings.m_hasHeader) // put back values from settings
    {
        ui->fecParity->blockSignals(true);
        ui->crc->blockSignals(true);
        ui->fecParity->setValue(m_settings.m_nbParityBits);
        ui->fecParityText->setText(tr("%1").arg(m_settings.m_nbParityBits));
        ui->crc->setChecked(m_settings.m_hasCRC);
        ui->fecParity->blockSignals(false);
        ui->crc->blockSignals(false);
    }

    ui->fecParity->setEnabled(state != Qt::Checked);
    ui->crc->setEnabled(state != Qt::Checked);

    applySettings();
}

void LoRaDemodGUI::on_fecParity_valueChanged(int value)
{
    m_settings.m_nbParityBits = value;
    ui->fecParityText->setText(tr("%1").arg(m_settings.m_nbParityBits));
    applySettings();
}

void LoRaDemodGUI::on_crc_stateChanged(int state)
{
    m_settings.m_hasCRC = (state == Qt::Checked);
    applySettings();
}

void LoRaDemodGUI::on_packetLength_valueChanged(int value)
{
    m_settings.m_packetLength = value;
    ui->packetLengthText->setText(tr("%1").arg(m_settings.m_packetLength));
    applySettings();
}

void LoRaDemodGUI::on_udpSend_stateChanged(int state)
{
    m_settings.m_sendViaUDP = (state == Qt::Checked);
    applySettings();
}

void LoRaDemodGUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySettings();
}

void LoRaDemodGUI::on_udpPort_editingFinished()
{
    bool ok;
    quint16 udpPort = ui->udpPort->text().toInt(&ok);

    if((!ok) || (udpPort < 1024)) {
        udpPort = 9998;
    }

    ui->udpPort->setText(tr("%1").arg(m_settings.m_udpPort));
    m_settings.m_udpPort = udpPort;
    applySettings();
}

void LoRaDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;
}

LoRaDemodGUI::LoRaDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::LoRaDemodGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
    m_basebandSampleRate(250000),
	m_doApplySettings(true),
    m_tickCount(0)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

	m_spectrumVis = new SpectrumVis(SDR_RX_SCALEF, ui->glSpectrum);
	m_LoRaDemod = (LoRaDemod*) rxChannel; //new LoRaDemod(m_deviceUISet->m_deviceSourceAPI);
	m_LoRaDemod->setSpectrumSink(m_spectrumVis);
    m_LoRaDemod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainWindow::getInstance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    ui->messageText->setReadOnly(true);
    ui->syncWord->setReadOnly(true);
    ui->hexText->setReadOnly(true);

	m_channelMarker.setMovable(true);
	m_channelMarker.setVisible(true);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));

	m_deviceUISet->registerRxChannelInstance(LoRaDemod::m_channelIdURI, this);
	m_deviceUISet->addChannelMarker(&m_channelMarker);
	m_deviceUISet->addRollupWidget(this);

	ui->spectrumGUI->setBuddies(m_spectrumVis->getInputMessageQueue(), m_spectrumVis, ui->glSpectrum);

	m_settings.setChannelMarker(&m_channelMarker);
	m_settings.setSpectrumGUI(ui->spectrumGUI);

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    setBandwidths();
	displaySettings();
    resetLoRaStatus();
	applySettings(true);
}

LoRaDemodGUI::~LoRaDemodGUI()
{
    m_deviceUISet->removeRxChannelInstance(this);
	delete m_LoRaDemod; // TODO: check this: when the GUI closes it has to delete the demodulator
	delete m_spectrumVis;
	delete ui;
}

void LoRaDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void LoRaDemodGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
        setTitleColor(m_channelMarker.getColor());
        LoRaDemod::MsgConfigureLoRaDemod* message = LoRaDemod::MsgConfigureLoRaDemod::create( m_settings, force);
        m_LoRaDemod->getInputMessageQueue()->push(message);
	}
}

void LoRaDemodGUI::displaySettings()
{
    int thisBW = LoRaDemodSettings::bandwidths[m_settings.m_bandwidthIndex];

    m_channelMarker.blockSignals(true);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(thisBW);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    setTitleColor(m_settings.m_rgbColor);

	ui->glSpectrum->setSampleRate(thisBW);
	ui->glSpectrum->setCenterFrequency(thisBW/2);

    ui->fecParity->setEnabled(!m_settings.m_hasHeader);
    ui->crc->setEnabled(!m_settings.m_hasHeader);
    ui->packetLength->setEnabled(!m_settings.m_hasHeader);

    blockApplySettings(true);
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    ui->BWText->setText(QString("%1 Hz").arg(thisBW));
    ui->BW->setValue(m_settings.m_bandwidthIndex);
    ui->Spread->setValue(m_settings.m_spreadFactor);
    ui->SpreadText->setText(tr("%1").arg(m_settings.m_spreadFactor));
    ui->deBits->setValue(m_settings.m_deBits);
    ui->deBitsText->setText(tr("%1").arg(m_settings.m_deBits));
    ui->preambleChirps->setValue(m_settings.m_preambleChirps);
    ui->preambleChirpsText->setText(tr("%1").arg(m_settings.m_preambleChirps));
    ui->scheme->setCurrentIndex((int) m_settings.m_codingScheme);
    ui->messageLengthText->setText(tr("%1").arg(m_settings.m_nbSymbolsMax));
    ui->messageLength->setValue(m_settings.m_nbSymbolsMax);
    ui->udpSend->setChecked(m_settings.m_sendViaUDP);
    ui->udpAddress->setText(m_settings.m_udpAddress);
    ui->udpPort->setText(tr("%1").arg(m_settings.m_udpPort));
    ui->header->setChecked(m_settings.m_hasHeader);

    if (!m_settings.m_hasHeader)
    {
        ui->fecParity->setValue(m_settings.m_nbParityBits);
        ui->fecParityText->setText(tr("%1").arg(m_settings.m_nbParityBits));
        ui->crc->setChecked(m_settings.m_hasCRC);
        ui->packetLength->setValue(m_settings.m_packetLength);
        ui->spectrumGUI->setFFTSize(m_settings.m_spreadFactor);
    }

    displaySquelch();

    blockApplySettings(false);
}

void LoRaDemodGUI::displaySquelch()
{
    ui->eomSquelch->setValue(m_settings.m_eomSquelchTenths);

    if (m_settings.m_eomSquelchTenths == ui->eomSquelch->maximum()) {
        ui->eomSquelchText->setText("---");
    } else {
        ui->eomSquelchText->setText(tr("%1").arg(m_settings.m_eomSquelchTenths / 10.0, 0, 'f', 1));
    }
}

void LoRaDemodGUI::displayLoRaStatus(int headerParityStatus, bool headerCRCStatus, int payloadParityStatus, bool payloadCRCStatus)
{
    if (m_settings.m_hasHeader && (headerParityStatus == (int) ParityOK)) {
        ui->headerHammingStatus->setStyleSheet("QLabel { background-color : green; }");
    } else if (m_settings.m_hasHeader && (headerParityStatus == (int) ParityError)) {
        ui->headerHammingStatus->setStyleSheet("QLabel { background-color : red; }");
    } else if (m_settings.m_hasHeader && (headerParityStatus == (int) ParityCorrected)) {
        ui->headerHammingStatus->setStyleSheet("QLabel { background-color : blue; }");
    } else {
        ui->headerHammingStatus->setStyleSheet("QLabel { background:rgb(79,79,79); }");
    }

    if (m_settings.m_hasHeader && headerCRCStatus) {
        ui->headerCRCStatus->setStyleSheet("QLabel { background-color : green; }");
    } else if (m_settings.m_hasHeader && !headerCRCStatus) {
        ui->headerCRCStatus->setStyleSheet("QLabel { background-color : red; }");
    } else {
        ui->headerCRCStatus->setStyleSheet("QLabel { background:rgb(79,79,79); }");
    }

    if (payloadParityStatus == (int) ParityOK) {
        ui->payloadFECStatus->setStyleSheet("QLabel { background-color : green; }");
    } else if (payloadParityStatus == (int) ParityError) {
        ui->payloadFECStatus->setStyleSheet("QLabel { background-color : red; }");
    } else if (payloadParityStatus == (int) ParityCorrected) {
        ui->payloadFECStatus->setStyleSheet("QLabel { background-color : blue; }");
    } else {
        ui->payloadFECStatus->setStyleSheet("QLabel { background:rgb(79,79,79); }");
    }

    if (payloadCRCStatus) {
        ui->payloadCRCStatus->setStyleSheet("QLabel { background-color : green; }");
    } else {
        ui->payloadCRCStatus->setStyleSheet("QLabel { background-color : red; }");
    }
}

void LoRaDemodGUI::resetLoRaStatus()
{
    ui->headerHammingStatus->setStyleSheet("QLabel { background:rgb(79,79,79); }");
    ui->headerCRCStatus->setStyleSheet("QLabel { background:rgb(79,79,79); }");
    ui->payloadFECStatus->setStyleSheet("QLabel { background:rgb(79,79,79); }");
    ui->payloadCRCStatus->setStyleSheet("QLabel { background:rgb(79,79,79); }");
    ui->nbSymbolsText->setText("---");
    ui->nbCodewordsText->setText("---");
}

void LoRaDemodGUI::setBandwidths()
{
    int maxBandwidth = m_basebandSampleRate/LoRaDemodSettings::oversampling;
    int maxIndex = 0;

    for (; (maxIndex < LoRaDemodSettings::nbBandwidths) && (LoRaDemodSettings::bandwidths[maxIndex] <= maxBandwidth); maxIndex++)
    {}

    if (maxIndex != 0)
    {
        qDebug("LoRaDemodGUI::setBandwidths: avl: %d max: %d", maxBandwidth, LoRaDemodSettings::bandwidths[maxIndex-1]);
        ui->BW->setMaximum(maxIndex - 1);
        int index = ui->BW->value();
        ui->BWText->setText(QString("%1 Hz").arg(LoRaDemodSettings::bandwidths[index]));
    }
}

void LoRaDemodGUI::showLoRaMessage(const Message& message)
{
    const LoRaDemod::MsgReportDecodeBytes& msg = (LoRaDemod::MsgReportDecodeBytes&) message;
    QByteArray bytes = msg.getBytes();
    QString syncWordStr((tr("%1").arg(msg.getSyncWord(), 2, 16, QChar('0'))));

    ui->syncWord->setText(tr("%1").arg(syncWordStr));
    ui->sText->setText(tr("%1").arg(msg.getSingalDb(), 0, 'f', 1));
    ui->snrText->setText(tr("%1").arg(msg.getSingalDb() - msg.getNoiseDb(), 0, 'f', 1));
    unsigned int packetLength;

    if (m_settings.m_hasHeader)
    {
        ui->fecParity->setValue(msg.getNbParityBits());
        ui->fecParityText->setText(tr("%1").arg(msg.getNbParityBits()));
        ui->crc->setChecked(msg.getHasCRC());
        ui->packetLength->setValue(msg.getPacketSize());
        ui->packetLengthText->setText(tr("%1").arg(msg.getPacketSize()));
        packetLength =  msg.getPacketSize();
    }
    else
    {
        packetLength = m_settings.m_packetLength;
    }

    QDateTime dt = QDateTime::currentDateTime();
    QString dateStr = dt.toString("HH:mm:ss");

    if (msg.getEarlyEOM())
    {
        QString loRaStatus = tr("%1 %2 S:%3 SN:%4 HF:%5 HC:%6 EOM:too early")
            .arg(dateStr)
            .arg(syncWordStr)
            .arg(msg.getSingalDb(), 0, 'f', 1)
            .arg(msg.getSingalDb() - msg.getNoiseDb(), 0, 'f', 1)
            .arg(getParityStr(msg.getHeaderParityStatus()))
            .arg(msg.getHeaderCRCStatus() ? "ok" : "err");

        displayStatus(loRaStatus);
        displayLoRaStatus(msg.getHeaderParityStatus(), msg.getHeaderCRCStatus(), (int) ParityUndefined, true);
        ui->payloadCRCStatus->setStyleSheet("QLabel { background:rgb(79,79,79); }"); // reset payload CRC
    }
    else
    {
        QString loRaHeader = tr("%1 %2 S:%3 SN:%4 HF:%5 HC:%6 FEC:%7 CRC:%8")
            .arg(dateStr)
            .arg(syncWordStr)
            .arg(msg.getSingalDb(), 0, 'f', 1)
            .arg(msg.getSingalDb() - msg.getNoiseDb(), 0, 'f', 1)
            .arg(getParityStr(msg.getHeaderParityStatus()))
            .arg(msg.getHeaderCRCStatus() ? "ok" : "err")
            .arg(getParityStr(msg.getPayloadParityStatus()))
            .arg(msg.getPayloadCRCStatus() ? "ok" : "err");

        displayBytes(loRaHeader, bytes);

        QByteArray bytesCopy(bytes);
        bytesCopy.truncate(packetLength);
        bytesCopy.replace('\0', " ");
        QString str = QString(bytesCopy.toStdString().c_str());
        QString textHeader(tr("%1 (%2)").arg(dateStr).arg(syncWordStr));

        displayText(textHeader, str);
        displayLoRaStatus(msg.getHeaderParityStatus(), msg.getHeaderCRCStatus(), msg.getPayloadParityStatus(), msg.getPayloadCRCStatus());
    }

    ui->nbSymbolsText->setText(tr("%1").arg(msg.getNbSymbols()));
    ui->nbCodewordsText->setText(tr("%1").arg(msg.getNbCodewords()));
}

void LoRaDemodGUI::showTextMessage(const Message& message)
{
    const LoRaDemod::MsgReportDecodeString& msg = (LoRaDemod::MsgReportDecodeString&) message;

    QDateTime dt = QDateTime::currentDateTime();
    QString dateStr = dt.toString("HH:mm:ss");
    QString syncWordStr((tr("%1").arg(msg.getSyncWord(), 2, 16, QChar('0'))));
    QString textHeader(tr("%1 (%2)").arg(dateStr).arg(syncWordStr));
    displayText(textHeader, msg.getString());
    ui->syncWord->setText(syncWordStr);
    ui->sText->setText(tr("%1").arg(msg.getSingalDb(), 0, 'f', 1));
    ui->snrText->setText(tr("%1").arg(msg.getSingalDb() - msg.getNoiseDb(), 0, 'f', 1));

    QString status = tr("%1 S:%2 SN:%3")
        .arg(dateStr)
        .arg(msg.getSingalDb(), 0, 'f', 1)
        .arg(msg.getSingalDb() - msg.getNoiseDb(), 0, 'f', 1);
    displayStatus(status);
}

void LoRaDemodGUI::displayText(const QString& header, const QString& text)
{
    QTextCursor cursor = ui->messageText->textCursor();
    cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    if (!ui->messageText->document()->isEmpty()) {
        cursor.insertText("\n");
    }
    cursor.insertText(tr("%1 %2").arg(header).arg(text));
    ui->messageText->verticalScrollBar()->setValue(ui->messageText->verticalScrollBar()->maximum());
}

void LoRaDemodGUI::displayBytes(const QString& header, const QByteArray& bytes)
{
    QTextCursor cursor = ui->hexText->textCursor();
    cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);

    if (!ui->hexText->document()->isEmpty()) {
        cursor.insertText("\n");
    }

    cursor.insertText(tr(">%1\n").arg(header));
    QByteArray::const_iterator it = bytes.begin();
    unsigned int i = 0;

    for (;it != bytes.end(); ++it, i++)
    {
        unsigned char b = *it;

        if (i%16 == 0) {
            cursor.insertText(tr("%1|").arg(i, 3, 10, QChar('0')));
        }

        cursor.insertText(tr("%1").arg(b, 2, 16, QChar('0')));

        if (i%16 == 15) {
            cursor.insertText("\n");
        } else if (i%4 == 3) {
            cursor.insertText("|");
        } else {
            cursor.insertText(" ");
        }
    }

    ui->hexText->verticalScrollBar()->setValue(ui->hexText->verticalScrollBar()->maximum());
}

void LoRaDemodGUI::displayStatus(const QString& status)
{
    QTextCursor cursor = ui->hexText->textCursor();
    cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);

    if (!ui->hexText->document()->isEmpty()) {
        cursor.insertText("\n");
    }

    cursor.insertText(tr(">%1").arg(status));
    ui->hexText->verticalScrollBar()->setValue(ui->hexText->verticalScrollBar()->maximum());
}

QString LoRaDemodGUI::getParityStr(int parityStatus)
{
    if (parityStatus == (int) ParityError) {
        return "err";
    } else if (parityStatus == (int) ParityCorrected) {
        return "fix";
    } else if (parityStatus == (int) ParityOK) {
        return "ok";
    } else {
        return "n/a";
    }
}

void LoRaDemodGUI::tick()
{
    if (m_tickCount < 10)
    {
        m_tickCount++;
    }
    else
    {
        m_tickCount = 0;

        ui->nText->setText(tr("%1").arg(CalcDb::dbPower(m_LoRaDemod->getCurrentNoiseLevel()), 0, 'f', 1));
        ui->channelPower->setText(tr("%1 dB").arg(CalcDb::dbPower(m_LoRaDemod->getTotalPower()), 0, 'f', 1));

        if (m_LoRaDemod->getDemodActive()) {
            ui->mute->setStyleSheet("QToolButton { background-color : green; }");
        } else {
            ui->mute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        }
    }
}

