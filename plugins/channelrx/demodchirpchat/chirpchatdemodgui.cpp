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
#include <QDateTime>

#include "ui_chirpchatdemodgui.h"
#include "dsp/spectrumvis.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "gui/glspectrum.h"
#include "gui/glspectrumgui.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "maincore.h"

#include "chirpchatdemod.h"
#include "chirpchatdemodgui.h"

ChirpChatDemodGUI* ChirpChatDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	ChirpChatDemodGUI* gui = new ChirpChatDemodGUI(pluginAPI, deviceUISet, rxChannel);
	return gui;
}

void ChirpChatDemodGUI::destroy()
{
	delete this;
}

void ChirpChatDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray ChirpChatDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool ChirpChatDemodGUI::deserialize(const QByteArray& data)
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

bool ChirpChatDemodGUI::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        int basebandSampleRate = notif.getSampleRate();
        qDebug() << "ChirpChatDemodGUI::handleMessage: DSPSignalNotification: m_basebandSampleRate: " << basebandSampleRate;

        if (basebandSampleRate != m_basebandSampleRate)
        {
            m_basebandSampleRate = basebandSampleRate;
            setBandwidths();
        }

        ui->deltaFrequency->setValueRange(false, 7, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();

        return true;
    }
    else if (ChirpChatDemod::MsgReportDecodeBytes::match(message))
    {
        if (m_settings.m_codingScheme == ChirpChatDemodSettings::CodingLoRa) {
            showLoRaMessage(message);
        }

        return true;
    }
    else if (ChirpChatDemod::MsgReportDecodeString::match(message))
    {
        if ((m_settings.m_codingScheme == ChirpChatDemodSettings::CodingASCII)
         || (m_settings.m_codingScheme == ChirpChatDemodSettings::CodingTTY)) {
             showTextMessage(message);
        }

        return true;
    }
    else if (ChirpChatDemod::MsgConfigureChirpChatDemod::match(message))
    {
        qDebug("ChirpChatDemodGUI::handleMessage: NFMDemod::MsgConfigureChirpChatDemod");
        const ChirpChatDemod::MsgConfigureChirpChatDemod& cfg = (ChirpChatDemod::MsgConfigureChirpChatDemod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        ui->spectrumGUI->updateSettings();
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);

        return true;
    }
    else
    {
    	return false;
    }
}

void ChirpChatDemodGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void ChirpChatDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
	applySettings();
}

void ChirpChatDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void ChirpChatDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void ChirpChatDemodGUI::on_BW_valueChanged(int value)
{
    if (value < 0) {
        m_settings.m_bandwidthIndex = 0;
    } else if (value < ChirpChatDemodSettings::nbBandwidths) {
        m_settings.m_bandwidthIndex = value;
    } else {
        m_settings.m_bandwidthIndex = ChirpChatDemodSettings::nbBandwidths - 1;
    }

	int thisBW = ChirpChatDemodSettings::bandwidths[value];
	ui->BWText->setText(QString("%1 Hz").arg(thisBW));
	m_channelMarker.setBandwidth(thisBW);
	ui->glSpectrum->setSampleRate(thisBW);
	ui->glSpectrum->setCenterFrequency(thisBW/2);

	applySettings();
}

void ChirpChatDemodGUI::on_Spread_valueChanged(int value)
{
    m_settings.m_spreadFactor = value;
    ui->SpreadText->setText(tr("%1").arg(value));
    ui->spectrumGUI->setFFTSize(m_settings.m_spreadFactor);

    applySettings();
}

void ChirpChatDemodGUI::on_deBits_valueChanged(int value)
{
    m_settings.m_deBits = value;
    ui->deBitsText->setText(tr("%1").arg(m_settings.m_deBits));
    applySettings();
}

void ChirpChatDemodGUI::on_fftWindow_currentIndexChanged(int index)
{
    m_settings.m_fftWindow = (FFTWindow::Function) index;
    applySettings();
}

void ChirpChatDemodGUI::on_preambleChirps_valueChanged(int value)
{
    m_settings.m_preambleChirps = value;
    ui->preambleChirpsText->setText(tr("%1").arg(m_settings.m_preambleChirps));
    applySettings();
}

void ChirpChatDemodGUI::on_scheme_currentIndexChanged(int index)
{
    m_settings.m_codingScheme = (ChirpChatDemodSettings::CodingScheme) index;

    if (m_settings.m_codingScheme != ChirpChatDemodSettings::CodingLoRa) {
        resetLoRaStatus();
    }

    applySettings();
}

