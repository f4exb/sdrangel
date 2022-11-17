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
#include "maincore.h"

#include "ui_chirpchatmodgui.h"
#include "chirpchatmodgui.h"


ChirpChatModGUI* ChirpChatModGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    ChirpChatModGUI* gui = new ChirpChatModGUI(pluginAPI, deviceUISet, channelTx);
	return gui;
}

void ChirpChatModGUI::destroy()
{
    delete this;
}

void ChirpChatModGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray ChirpChatModGUI::serialize() const
{
    return m_settings.serialize();
}

bool ChirpChatModGUI::deserialize(const QByteArray& data)
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

bool ChirpChatModGUI::handleMessage(const Message& message)
{
    if (ChirpChatMod::MsgConfigureChirpChatMod::match(message))
    {
        const ChirpChatMod::MsgConfigureChirpChatMod& cfg = (ChirpChatMod::MsgConfigureChirpChatMod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (ChirpChatMod::MsgReportPayloadTime::match(message))
    {
        const ChirpChatMod::MsgReportPayloadTime& rpt = (ChirpChatMod::MsgReportPayloadTime&) message;
        float fourthsMs = ((1<<m_settings.m_spreadFactor) * 250.0) / ChirpChatModSettings::bandwidths[m_settings.m_bandwidthIndex];
        int fourthsChirps = 4*m_settings.m_preambleChirps;
        fourthsChirps += m_settings.hasSyncWord() ? 8 : 0;
        fourthsChirps += m_settings.getNbSFDFourths();
        float controlMs = fourthsChirps * fourthsMs; // preamble + sync word + SFD
        ui->timePayloadText->setText(tr("%1 ms").arg(QString::number(rpt.getPayloadTimeMs(), 'f', 0)));
        ui->timeTotalText->setText(tr("%1 ms").arg(QString::number(rpt.getPayloadTimeMs() + controlMs, 'f', 0)));
        ui->timeSymbolText->setText(tr("%1 ms").arg(QString::number(4.0*fourthsMs, 'f', 1)));
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        int basebandSampleRate = notif.getSampleRate();
        qDebug() << "ChirpChatModGUI::handleMessage: DSPSignalNotification: m_basebandSampleRate: " << basebandSampleRate;

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
    else
    {
        return false;
    }
}

void ChirpChatModGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
	applySettings();
}

void ChirpChatModGUI::handleSourceMessages()
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

void ChirpChatModGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void ChirpChatModGUI::on_bw_valueChanged(int value)
{
    if (value < 0) {
        m_settings.m_bandwidthIndex = 0;
    } else if (value < ChirpChatModSettings::nbBandwidths) {
        m_settings.m_bandwidthIndex = value;
    } else {
        m_settings.m_bandwidthIndex = ChirpChatModSettings::nbBandwidths - 1;
    }

	int thisBW = ChirpChatModSettings::bandwidths[value];
	ui->bwText->setText(QString("%1 Hz").arg(thisBW));
	m_channelMarker.setBandwidth(thisBW);

	applySettings();
}

void ChirpChatModGUI::on_channelMute_toggled(bool checked)
{
    m_settings.m_channelMute = checked;
	applySettings();
}

void ChirpChatModGUI::on_spread_valueChanged(int value)
{
    m_settings.m_spreadFactor = value;
    ui->spreadText->setText(tr("%1").arg(value));

    applySettings();
}

void ChirpChatModGUI::on_deBits_valueChanged(int value)
{
    m_settings.m_deBits = value;
    ui->deBitsText->setText(tr("%1").arg(m_settings.m_deBits));
    applySettings();
}

void ChirpChatModGUI::on_preambleChirps_valueChanged(int value)
{
    m_settings.m_preambleChirps = value;
    ui->preambleChirpsText->setText(tr("%1").arg(m_settings.m_preambleChirps));
    applySettings();
}

void ChirpChatModGUI::on_idleTime_valueChanged(int value)
{
    m_settings.m_quietMillis = value * 100;
    ui->idleTimeText->setText(tr("%1").arg(m_settings.m_quietMillis / 1000.0, 0, 'f', 1));
    applySettings();
}

void ChirpChatModGUI::on_syncWord_editingFinished()
{
    bool ok;
    unsigned int syncWord = ui->syncWord->text().toUInt(&ok, 16);

    if (ok)
    {
        m_settings.m_syncWord = syncWord > 255 ? 0 : syncWord;
        applySettings();
    }
}

void ChirpChatModGUI::on_scheme_currentIndexChanged(int index)
{
    m_settings.m_codingScheme = (ChirpChatModSettings::CodingScheme) index;
    ui->fecParity->setEnabled(m_settings.m_codingScheme == ChirpChatModSettings::CodingLoRa);
    ui->crc->setEnabled(m_settings.m_codingScheme == ChirpChatModSettings::CodingLoRa);
    ui->header->setEnabled(m_settings.m_codingScheme == ChirpChatModSettings::CodingLoRa);
    applySettings();
}

void ChirpChatModGUI::on_fecParity_valueChanged(int value)
{
    m_settings.m_nbParityBits = value;
    ui->fecParityText->setText(tr("%1").arg(m_settings.m_nbParityBits));
    applySettings();
}

void ChirpChatModGUI::on_crc_stateChanged(int state)
{
	m_settings.m_hasCRC = (state == Qt::Checked);
	applySettings();
}

void ChirpChatModGUI::on_header_stateChanged(int state)
{
	m_settings.m_hasHeader = (state == Qt::Checked);
	applySettings();
}

void ChirpChatModGUI::on_myCall_editingFinished()
{
    m_settings.m_myCall = ui->myCall->text();
    applySettings();
}

void ChirpChatModGUI::on_urCall_editingFinished()
{
    m_settings.m_urCall = ui->urCall->text();
    applySettings();
}

void ChirpChatModGUI::on_myLocator_editingFinished()
{
    m_settings.m_myLoc = ui->myLocator->text();
    applySettings();
}

void ChirpChatModGUI::on_report_editingFinished()
{
    m_settings.m_myRpt = ui->report->text();
    applySettings();
}

void ChirpChatModGUI::on_msgType_currentIndexChanged(int index)
{
    m_settings.m_messageType = (ChirpChatModSettings::MessageType) index;
    displayCurrentPayloadMessage();
    applySettings();
}

void ChirpChatModGUI::on_resetMessages_clicked(bool checked)
{
    (void) checked;
    m_settings.setDefaultTemplates();
    displayCurrentPayloadMessage();
    applySettings();
}

void ChirpChatModGUI::on_playMessage_clicked(bool checked)
{
    (void) checked;
    // Switch to message None then back to current message type to trigger sending process
    ChirpChatModSettings::MessageType msgType = m_settings.m_messageType;
    m_settings.m_messageType = ChirpChatModSettings::MessageNone;
    applySettings();
    m_settings.m_messageType = msgType;
    applySettings();
}

void ChirpChatModGUI::on_repeatMessage_valueChanged(int value)
{
    m_settings.m_messageRepeat = value;
    ui->repeatText->setText(tr("%1").arg(m_settings.m_messageRepeat));
    applySettings();
}

void ChirpChatModGUI::on_generateMessages_clicked(bool checked)
{
    (void) checked;
    m_settings.generateMessages();
    displayCurrentPayloadMessage();
    applySettings();
}

void ChirpChatModGUI::on_messageText_editingFinished()
{
    if (m_settings.m_messageType == ChirpChatModSettings::MessageBeacon) {
        m_settings.m_beaconMessage = ui->messageText->toPlainText();
    } else if (m_settings.m_messageType == ChirpChatModSettings::MessageCQ) {
        m_settings.m_cqMessage = ui->messageText->toPlainText();
    } else if (m_settings.m_messageType == ChirpChatModSettings::MessageReply) {
        m_settings.m_replyMessage = ui->messageText->toPlainText();
    } else if (m_settings.m_messageType == ChirpChatModSettings::MessageReport) {
        m_settings.m_reportMessage = ui->messageText->toPlainText();
    } else if (m_settings.m_messageType == ChirpChatModSettings::MessageReplyReport) {
        m_settings.m_replyReportMessage = ui->messageText->toPlainText();
    } else if (m_settings.m_messageType == ChirpChatModSettings::MessageRRR) {
        m_settings.m_rrrMessage = ui->messageText->toPlainText();
    } else if (m_settings.m_messageType == ChirpChatModSettings::Message73) {
        m_settings.m_73Message = ui->messageText->toPlainText();
    } else if (m_settings.m_messageType == ChirpChatModSettings::MessageQSOText) {
        m_settings.m_qsoTextMessage = ui->messageText->toPlainText();
    } else if (m_settings.m_messageType == ChirpChatModSettings::MessageText) {
        m_settings.m_textMessage = ui->messageText->toPlainText();
    }

    applySettings();
}

void ChirpChatModGUI::on_hexText_editingFinished()
{
    m_settings.m_bytesMessage = QByteArray::fromHex(ui->hexText->text().toLatin1());
    applySettings();
}

void ChirpChatModGUI::on_udpEnabled_clicked(bool checked)
{
    m_settings.m_udpEnabled = checked;
    applySettings();
}

void ChirpChatModGUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySettings();
}

void ChirpChatModGUI::on_udpPort_editingFinished()
{
    m_settings.m_udpPort = ui->udpPort->text().toInt();
    applySettings();
}

void ChirpChatModGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void ChirpChatModGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_chirpChatMod->getNumberOfDeviceStreams());
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

