///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Alejandro Aleman                                           //
// Copyright (C) 2020-2026 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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
    case MeshtasticModSettings::MessageText:
        return m_settings.m_textMessage;
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

    if (!modemmeshtastic::Packet::isCommand(payloadText)) {
        return;
    }

    modemmeshtastic::TxRadioSettings meshRadio;
    QString error;
    if (!modemmeshtastic::Packet::deriveTxRadioSettings(payloadText, meshRadio, error))
    {
        qWarning() << "MeshtasticModGUI::applyMeshtasticRadioSettingsIfPresent:" << error;
        return;
    }

    bool changed = false;

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

void MeshtasticModGUI::applyMeshtasticProfileFromSelection()
{
    const QString region = ui->meshRegion->currentData().toString();
    const QString preset = ui->meshPreset->currentData().toString();
    const int meshChannel = ui->meshChannel->currentData().toInt();
    const int channelNum = meshChannel + 1; // planner expects 1-based channel_num

    if (region.isEmpty() || preset.isEmpty()) {
        return;
    }

    const QString command = QString("MESH:preset=%1;region=%2;channel_num=%3").arg(preset, region).arg(channelNum);
    modemmeshtastic::TxRadioSettings meshRadio;
    QString error;

    if (!modemmeshtastic::Packet::deriveTxRadioSettings(command, meshRadio, error))
    {
        qWarning() << "MeshtasticModGUI::applyMeshtasticProfileFromSelection:" << error;
        return;
    }

    bool changed = false;
    bool selectionStateChanged = false;

    if (m_settings.m_meshtasticRegionCode != region)
    {
        m_settings.m_meshtasticRegionCode = region;
        selectionStateChanged = true;
    }
    if (m_settings.m_meshtasticPresetName != preset)
    {
        m_settings.m_meshtasticPresetName = preset;
        selectionStateChanged = true;
    }
    if (m_settings.m_meshtasticChannelIndex != meshChannel)
    {
        m_settings.m_meshtasticChannelIndex = meshChannel;
        selectionStateChanged = true;
    }

    const int bwIndex = findBandwidthIndex(meshRadio.bandwidthHz);
    if (bwIndex >= 0 && bwIndex != m_settings.m_bandwidthIndex)
    {
        m_settings.m_bandwidthIndex = bwIndex;
        changed = true;
    }

    if (meshRadio.spreadFactor > 0 && meshRadio.spreadFactor != m_settings.m_spreadFactor)
    {
        m_settings.m_spreadFactor = meshRadio.spreadFactor;
        changed = true;
    }

    if (meshRadio.deBits != m_settings.m_deBits)
    {
        m_settings.m_deBits = meshRadio.deBits;
        changed = true;
    }

    if (meshRadio.parityBits > 0 && meshRadio.parityBits != m_settings.m_nbParityBits)
    {
        m_settings.m_nbParityBits = meshRadio.parityBits;
        changed = true;
    }

    const int meshPreambleChirps = meshRadio.preambleChirps;
    if (m_settings.m_preambleChirps != static_cast<unsigned int>(meshPreambleChirps))
    {
        m_settings.m_preambleChirps = static_cast<unsigned int>(meshPreambleChirps);
        changed = true;
    }

    if (meshRadio.syncWord != m_settings.m_syncWord)
    {
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
            qWarning() << "MeshtasticModGUI::applyMeshtasticProfileFromSelection: device center frequency unknown, cannot auto-center";
        }
    }

    if (!changed && !selectionStateChanged) {
        return;
    }

    qInfo() << "MeshtasticModGUI::applyMeshtasticProfileFromSelection:" << meshRadio.summary;

    if (!changed)
    {
        applySettings();
        return;
    }

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
    ui->preambleChirps->setValue(m_settings.m_preambleChirps);
    ui->preambleChirpsText->setText(tr("%1").arg(m_settings.m_preambleChirps));
    ui->fecParity->setValue(m_settings.m_nbParityBits);
    ui->fecParityText->setText(tr("%1").arg(m_settings.m_nbParityBits));
    ui->syncWord->setText(tr("%1").arg(m_settings.m_syncWord, 2, 16));
    blockApplySettings(false);

    updateAbsoluteCenterFrequency();
    applySettings();
}