void ChirpChatDemodGUI::on_mute_toggled(bool checked)
{
    m_settings.m_decodeActive = !checked;
    applySettings();
}

void ChirpChatDemodGUI::on_clear_clicked(bool checked)
{
    (void) checked;
    ui->messageText->clear();
    ui->messageText->clear();
}

void ChirpChatDemodGUI::on_eomSquelch_valueChanged(int value)
{
    m_settings.m_eomSquelchTenths = value;
    displaySquelch();
    applySettings();
}

void ChirpChatDemodGUI::on_messageLength_valueChanged(int value)
{
    m_settings.m_nbSymbolsMax = value;
    ui->messageLengthText->setText(tr("%1").arg(m_settings.m_nbSymbolsMax));
    applySettings();
}

void ChirpChatDemodGUI::on_messageLengthAuto_stateChanged(int state)
{
    m_settings.m_autoNbSymbolsMax = (state == Qt::Checked);
    applySettings();
}

void ChirpChatDemodGUI::on_header_stateChanged(int state)
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

void ChirpChatDemodGUI::on_fecParity_valueChanged(int value)
{
    m_settings.m_nbParityBits = value;
    ui->fecParityText->setText(tr("%1").arg(m_settings.m_nbParityBits));
    applySettings();
}

void ChirpChatDemodGUI::on_crc_stateChanged(int state)
{
    m_settings.m_hasCRC = (state == Qt::Checked);
    applySettings();
}

void ChirpChatDemodGUI::on_packetLength_valueChanged(int value)
{
    m_settings.m_packetLength = value;
    ui->packetLengthText->setText(tr("%1").arg(m_settings.m_packetLength));
    applySettings();
}

void ChirpChatDemodGUI::on_udpSend_stateChanged(int state)
{
    m_settings.m_sendViaUDP = (state == Qt::Checked);
    applySettings();
}

void ChirpChatDemodGUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySettings();
}

void ChirpChatDemodGUI::on_udpPort_editingFinished()
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

void ChirpChatDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void ChirpChatDemodGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
    {
        BasicChannelSettingsDialog dialog(&m_channelMarker, this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);
        dialog.setDefaultTitle(m_displayedName);

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            dialog.setNumberOfStreams(m_chirpChatDemod->getNumberOfDeviceStreams());
            dialog.setStreamIndex(m_settings.m_streamIndex);
        }

        dialog.move(p);
        dialog.exec();

        m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
        m_settings.m_title = m_channelMarker.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();

        setWindowTitle(m_settings.m_title);
        setTitle(m_channelMarker.getTitle());
        setTitleColor(m_settings.m_rgbColor);

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
            m_channelMarker.clearStreamIndexes();
            m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
            updateIndexLabel();
        }

        applySettings();
    }

    resetContextMenuType();
}

ChirpChatDemodGUI::ChirpChatDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
	ChannelGUI(parent),
	ui(new Ui::ChirpChatDemodGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(250000),
	m_doApplySettings(true),
    m_tickCount(0)
{
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodchirpchat/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_chirpChatDemod = (ChirpChatDemod*) rxChannel;
    m_spectrumVis = m_chirpChatDemod->getSpectrumVis();
	m_spectrumVis->setGLSpectrum(ui->glSpectrum);
    m_chirpChatDemod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    ui->messageText->setReadOnly(true);
    ui->messageText->setReadOnly(true);

	m_channelMarker.setMovable(true);
	m_channelMarker.setVisible(true);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));

	m_deviceUISet->addChannelMarker(&m_channelMarker);

	ui->spectrumGUI->setBuddies(m_spectrumVis, ui->glSpectrum);

	m_settings.setChannelMarker(&m_channelMarker);
	m_settings.setSpectrumGUI(ui->spectrumGUI);
    m_settings.setRollupState(&m_rollupState);

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    setBandwidths();
	displaySettings();
    makeUIConnections();
    resetLoRaStatus();
	applySettings(true);
}

ChirpChatDemodGUI::~ChirpChatDemodGUI()
{
	delete ui;
}

void ChirpChatDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void ChirpChatDemodGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
        setTitleColor(m_channelMarker.getColor());
        ChirpChatDemod::MsgConfigureChirpChatDemod* message = ChirpChatDemod::MsgConfigureChirpChatDemod::create( m_settings, force);
        m_chirpChatDemod->getInputMessageQueue()->push(message);
	}
}