ChirpChatModGUI::ChirpChatModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
	ChannelGUI(parent),
	ui(new Ui::ChirpChatModGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(125000),
	m_doApplySettings(true),
    m_tickCount(0)
{
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channeltx/modchirpchat/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_chirpChatMod = (ChirpChatMod*) channelTx;
	m_chirpChatMod->setMessageQueueToGUI(getInputMessageQueue());

	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::red);
    m_channelMarker.setBandwidth(12500);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("ChirpChat Modulator");
    m_channelMarker.setSourceOrSinkStream(false);
    m_channelMarker.blockSignals(false);
	m_channelMarker.setVisible(true); // activate signal on the last setting only

	m_deviceUISet->addChannelMarker(&m_channelMarker);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));

	connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    setBandwidths();
    displaySettings();
    makeUIConnections();
    applySettings();
}

ChirpChatModGUI::~ChirpChatModGUI()
{
	delete ui;
}

void ChirpChatModGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void ChirpChatModGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
		ChirpChatMod::MsgConfigureChirpChatMod *msg = ChirpChatMod::MsgConfigureChirpChatMod::create(m_settings, force);
		m_chirpChatMod->getInputMessageQueue()->push(msg);
	}
}

void ChirpChatModGUI::displaySettings()
{
    int thisBW = ChirpChatModSettings::bandwidths[m_settings.m_bandwidthIndex];

    m_channelMarker.blockSignals(true);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(thisBW);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    setTitleColor(m_settings.m_rgbColor);

    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());
    updateIndexLabel();
    displayCurrentPayloadMessage();
    displayBinaryMessage();

    ui->fecParity->setEnabled(m_settings.m_codingScheme == ChirpChatModSettings::CodingLoRa);
    ui->crc->setEnabled(m_settings.m_codingScheme == ChirpChatModSettings::CodingLoRa);
    ui->header->setEnabled(m_settings.m_codingScheme == ChirpChatModSettings::CodingLoRa);

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
    ui->udpEnabled->setChecked(m_settings.m_udpEnabled);
    ui->udpAddress->setText(m_settings.m_udpAddress);
    ui->udpPort->setText(QString::number(m_settings.m_udpPort));
    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void ChirpChatModGUI::displayCurrentPayloadMessage()
{
    ui->messageText->blockSignals(true);

    if (m_settings.m_messageType == ChirpChatModSettings::MessageNone) {
        ui->messageText->clear();
    } else if (m_settings.m_messageType == ChirpChatModSettings::MessageBeacon) {
        ui->messageText->setText(m_settings.m_beaconMessage);
    } else if (m_settings.m_messageType == ChirpChatModSettings::MessageCQ) {
        ui->messageText->setText(m_settings.m_cqMessage);
    } else if (m_settings.m_messageType == ChirpChatModSettings::MessageReply) {
        ui->messageText->setText(m_settings.m_replyMessage);
    } else if (m_settings.m_messageType == ChirpChatModSettings::MessageReport) {
        ui->messageText->setText(m_settings.m_reportMessage);
    } else if (m_settings.m_messageType == ChirpChatModSettings::MessageReplyReport) {
        ui->messageText->setText(m_settings.m_replyReportMessage);
    } else if (m_settings.m_messageType == ChirpChatModSettings::MessageRRR) {
        ui->messageText->setText(m_settings.m_rrrMessage);
    } else if (m_settings.m_messageType == ChirpChatModSettings::Message73) {
        ui->messageText->setText(m_settings.m_73Message);
    } else if (m_settings.m_messageType == ChirpChatModSettings::MessageQSOText) {
        ui->messageText->setText(m_settings.m_qsoTextMessage);
    } else if (m_settings.m_messageType == ChirpChatModSettings::MessageText) {
        ui->messageText->setText(m_settings.m_textMessage);
    }

    ui->messageText->blockSignals(false);
}

