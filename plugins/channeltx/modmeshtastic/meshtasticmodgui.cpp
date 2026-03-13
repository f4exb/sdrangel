///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2021-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
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
#include <cmath>

#include "device/deviceuiset.h"
#include "plugin/pluginapi.h"
#include "util/db.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "maincore.h"

#include "ui_meshtasticmodgui.h"
#include "meshtasticmodgui.h"
#include "meshtasticpacket.h"


MeshtasticModGUI* MeshtasticModGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    MeshtasticModGUI* gui = new MeshtasticModGUI(pluginAPI, deviceUISet, channelTx);
	return gui;
}

void MeshtasticModGUI::destroy()
{
    delete this;
}

void MeshtasticModGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray MeshtasticModGUI::serialize() const
{
    return m_settings.serialize();
}

bool MeshtasticModGUI::deserialize(const QByteArray& data)
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

bool MeshtasticModGUI::handleMessage(const Message& message)
{
    if (MeshtasticMod::MsgConfigureMeshtasticMod::match(message))
    {
        const MeshtasticMod::MsgConfigureMeshtasticMod& cfg = (MeshtasticMod::MsgConfigureMeshtasticMod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (MeshtasticMod::MsgReportPayloadTime::match(message))
    {
        const MeshtasticMod::MsgReportPayloadTime& rpt = (MeshtasticMod::MsgReportPayloadTime&) message;
        float fourthsMs = ((1<<m_settings.m_spreadFactor) * 250.0) / MeshtasticModSettings::bandwidths[m_settings.m_bandwidthIndex];
        int fourthsChirps = 4*m_settings.m_preambleChirps;
        fourthsChirps += m_settings.hasSyncWord() ? 8 : 0;
        fourthsChirps += m_settings.getNbSFDFourths();
        float controlMs = fourthsChirps * fourthsMs; // preamble + sync word + SFD
        ui->timeMessageLengthText->setText(tr("%1").arg(rpt.getNbSymbols()));
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
        qDebug() << "MeshtasticModGUI::handleMessage: DSPSignalNotification: m_basebandSampleRate: " << basebandSampleRate;

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

void MeshtasticModGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
	applySettings();
}

void MeshtasticModGUI::handleSourceMessages()
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

QString MeshtasticModGUI::getActivePayloadText() const
{
    switch (m_settings.m_messageType)
    {
    case MeshtasticModSettings::MessageBeacon:
        return m_settings.m_beaconMessage;
    case MeshtasticModSettings::MessageCQ:
        return m_settings.m_cqMessage;
    case MeshtasticModSettings::MessageReply:
        return m_settings.m_replyMessage;
    case MeshtasticModSettings::MessageReport:
        return m_settings.m_reportMessage;
    case MeshtasticModSettings::MessageReplyReport:
        return m_settings.m_replyReportMessage;
    case MeshtasticModSettings::MessageRRR:
        return m_settings.m_rrrMessage;
    case MeshtasticModSettings::Message73:
        return m_settings.m_73Message;
    case MeshtasticModSettings::MessageQSOText:
        return m_settings.m_qsoTextMessage;
    case MeshtasticModSettings::MessageText:
        return m_settings.m_textMessage;
    case MeshtasticModSettings::MessageBytes:
        return QString::fromUtf8(m_settings.m_bytesMessage);
    default:
        return QString();
    }
}

int MeshtasticModGUI::findBandwidthIndex(int bandwidthHz) const
{
    int bestIndex = -1;
    int bestDelta = 1 << 30;

    for (int i = 0; i < MeshtasticModSettings::nbBandwidths; ++i)
    {
        const int delta = std::abs(MeshtasticModSettings::bandwidths[i] - bandwidthHz);
        if (delta < bestDelta)
        {
            bestDelta = delta;
            bestIndex = i;
        }
    }

    return bestIndex;
}

void MeshtasticModGUI::applyMeshtasticRadioSettingsIfPresent(const QString& payloadText)
{
    if (m_settings.m_codingScheme != MeshtasticModSettings::CodingLoRa) {
        return;
    }

    if (!Meshtastic::Packet::isCommand(payloadText)) {
        return;
    }

    Meshtastic::TxRadioSettings meshRadio;
    QString error;
    if (!Meshtastic::Packet::deriveTxRadioSettings(payloadText, meshRadio, error))
    {
        qWarning() << "MeshtasticModGUI::applyMeshtasticRadioSettingsIfPresent:" << error;
        return;
    }

    bool changed = false;
    const int bwIndex = findBandwidthIndex(meshRadio.bandwidthHz);
    if (bwIndex >= 0 && bwIndex != m_settings.m_bandwidthIndex) {
        m_settings.m_bandwidthIndex = bwIndex;
        changed = true;
    }

    if (meshRadio.spreadFactor > 0 && meshRadio.spreadFactor != m_settings.m_spreadFactor) {
        m_settings.m_spreadFactor = meshRadio.spreadFactor;
        changed = true;
    }

    if (meshRadio.parityBits > 0 && meshRadio.parityBits != m_settings.m_nbParityBits) {
        m_settings.m_nbParityBits = meshRadio.parityBits;
        changed = true;
    }

    if (meshRadio.deBits != m_settings.m_deBits) {
        m_settings.m_deBits = meshRadio.deBits;
        changed = true;
    }

    if (meshRadio.syncWord != m_settings.m_syncWord) {
        m_settings.m_syncWord = meshRadio.syncWord;
        changed = true;
    }

    if (meshRadio.hasCenterFrequency)
    {
        if (m_deviceCenterFrequency != 0)
        {
            const qint64 wantedOffset = meshRadio.centerFrequencyHz - m_deviceCenterFrequency;
            if (wantedOffset != m_settings.m_inputFrequencyOffset)
            {
                m_settings.m_inputFrequencyOffset = static_cast<int>(wantedOffset);
                changed = true;
            }
        }
        else
        {
            qWarning() << "MeshtasticModGUI::applyMeshtasticRadioSettingsIfPresent: device center frequency unknown, cannot auto-center";
        }
    }

    if (!changed) {
        return;
    }

    qInfo() << "MeshtasticModGUI::applyMeshtasticRadioSettingsIfPresent:" << meshRadio.summary;

    const int thisBW = MeshtasticModSettings::bandwidths[m_settings.m_bandwidthIndex];
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(thisBW);
    m_channelMarker.blockSignals(false);

    blockApplySettings(true);
    ui->deltaFrequency->setValue(m_settings.m_inputFrequencyOffset);
    ui->bw->setValue(m_settings.m_bandwidthIndex);
    ui->bwText->setText(QString("%1 Hz").arg(thisBW));
    ui->spread->setValue(m_settings.m_spreadFactor);
    ui->spreadText->setText(tr("%1").arg(m_settings.m_spreadFactor));
    ui->deBits->setValue(m_settings.m_deBits);
    ui->deBitsText->setText(tr("%1").arg(m_settings.m_deBits));
    ui->fecParity->setValue(m_settings.m_nbParityBits);
    ui->fecParityText->setText(tr("%1").arg(m_settings.m_nbParityBits));
    ui->syncWord->setText(tr("%1").arg(m_settings.m_syncWord, 2, 16));
    blockApplySettings(false);

    updateAbsoluteCenterFrequency();
}

void MeshtasticModGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void MeshtasticModGUI::on_bw_valueChanged(int value)
{
    if (value < 0) {
        m_settings.m_bandwidthIndex = 0;
    } else if (value < MeshtasticModSettings::nbBandwidths) {
        m_settings.m_bandwidthIndex = value;
    } else {
        m_settings.m_bandwidthIndex = MeshtasticModSettings::nbBandwidths - 1;
    }

	int thisBW = MeshtasticModSettings::bandwidths[value];
	ui->bwText->setText(QString("%1 Hz").arg(thisBW));
	m_channelMarker.setBandwidth(thisBW);

	applySettings();
}

void MeshtasticModGUI::on_channelMute_toggled(bool checked)
{
    m_settings.m_channelMute = checked;
	applySettings();
}

void MeshtasticModGUI::on_spread_valueChanged(int value)
{
    m_settings.m_spreadFactor = value;
    ui->spreadText->setText(tr("%1").arg(value));

    applySettings();
}

void MeshtasticModGUI::on_deBits_valueChanged(int value)
{
    m_settings.m_deBits = value;
    ui->deBitsText->setText(tr("%1").arg(m_settings.m_deBits));
    applySettings();
}

void MeshtasticModGUI::on_preambleChirps_valueChanged(int value)
{
    m_settings.m_preambleChirps = value;
    ui->preambleChirpsText->setText(tr("%1").arg(m_settings.m_preambleChirps));
    applySettings();
}

void MeshtasticModGUI::on_idleTime_valueChanged(int value)
{
    m_settings.m_quietMillis = value * 100;
    ui->idleTimeText->setText(tr("%1").arg(m_settings.m_quietMillis / 1000.0, 0, 'f', 1));
    applySettings();
}

void MeshtasticModGUI::on_syncWord_editingFinished()
{
    bool ok;
    unsigned int syncWord = ui->syncWord->text().toUInt(&ok, 16);

    if (ok)
    {
        m_settings.m_syncWord = syncWord > 255 ? 0 : syncWord;
        applySettings();
    }
}

void MeshtasticModGUI::on_scheme_currentIndexChanged(int index)
{
    m_settings.m_codingScheme = (MeshtasticModSettings::CodingScheme) index;
    ui->fecParity->setEnabled(m_settings.m_codingScheme == MeshtasticModSettings::CodingLoRa);
    ui->crc->setEnabled(m_settings.m_codingScheme == MeshtasticModSettings::CodingLoRa);
    ui->header->setEnabled(m_settings.m_codingScheme == MeshtasticModSettings::CodingLoRa);
    applySettings();
}

void MeshtasticModGUI::on_fecParity_valueChanged(int value)
{
    m_settings.m_nbParityBits = value;
    ui->fecParityText->setText(tr("%1").arg(m_settings.m_nbParityBits));
    applySettings();
}

void MeshtasticModGUI::on_crc_stateChanged(int state)
{
	m_settings.m_hasCRC = (state == Qt::Checked);
	applySettings();
}

void MeshtasticModGUI::on_header_stateChanged(int state)
{
	m_settings.m_hasHeader = (state == Qt::Checked);
	applySettings();
}

void MeshtasticModGUI::on_myCall_editingFinished()
{
    m_settings.m_myCall = ui->myCall->text();
    applySettings();
}

void MeshtasticModGUI::on_urCall_editingFinished()
{
    m_settings.m_urCall = ui->urCall->text();
    applySettings();
}

void MeshtasticModGUI::on_myLocator_editingFinished()
{
    m_settings.m_myLoc = ui->myLocator->text();
    applySettings();
}

void MeshtasticModGUI::on_report_editingFinished()
{
    m_settings.m_myRpt = ui->report->text();
    applySettings();
}

void MeshtasticModGUI::on_msgType_currentIndexChanged(int index)
{
    m_settings.m_messageType = (MeshtasticModSettings::MessageType) index;
    displayCurrentPayloadMessage();
    applyMeshtasticRadioSettingsIfPresent(getActivePayloadText());
    applySettings();
}

void MeshtasticModGUI::on_resetMessages_clicked(bool checked)
{
    (void) checked;
    m_settings.setDefaultTemplates();
    displayCurrentPayloadMessage();
    applySettings();
}

void MeshtasticModGUI::on_playMessage_clicked(bool checked)
{
    (void) checked;
    applyMeshtasticRadioSettingsIfPresent(getActivePayloadText());
    // Switch to message None then back to current message type to trigger sending process
    MeshtasticModSettings::MessageType msgType = m_settings.m_messageType;
    m_settings.m_messageType = MeshtasticModSettings::MessageNone;
    applySettings();
    m_settings.m_messageType = msgType;
    applySettings();
}

void MeshtasticModGUI::on_repeatMessage_valueChanged(int value)
{
    m_settings.m_messageRepeat = value;
    ui->repeatText->setText(tr("%1").arg(m_settings.m_messageRepeat));
    applySettings();
}

void MeshtasticModGUI::on_generateMessages_clicked(bool checked)
{
    (void) checked;
    m_settings.generateMessages();
    displayCurrentPayloadMessage();
    applySettings();
}

void MeshtasticModGUI::on_messageText_editingFinished()
{
    if (m_settings.m_messageType == MeshtasticModSettings::MessageBeacon) {
        m_settings.m_beaconMessage = ui->messageText->toPlainText();
    } else if (m_settings.m_messageType == MeshtasticModSettings::MessageCQ) {
        m_settings.m_cqMessage = ui->messageText->toPlainText();
    } else if (m_settings.m_messageType == MeshtasticModSettings::MessageReply) {
        m_settings.m_replyMessage = ui->messageText->toPlainText();
    } else if (m_settings.m_messageType == MeshtasticModSettings::MessageReport) {
        m_settings.m_reportMessage = ui->messageText->toPlainText();
    } else if (m_settings.m_messageType == MeshtasticModSettings::MessageReplyReport) {
        m_settings.m_replyReportMessage = ui->messageText->toPlainText();
    } else if (m_settings.m_messageType == MeshtasticModSettings::MessageRRR) {
        m_settings.m_rrrMessage = ui->messageText->toPlainText();
    } else if (m_settings.m_messageType == MeshtasticModSettings::Message73) {
        m_settings.m_73Message = ui->messageText->toPlainText();
    } else if (m_settings.m_messageType == MeshtasticModSettings::MessageQSOText) {
        m_settings.m_qsoTextMessage = ui->messageText->toPlainText();
    } else if (m_settings.m_messageType == MeshtasticModSettings::MessageText) {
        m_settings.m_textMessage = ui->messageText->toPlainText();
    }

    applyMeshtasticRadioSettingsIfPresent(getActivePayloadText());
    applySettings();
}

void MeshtasticModGUI::on_hexText_editingFinished()
{
    m_settings.m_bytesMessage = QByteArray::fromHex(ui->hexText->text().toLatin1());
    applyMeshtasticRadioSettingsIfPresent(getActivePayloadText());
    applySettings();
}

void MeshtasticModGUI::on_udpEnabled_clicked(bool checked)
{
    m_settings.m_udpEnabled = checked;
    applySettings();
}

void MeshtasticModGUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySettings();
}

void MeshtasticModGUI::on_udpPort_editingFinished()
{
    m_settings.m_udpPort = ui->udpPort->text().toInt();
    applySettings();
}

void MeshtasticModGUI::on_invertRamps_stateChanged(int state)
{
    m_settings.m_invertRamps = (state == Qt::Checked);
    applySettings();
}

void MeshtasticModGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void MeshtasticModGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuType::ContextMenuChannelSettings)
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
        new DialogPositioner(&dialog, false);
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

MeshtasticModGUI::MeshtasticModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
	ChannelGUI(parent),
	ui(new Ui::MeshtasticModGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(125000),
	m_doApplySettings(true),
    m_tickCount(0)
{
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channeltx/modmeshtastic/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_chirpChatMod = (MeshtasticMod*) channelTx;
	m_chirpChatMod->setMessageQueueToGUI(getInputMessageQueue());

	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->deltaFrequency->setToolTip(tr("Offset from device center frequency (Hz)."));
    ui->deltaFrequencyLabel->setToolTip(tr("Frequency offset control for the modulator channel."));
    ui->deltaUnits->setToolTip(tr("Frequency unit for the offset control."));
    ui->bw->setToolTip(tr("LoRa transmit bandwidth."));
    ui->bwLabel->setToolTip(tr("LoRa transmit bandwidth selector."));
    ui->bwText->setToolTip(tr("Current LoRa transmit bandwidth in Hz."));
    ui->spread->setToolTip(tr("LoRa spreading factor (SF)."));
    ui->spreadLabel->setToolTip(tr("LoRa spreading factor selector."));
    ui->spreadText->setToolTip(tr("Current spreading factor value."));
    ui->deBits->setToolTip(tr("Low data-rate optimization bits (DE)."));
    ui->deBitsLabel->setToolTip(tr("Low data-rate optimization setting."));
    ui->deBitsText->setToolTip(tr("Current low data-rate optimization value."));
    ui->preambleChirps->setToolTip(tr("LoRa preamble chirp count."));
    ui->preambleChirpsLabel->setToolTip(tr("LoRa preamble chirp count selector."));
    ui->preambleChirpsText->setToolTip(tr("Current preamble chirp value."));
    ui->idleTime->setToolTip(tr("Silence interval between repeated messages (x0.1s)."));
    ui->idleTimeLabel->setToolTip(tr("Idle interval between repeated transmissions."));
    ui->idleTimeText->setToolTip(tr("Current idle interval in seconds."));
    ui->syncWord->setToolTip(tr("LoRa sync word in hexadecimal (00-ff)."));
    ui->syncLabel->setToolTip(tr("LoRa sync word."));
    ui->scheme->setToolTip(tr("Encoder mode. Use LoRa for Meshtastic-compatible payloads."));
    ui->schemeLabel->setToolTip(tr("Select encoding scheme."));
    ui->fecParity->setToolTip(tr("LoRa coding rate parity denominator (CR)."));
    ui->fecParityLabel->setToolTip(tr("LoRa coding rate parity setting."));
    ui->fecParityText->setToolTip(tr("Current coding rate parity value."));
    ui->crc->setToolTip(tr("Append payload CRC."));
    ui->header->setToolTip(tr("Use explicit LoRa header mode."));
    ui->channelMute->setToolTip(tr("Mute this channel output."));
    ui->myCall->setToolTip(tr("Source callsign used by template messages."));
    ui->myCallLabel->setToolTip(tr("Source callsign field."));
    ui->urCall->setToolTip(tr("Destination callsign used by template messages."));
    ui->urCallLabel->setToolTip(tr("Destination callsign field."));
    ui->myLocator->setToolTip(tr("Source locator used by template messages."));
    ui->myLocatorLabel->setToolTip(tr("Source locator field."));
    ui->report->setToolTip(tr("Signal report used by template messages."));
    ui->reportLabel->setToolTip(tr("Signal report field."));
    ui->msgType->setToolTip(tr("Select which payload template is edited/transmitted."));
    ui->msgTypeLabel->setToolTip(tr("Payload template type."));
    ui->resetMessages->setToolTip(tr("Reset payload templates to defaults."));
    ui->playMessage->setToolTip(tr("Queue one transmission of current message type."));
    ui->repeatMessage->setToolTip(tr("Number of repetitions for each triggered transmission."));
    ui->repeatLabel->setToolTip(tr("Transmission repetition count."));
    ui->generateMessages->setToolTip(tr("Generate standardized payload templates from current fields."));
    ui->messageText->setToolTip(tr("Text payload editor. Meshtastic MESH: commands can auto-apply radio settings."));
    ui->msgLabel->setToolTip(tr("Message payload editor."));
    ui->hexText->setToolTip(tr("Raw hexadecimal payload bytes."));
    ui->hexLabel->setToolTip(tr("Hexadecimal payload editor."));
    ui->udpEnabled->setToolTip(tr("Receive message payloads from UDP input."));
    ui->udpAddress->setToolTip(tr("UDP listen address for incoming payloads."));
    ui->udpPort->setToolTip(tr("UDP listen port for incoming payloads."));
    ui->udpSeparator->setToolTip(tr("UDP input controls."));
    ui->invertRamps->setToolTip(tr("Invert chirp ramp direction."));
    ui->channelPower->setToolTip(tr("Estimated channel output power."));
    ui->timesLabel->setToolTip(tr("Estimated timing values for current LoRa frame."));
    ui->timeSymbolText->setToolTip(tr("Estimated LoRa symbol time."));
    ui->timeSymbolLabel->setToolTip(tr("LoRa symbol time estimate."));
    ui->timeMessageLengthText->setToolTip(tr("Estimated payload symbol count."));
    ui->timeMessageLengthLabel->setToolTip(tr("Payload symbol count estimate."));
    ui->timePayloadText->setToolTip(tr("Estimated payload airtime."));
    ui->timePayloadLabel->setToolTip(tr("Payload airtime estimate."));
    ui->timeTotalText->setToolTip(tr("Estimated total airtime including preamble/control."));
    ui->timeTotalLabel->setToolTip(tr("Total frame airtime estimate."));
    ui->repeatText->setToolTip(tr("Current repetition count."));

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::red);
    m_channelMarker.setBandwidth(12500);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("Meshtastic Modulator");
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
    DialPopup::addPopupsToChildDials(this);
    m_resizer.enableChildMouseTracking();
}

MeshtasticModGUI::~MeshtasticModGUI()
{
	delete ui;
}

void MeshtasticModGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void MeshtasticModGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
		MeshtasticMod::MsgConfigureMeshtasticMod *msg = MeshtasticMod::MsgConfigureMeshtasticMod::create(m_settings, force);
		m_chirpChatMod->getInputMessageQueue()->push(msg);
	}
}

