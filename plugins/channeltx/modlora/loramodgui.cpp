///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include <QDockWidget>
#include <QMainWindow>
#include <QFileDialog>
#include <QTime>
#include <QDebug>

#include "device/deviceuiset.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "gui/crightclickenabler.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/devicestreamselectiondialog.h"
#include "mainwindow.h"

#include "ui_loramodgui.h"
#include "loramodgui.h"


LoRaModGUI* LoRaModGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    LoRaModGUI* gui = new LoRaModGUI(pluginAPI, deviceUISet, channelTx);
	return gui;
}

void LoRaModGUI::destroy()
{
    delete this;
}

void LoRaModGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString LoRaModGUI::getName() const
{
	return objectName();
}

qint64 LoRaModGUI::getCenterFrequency() const {
	return m_channelMarker.getCenterFrequency();
}

void LoRaModGUI::setCenterFrequency(qint64 centerFrequency)
{
	m_channelMarker.setCenterFrequency(centerFrequency);
	applySettings();
}

void LoRaModGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray LoRaModGUI::serialize() const
{
    return m_settings.serialize();
}

bool LoRaModGUI::deserialize(const QByteArray& data)
{
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

bool LoRaModGUI::handleMessage(const Message& message)
{
    if (LoRaMod::MsgConfigureLoRaMod::match(message))
    {
        const LoRaMod::MsgConfigureLoRaMod& cfg = (LoRaMod::MsgConfigureLoRaMod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (LoRaMod::MsgReportPayloadTime::match(message))
    {
        const LoRaMod::MsgReportPayloadTime& rpt = (LoRaMod::MsgReportPayloadTime&) message;
        float fourthsMs = ((1<<m_settings.m_spreadFactor) * 250.0) / LoRaModSettings::bandwidths[m_settings.m_bandwidthIndex];
        float controlMs = (4*m_settings.m_preambleChirps + 8 + 9) * fourthsMs; // preamble + sync word + SFD
        ui->timePayloadText->setText(tr("%1 ms").arg(QString::number(rpt.getPayloadTimeMs(), 'f', 0)));
        ui->timeTotalText->setText(tr("%1 ms").arg(QString::number(rpt.getPayloadTimeMs() + controlMs, 'f', 0)));
        ui->timeSymbolText->setText(tr("%1 ms").arg(QString::number(4.0*fourthsMs, 'f', 1)));
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        int basebandSampleRate = notif.getSampleRate();
        qDebug() << "LoRaModGUI::handleMessage: DSPSignalNotification: m_basebandSampleRate: " << basebandSampleRate;

        if (basebandSampleRate != m_basebandSampleRate)
        {
            m_basebandSampleRate = basebandSampleRate;
            setBandwidths();
        }

        return true;
    }
    else
    {
        return false;
    }
}

void LoRaModGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
	applySettings();
}

void LoRaModGUI::handleSourceMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

void LoRaModGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void LoRaModGUI::on_bw_valueChanged(int value)
{
    if (value < 0) {
        m_settings.m_bandwidthIndex = 0;
    } else if (value < LoRaModSettings::nbBandwidths) {
        m_settings.m_bandwidthIndex = value;
    } else {
        m_settings.m_bandwidthIndex = LoRaModSettings::nbBandwidths - 1;
    }

	int thisBW = LoRaModSettings::bandwidths[value];
	ui->bwText->setText(QString("%1 Hz").arg(thisBW));
	m_channelMarker.setBandwidth(thisBW);

	applySettings();
}

void LoRaModGUI::on_channelMute_toggled(bool checked)
{
    m_settings.m_channelMute = checked;
	applySettings();
}

void LoRaModGUI::on_spread_valueChanged(int value)
{
    m_settings.m_spreadFactor = value;
    ui->spreadText->setText(tr("%1").arg(value));

    applySettings();
}

void LoRaModGUI::on_deBits_valueChanged(int value)
{
    m_settings.m_deBits = value;
    ui->deBitsText->setText(tr("%1").arg(m_settings.m_deBits));
    applySettings();
}

void LoRaModGUI::on_preambleChirps_valueChanged(int value)
{
    m_settings.m_preambleChirps = value;
    ui->preambleChirpsText->setText(tr("%1").arg(m_settings.m_preambleChirps));
    applySettings();
}

void LoRaModGUI::on_idleTime_valueChanged(int value)
{
    m_settings.m_quietMillis = value * 100;
    ui->idleTimeText->setText(tr("%1").arg(m_settings.m_quietMillis / 1000.0, 0, 'f', 1));
    applySettings();
}

void LoRaModGUI::on_syncWord_editingFinished()
{
    bool ok;
    unsigned int syncWord = ui->syncWord->text().toUInt(&ok, 16);

    if (ok)
    {
        m_settings.m_syncWord = syncWord > 255 ? 0 : syncWord;
        applySettings();
    }
}

void LoRaModGUI::on_scheme_currentIndexChanged(int index)
{
    m_settings.m_codingScheme = (LoRaModSettings::CodingScheme) index;
    ui->fecParity->setEnabled(m_settings.m_codingScheme == LoRaModSettings::CodingLoRa);
    ui->crc->setEnabled(m_settings.m_codingScheme == LoRaModSettings::CodingLoRa);
    ui->header->setEnabled(m_settings.m_codingScheme == LoRaModSettings::CodingLoRa);
    applySettings();
}

void LoRaModGUI::on_fecParity_valueChanged(int value)
{
    m_settings.m_nbParityBits = value;
    ui->fecParityText->setText(tr("%1").arg(m_settings.m_nbParityBits));
    applySettings();
}

void LoRaModGUI::on_crc_stateChanged(int state)
{
	m_settings.m_hasCRC = (state == Qt::Checked);
	applySettings();
}

void LoRaModGUI::on_header_stateChanged(int state)
{
	m_settings.m_hasHeader = (state == Qt::Checked);
	applySettings();
}

void LoRaModGUI::on_myCall_editingFinished()
{
    m_settings.m_myCall = ui->myCall->text();
    applySettings();
}

void LoRaModGUI::on_urCall_editingFinished()
{
    m_settings.m_urCall = ui->urCall->text();
    applySettings();
}

void LoRaModGUI::on_myLocator_editingFinished()
{
    m_settings.m_myLoc = ui->myLocator->text();
    applySettings();
}

void LoRaModGUI::on_report_editingFinished()
{
    m_settings.m_myRpt = ui->report->text();
    applySettings();
}

void LoRaModGUI::on_msgType_currentIndexChanged(int index)
{
    m_settings.m_messageType = (LoRaModSettings::MessageType) index;
    displayCurrentPayloadMessage();
    applySettings();
}

void LoRaModGUI::on_resetMessages_clicked(bool checked)
{
    (void) checked;
    m_settings.setDefaultTemplates();
    displayCurrentPayloadMessage();
    applySettings();
}

void LoRaModGUI::on_playMessage_clicked(bool checked)
{
    (void) checked;
    // Switch to message None then back to current message type to trigger sending process
    LoRaModSettings::MessageType msgType = m_settings.m_messageType;
    m_settings.m_messageType = LoRaModSettings::MessageNone;
    applySettings();
    m_settings.m_messageType = msgType;
    applySettings();
}

void LoRaModGUI::on_repeatMessage_valueChanged(int value)
{
    m_settings.m_messageRepeat = value;
    ui->repeatText->setText(tr("%1").arg(m_settings.m_messageRepeat));
    applySettings();
}

void LoRaModGUI::on_generateMessages_clicked(bool checked)
{
    (void) checked;
    m_settings.generateMessages();
    displayCurrentPayloadMessage();
    applySettings();
}

void LoRaModGUI::on_messageText_editingFinished()
{
    if (m_settings.m_messageType == LoRaModSettings::MessageBeacon) {
        m_settings.m_beaconMessage = ui->messageText->toPlainText();
    } else if (m_settings.m_messageType == LoRaModSettings::MessageCQ) {
        m_settings.m_cqMessage = ui->messageText->toPlainText();
    } else if (m_settings.m_messageType == LoRaModSettings::MessageReply) {
        m_settings.m_replyMessage = ui->messageText->toPlainText();
    } else if (m_settings.m_messageType == LoRaModSettings::MessageReport) {
        m_settings.m_reportMessage = ui->messageText->toPlainText();
    } else if (m_settings.m_messageType == LoRaModSettings::MessageReplyReport) {
        m_settings.m_replyReportMessage = ui->messageText->toPlainText();
    } else if (m_settings.m_messageType == LoRaModSettings::MessageRRR) {
        m_settings.m_rrrMessage = ui->messageText->toPlainText();
    } else if (m_settings.m_messageType == LoRaModSettings::Message73) {
        m_settings.m_73Message = ui->messageText->toPlainText();
    } else if (m_settings.m_messageType == LoRaModSettings::MessageQSOText) {
        m_settings.m_qsoTextMessage = ui->messageText->toPlainText();
    } else if (m_settings.m_messageType == LoRaModSettings::MessageText) {
        m_settings.m_textMessage = ui->messageText->toPlainText();
    }

    applySettings();
}

void LoRaModGUI::on_hexText_editingFinished()
{

}

void LoRaModGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;
}

void LoRaModGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuChannelSettings)
    {
        BasicChannelSettingsDialog dialog(&m_channelMarker, this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);
        dialog.move(p);
        dialog.exec();

        m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
        m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
        m_settings.m_title = m_channelMarker.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();

        setWindowTitle(m_settings.m_title);
        setTitleColor(m_settings.m_rgbColor);

        applySettings();
    }
    else if ((m_contextMenuType == ContextMenuStreamSettings) && (m_deviceUISet->m_deviceMIMOEngine))
    {
        DeviceStreamSelectionDialog dialog(this);
        dialog.setNumberOfStreams(m_loRaMod->getNumberOfDeviceStreams());
        dialog.setStreamIndex(m_settings.m_streamIndex);
        dialog.move(p);
        dialog.exec();

        m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
        m_channelMarker.clearStreamIndexes();
        m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
        displayStreamIndex();
        applySettings();
    }

    resetContextMenuType();
}