void ChirpChatModGUI::displayBinaryMessage()
{
    ui->hexText->setText(m_settings.m_bytesMessage.toHex());
}

void ChirpChatModGUI::setBandwidths()
{
    int maxBandwidth = m_basebandSampleRate / ChirpChatModSettings::oversampling;
    int maxIndex = 0;

    for (; (maxIndex < ChirpChatModSettings::nbBandwidths) && (ChirpChatModSettings::bandwidths[maxIndex] <= maxBandwidth); maxIndex++)
    {}

    if (maxIndex != 0)
    {
        qDebug("ChirpChatModGUI::setBandwidths: avl: %d max: %d", maxBandwidth, ChirpChatModSettings::bandwidths[maxIndex-1]);
        ui->bw->setMaximum(maxIndex - 1);
        int index = ui->bw->value();
        ui->bwText->setText(QString("%1 Hz").arg(ChirpChatModSettings::bandwidths[index]));
    }
}

void ChirpChatModGUI::leaveEvent(QEvent* event)
{
	m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void ChirpChatModGUI::enterEvent(EnterEventType* event)
{
	m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void ChirpChatModGUI::tick()
{
    if (m_tickCount < 10)
    {
        m_tickCount++;
    }
    else
    {
        m_tickCount = 0;
        double powDb = CalcDb::dbPower(m_chirpChatMod->getMagSq());
        m_channelPowerDbAvg(powDb);
        ui->channelPower->setText(tr("%1 dB").arg(m_channelPowerDbAvg.asDouble(), 0, 'f', 1));

        if (m_chirpChatMod->getModulatorActive()) {
            ui->playMessage->setStyleSheet("QPushButton { background-color : green; }");
        } else {
            ui->playMessage->setStyleSheet("QPushButton { background:rgb(79,79,79); }");
        }
    }
}

void ChirpChatModGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &ChirpChatModGUI::on_deltaFrequency_changed);
    QObject::connect(ui->bw, &QSlider::valueChanged, this, &ChirpChatModGUI::on_bw_valueChanged);
    QObject::connect(ui->spread, &QSlider::valueChanged, this, &ChirpChatModGUI::on_spread_valueChanged);
    QObject::connect(ui->deBits, &QSlider::valueChanged, this, &ChirpChatModGUI::on_deBits_valueChanged);
    QObject::connect(ui->preambleChirps, &QSlider::valueChanged, this, &ChirpChatModGUI::on_preambleChirps_valueChanged);
    QObject::connect(ui->idleTime, &QSlider::valueChanged, this, &ChirpChatModGUI::on_idleTime_valueChanged);
    QObject::connect(ui->syncWord, &QLineEdit::editingFinished, this, &ChirpChatModGUI::on_syncWord_editingFinished);
    QObject::connect(ui->channelMute, &QToolButton::toggled, this, &ChirpChatModGUI::on_channelMute_toggled);
    QObject::connect(ui->scheme, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ChirpChatModGUI::on_scheme_currentIndexChanged);
    QObject::connect(ui->fecParity, &QDial::valueChanged, this, &ChirpChatModGUI::on_fecParity_valueChanged);
    QObject::connect(ui->crc, &QCheckBox::stateChanged, this, &ChirpChatModGUI::on_crc_stateChanged);
    QObject::connect(ui->header, &QCheckBox::stateChanged, this, &ChirpChatModGUI::on_header_stateChanged);
    QObject::connect(ui->myCall, &QLineEdit::editingFinished, this, &ChirpChatModGUI::on_myCall_editingFinished);
    QObject::connect(ui->urCall, &QLineEdit::editingFinished, this, &ChirpChatModGUI::on_urCall_editingFinished);
    QObject::connect(ui->myLocator, &QLineEdit::editingFinished, this, &ChirpChatModGUI::on_myLocator_editingFinished);
    QObject::connect(ui->report, &QLineEdit::editingFinished, this, &ChirpChatModGUI::on_report_editingFinished);
    QObject::connect(ui->msgType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ChirpChatModGUI::on_msgType_currentIndexChanged);
    QObject::connect(ui->resetMessages, &QPushButton::clicked, this, &ChirpChatModGUI::on_resetMessages_clicked);
    QObject::connect(ui->playMessage, &QPushButton::clicked, this, &ChirpChatModGUI::on_playMessage_clicked);
    QObject::connect(ui->repeatMessage, &QDial::valueChanged, this, &ChirpChatModGUI::on_repeatMessage_valueChanged);
    QObject::connect(ui->generateMessages, &QPushButton::clicked, this, &ChirpChatModGUI::on_generateMessages_clicked);
    QObject::connect(ui->messageText, &CustomTextEdit::editingFinished, this, &ChirpChatModGUI::on_messageText_editingFinished);
    QObject::connect(ui->hexText, &QLineEdit::editingFinished, this, &ChirpChatModGUI::on_hexText_editingFinished);
    QObject::connect(ui->udpEnabled, &QCheckBox::clicked, this, &ChirpChatModGUI::on_udpEnabled_clicked);
    QObject::connect(ui->udpAddress, &QLineEdit::editingFinished, this, &ChirpChatModGUI::on_udpAddress_editingFinished);
    QObject::connect(ui->udpPort, &QLineEdit::editingFinished, this, &ChirpChatModGUI::on_udpPort_editingFinished);
}

void ChirpChatModGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