void MeshtasticModGUI::displaySettings()
{
    int thisBW = MeshtasticModSettings::bandwidths[m_settings.m_bandwidthIndex];

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

    ui->fecParity->setEnabled(m_settings.m_codingScheme == MeshtasticModSettings::CodingLoRa);
    ui->crc->setEnabled(m_settings.m_codingScheme == MeshtasticModSettings::CodingLoRa);
    ui->header->setEnabled(m_settings.m_codingScheme == MeshtasticModSettings::CodingLoRa);

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
    ui->invertRamps->setChecked(m_settings.m_invertRamps);
    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void MeshtasticModGUI::displayCurrentPayloadMessage()
{
    ui->messageText->blockSignals(true);

    if (m_settings.m_messageType == MeshtasticModSettings::MessageNone) {
        ui->messageText->clear();
    } else if (m_settings.m_messageType == MeshtasticModSettings::MessageBeacon) {
        ui->messageText->setText(m_settings.m_beaconMessage);
    } else if (m_settings.m_messageType == MeshtasticModSettings::MessageCQ) {
        ui->messageText->setText(m_settings.m_cqMessage);
    } else if (m_settings.m_messageType == MeshtasticModSettings::MessageReply) {
        ui->messageText->setText(m_settings.m_replyMessage);
    } else if (m_settings.m_messageType == MeshtasticModSettings::MessageReport) {
        ui->messageText->setText(m_settings.m_reportMessage);
    } else if (m_settings.m_messageType == MeshtasticModSettings::MessageReplyReport) {
        ui->messageText->setText(m_settings.m_replyReportMessage);
    } else if (m_settings.m_messageType == MeshtasticModSettings::MessageRRR) {
        ui->messageText->setText(m_settings.m_rrrMessage);
    } else if (m_settings.m_messageType == MeshtasticModSettings::Message73) {
        ui->messageText->setText(m_settings.m_73Message);
    } else if (m_settings.m_messageType == MeshtasticModSettings::MessageQSOText) {
        ui->messageText->setText(m_settings.m_qsoTextMessage);
    } else if (m_settings.m_messageType == MeshtasticModSettings::MessageText) {
        ui->messageText->setText(m_settings.m_textMessage);
    }

    ui->messageText->blockSignals(false);
}

void MeshtasticModGUI::displayBinaryMessage()
{
    ui->hexText->setText(m_settings.m_bytesMessage.toHex());
}

void MeshtasticModGUI::setBandwidths()
{
    int maxBandwidth = m_basebandSampleRate / MeshtasticModSettings::oversampling;
    int maxIndex = 0;

    for (; (maxIndex < MeshtasticModSettings::nbBandwidths) && (MeshtasticModSettings::bandwidths[maxIndex] <= maxBandwidth); maxIndex++)
    {}

    if (maxIndex != 0)
    {
        qDebug("MeshtasticModGUI::setBandwidths: avl: %d max: %d", maxBandwidth, MeshtasticModSettings::bandwidths[maxIndex-1]);
        ui->bw->setMaximum(maxIndex - 1);
        int index = ui->bw->value();
        ui->bwText->setText(QString("%1 Hz").arg(MeshtasticModSettings::bandwidths[index]));
    }
}

void MeshtasticModGUI::leaveEvent(QEvent* event)
{
	m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void MeshtasticModGUI::enterEvent(EnterEventType* event)
{
	m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void MeshtasticModGUI::tick()
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

void MeshtasticModGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &MeshtasticModGUI::on_deltaFrequency_changed);
    QObject::connect(ui->bw, &QSlider::valueChanged, this, &MeshtasticModGUI::on_bw_valueChanged);
    QObject::connect(ui->spread, &QSlider::valueChanged, this, &MeshtasticModGUI::on_spread_valueChanged);
    QObject::connect(ui->deBits, &QSlider::valueChanged, this, &MeshtasticModGUI::on_deBits_valueChanged);
    QObject::connect(ui->preambleChirps, &QSlider::valueChanged, this, &MeshtasticModGUI::on_preambleChirps_valueChanged);
    QObject::connect(ui->idleTime, &QSlider::valueChanged, this, &MeshtasticModGUI::on_idleTime_valueChanged);
    QObject::connect(ui->syncWord, &QLineEdit::editingFinished, this, &MeshtasticModGUI::on_syncWord_editingFinished);
    QObject::connect(ui->channelMute, &QToolButton::toggled, this, &MeshtasticModGUI::on_channelMute_toggled);
    QObject::connect(ui->scheme, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MeshtasticModGUI::on_scheme_currentIndexChanged);
    QObject::connect(ui->fecParity, &QDial::valueChanged, this, &MeshtasticModGUI::on_fecParity_valueChanged);
    QObject::connect(ui->crc, &QCheckBox::stateChanged, this, &MeshtasticModGUI::on_crc_stateChanged);
    QObject::connect(ui->header, &QCheckBox::stateChanged, this, &MeshtasticModGUI::on_header_stateChanged);
    QObject::connect(ui->myCall, &QLineEdit::editingFinished, this, &MeshtasticModGUI::on_myCall_editingFinished);
    QObject::connect(ui->urCall, &QLineEdit::editingFinished, this, &MeshtasticModGUI::on_urCall_editingFinished);
    QObject::connect(ui->myLocator, &QLineEdit::editingFinished, this, &MeshtasticModGUI::on_myLocator_editingFinished);
    QObject::connect(ui->report, &QLineEdit::editingFinished, this, &MeshtasticModGUI::on_report_editingFinished);
    QObject::connect(ui->msgType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MeshtasticModGUI::on_msgType_currentIndexChanged);
    QObject::connect(ui->resetMessages, &QPushButton::clicked, this, &MeshtasticModGUI::on_resetMessages_clicked);
    QObject::connect(ui->playMessage, &QPushButton::clicked, this, &MeshtasticModGUI::on_playMessage_clicked);
    QObject::connect(ui->repeatMessage, &QDial::valueChanged, this, &MeshtasticModGUI::on_repeatMessage_valueChanged);
    QObject::connect(ui->generateMessages, &QPushButton::clicked, this, &MeshtasticModGUI::on_generateMessages_clicked);
    QObject::connect(ui->messageText, &CustomTextEdit::editingFinished, this, &MeshtasticModGUI::on_messageText_editingFinished);
    QObject::connect(ui->hexText, &QLineEdit::editingFinished, this, &MeshtasticModGUI::on_hexText_editingFinished);
    QObject::connect(ui->udpEnabled, &QCheckBox::clicked, this, &MeshtasticModGUI::on_udpEnabled_clicked);
    QObject::connect(ui->udpAddress, &QLineEdit::editingFinished, this, &MeshtasticModGUI::on_udpAddress_editingFinished);
    QObject::connect(ui->udpPort, &QLineEdit::editingFinished, this, &MeshtasticModGUI::on_udpPort_editingFinished);
    QObject::connect(ui->invertRamps, &QCheckBox::stateChanged, this, &MeshtasticModGUI::on_invertRamps_stateChanged);
}

void MeshtasticModGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