LoRaModGUI::LoRaModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::LoRaModGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
    m_basebandSampleRate(125000),
	m_doApplySettings(true),
    m_tickCount(0)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);

	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_loRaMod = (LoRaMod*) channelTx;
	m_loRaMod->setMessageQueueToGUI(getInputMessageQueue());

	connect(&MainWindow::getInstance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::red);
    m_channelMarker.setBandwidth(12500);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("LoRa Modulator");
    m_channelMarker.setSourceOrSinkStream(false);
    m_channelMarker.blockSignals(false);
	m_channelMarker.setVisible(true); // activate signal on the last setting only

	m_deviceUISet->registerTxChannelInstance(LoRaMod::m_channelIdURI, this);
	m_deviceUISet->addChannelMarker(&m_channelMarker);
	m_deviceUISet->addRollupWidget(this);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));

	connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));

    m_settings.setChannelMarker(&m_channelMarker);

    setBandwidths();
    displaySettings();
    applySettings();
}

LoRaModGUI::~LoRaModGUI()
{
    m_deviceUISet->removeTxChannelInstance(this);
	delete m_loRaMod; // TODO: check this: when the GUI closes it has to delete the modulator
	delete ui;
}

void LoRaModGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void LoRaModGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
		LoRaMod::MsgConfigureLoRaMod *msg = LoRaMod::MsgConfigureLoRaMod::create(m_settings, force);
		m_loRaMod->getInputMessageQueue()->push(msg);
	}
}