void MeshtasticModGUI::rebuildMeshtasticChannelOptions()
{
    const QString region = ui->meshRegion->currentData().toString();
    const QString preset = ui->meshPreset->currentData().toString();
    const int previousChannel = ui->meshChannel->currentData().toInt();

    m_meshControlsUpdating = true;
    ui->meshChannel->clear();

    int added = 0;
    for (int meshChannel = 0; meshChannel <= 200; ++meshChannel)
    {
        modemmeshtastic::TxRadioSettings meshRadio;
        QString error;
        const int channelNum = meshChannel + 1; // planner expects 1-based channel_num
        const QString command = QString("MESH:preset=%1;region=%2;channel_num=%3").arg(preset, region).arg(channelNum);

        if (!modemmeshtastic::Packet::deriveTxRadioSettings(command, meshRadio, error))
        {
            if (added > 0) {
                break;
            } else {
                continue;
            }
        }

        const QString label = meshRadio.hasCenterFrequency
            ? QString("%1 (%2 MHz)").arg(meshChannel).arg(meshRadio.centerFrequencyHz / 1000000.0, 0, 'f', 3)
            : QString::number(meshChannel);

        ui->meshChannel->addItem(label, meshChannel);
        added++;
    }

    if (added == 0) {
        ui->meshChannel->addItem("0", 0);
    }

    ui->meshChannel->setToolTip(tr("Meshtastic channel number (%1 available for %2/%3)")
        .arg(added)
        .arg(region)
        .arg(preset));
    int restoreIndex = ui->meshChannel->findData(previousChannel);
    if (restoreIndex < 0) {
        restoreIndex = 0;
    }
    ui->meshChannel->setCurrentIndex(restoreIndex);
    m_meshControlsUpdating = false;

    qInfo() << "MeshtasticModGUI::rebuildMeshtasticChannelOptions:"
            << "region=" << region
            << "preset=" << preset
            << "channels=" << added;

    QMetaObject::invokeMethod(this, [this]() {
        if (!m_meshControlsUpdating) {
            applyMeshtasticProfileFromSelection();
        }
    }, Qt::QueuedConnection);
}