void ChirpChatDemodGUI::displaySettings()
{
    int thisBW = ChirpChatDemodSettings::bandwidths[m_settings.m_bandwidthIndex];

    m_channelMarker.blockSignals(true);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(thisBW);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    setTitleColor(m_settings.m_rgbColor);
    setTitle(m_channelMarker.getTitle());

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
    ui->fftWindow->setCurrentIndex((int) m_settings.m_fftWindow);
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

    ui->messageLengthAuto->setChecked(m_settings.m_autoNbSymbolsMax);

    displaySquelch();
    updateIndexLabel();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void ChirpChatDemodGUI::displaySquelch()
{
    ui->eomSquelch->setValue(m_settings.m_eomSquelchTenths);

    if (m_settings.m_eomSquelchTenths == ui->eomSquelch->maximum()) {
        ui->eomSquelchText->setText("---");
    } else {
        ui->eomSquelchText->setText(tr("%1").arg(m_settings.m_eomSquelchTenths / 10.0, 0, 'f', 1));
    }
}

void ChirpChatDemodGUI::displayLoRaStatus(int headerParityStatus, bool headerCRCStatus, int payloadParityStatus, bool payloadCRCStatus)
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

void ChirpChatDemodGUI::resetLoRaStatus()
{
    ui->headerHammingStatus->setStyleSheet("QLabel { background:rgb(79,79,79); }");
    ui->headerCRCStatus->setStyleSheet("QLabel { background:rgb(79,79,79); }");
    ui->payloadFECStatus->setStyleSheet("QLabel { background:rgb(79,79,79); }");
    ui->payloadCRCStatus->setStyleSheet("QLabel { background:rgb(79,79,79); }");
    ui->nbSymbolsText->setText("---");
    ui->nbCodewordsText->setText("---");
}

void ChirpChatDemodGUI::setBandwidths()
{
    int maxBandwidth = m_basebandSampleRate/ChirpChatDemodSettings::oversampling;
    int maxIndex = 0;

    for (; (maxIndex < ChirpChatDemodSettings::nbBandwidths) && (ChirpChatDemodSettings::bandwidths[maxIndex] <= maxBandwidth); maxIndex++)
    {}

    if (maxIndex != 0)
    {
        qDebug("ChirpChatDemodGUI::setBandwidths: avl: %d max: %d", maxBandwidth, ChirpChatDemodSettings::bandwidths[maxIndex-1]);
        ui->BW->setMaximum(maxIndex - 1);
        int index = ui->BW->value();
        ui->BWText->setText(QString("%1 Hz").arg(ChirpChatDemodSettings::bandwidths[index]));
    }
}

void ChirpChatDemodGUI::showLoRaMessage(const Message& message)
{
    const ChirpChatDemod::MsgReportDecodeBytes& msg = (ChirpChatDemod::MsgReportDecodeBytes&) message;
    QByteArray bytes = msg.getBytes();
    QString syncWordStr((tr("%1").arg(msg.getSyncWord(), 2, 16, QChar('0'))));

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

        displayStatus(loRaHeader);
        displayBytes(bytes);

        QByteArray bytesCopy(bytes);
        bytesCopy.truncate(packetLength);
        bytesCopy.replace('\0', " ");
        QString str = QString(bytesCopy.toStdString().c_str());
        QString textHeader(tr("%1 (%2)").arg(dateStr).arg(syncWordStr));

        displayText(str);
        displayLoRaStatus(msg.getHeaderParityStatus(), msg.getHeaderCRCStatus(), msg.getPayloadParityStatus(), msg.getPayloadCRCStatus());
    }

    ui->nbSymbolsText->setText(tr("%1").arg(msg.getNbSymbols()));
    ui->nbCodewordsText->setText(tr("%1").arg(msg.getNbCodewords()));
}

void ChirpChatDemodGUI::showTextMessage(const Message& message)
{
    const ChirpChatDemod::MsgReportDecodeString& msg = (ChirpChatDemod::MsgReportDecodeString&) message;

    QDateTime dt = QDateTime::currentDateTime();
    QString dateStr = dt.toString("HH:mm:ss");
    ui->sText->setText(tr("%1").arg(msg.getSingalDb(), 0, 'f', 1));
    ui->snrText->setText(tr("%1").arg(msg.getSingalDb() - msg.getNoiseDb(), 0, 'f', 1));

    QString status = tr("%1 S:%2 SN:%3")
        .arg(dateStr)
        .arg(msg.getSingalDb(), 0, 'f', 1)
        .arg(msg.getSingalDb() - msg.getNoiseDb(), 0, 'f', 1);

    displayStatus(status);
    displayText(msg.getString());
}