void LoRaModGUI::displaySettings()
{
    int thisBW = LoRaModSettings::bandwidths[m_settings.m_bandwidthIndex];

    m_channelMarker.blockSignals(true);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(thisBW);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    setTitleColor(m_settings.m_rgbColor);

    setWindowTitle(m_channelMarker.getTitle());
    displayStreamIndex();
    displayCurrentPayloadMessage();

    ui->fecParity->setEnabled(m_settings.m_codingScheme == LoRaModSettings::CodingLoRa);
    ui->crc->setEnabled(m_settings.m_codingScheme == LoRaModSettings::CodingLoRa);
    ui->header->setEnabled(m_settings.m_codingScheme == LoRaModSettings::CodingLoRa);

    blockApplySettings(true);
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    ui->bwText->setText(QString("%1 Hz").arg(thisBW));
    ui->bw->setValue(m_settings.m_bandwidthIndex);
    ui->spread->setValue(m_settings.m_spreadFactor);
    ui->spreadText->setText(tr("%1").arg(m_settings.m_spreadFactor));
    ui->deBits->setValue(m_settings.m_deBits);
    ui->deBitsText->setText(tr("%1").arg(m_settings.m_deBits));
    ui->preambleChirps->setValue(m_settings.m_preambleChirps);
    ui->preambleChirpsText->setText(tr("%1").arg(m_settings.m_preambleChirps));
    ui->idleTime->setValue(m_settings.m_quietMillis / 100);
    ui->idleTimeText->setText(tr("%1").arg(m_settings.m_quietMillis / 1000.0, 0, 'f', 1));
    ui->syncWord->setText((tr("%1").arg(m_settings.m_syncWord, 2, 16)));
    ui->channelMute->setChecked(m_settings.m_channelMute);
    ui->scheme->setCurrentIndex((int) m_settings.m_codingScheme);
    ui->fecParity->setValue(m_settings.m_nbParityBits);
    ui->fecParityText->setText(tr("%1").arg(m_settings.m_nbParityBits));
    ui->crc->setChecked(m_settings.m_hasCRC);
    ui->header->setChecked(m_settings.m_hasHeader);
    ui->myCall->setText(m_settings.m_myCall);
    ui->urCall->setText(m_settings.m_urCall);
    ui->myLocator->setText(m_settings.m_myLoc);
    ui->report->setText(m_settings.m_myRpt);
    ui->repeatMessage->setValue(m_settings.m_messageRepeat);
    ui->repeatText->setText(tr("%1").arg(m_settings.m_messageRepeat));
    ui->msgType->setCurrentIndex((int) m_settings.m_messageType);
    blockApplySettings(false);
}