void MeshtasticModGUI::setupMeshtasticAutoProfileControls()
{
    for (int i = 0; i < ui->meshRegion->count(); ++i) {
        ui->meshRegion->setItemData(i, ui->meshRegion->itemText(i), Qt::UserRole);
    }

    for (int i = 0; i < ui->meshPreset->count(); ++i) {
        ui->meshPreset->setItemData(i, ui->meshPreset->itemText(i), Qt::UserRole);
    }

    QObject::connect(ui->meshRegion, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MeshtasticModGUI::on_meshRegion_currentIndexChanged);
    QObject::connect(ui->meshPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MeshtasticModGUI::on_meshPreset_currentIndexChanged);
    QObject::connect(ui->meshChannel, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MeshtasticModGUI::on_meshChannel_currentIndexChanged);
    QObject::connect(ui->meshApply, &QPushButton::clicked, this, &MeshtasticModGUI::on_meshApply_clicked);

    rebuildMeshtasticChannelOptions();
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

void MeshtasticModGUI::on_fecParity_valueChanged(int value)
{
    m_settings.m_nbParityBits = value;
    ui->fecParityText->setText(tr("%1").arg(m_settings.m_nbParityBits));
    applySettings();
}

void MeshtasticModGUI::on_playMessage_clicked(bool checked)
{
    (void) checked;
    applyMeshtasticRadioSettingsIfPresent(getActivePayloadText());
    applySettings();
    m_meshtasticMod->sendMessage();
}

void MeshtasticModGUI::on_repeatMessage_valueChanged(int value)
{
    m_settings.m_messageRepeat = value;
    ui->repeatText->setText(tr("%1").arg(m_settings.m_messageRepeat));
    applySettings();
}

void MeshtasticModGUI::on_messageText_editingFinished()
{
    if (m_settings.m_messageType == MeshtasticModSettings::MessageText) {
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

void MeshtasticModGUI::on_meshRegion_currentIndexChanged(int index)
{
    (void) index;
    if (m_meshControlsUpdating) {
        return;
    }

    rebuildMeshtasticChannelOptions();
    applyMeshtasticProfileFromSelection();
}

void MeshtasticModGUI::on_meshPreset_currentIndexChanged(int index)
{
    (void) index;
    if (m_meshControlsUpdating) {
        return;
    }

    rebuildMeshtasticChannelOptions();
    applyMeshtasticProfileFromSelection();
}

void MeshtasticModGUI::on_meshChannel_currentIndexChanged(int index)
{
    (void) index;
    if (m_meshControlsUpdating) {
        return;
    }

    applyMeshtasticProfileFromSelection();
}

void MeshtasticModGUI::on_meshApply_clicked(bool checked)
{
    (void) checked;
    if (m_meshControlsUpdating) {
        return;
    }

    rebuildMeshtasticChannelOptions();
    applyMeshtasticProfileFromSelection();
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
            dialog.setNumberOfStreams(m_meshtasticMod->getNumberOfDeviceStreams());
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
    m_meshControlsUpdating(false),
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

	m_meshtasticMod = (MeshtasticMod*) channelTx;
	m_meshtasticMod->setMessageQueueToGUI(getInputMessageQueue());

	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    ui->fecParity->setEnabled(true);

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
    ui->fecParity->setToolTip(tr("LoRa coding rate parity denominator (CR)."));
    ui->fecParityLabel->setToolTip(tr("LoRa coding rate parity setting."));
    ui->fecParityText->setToolTip(tr("Current coding rate parity value."));
    ui->channelMute->setToolTip(tr("Mute this channel output."));
    ui->playMessage->setToolTip(tr("Queue one transmission of current message type."));
    ui->repeatMessage->setToolTip(tr("Number of repetitions for each triggered transmission."));
    ui->repeatLabel->setToolTip(tr("Transmission repetition count."));
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
    setupMeshtasticAutoProfileControls();
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
		m_meshtasticMod->getInputMessageQueue()->push(msg);
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
    ui->fecParity->setValue(m_settings.m_nbParityBits);
    ui->fecParityText->setText(tr("%1").arg(m_settings.m_nbParityBits));
    ui->repeatMessage->setValue(m_settings.m_messageRepeat);
    ui->repeatText->setText(tr("%1").arg(m_settings.m_messageRepeat));
    ui->udpEnabled->setChecked(m_settings.m_udpEnabled);
    ui->udpAddress->setText(m_settings.m_udpAddress);
    ui->udpPort->setText(QString::number(m_settings.m_udpPort));
    ui->invertRamps->setChecked(m_settings.m_invertRamps);

    m_meshControlsUpdating = true;

    int regionIndex = ui->meshRegion->findData(m_settings.m_meshtasticRegionCode);
    if (regionIndex < 0) {
        regionIndex = ui->meshRegion->findData("US");
    }
    if (regionIndex < 0) {
        regionIndex = 0;
    }
    ui->meshRegion->setCurrentIndex(regionIndex);

    int presetIndex = ui->meshPreset->findData(m_settings.m_meshtasticPresetName);
    if (presetIndex < 0) {
        presetIndex = ui->meshPreset->findData("LONG_FAST");
    }
    if (presetIndex < 0) {
        presetIndex = 0;
    }
    ui->meshPreset->setCurrentIndex(presetIndex);
    m_meshControlsUpdating = false;

    rebuildMeshtasticChannelOptions();

    m_meshControlsUpdating = true;
    int channelIndex = ui->meshChannel->findData(m_settings.m_meshtasticChannelIndex);
    if (channelIndex < 0) {
        channelIndex = 0;
    }
    ui->meshChannel->setCurrentIndex(channelIndex);
    m_meshControlsUpdating = false;

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void MeshtasticModGUI::displayCurrentPayloadMessage()
{
    ui->messageText->blockSignals(true);

    if (m_settings.m_messageType == MeshtasticModSettings::MessageText) {
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
        double powDb = CalcDb::dbPower(m_meshtasticMod->getMagSq());
        m_channelPowerDbAvg(powDb);
        ui->channelPower->setText(tr("%1 dB").arg(m_channelPowerDbAvg.asDouble(), 0, 'f', 1));

        if (m_meshtasticMod->getModulatorActive()) {
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
    QObject::connect(ui->fecParity, &QDial::valueChanged, this, &MeshtasticModGUI::on_fecParity_valueChanged);
    QObject::connect(ui->playMessage, &QPushButton::clicked, this, &MeshtasticModGUI::on_playMessage_clicked);
    QObject::connect(ui->repeatMessage, &QDial::valueChanged, this, &MeshtasticModGUI::on_repeatMessage_valueChanged);
    QObject::connect(ui->messageText, &CustomTextEdit::editingFinished, this, &MeshtasticModGUI::on_messageText_editingFinished);
    QObject::connect(ui->hexText, &QLineEdit::editingFinished, this, &MeshtasticModGUI::on_hexText_editingFinished);
    QObject::connect(ui->udpEnabled, &QCheckBox::clicked, this, &MeshtasticModGUI::on_udpEnabled_clicked);
    QObject::connect(ui->udpAddress, &QLineEdit::editingFinished, this, &MeshtasticModGUI::on_udpAddress_editingFinished);
    QObject::connect(ui->udpPort, &QLineEdit::editingFinished, this, &MeshtasticModGUI::on_udpPort_editingFinished);
    QObject::connect(ui->invertRamps, &QCheckBox::stateChanged, this, &MeshtasticModGUI::on_invertRamps_stateChanged);
    QObject::connect(ui->meshRegion, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MeshtasticModGUI::on_meshRegion_currentIndexChanged);
    QObject::connect(ui->meshPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MeshtasticModGUI::on_meshPreset_currentIndexChanged);
    QObject::connect(ui->meshChannel, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MeshtasticModGUI::on_meshChannel_currentIndexChanged);
    QObject::connect(ui->meshApply, &QPushButton::clicked, this, &MeshtasticModGUI::on_meshApply_clicked);
}

void MeshtasticModGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