void ChirpChatDemodGUI::displayText(const QString& text)
{
    QTextCursor cursor = ui->messageText->textCursor();
    cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    if (!ui->messageText->document()->isEmpty()) {
        cursor.insertText("\n");
    }
    cursor.insertText(tr("TXT|%1").arg(text));
    ui->messageText->verticalScrollBar()->setValue(ui->messageText->verticalScrollBar()->maximum());
}

void ChirpChatDemodGUI::displayBytes(const QByteArray& bytes)
{
    QTextCursor cursor = ui->messageText->textCursor();
    cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);

    if (!ui->messageText->document()->isEmpty()) {
        cursor.insertText("\n");
    }

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

    ui->messageText->verticalScrollBar()->setValue(ui->messageText->verticalScrollBar()->maximum());
}

void ChirpChatDemodGUI::displayStatus(const QString& status)
{
    QTextCursor cursor = ui->messageText->textCursor();
    cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);

    if (!ui->messageText->document()->isEmpty()) {
        cursor.insertText("\n");
    }

    cursor.insertText(tr(">%1").arg(status));
    ui->messageText->verticalScrollBar()->setValue(ui->messageText->verticalScrollBar()->maximum());
}

QString ChirpChatDemodGUI::getParityStr(int parityStatus)
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

void ChirpChatDemodGUI::tick()
{
    if (m_tickCount < 10)
    {
        m_tickCount++;
    }
    else
    {
        m_tickCount = 0;

        ui->nText->setText(tr("%1").arg(CalcDb::dbPower(m_chirpChatDemod->getCurrentNoiseLevel()), 0, 'f', 1));
        ui->channelPower->setText(tr("%1 dB").arg(CalcDb::dbPower(m_chirpChatDemod->getTotalPower()), 0, 'f', 1));

        if (m_chirpChatDemod->getDemodActive()) {
            ui->mute->setStyleSheet("QToolButton { background-color : green; }");
        } else {
            ui->mute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        }
    }
}

void ChirpChatDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &ChirpChatDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->BW, &QSlider::valueChanged, this, &ChirpChatDemodGUI::on_BW_valueChanged);
    QObject::connect(ui->Spread, &QSlider::valueChanged, this, &ChirpChatDemodGUI::on_Spread_valueChanged);
    QObject::connect(ui->deBits, &QSlider::valueChanged, this, &ChirpChatDemodGUI::on_deBits_valueChanged);
    QObject::connect(ui->fftWindow, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ChirpChatDemodGUI::on_fftWindow_currentIndexChanged);
    QObject::connect(ui->preambleChirps, &QSlider::valueChanged, this, &ChirpChatDemodGUI::on_preambleChirps_valueChanged);
    QObject::connect(ui->scheme, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ChirpChatDemodGUI::on_scheme_currentIndexChanged);
    QObject::connect(ui->mute, &QToolButton::toggled, this, &ChirpChatDemodGUI::on_mute_toggled);
    QObject::connect(ui->clear, &QPushButton::clicked, this, &ChirpChatDemodGUI::on_clear_clicked);
    QObject::connect(ui->eomSquelch, &QDial::valueChanged, this, &ChirpChatDemodGUI::on_eomSquelch_valueChanged);
    QObject::connect(ui->messageLength, &QDial::valueChanged, this, &ChirpChatDemodGUI::on_messageLength_valueChanged);
    QObject::connect(ui->messageLengthAuto, &QCheckBox::stateChanged, this, &ChirpChatDemodGUI::on_messageLengthAuto_stateChanged);
    QObject::connect(ui->header, &QCheckBox::stateChanged, this, &ChirpChatDemodGUI::on_header_stateChanged);
    QObject::connect(ui->fecParity, &QDial::valueChanged, this, &ChirpChatDemodGUI::on_fecParity_valueChanged);
    QObject::connect(ui->crc, &QCheckBox::stateChanged, this, &ChirpChatDemodGUI::on_crc_stateChanged);
    QObject::connect(ui->packetLength, &QDial::valueChanged, this, &ChirpChatDemodGUI::on_packetLength_valueChanged);
    QObject::connect(ui->udpSend, &QCheckBox::stateChanged, this, &ChirpChatDemodGUI::on_udpSend_stateChanged);
    QObject::connect(ui->udpAddress, &QLineEdit::editingFinished, this, &ChirpChatDemodGUI::on_udpAddress_editingFinished);
    QObject::connect(ui->udpPort, &QLineEdit::editingFinished, this, &ChirpChatDemodGUI::on_udpPort_editingFinished);
}

void ChirpChatDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