void LoRaModGUI::displayStreamIndex()
{
    if (m_deviceUISet->m_deviceMIMOEngine) {
        setStreamIndicator(tr("%1").arg(m_settings.m_streamIndex));
    } else {
        setStreamIndicator("S"); // single channel indicator
    }
}

void LoRaModGUI::displayCurrentPayloadMessage()
{
    ui->messageText->blockSignals(true);

    if (m_settings.m_messageType == LoRaModSettings::MessageNone) {
        ui->messageText->clear();
    } else if (m_settings.m_messageType == LoRaModSettings::MessageBeacon) {
        ui->messageText->setText(m_settings.m_beaconMessage);
    } else if (m_settings.m_messageType == LoRaModSettings::MessageCQ) {
        ui->messageText->setText(m_settings.m_cqMessage);
    } else if (m_settings.m_messageType == LoRaModSettings::MessageReply) {
        ui->messageText->setText(m_settings.m_replyMessage);
    } else if (m_settings.m_messageType == LoRaModSettings::MessageReport) {
        ui->messageText->setText(m_settings.m_reportMessage);
    } else if (m_settings.m_messageType == LoRaModSettings::MessageReplyReport) {
        ui->messageText->setText(m_settings.m_replyReportMessage);
    } else if (m_settings.m_messageType == LoRaModSettings::MessageRRR) {
        ui->messageText->setText(m_settings.m_rrrMessage);
    } else if (m_settings.m_messageType == LoRaModSettings::Message73) {
        ui->messageText->setText(m_settings.m_73Message);
    } else if (m_settings.m_messageType == LoRaModSettings::MessageQSOText) {
        ui->messageText->setText(m_settings.m_qsoTextMessage);
    } else if (m_settings.m_messageType == LoRaModSettings::MessageText) {
        ui->messageText->setText(m_settings.m_textMessage);
    }

    ui->messageText->blockSignals(false);
}

void LoRaModGUI::setBandwidths()
{
    int maxBandwidth = m_basebandSampleRate / LoRaModSettings::oversampling;
    int maxIndex = 0;

    for (; (maxIndex < LoRaModSettings::nbBandwidths) && (LoRaModSettings::bandwidths[maxIndex] <= maxBandwidth); maxIndex++)
    {}

    if (maxIndex != 0)
    {
        qDebug("LoRaModGUI::setBandwidths: avl: %d max: %d", maxBandwidth, LoRaModSettings::bandwidths[maxIndex-1]);
        ui->bw->setMaximum(maxIndex - 1);
        int index = ui->bw->value();
        ui->bwText->setText(QString("%1 Hz").arg(LoRaModSettings::bandwidths[index]));
    }
}

void LoRaModGUI::leaveEvent(QEvent*)
{
	m_channelMarker.setHighlighted(false);
}

void LoRaModGUI::enterEvent(QEvent*)
{
	m_channelMarker.setHighlighted(true);
}

void LoRaModGUI::tick()
{
    if (m_tickCount < 10)
    {
        m_tickCount++;
    }
    else
    {
        m_tickCount = 0;
        double powDb = CalcDb::dbPower(m_loRaMod->getMagSq());
        m_channelPowerDbAvg(powDb);
        ui->channelPower->setText(tr("%1 dB").arg(m_channelPowerDbAvg.asDouble(), 0, 'f', 1));

        if (m_loRaMod->getModulatorActive()) {
            ui->playMessage->setStyleSheet("QPushButton { background-color : green; }");
        } else {
            ui->playMessage->setStyleSheet("QPushButton { background:rgb(79,79,79); }");
        }
    }
}
