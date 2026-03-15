///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2015 John Greb <hexameron@spam.no>                              //
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

#include "device/deviceuiset.h"
#include "device/deviceapi.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QScrollBar>
#include <QDebug>
#include <QCoreApplication>
#include <QEventLoop>
#include <QDateTime>
#include <QComboBox>
#include <QCheckBox>
#include <QMessageBox>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QList>
#include <QMap>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

#include "ui_meshtasticdemodgui.h"
#include "dsp/spectrumvis.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplemimo.h"
#include "gui/glspectrum.h"
#include "gui/glspectrumgui.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "plugin/pluginapi.h"
#include "channel/channelwebapiutils.h"
#include "util/db.h"
#include "maincore.h"

#include "meshtasticdemod.h"
#include "meshtasticdemodmsg.h"
#include "meshtasticdemodgui.h"
#include "meshtasticpacket.h"

namespace
{
    static const int kMeshAutoLockCandidateTimeoutMs = 12000;
    static const int kMeshAutoLockArmTimeoutMs = 12000;
    static const int kMeshAutoLockMinObservationsPerCandidate = 3;
    static const int kMeshAutoLockMinSourceObservationsPerCandidate = 6;
    static const int kMeshAutoLockMinDecodeSamplesForApply = 3;
    static const double kMeshAutoLockMinDecodeAverageForApply = 0.5;
    static const double kMeshAutoLockActivityP2NThresholdDb = 4.0;
    static const int kMeshAutoLockOffsetMultipliers[] = {
        0, -1, 1, -2, 2, -3, 3, -4, 4, -6, 6, -8, 8, -10, 10, -12, 12, -16, 16, -24, 24, -32, 32, -48, 48, -64, 64
    };
    static const int kTreeRawKeyRole = Qt::UserRole;
    static const int kTreeDisplayLabelRole = Qt::UserRole + 1;
    static const int kTreeRawValueRole = Qt::UserRole + 2;
    static const int kTreeMessageKeyRole = Qt::UserRole + 3;

    void alignTextViewToLatestLineLeft(QPlainTextEdit *textView)
    {
        if (!textView) {
            return;
        }

        QScrollBar *verticalScroll = textView->verticalScrollBar();

        if (verticalScroll) {
            verticalScroll->setValue(verticalScroll->maximum());
        }

        QScrollBar *horizontalScroll = textView->horizontalScrollBar();

        if (horizontalScroll) {
            horizontalScroll->setValue(horizontalScroll->minimum());
        }
    }

    void alignTreeViewToLatestEntryLeft(QTreeWidget *treeWidget, QTreeWidgetItem *item)
    {
        if (!treeWidget) {
            return;
        }

        if (item) {
            treeWidget->scrollToItem(item);
        }

        QScrollBar *verticalScroll = treeWidget->verticalScrollBar();

        if (verticalScroll) {
            verticalScroll->setValue(verticalScroll->maximum());
        }

        QScrollBar *horizontalScroll = treeWidget->horizontalScrollBar();

        if (horizontalScroll) {
            horizontalScroll->setValue(horizontalScroll->minimum());
        }
    }
}

MeshtasticDemodGUI* MeshtasticDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	MeshtasticDemodGUI* gui = new MeshtasticDemodGUI(pluginAPI, deviceUISet, rxChannel);
	return gui;
}

void MeshtasticDemodGUI::destroy()
{
	delete this;
}

void MeshtasticDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(true);
}

QByteArray MeshtasticDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool MeshtasticDemodGUI::deserialize(const QByteArray& data)
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

bool MeshtasticDemodGUI::handleMessage(const Message& message)
{
    if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        int basebandSampleRate = notif.getSampleRate();
        qDebug() << "MeshtasticDemodGUI::handleMessage: DSPSignalNotification: m_basebandSampleRate: " << basebandSampleRate;

        if (basebandSampleRate != m_basebandSampleRate)
        {
            m_basebandSampleRate = basebandSampleRate;
            setBandwidths();
        }

        ui->deltaFrequency->setValueRange(false, 7, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();

        if (m_remoteTcpReconnectAutoApplyPending)
        {
            m_remoteTcpReconnectAutoApplyPending = false;
            m_remoteTcpReconnectAutoApplyWaitTicks = 0;
            qInfo() << "MeshtasticDemodGUI::handleMessage: DSPSignalNotification after RemoteTCP reconnect - reapplying Meshtastic profile";
            QMetaObject::invokeMethod(this, &MeshtasticDemodGUI::applyMeshtasticProfileFromSelection, Qt::QueuedConnection);
        }

        return true;
    }
    else if (MeshtasticDemodMsg::MsgReportDecodeBytes::match(message))
    {
        const MeshtasticDemodMsg::MsgReportDecodeBytes& msg = (MeshtasticDemodMsg::MsgReportDecodeBytes&) message;
        handleMeshAutoLockObservation(msg);

        if (m_settings.m_codingScheme == MeshtasticDemodSettings::CodingLoRa) {
            showLoRaMessage(message);
        }

        return true;
    }
    else if (MeshtasticDemodMsg::MsgReportDecodeString::match(message))
    {
        if ((m_settings.m_codingScheme == MeshtasticDemodSettings::CodingLoRa)) {
            showTextMessage(message);
        }

        return true;
    }
    else if (MeshtasticDemod::MsgConfigureMeshtasticDemod::match(message))
    {
        qDebug("MeshtasticDemodGUI::handleMessage: NFMDemod::MsgConfigureMeshtasticDemod");
        const MeshtasticDemod::MsgConfigureMeshtasticDemod& cfg = (MeshtasticDemod::MsgConfigureMeshtasticDemod&) message;
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

void MeshtasticDemodGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void MeshtasticDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
	applySettings();
}

void MeshtasticDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void MeshtasticDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void MeshtasticDemodGUI::on_BW_valueChanged(int value)
{
    if (value < 0) {
        m_settings.m_bandwidthIndex = 0;
    } else if (value < MeshtasticDemodSettings::nbBandwidths) {
        m_settings.m_bandwidthIndex = value;
    } else {
        m_settings.m_bandwidthIndex = MeshtasticDemodSettings::nbBandwidths - 1;
    }

	int thisBW = MeshtasticDemodSettings::bandwidths[value];
	ui->BWText->setText(QString("%1 Hz").arg(thisBW));
	m_channelMarker.setBandwidth(thisBW);
	ui->glSpectrum->setSampleRate(thisBW);
	ui->glSpectrum->setCenterFrequency(thisBW/2);

	applySettings();
}

void MeshtasticDemodGUI::on_Spread_valueChanged(int value)
{
    m_settings.m_spreadFactor = value;
    ui->SpreadText->setText(tr("%1").arg(value));
    ui->spectrumGUI->setFFTSize(m_settings.m_spreadFactor);

    applySettings();
}

void MeshtasticDemodGUI::on_deBits_valueChanged(int value)
{
    m_settings.m_deBits = value;
    ui->deBitsText->setText(tr("%1").arg(m_settings.m_deBits));
    applySettings();
}

void MeshtasticDemodGUI::on_fftWindow_currentIndexChanged(int index)
{
    m_settings.m_fftWindow = (FFTWindow::Function) index;
    applySettings();
}

void MeshtasticDemodGUI::on_preambleChirps_valueChanged(int value)
{
    m_settings.m_preambleChirps = value;
    ui->preambleChirpsText->setText(tr("%1").arg(m_settings.m_preambleChirps));
    applySettings();
}

void MeshtasticDemodGUI::on_mute_toggled(bool checked)
{
    m_settings.m_decodeActive = !checked;
    applySettings();
}

void MeshtasticDemodGUI::on_clear_clicked(bool checked)
{
    (void) checked;
    clearPipelineViews();
    setDechirpInspectionMode(false);
}

void MeshtasticDemodGUI::on_eomSquelch_valueChanged(int value)
{
    m_settings.m_eomSquelchTenths = value;
    displaySquelch();
    applySettings();
}

void MeshtasticDemodGUI::on_messageLength_valueChanged(int value)
{
    m_settings.m_nbSymbolsMax = value;
    ui->messageLengthText->setText(tr("%1").arg(m_settings.m_nbSymbolsMax));
    applySettings();
}

void MeshtasticDemodGUI::on_udpSend_stateChanged(int state)
{
    m_settings.m_sendViaUDP = (state == Qt::Checked);
    applySettings();
}

void MeshtasticDemodGUI::on_udpAddress_editingFinished()
{
    m_settings.m_udpAddress = ui->udpAddress->text();
    applySettings();
}

void MeshtasticDemodGUI::on_udpPort_editingFinished()
{
    bool ok;
    quint16 udpPort = ui->udpPort->text().toInt(&ok);

    if((!ok) || (udpPort < 1024)) {
        udpPort = 9998;
    }

    m_settings.m_udpPort = udpPort;
    ui->udpPort->setText(tr("%1").arg(m_settings.m_udpPort));
    applySettings();
}

void MeshtasticDemodGUI::on_invertRamps_stateChanged(int state)
{
    m_settings.m_invertRamps = (state == Qt::Checked);
    applySettings();
}

void MeshtasticDemodGUI::on_meshRegion_currentIndexChanged(int index)
{
    (void) index;
    if (m_meshControlsUpdating) {
        return;
    }

    rebuildMeshtasticChannelOptions();
    applyMeshtasticProfileFromSelection();
}

void MeshtasticDemodGUI::on_meshPreset_currentIndexChanged(int index)
{
    (void) index;
    if (m_meshControlsUpdating) {
        return;
    }

    rebuildMeshtasticChannelOptions();
    applyMeshtasticProfileFromSelection();
}

void MeshtasticDemodGUI::on_meshChannel_currentIndexChanged(int index)
{
    (void) index;
    if (m_meshControlsUpdating) {
        return;
    }

    applyMeshtasticProfileFromSelection();
}

void MeshtasticDemodGUI::on_meshApply_clicked(bool checked)
{
    (void) checked;

    if (m_meshControlsUpdating) {
        return;
    }

    // Rebuild first so region/preset changes refresh the valid channel list before applying.
    rebuildMeshtasticChannelOptions();
    applyMeshtasticProfileFromSelection();
}

void MeshtasticDemodGUI::on_meshKeys_clicked(bool checked)
{
    (void) checked;
    editMeshtasticKeys();
}

void MeshtasticDemodGUI::on_meshAutoSampleRate_toggled(bool checked)
{
    if (m_meshControlsUpdating) {
        return;
    }

    m_settings.m_meshtasticAutoSampleRate = checked;
    applySettings();

    if (checked) {
        applyMeshtasticProfileFromSelection();
    } else {
        displayStatus(tr("MESH CFG|auto input tuning disabled"));
    }
}

void MeshtasticDemodGUI::on_meshAutoLock_clicked(bool checked)
{
    if (checked) {
        startMeshAutoLock();
    } else {
        stopMeshAutoLock(true);
    }
}

void MeshtasticDemodGUI::startMeshAutoLock()
{
    if (m_meshAutoLockActive) {
        return;
    }

    if (m_settings.m_codingScheme != MeshtasticDemodSettings::CodingLoRa)
    {
        displayStatus(tr("MESH LOCK|switch decoder scheme to LoRa before auto-lock"));

        if (m_meshAutoLockButton)
        {
            m_meshAutoLockButton->blockSignals(true);
            m_meshAutoLockButton->setChecked(false);
            m_meshAutoLockButton->blockSignals(false);
        }

        return;
    }

    const int bandwidthHz = MeshtasticDemodSettings::bandwidths[m_settings.m_bandwidthIndex];
    const int sf = std::max(1, m_settings.m_spreadFactor);
    const int symbolBins = 1 << std::min(15, sf);
    const int stepHz = std::max(100, bandwidthHz / symbolBins);
    const bool invertOrder[] = {m_settings.m_invertRamps, !m_settings.m_invertRamps};

    m_meshAutoLockCandidates.clear();
    m_meshAutoLockBaseOffsetHz = m_settings.m_inputFrequencyOffset;
    m_meshAutoLockBaseInvert = m_settings.m_invertRamps;
    m_meshAutoLockBaseDeBits = m_settings.m_deBits;

    QVector<int> deCandidates;
    deCandidates.push_back(m_settings.m_deBits);

    // SDR decode compatibility scan:
    // for high SF profiles, also probe DE=0 and DE=2 even if the profile selects one of them.
    if (sf >= 11)
    {
        if (std::find(deCandidates.begin(), deCandidates.end(), 0) == deCandidates.end()) {
            deCandidates.push_back(0);
        }
        if (std::find(deCandidates.begin(), deCandidates.end(), 2) == deCandidates.end()) {
            deCandidates.push_back(2);
        }
    }

    for (bool invert : invertOrder)
    {
        for (int multiplier : kMeshAutoLockOffsetMultipliers)
        {
            for (int deBits : deCandidates)
            {
                MeshAutoLockCandidate candidate;
                candidate.inputOffsetHz = m_meshAutoLockBaseOffsetHz + multiplier * stepHz;
                candidate.invertRamps = invert;
                candidate.deBits = deBits;
                candidate.score = 0.0;
                candidate.samples = 0;
                candidate.sourceScore = 0.0;
                candidate.sourceSamples = 0;
                candidate.syncWordZeroCount = 0;
                candidate.headerParityOkOrFixCount = 0;
                candidate.headerCRCCount = 0;
                candidate.payloadCRCCount = 0;
                candidate.earlyEOMCount = 0;
                m_meshAutoLockCandidates.push_back(candidate);
            }
        }
    }

    if (m_meshAutoLockCandidates.isEmpty())
    {
        displayStatus(tr("MESH LOCK|no candidates generated"));
        if (m_meshAutoLockButton)
        {
            m_meshAutoLockButton->blockSignals(true);
            m_meshAutoLockButton->setChecked(false);
            m_meshAutoLockButton->blockSignals(false);
        }
        return;
    }

    m_meshAutoLockActive = true;
    m_meshAutoLockCandidateIndex = 0;
    m_meshAutoLockObservedSamplesForCandidate = 0;
    m_meshAutoLockObservedSourceSamplesForCandidate = 0;
    m_meshAutoLockTotalDecodeSamples = 0;
    m_meshAutoLockTrafficSeen = false;
    m_meshAutoLockActivityTicks = 0;
    m_meshAutoLockArmStartMs = QDateTime::currentMSecsSinceEpoch();
    m_meshAutoLockCandidateStartMs = QDateTime::currentMSecsSinceEpoch();

    if (m_meshAutoLockButton) {
        m_meshAutoLockButton->setText(tr("Locking..."));
    }

    applyMeshAutoLockCandidate(m_meshAutoLockCandidates[m_meshAutoLockCandidateIndex], true);

    QString deSummary;
    for (int i = 0; i < deCandidates.size(); ++i)
    {
        if (!deSummary.isEmpty()) {
            deSummary += "/";
        }
        deSummary += QString::number(deCandidates[i]);
    }

    displayStatus(tr("MESH LOCK|armed %1 candidates step=%2Hz de=%3. Waiting for on-air activity before scanning.")
        .arg(m_meshAutoLockCandidates.size())
        .arg(stepHz)
        .arg(deSummary));
}

void MeshtasticDemodGUI::stopMeshAutoLock(bool keepBestCandidate)
{
    if (!m_meshAutoLockActive && (!m_meshAutoLockButton || !m_meshAutoLockButton->isChecked()))
    {
        return;
    }

    int bestIndex = -1;
    int bestFallbackIndex = -1;
    double bestWeightedScore = -std::numeric_limits<double>::infinity();
    double bestFallbackWeightedScore = -std::numeric_limits<double>::infinity();

    if (keepBestCandidate)
    {
        for (int i = 0; i < m_meshAutoLockCandidates.size(); ++i)
        {
            const MeshAutoLockCandidate& candidate = m_meshAutoLockCandidates[i];
            const bool hasStrongDecodeEvidence = (candidate.payloadCRCCount > 0) || (candidate.headerCRCCount > 0);
            const bool hasDecodeSamples = (candidate.samples >= kMeshAutoLockMinDecodeSamplesForApply) || hasStrongDecodeEvidence;
            const bool hasSourceSamples = candidate.sourceSamples > 0;

            if (!hasDecodeSamples && !hasSourceSamples) {
                continue;
            }

            const double averageDecodeScore = candidate.samples > 0 ? (candidate.score / candidate.samples) : -12.0;
            const double averageSourceScore = hasSourceSamples ? (candidate.sourceScore / candidate.sourceSamples) : -2.0;
            const bool hasCRCBackedDecode = candidate.payloadCRCCount > 0;
            const bool hasHeaderBackedDecode = candidate.headerCRCCount > 0;
            const bool decodeEvidence = hasCRCBackedDecode || hasHeaderBackedDecode;

            // Hard floor: no auto-apply on weak/noisy candidates that never show valid header/payload structure.
            if (!decodeEvidence) {
                continue;
            }

            const double confidenceBoost = std::min(candidate.samples, 8) * 0.25
                + std::min(candidate.sourceSamples, 20) * 0.03;
            const double weightedScore = (averageDecodeScore * 1.1)
                + (averageSourceScore * 0.2)
                + (candidate.headerCRCCount * 4.0)
                + (candidate.payloadCRCCount * 8.0)
                + confidenceBoost;

            const bool strongDecodeEvidence = hasCRCBackedDecode
                || (hasHeaderBackedDecode
                    && (averageDecodeScore >= kMeshAutoLockMinDecodeAverageForApply));

            if (strongDecodeEvidence && (weightedScore > bestWeightedScore))
            {
                bestWeightedScore = weightedScore;
                bestIndex = i;
            }
            else if (hasHeaderBackedDecode && (averageDecodeScore >= -2.0) && (weightedScore > bestFallbackWeightedScore))
            {
                bestFallbackWeightedScore = weightedScore;
                bestFallbackIndex = i;
            }
        }
    }

    m_meshAutoLockActive = false;
    m_meshAutoLockCandidateIndex = 0;
    m_meshAutoLockCandidateStartMs = 0;
    m_meshAutoLockObservedSamplesForCandidate = 0;
    m_meshAutoLockObservedSourceSamplesForCandidate = 0;
    m_meshAutoLockTotalDecodeSamples = 0;
    m_meshAutoLockTrafficSeen = false;
    m_meshAutoLockActivityTicks = 0;
    m_meshAutoLockArmStartMs = 0;

    if (m_meshAutoLockButton)
    {
        m_meshAutoLockButton->blockSignals(true);
        m_meshAutoLockButton->setChecked(false);
        m_meshAutoLockButton->setText(tr("Auto Lock"));
        m_meshAutoLockButton->blockSignals(false);
    }

    if (keepBestCandidate && (bestIndex >= 0 || bestFallbackIndex >= 0))
    {
        const bool provisional = bestIndex < 0;
        const MeshAutoLockCandidate& best = m_meshAutoLockCandidates[provisional ? bestFallbackIndex : bestIndex];
        applyMeshAutoLockCandidate(best, true);
        const double avgScore = best.samples > 0 ? (best.score / best.samples) : 0.0;
        const double avgSourceScore = best.sourceSamples > 0 ? (best.sourceScore / best.sourceSamples) : 0.0;
        const double syncRatio = best.samples > 0 ? (100.0 * best.syncWordZeroCount / best.samples) : 0.0;
        displayStatus(tr("MESH LOCK|applied %1candidate df=%2Hz inv=%3 de=%4 decode=%5/%6 source=%7/%8 sync00=%9% hc=%10 crc=%11")
            .arg(provisional ? "provisional " : "best ")
            .arg(best.inputOffsetHz)
            .arg(best.invertRamps ? "on" : "off")
            .arg(best.deBits)
            .arg(avgScore, 0, 'f', 2)
            .arg(best.samples)
            .arg(avgSourceScore, 0, 'f', 2)
            .arg(best.sourceSamples)
            .arg(syncRatio, 0, 'f', 1)
            .arg(best.headerCRCCount)
            .arg(best.payloadCRCCount));
    }
    else
    {
        MeshAutoLockCandidate baseCandidate;
        baseCandidate.inputOffsetHz = m_meshAutoLockBaseOffsetHz;
        baseCandidate.invertRamps = m_meshAutoLockBaseInvert;
        baseCandidate.deBits = m_meshAutoLockBaseDeBits;
        baseCandidate.score = 0.0;
        baseCandidate.samples = 0;
        baseCandidate.sourceScore = 0.0;
        baseCandidate.sourceSamples = 0;
        baseCandidate.syncWordZeroCount = 0;
        baseCandidate.headerParityOkOrFixCount = 0;
        baseCandidate.headerCRCCount = 0;
        baseCandidate.payloadCRCCount = 0;
        baseCandidate.earlyEOMCount = 0;
        applyMeshAutoLockCandidate(baseCandidate, true);
        displayStatus(tr("MESH LOCK|stopped (no decode-backed lock). Baseline restored."));
    }
}

void MeshtasticDemodGUI::applyMeshAutoLockCandidate(const MeshAutoLockCandidate& candidate, bool applySettingsNow)
{
    m_settings.m_inputFrequencyOffset = candidate.inputOffsetHz;
    m_settings.m_invertRamps = candidate.invertRamps;
    m_settings.m_deBits = candidate.deBits;

    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(candidate.inputOffsetHz);
    m_channelMarker.blockSignals(false);

    ui->deltaFrequency->blockSignals(true);
    ui->deltaFrequency->setValue(candidate.inputOffsetHz);
    ui->deltaFrequency->blockSignals(false);

    ui->invertRamps->blockSignals(true);
    ui->invertRamps->setChecked(candidate.invertRamps);
    ui->invertRamps->blockSignals(false);

    ui->deBits->blockSignals(true);
    ui->deBits->setValue(candidate.deBits);
    ui->deBits->blockSignals(false);
    ui->deBitsText->setText(tr("%1").arg(candidate.deBits));

    updateAbsoluteCenterFrequency();

    if (applySettingsNow) {
        applySettings();
    }
}

void MeshtasticDemodGUI::handleMeshAutoLockObservation(const MeshtasticDemodMsg::MsgReportDecodeBytes& msg)
{
    if (!m_meshAutoLockActive) {
        return;
    }

    if ((m_meshAutoLockCandidateIndex < 0) || (m_meshAutoLockCandidateIndex >= m_meshAutoLockCandidates.size())) {
        return;
    }

    MeshAutoLockCandidate& candidate = m_meshAutoLockCandidates[m_meshAutoLockCandidateIndex];
    const double snrDb = msg.getSingalDb() - msg.getNoiseDb();
    const double clippedSnr = std::max(-10.0, std::min(30.0, static_cast<double>(snrDb)));
    double score = clippedSnr * 0.2;

    const bool earlyEOM = msg.getEarlyEOM();
    if (earlyEOM) {
        score -= 8.0;
    } else {
        score += 3.0;
    }

    const int headerParityStatus = msg.getHeaderParityStatus();
    if (headerParityStatus == (int) MeshtasticDemodSettings::ParityOK) {
        score += 8.0;
    } else if (headerParityStatus == (int) MeshtasticDemodSettings::ParityCorrected) {
        score += 5.0;
    } else if (headerParityStatus == (int) MeshtasticDemodSettings::ParityError) {
        score -= 7.0;
    }

    const bool headerCRCStatus = msg.getHeaderCRCStatus();
    if (headerCRCStatus) {
        score += 10.0;
    } else {
        score -= 8.0;
    }

    const int payloadParityStatus = msg.getPayloadParityStatus();
    const bool payloadCRCStatus = msg.getPayloadCRCStatus();

    if (!earlyEOM)
    {
        if (payloadParityStatus == (int) MeshtasticDemodSettings::ParityOK) {
            score += 6.0;
        } else if (payloadParityStatus == (int) MeshtasticDemodSettings::ParityCorrected) {
            score += 3.0;
        } else if (payloadParityStatus == (int) MeshtasticDemodSettings::ParityError) {
            score -= 4.0;
        }

        if (payloadCRCStatus) {
            score += 12.0;
        } else {
            score -= 4.0;
        }
    }

    const bool syncWordZero = msg.getSyncWord() == 0x00;

    candidate.score += score;
    candidate.samples++;
    candidate.syncWordZeroCount += syncWordZero ? 1 : 0;
    candidate.headerParityOkOrFixCount += (headerParityStatus == (int) MeshtasticDemodSettings::ParityOK
        || headerParityStatus == (int) MeshtasticDemodSettings::ParityCorrected) ? 1 : 0;
    candidate.headerCRCCount += headerCRCStatus ? 1 : 0;
    candidate.payloadCRCCount += payloadCRCStatus ? 1 : 0;
    candidate.earlyEOMCount += earlyEOM ? 1 : 0;
    m_meshAutoLockObservedSamplesForCandidate++;
    m_meshAutoLockTotalDecodeSamples++;

    if (!earlyEOM && headerCRCStatus && payloadCRCStatus)
    {
        displayStatus(tr("MESH LOCK|strong lock found (HF/HC/CRC good), finishing scan"));
        stopMeshAutoLock(true);
    }
}

void MeshtasticDemodGUI::handleMeshAutoLockSourceObservation()
{
    if (!m_meshAutoLockActive) {
        return;
    }

    if ((m_meshAutoLockCandidateIndex < 0) || (m_meshAutoLockCandidateIndex >= m_meshAutoLockCandidates.size())) {
        return;
    }

    MeshAutoLockCandidate& candidate = m_meshAutoLockCandidates[m_meshAutoLockCandidateIndex];
    const double totalPower = std::max(1e-12, m_meshtasticDemod->getTotalPower());
    const double noisePower = std::max(1e-12, m_meshtasticDemod->getCurrentNoiseLevel());
    const double totalDb = CalcDb::dbPower(totalPower);
    const double noiseDb = CalcDb::dbPower(noisePower);
    const double p2nDb = std::max(-20.0, std::min(40.0, totalDb - noiseDb));
    const bool demodActive = m_meshtasticDemod->getDemodActive();

    if (!m_meshAutoLockTrafficSeen)
    {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const bool sourceActive = p2nDb >= kMeshAutoLockActivityP2NThresholdDb;

        if (sourceActive) {
            m_meshAutoLockActivityTicks++;
        }

        if (demodActive) {
            m_meshAutoLockActivityTicks++;
        }

        if (!sourceActive && !demodActive && (m_meshAutoLockActivityTicks > 0)) {
            m_meshAutoLockActivityTicks--;
        }

        if (m_meshAutoLockActivityTicks >= 3)
        {
            m_meshAutoLockTrafficSeen = true;
            m_meshAutoLockCandidateStartMs = nowMs;
            m_meshAutoLockObservedSamplesForCandidate = 0;
            m_meshAutoLockObservedSourceSamplesForCandidate = 0;
            displayStatus(tr("MESH LOCK|traffic detected, starting scan"));
        }
        else if ((m_meshAutoLockArmStartMs > 0) && ((nowMs - m_meshAutoLockArmStartMs) >= kMeshAutoLockArmTimeoutMs))
        {
            m_meshAutoLockTrafficSeen = true;
            m_meshAutoLockCandidateStartMs = nowMs;
            m_meshAutoLockObservedSamplesForCandidate = 0;
            m_meshAutoLockObservedSourceSamplesForCandidate = 0;
            displayStatus(tr("MESH LOCK|no clear activity detected. Starting scan anyway."));
        }
        else
        {
            return;
        }
    }

    // Source-only quality proxy:
    // - prefer sustained demod activity
    // - prefer clearer power/noise separation
    double sourceScore = (demodActive ? 0.7 : -0.1) + (p2nDb * 0.03);
    if (p2nDb < 1.0) {
        sourceScore -= 0.3;
    }

    candidate.sourceScore += sourceScore;
    candidate.sourceSamples++;
    m_meshAutoLockObservedSourceSamplesForCandidate++;
}

void MeshtasticDemodGUI::advanceMeshAutoLock()
{
    if (!m_meshAutoLockActive) {
        return;
    }

    if (!m_meshAutoLockTrafficSeen) {
        return;
    }

    if ((m_meshAutoLockCandidateIndex < 0) || (m_meshAutoLockCandidateIndex >= m_meshAutoLockCandidates.size()))
    {
        stopMeshAutoLock(true);
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const bool enoughObservations = m_meshAutoLockObservedSamplesForCandidate >= kMeshAutoLockMinObservationsPerCandidate;
    const bool timedOut = (nowMs - m_meshAutoLockCandidateStartMs) >= kMeshAutoLockCandidateTimeoutMs;

    if (m_meshAutoLockTotalDecodeSamples == 0)
    {
        int sourceCount = 0;
        double minSourceAvg = std::numeric_limits<double>::infinity();
        double maxSourceAvg = -std::numeric_limits<double>::infinity();

        for (const MeshAutoLockCandidate& candidate : m_meshAutoLockCandidates)
        {
            if (candidate.sourceSamples < kMeshAutoLockMinSourceObservationsPerCandidate) {
                continue;
            }

            const double sourceAvg = candidate.sourceScore / candidate.sourceSamples;
            minSourceAvg = std::min(minSourceAvg, sourceAvg);
            maxSourceAvg = std::max(maxSourceAvg, sourceAvg);
            sourceCount++;
        }

        // Source-only scoring is not discriminative: avoid sweeping every candidate and restore baseline.
        if ((sourceCount >= 6) && ((maxSourceAvg - minSourceAvg) < 0.15))
        {
            displayStatus(tr("MESH LOCK|source-only signal is flat/inconclusive. Stopping early."));
            stopMeshAutoLock(false);
            return;
        }
    }

    // Move to next candidate only if we have decode evidence or dwell timeout elapsed.
    if (!enoughObservations && !timedOut) {
        return;
    }

    m_meshAutoLockCandidateIndex++;
    m_meshAutoLockObservedSamplesForCandidate = 0;
    m_meshAutoLockObservedSourceSamplesForCandidate = 0;
    m_meshAutoLockCandidateStartMs = nowMs;

    if (m_meshAutoLockCandidateIndex >= m_meshAutoLockCandidates.size())
    {
        stopMeshAutoLock(true);
        return;
    }

    const MeshAutoLockCandidate& candidate = m_meshAutoLockCandidates[m_meshAutoLockCandidateIndex];
    applyMeshAutoLockCandidate(candidate, true);

    displayStatus(tr("MESH LOCK|candidate %1/%2 df=%3Hz inv=%4 de=%5")
        .arg(m_meshAutoLockCandidateIndex + 1)
        .arg(m_meshAutoLockCandidates.size())
        .arg(candidate.inputOffsetHz)
        .arg(candidate.invertRamps ? "on" : "off")
        .arg(candidate.deBits));
}

void MeshtasticDemodGUI::editMeshtasticKeys()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Meshtastic Keys"));
    dialog.resize(760, 460);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    QLabel* helpLabel = new QLabel(tr(
        "One key entry per line (or comma/semicolon separated).\n"
        "Formats: default, none, simple1..simple10, hex:<hex>, b64:<base64>, base64:<base64>, raw hex, raw base64.\n"
        "Optional channel mapping: channelName=keySpec (example: LongFast=default)."),
        &dialog);
    helpLabel->setWordWrap(true);
    layout->addWidget(helpLabel);

    QTextEdit* keyEditor = new QTextEdit(&dialog);
    keyEditor->setPlainText(m_settings.m_meshtasticKeySpecList);
    keyEditor->setToolTip(tr("Enter one or more key specs used to decrypt Meshtastic packets."));
    keyEditor->setPlaceholderText("LongFast=default\nnone\nLongSlow=hex:00112233445566778899aabbccddeeff");
    layout->addWidget(keyEditor, 1);

    QHBoxLayout* statusLayout = new QHBoxLayout();
    QPushButton* validateButton = new QPushButton(tr("Validate"), &dialog);
    validateButton->setToolTip(tr("Validate key syntax and count without saving."));
    QLabel* statusLabel = new QLabel(&dialog);
    statusLabel->setToolTip(tr("Validation status for the current key list."));
    statusLabel->setWordWrap(true);
    statusLayout->addWidget(validateButton);
    statusLayout->addWidget(statusLabel, 1);
    layout->addLayout(statusLayout);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);

    auto validateInput = [keyEditor, statusLabel]() -> bool {
        const QString keyText = keyEditor->toPlainText().trimmed();

        if (keyText.isEmpty())
        {
            statusLabel->setStyleSheet("QLabel { color: #bbbbbb; }");
            statusLabel->setText(QObject::tr("No custom keys set. Decoder will use environment/default keys."));
            return true;
        }

        QString error;
        int keyCount = 0;

        if (!modemmeshtastic::Packet::validateKeySpecList(keyText, error, &keyCount))
        {
            statusLabel->setStyleSheet("QLabel { color: #ff5555; }");
            statusLabel->setText(QObject::tr("Invalid key list: %1").arg(error));
            return false;
        }

        statusLabel->setStyleSheet("QLabel { color: #7cd67c; }");
        statusLabel->setText(QObject::tr("Valid: %1 key(s) parsed").arg(keyCount));
        return true;
    };

    QObject::connect(validateButton, &QPushButton::clicked, &dialog, [validateInput]() {
        validateInput();
    });

    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [this, &dialog, keyEditor, validateInput]() {
        if (!validateInput())
        {
            QMessageBox::warning(this, tr("Invalid Keys"), tr("Fix the Meshtastic key list before saving."));
            return;
        }

        m_settings.m_meshtasticKeySpecList = keyEditor->toPlainText().trimmed();

        if (m_meshKeysButton)
        {
            const bool hasCustomKeys = !m_settings.m_meshtasticKeySpecList.isEmpty();
            m_meshKeysButton->setText(hasCustomKeys ? tr("Keys*") : tr("Keys..."));
            m_meshKeysButton->setToolTip(hasCustomKeys ?
                tr("Custom Meshtastic decode keys configured. Click to edit.") :
                tr("Open Meshtastic key manager."));
        }

        applySettings();

        if (m_settings.m_meshtasticKeySpecList.isEmpty()) {
            displayStatus(tr("MESH KEYS|using environment/default key set"));
        } else {
            displayStatus(tr("MESH KEYS|custom key set saved"));
        }

        dialog.accept();
    });

    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    validateInput();
    dialog.exec();
}

int MeshtasticDemodGUI::findBandwidthIndex(int bandwidthHz) const
{
    int bestIndex = -1;
    int bestDelta = 1 << 30;

    for (int i = 0; i < MeshtasticDemodSettings::nbBandwidths; ++i)
    {
        const int delta = std::abs(MeshtasticDemodSettings::bandwidths[i] - bandwidthHz);
        if (delta < bestDelta)
        {
            bestDelta = delta;
            bestIndex = i;
        }
    }

    return bestIndex;
}

bool MeshtasticDemodGUI::retuneDeviceToFrequency(qint64 centerFrequencyHz)
{
    if (!m_deviceUISet || !m_deviceUISet->m_deviceAPI) {
        return false;
    }

    DeviceAPI* deviceAPI = m_deviceUISet->m_deviceAPI;

    if (deviceAPI->getDeviceSourceEngine() && deviceAPI->getSampleSource())
    {
        deviceAPI->getSampleSource()->setCenterFrequency(centerFrequencyHz);
        return true;
    }

    if (deviceAPI->getDeviceMIMOEngine() && deviceAPI->getSampleMIMO())
    {
        deviceAPI->getSampleMIMO()->setSourceCenterFrequency(centerFrequencyHz, m_settings.m_streamIndex);
        return true;
    }

    return false;
}

bool MeshtasticDemodGUI::autoTuneDeviceSampleRateForBandwidth(int bandwidthHz, QString& summary)
{
    summary.clear();

    if (!m_meshtasticDemod) {
        return false;
    }

    const int deviceSetIndex = m_meshtasticDemod->getDeviceSetIndex();

    if (deviceSetIndex < 0) {
        return false;
    }

    int devSampleRate = 0;
    int log2Decim = 0;
    QString sourceProtocol;

    if (!ChannelWebAPIUtils::getDevSampleRate(deviceSetIndex, devSampleRate)
        || !ChannelWebAPIUtils::getSoftDecim(deviceSetIndex, log2Decim))
    {
        summary = "auto sample-rate control: unsupported by source";
        return false;
    }

    if (devSampleRate <= 0) {
        summary = "auto sample-rate control: invalid device sample-rate";
        return false;
    }

    if (log2Decim < 0) {
        log2Decim = 0;
    }

    const bool hasSourceProtocol = ChannelWebAPIUtils::getDeviceSetting(deviceSetIndex, "protocol", sourceProtocol);
    const bool isSpyServerProtocol = hasSourceProtocol && (sourceProtocol.compare("Spy Server", Qt::CaseInsensitive) == 0);
    const int initialDevSampleRate = devSampleRate;
    const int initialLog2Decim = log2Decim;
    const int minEffectiveRate = std::max(500000, bandwidthHz * 4); // Keep margin above 2*BW for robust LoRa decode.

    int newLog2Decim = log2Decim;
    const int maxLog2Decim = 16; // Practical upper bound for software decimation controls.

    // If current decimation undershoots the required effective rate, lower it first.
    while ((newLog2Decim > 0) && ((devSampleRate >> newLog2Decim) < minEffectiveRate)) {
        newLog2Decim--;
    }

    // Then push decimation as high as possible while keeping enough effective sample-rate.
    while ((newLog2Decim < maxLog2Decim) && ((devSampleRate >> (newLog2Decim + 1)) >= minEffectiveRate)) {
        newLog2Decim++;
    }

    if (newLog2Decim != log2Decim)
    {
        if (!ChannelWebAPIUtils::setSoftDecim(deviceSetIndex, newLog2Decim)) {
            newLog2Decim = log2Decim;
        }
    }

    if ((devSampleRate >> newLog2Decim) < minEffectiveRate && !isSpyServerProtocol)
    {
        const qint64 requiredDevRate = static_cast<qint64>(minEffectiveRate) << newLog2Decim;

        if ((requiredDevRate > 0) && (requiredDevRate <= std::numeric_limits<int>::max()))
        {
            ChannelWebAPIUtils::setDevSampleRate(deviceSetIndex, static_cast<int>(requiredDevRate));
        }
    }

    int finalDevSampleRate = devSampleRate;
    int finalLog2Decim = newLog2Decim;
    ChannelWebAPIUtils::getDevSampleRate(deviceSetIndex, finalDevSampleRate);
    ChannelWebAPIUtils::getSoftDecim(deviceSetIndex, finalLog2Decim);
    if (finalLog2Decim < 0) {
        finalLog2Decim = 0;
    }

    int finalEffectiveRate = finalDevSampleRate >> finalLog2Decim;
    bool channelSampleRateSynced = false;
    bool channelDecimationDisabled = false;
    bool dcBlockSupported = false;
    bool iqCorrectionSupported = false;
    bool agcSupported = false;
    bool dcBlockEnabled = false;
    bool iqCorrectionEnabled = false;
    bool agcEnabled = false;
    bool dcBlockApplied = false;
    bool iqCorrectionApplied = false;
    bool agcApplied = false;
    int channelSampleRate = 0;

    // Some sources (for example RemoteTCPInput) need channel sample-rate to be patched
    // explicitly when decimation/sample-rate changes over WebAPI.
    if (ChannelWebAPIUtils::getDeviceSetting(deviceSetIndex, "channelSampleRate", channelSampleRate))
    {
        int channelDecimation = 0;

        if (ChannelWebAPIUtils::getDeviceSetting(deviceSetIndex, "channelDecimation", channelDecimation) && (channelDecimation != 0)) {
            channelDecimationDisabled = ChannelWebAPIUtils::patchDeviceSetting(deviceSetIndex, "channelDecimation", 0);
        }

        if (channelSampleRate != finalEffectiveRate)
        {
            channelSampleRateSynced = ChannelWebAPIUtils::patchDeviceSetting(deviceSetIndex, "channelSampleRate", finalEffectiveRate);
            if (channelSampleRateSynced) {
                channelSampleRate = finalEffectiveRate;
            }
        }

        finalEffectiveRate = channelSampleRate;
    }

    // Input-quality autotune for sources exposing these keys (e.g. RemoteTCPInput).
    int settingValue = 0;

    if (ChannelWebAPIUtils::getDeviceSetting(deviceSetIndex, "dcBlock", settingValue))
    {
        dcBlockSupported = true;
        if (settingValue == 0) {
            dcBlockApplied = ChannelWebAPIUtils::patchDeviceSetting(deviceSetIndex, "dcBlock", 1);
            dcBlockEnabled = dcBlockApplied;
        } else {
            dcBlockEnabled = true;
        }
    }

    if (ChannelWebAPIUtils::getDeviceSetting(deviceSetIndex, "iqCorrection", settingValue))
    {
        iqCorrectionSupported = true;
        if (settingValue == 0) {
            iqCorrectionApplied = ChannelWebAPIUtils::patchDeviceSetting(deviceSetIndex, "iqCorrection", 1);
            iqCorrectionEnabled = iqCorrectionApplied;
        } else {
            iqCorrectionEnabled = true;
        }
    }

    if (!isSpyServerProtocol && ChannelWebAPIUtils::getDeviceSetting(deviceSetIndex, "agc", settingValue))
    {
        agcSupported = true;
        if (settingValue == 0) {
            agcApplied = ChannelWebAPIUtils::patchDeviceSetting(deviceSetIndex, "agc", 1);
            agcEnabled = agcApplied;
        } else {
            agcEnabled = true;
        }
    }

    const bool belowTarget = finalEffectiveRate < minEffectiveRate;
    const bool changed = (finalDevSampleRate != initialDevSampleRate)
        || (finalLog2Decim != initialLog2Decim)
        || channelSampleRateSynced
        || channelDecimationDisabled
        || dcBlockApplied
        || iqCorrectionApplied
        || agcApplied;

    summary = QString("effective sample-rate=%1Hz device sample-rate=%2Hz decimation=2^%3 required minimum=%4Hz%5")
        .arg(finalEffectiveRate)
        .arg(finalDevSampleRate)
        .arg(finalLog2Decim)
        .arg(minEffectiveRate)
        .arg(belowTarget ? " (below target)" : "");

    if (isSpyServerProtocol) {
        summary += " source=SpyServer(fixed dev sample-rate)";
    }
    if (channelDecimationDisabled || channelSampleRateSynced) {
        summary += " channel sample-rate synced";
    }
    summary += QString(" dcBlock=%1 iqCorrection=%2 agc=%3")
        .arg(dcBlockSupported ? (dcBlockEnabled ? "on" : "off") : "n/a")
        .arg(iqCorrectionSupported ? (iqCorrectionEnabled ? "on" : "off") : "n/a")
        .arg(agcSupported ? (agcEnabled ? "on" : "off") : (isSpyServerProtocol ? "n/a(SpyServer)" : "n/a"));

    return changed;
}

void MeshtasticDemodGUI::applyMeshtasticProfileFromSelection()
{
    if (!m_meshRegionCombo || !m_meshPresetCombo || !m_meshChannelCombo) {
        return;
    }

    const QString region = m_meshRegionCombo->currentData().toString();
    const QString preset = m_meshPresetCombo->currentData().toString();
    const int meshChannel = m_meshChannelCombo->currentData().toInt();
    const int channelNum = meshChannel + 1; // planner expects 1-based channel_num

    if (region.isEmpty() || preset.isEmpty()) {
        return;
    }

    const QString command = QString("MESH:preset=%1;region=%2;channel_num=%3").arg(preset, region).arg(channelNum);
    modemmeshtastic::TxRadioSettings meshRadio;
    QString error;

    if (!modemmeshtastic::Packet::deriveTxRadioSettings(command, meshRadio, error))
    {
        qWarning() << "MeshtasticDemodGUI::applyMeshtasticProfileFromSelection:" << error;
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

    if (meshRadio.hasCenterFrequency)
    {
        if (retuneDeviceToFrequency(meshRadio.centerFrequencyHz))
        {
            m_deviceCenterFrequency = meshRadio.centerFrequencyHz;
            if (m_settings.m_inputFrequencyOffset != 0)
            {
                m_settings.m_inputFrequencyOffset = 0;
                changed = true;
            }
        }
        else if (m_deviceCenterFrequency != 0)
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
            qWarning() << "MeshtasticDemodGUI::applyMeshtasticProfileFromSelection: cannot retune device and device center frequency unknown";
        }
    }

    const int thisBW = MeshtasticDemodSettings::bandwidths[m_settings.m_bandwidthIndex];
    QString sampleRateSummary;
    bool sampleRateChanged = false;

    if (m_settings.m_meshtasticAutoSampleRate) {
        sampleRateChanged = autoTuneDeviceSampleRateForBandwidth(thisBW, sampleRateSummary);
    } else {
        sampleRateSummary = "auto sample-rate control: disabled";
    }

    if (!changed && !sampleRateChanged && !selectionStateChanged) {
        return;
    }

    qInfo() << "MeshtasticDemodGUI::applyMeshtasticProfileFromSelection:" << meshRadio.summary
            << sampleRateSummary;

    QString status = tr("MESH CFG|region=%1 preset=%2 ch=%3 %4")
        .arg(region)
        .arg(preset)
        .arg(meshChannel)
        .arg(meshRadio.summary);

    status += QString(" preamble=%1").arg(meshPreambleChirps);

    if (!sampleRateSummary.isEmpty()) {
        status += " " + sampleRateSummary;
    }

    if (!changed)
    {
        applySettings();
        displayStatus(status);
        return;
    }

    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(thisBW);
    m_channelMarker.blockSignals(false);

    blockApplySettings(true);
    ui->deltaFrequency->setValue(m_settings.m_inputFrequencyOffset);
    ui->BW->setValue(m_settings.m_bandwidthIndex);
    ui->BWText->setText(QString("%1 Hz").arg(thisBW));
    ui->Spread->setValue(m_settings.m_spreadFactor);
    ui->SpreadText->setText(tr("%1").arg(m_settings.m_spreadFactor));
    ui->deBits->setValue(m_settings.m_deBits);
    ui->deBitsText->setText(tr("%1").arg(m_settings.m_deBits));
    ui->preambleChirps->setValue(m_settings.m_preambleChirps);
    ui->preambleChirpsText->setText(tr("%1").arg(m_settings.m_preambleChirps));
    ui->header->setChecked(m_settings.m_hasHeader);
    ui->fecParity->setValue(m_settings.m_nbParityBits);
    ui->fecParityText->setText(tr("%1").arg(m_settings.m_nbParityBits));
    ui->crc->setChecked(m_settings.m_hasCRC);
    blockApplySettings(false);
    updateControlAvailabilityHints();

    ui->glSpectrum->setSampleRate(thisBW);
    ui->glSpectrum->setCenterFrequency(thisBW/2);

    updateAbsoluteCenterFrequency();
    applySettings();
    displayStatus(status);
}

void MeshtasticDemodGUI::setupMeshtasticAutoProfileControls()
{
    QHBoxLayout* meshLayout = new QHBoxLayout();
    meshLayout->setSpacing(2);

    QLabel* regionLabel = new QLabel("Region", this);
    regionLabel->setToolTip("Meshtastic region (defines allowed frequency band)");
    m_meshRegionCombo = new QComboBox(this);
    m_meshRegionCombo->setToolTip("Meshtastic region. Combined with preset/channel to auto-apply LoRa receive parameters.");
    m_meshRegionCombo->addItem("US", "US");
    m_meshRegionCombo->addItem("EU_433", "EU_433");
    m_meshRegionCombo->addItem("EU_868", "EU_868");
    m_meshRegionCombo->addItem("ANZ", "ANZ");
    m_meshRegionCombo->addItem("JP", "JP");
    m_meshRegionCombo->addItem("CN", "CN");
    m_meshRegionCombo->addItem("KR", "KR");
    m_meshRegionCombo->addItem("TW", "TW");
    m_meshRegionCombo->addItem("IN", "IN");
    m_meshRegionCombo->addItem("TH", "TH");
    m_meshRegionCombo->addItem("BR_902", "BR_902");
    m_meshRegionCombo->addItem("LORA_24", "LORA_24");

    QLabel* presetLabel = new QLabel("Preset", this);
    presetLabel->setToolTip("Meshtastic modem preset (LongFast, MediumSlow, ...)");
    m_meshPresetCombo = new QComboBox(this);
    m_meshPresetCombo->setToolTip("Meshtastic modem preset. Applies LoRa BW/SF/CR/DE and header/CRC expectations.");
    m_meshPresetCombo->addItem("LONG_FAST", "LONG_FAST");
    m_meshPresetCombo->addItem("LONG_SLOW", "LONG_SLOW");
    m_meshPresetCombo->addItem("LONG_MODERATE", "LONG_MODERATE");
    m_meshPresetCombo->addItem("LONG_TURBO", "LONG_TURBO");
    m_meshPresetCombo->addItem("MEDIUM_FAST", "MEDIUM_FAST");
    m_meshPresetCombo->addItem("MEDIUM_SLOW", "MEDIUM_SLOW");
    m_meshPresetCombo->addItem("SHORT_FAST", "SHORT_FAST");
    m_meshPresetCombo->addItem("SHORT_SLOW", "SHORT_SLOW");
    m_meshPresetCombo->addItem("SHORT_TURBO", "SHORT_TURBO");

    QLabel* channelLabel = new QLabel("Channel", this);
    channelLabel->setToolTip("Meshtastic channel number (zero-based)");
    m_meshChannelCombo = new QComboBox(this);
    m_meshChannelCombo->setToolTip("Meshtastic channel number (zero-based, shown with center frequency)");
    m_meshApplyButton = new QPushButton("Apply", this);
    m_meshApplyButton->setToolTip("Apply the currently selected Meshtastic region/preset/channel profile now.");
    m_meshKeysButton = new QPushButton("Keys...", this);
    m_meshKeysButton->setToolTip("Open key manager to configure Meshtastic decryption keys (hex/base64/default/simple).");
    m_meshAutoLockButton = new QPushButton("Auto Lock", this);
    m_meshAutoLockButton->setCheckable(true);
    m_meshAutoLockButton->setToolTip(
        "Scan Invert + frequency offset candidates and keep the best lock.\n"
        "Arms and waits for on-air activity, then scans candidates.\n"
        "Scores using decode quality plus source-side intensity (demod activity and power/noise).");
    m_meshAutoSampleRateCheck = new QCheckBox("Auto Input Tune", this);
    m_meshAutoSampleRateCheck->setChecked(m_settings.m_meshtasticAutoSampleRate);
    m_meshAutoSampleRateCheck->setToolTip(
        "Automatically tune source parameters for the selected LoRa profile.\n"
        "Includes sample-rate/decimation and, where supported, dcBlock/iqCorrection/agc.");

    meshLayout->addWidget(regionLabel);
    meshLayout->addWidget(m_meshRegionCombo, 1);
    meshLayout->addWidget(presetLabel);
    meshLayout->addWidget(m_meshPresetCombo, 1);
    meshLayout->addWidget(channelLabel);
    meshLayout->addWidget(m_meshChannelCombo);
    meshLayout->addWidget(m_meshApplyButton);
    meshLayout->addWidget(m_meshKeysButton);
    meshLayout->addWidget(m_meshAutoLockButton);
    meshLayout->addWidget(m_meshAutoSampleRateCheck);

    ui->payloadLayout->insertLayout(0, meshLayout);

    QObject::connect(m_meshRegionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MeshtasticDemodGUI::on_meshRegion_currentIndexChanged);
    QObject::connect(m_meshPresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MeshtasticDemodGUI::on_meshPreset_currentIndexChanged);
    QObject::connect(m_meshChannelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MeshtasticDemodGUI::on_meshChannel_currentIndexChanged);
    QObject::connect(m_meshApplyButton, &QPushButton::clicked, this, &MeshtasticDemodGUI::on_meshApply_clicked);
    QObject::connect(m_meshKeysButton, &QPushButton::clicked, this, &MeshtasticDemodGUI::on_meshKeys_clicked);
    QObject::connect(m_meshAutoLockButton, &QPushButton::clicked, this, &MeshtasticDemodGUI::on_meshAutoLock_clicked);
    QObject::connect(m_meshAutoSampleRateCheck, &QCheckBox::toggled, this, &MeshtasticDemodGUI::on_meshAutoSampleRate_toggled);

    rebuildMeshtasticChannelOptions();
}

void MeshtasticDemodGUI::rebuildMeshtasticChannelOptions()
{
    if (!m_meshRegionCombo || !m_meshPresetCombo || !m_meshChannelCombo) {
        return;
    }

    const QString region = m_meshRegionCombo->currentData().toString();
    const QString preset = m_meshPresetCombo->currentData().toString();
    const int previousChannel = m_meshChannelCombo->currentData().toInt();

    m_meshControlsUpdating = true;
    m_meshChannelCombo->clear();

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

        m_meshChannelCombo->addItem(label, meshChannel);
        added++;
    }

    if (added == 0) {
        m_meshChannelCombo->addItem("0", 0);
    }

    m_meshChannelCombo->setToolTip(tr("Meshtastic channel number (%1 available for %2/%3)")
        .arg(added)
        .arg(region)
        .arg(preset));
    int restoreIndex = m_meshChannelCombo->findData(previousChannel);
    if (restoreIndex < 0) {
        restoreIndex = 0;
    }
    m_meshChannelCombo->setCurrentIndex(restoreIndex);
    m_meshControlsUpdating = false;

    qInfo() << "MeshtasticDemodGUI::rebuildMeshtasticChannelOptions:"
            << "region=" << region
            << "preset=" << preset
            << "channels=" << added;

    // Ensure the rebuilt combo state is actually applied, even when the rebuild
    // was triggered from code paths where index-change handlers are suppressed.
    QMetaObject::invokeMethod(this, [this]() {
        if (!m_meshControlsUpdating) {
            applyMeshtasticProfileFromSelection();
        }
    }, Qt::QueuedConnection);
}

void MeshtasticDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void MeshtasticDemodGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_meshtasticDemod->getNumberOfDeviceStreams());
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

MeshtasticDemodGUI::MeshtasticDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
	ChannelGUI(parent),
	ui(new Ui::MeshtasticDemodGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(250000),
	m_doApplySettings(true),
    m_meshRegionCombo(nullptr),
    m_meshPresetCombo(nullptr),
    m_meshChannelCombo(nullptr),
    m_meshApplyButton(nullptr),
    m_meshKeysButton(nullptr),
    m_meshAutoLockButton(nullptr),
    m_dechirpLiveFollowButton(nullptr),
    m_meshAutoSampleRateCheck(nullptr),
    m_pipelineTabs(nullptr),
    m_meshControlsUpdating(false),
    m_meshAutoLockActive(false),
    m_meshAutoLockCandidateIndex(0),
    m_meshAutoLockCandidateStartMs(0),
    m_meshAutoLockObservedSamplesForCandidate(0),
    m_meshAutoLockObservedSourceSamplesForCandidate(0),
    m_meshAutoLockTotalDecodeSamples(0),
    m_meshAutoLockTrafficSeen(false),
    m_meshAutoLockActivityTicks(0),
    m_meshAutoLockArmStartMs(0),
    m_meshAutoLockBaseOffsetHz(0),
    m_meshAutoLockBaseInvert(false),
    m_meshAutoLockBaseDeBits(0),
    m_remoteTcpReconnectAutoApplyPending(false),
    m_remoteTcpReconnectAutoApplyWaitTicks(0),
    m_remoteTcpLastRunningState(false),
    m_dechirpInspectionActive(false),
    m_replayPendingHasSelection(false),
    m_replaySelectionQueued(false),
    m_pipelineMessageSequence(0),
    m_tickCount(0)
{
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodmeshtastic/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setupMeshtasticAutoProfileControls();
    setupPipelineViews();
    if (ui->messageLayout)
    {
        m_dechirpLiveFollowButton = new QPushButton(tr("Live"), this);
        m_dechirpLiveFollowButton->setAutoDefault(false);
        m_dechirpLiveFollowButton->setMaximumSize(QSize(42, 24));
        m_dechirpLiveFollowButton->setToolTip(tr("Return de-chirped spectrum to live follow mode."));
        ui->messageLayout->insertWidget(2, m_dechirpLiveFollowButton);
        QObject::connect(m_dechirpLiveFollowButton, &QPushButton::clicked, this, [this](bool) {
            setDechirpInspectionMode(false);
        });
    }
    updateDechirpModeUI();
    // Mark major sections as vertically expanding so RollupContents does not clamp max height.
    ui->verticalLayoutWidget_2->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    ui->spectrumContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    if (m_pipelineTabs) {
        m_pipelineTabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    } else {
        ui->messageText->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_meshtasticDemod = (MeshtasticDemod*) rxChannel;
    m_spectrumVis = m_meshtasticDemod->getSpectrumVis();
	m_spectrumVis->setGLSpectrum(ui->glSpectrum);
    m_meshtasticDemod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    SpectrumSettings spectrumSettings = m_spectrumVis->getSettings();
    // Meshtastic dechirp view defaults: keep lower pane active so replay-on-click
    // always has a visible target.
    spectrumSettings.m_displayWaterfall = true;
    spectrumSettings.m_display3DSpectrogram = false;
    spectrumSettings.m_displayCurrent = true;
    spectrumSettings.m_displayHistogram = false;
    spectrumSettings.m_displayMaxHold = false;
    spectrumSettings.m_averagingMode = SpectrumSettings::AvgModeNone;
    spectrumSettings.m_refLevel = -10.0f;
    spectrumSettings.m_powerRange = 45.0f;
    SpectrumVis::MsgConfigureSpectrumVis *msg = SpectrumVis::MsgConfigureSpectrumVis::create(spectrumSettings, false);
    m_spectrumVis->getInputMessageQueue()->push(msg);

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->deltaFrequency->setToolTip(tr("Offset from device center frequency (Hz)."));
    ui->deltaFrequencyLabel->setToolTip(tr("Frequency offset control for the demodulator channel."));
    ui->deltaUnits->setToolTip(tr("Frequency unit for the offset control."));
    ui->BW->setToolTip(tr("LoRa bandwidth selection. Meshtastic presets auto-set this."));
    ui->bwLabel->setToolTip(tr("LoRa bandwidth selector."));
    ui->BWText->setToolTip(tr("Current LoRa bandwidth in Hz."));
    ui->Spread->setToolTip(tr("LoRa spreading factor (SF). Higher SF increases range but lowers rate."));
    ui->spreadLabel->setToolTip(tr("LoRa spreading factor selector."));
    ui->SpreadText->setToolTip(tr("Current spreading factor value."));
    ui->deBits->setToolTip(tr("Low data-rate optimization bits (DE)."));
    ui->deBitsLabel->setToolTip(tr("Low data-rate optimization setting."));
    ui->deBitsText->setToolTip(tr("Current low data-rate optimization value."));
    ui->fftWindow->setToolTip(tr("FFT window used by the LoRa de-chirping stage."));
    ui->fftWindowLabel->setToolTip(tr("FFT window function."));
    ui->preambleChirps->setToolTip(tr("Expected LoRa preamble chirp count. Meshtastic profiles default to 17 (sub-GHz) or 12 (2.4 GHz)."));
    ui->preambleChirpsLabel->setToolTip(tr("Expected LoRa preamble length in chirps."));
    ui->preambleChirpsText->setToolTip(tr("Current preamble chirp value."));
    ui->mute->setToolTip(tr("Disable decoder output."));
    ui->clear->setToolTip(tr("Clear decoded message log."));
    ui->eomSquelch->setToolTip(tr("End-of-message squelch threshold."));
    ui->eomSquelchLabel->setToolTip(tr("End-of-message squelch level."));
    ui->eomSquelchText->setToolTip(tr("Current end-of-message squelch value."));
    ui->messageLength->setToolTip(tr("Maximum payload symbol length when auto is disabled."));
    ui->messageLengthAuto->setToolTip(tr("Auto-detect payload symbol length from headers."));
    ui->messageLengthLabel->setToolTip(tr("Maximum payload symbol length."));
    ui->messageLengthText->setToolTip(tr("Current payload symbol length setting."));
    ui->header->setToolTip(tr("Assume explicit LoRa header mode."));
    ui->fecParity->setToolTip(tr("LoRa coding rate parity denominator (CR)."));
    ui->fecParityLabel->setToolTip(tr("LoRa coding rate parity setting."));
    ui->fecParityText->setToolTip(tr("Current coding rate parity value."));
    ui->crc->setToolTip(tr("Expect payload CRC."));
    ui->packetLength->setToolTip(tr("Fixed packet length for implicit-header mode."));
    ui->packetLengthLabel->setToolTip(tr("Fixed packet length for implicit header mode."));
    ui->packetLengthText->setToolTip(tr("Current fixed packet length."));
    ui->invertRamps->setToolTip(tr("Invert chirp ramp direction."));
    ui->messageText->setToolTip(tr("Decoded packet and status log."));
    ui->messageLabel->setToolTip(tr("Decoded output area."));
    ui->udpSend->setToolTip(tr("Forward decoded payload bytes to UDP."));
    ui->udpAddress->setToolTip(tr("Destination UDP address for forwarded payloads."));
    ui->udpPort->setToolTip(tr("Destination UDP port for forwarded payloads."));
    ui->udpSeparator->setToolTip(tr("UDP forwarding controls."));
    ui->glSpectrum->setToolTip(tr("De-chirped spectrum view of the selected LoRa channel."));
    ui->spectrumGUI->setToolTip(tr("Spectrum and waterfall display controls."));
    ui->headerHammingStatus->setToolTip(tr("Header FEC status indicator."));
    ui->headerCRCStatus->setToolTip(tr("Header CRC status indicator."));
    ui->payloadFECStatus->setToolTip(tr("Payload FEC status indicator."));
    ui->payloadCRCStatus->setToolTip(tr("Payload CRC status indicator."));
    ui->channelPower->setToolTip(tr("Estimated channel power."));
    ui->nLabel->setToolTip(tr("Estimated symbol count."));
    ui->nText->setToolTip(tr("Current estimated symbol count."));
    ui->nbSymbolsText->setToolTip(tr("Current raw LoRa symbol counter."));
    ui->nbCodewordsText->setToolTip(tr("Current raw LoRa codeword counter."));
    ui->sLabel->setToolTip(tr("Estimated codeword count."));
    ui->sText->setToolTip(tr("Current estimated codeword count."));
    ui->snrLabel->setToolTip(tr("Estimated signal-to-noise ratio."));
    ui->snrText->setToolTip(tr("Current estimated SNR."));
    ui->sUnits->setToolTip(tr("Unit for SNR."));
    ui->loraLabel->setToolTip(tr("LoRa header/payload status indicators."));
    ui->symbolsCodewordsSeparator->setToolTip(tr("Separator between symbol and codeword counters."));

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
    // On first creation, combo signals haven't fired yet. Apply selected Meshtastic profile once.
    applyMeshtasticProfileFromSelection();
    DialPopup::addPopupsToChildDials(this);
    m_resizer.enableChildMouseTracking();
}

MeshtasticDemodGUI::~MeshtasticDemodGUI()
{
	delete ui;
}

void MeshtasticDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void MeshtasticDemodGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
        setTitleColor(m_channelMarker.getColor());
        MeshtasticDemod::MsgConfigureMeshtasticDemod* message = MeshtasticDemod::MsgConfigureMeshtasticDemod::create( m_settings, force);
        m_meshtasticDemod->getInputMessageQueue()->push(message);
	}
}

void MeshtasticDemodGUI::updateControlAvailabilityHints()
{
    const bool loRaMode = m_settings.m_codingScheme == MeshtasticDemodSettings::CodingLoRa;
    const bool explicitHeaderMode = loRaMode && m_settings.m_hasHeader;

    const QString fftWindowEnabledTip = tr("FFT window used by the de-chirping stage.");
    const QString fftWindowDisabledTip = tr("Ignored in LoRa mode. The LoRa demodulator uses a fixed internal FFT window.");
    ui->fftWindow->setEnabled(!loRaMode);
    ui->fftWindow->setToolTip(loRaMode ? fftWindowDisabledTip : fftWindowEnabledTip);
    ui->fftWindowLabel->setToolTip(loRaMode ? fftWindowDisabledTip : tr("FFT window function."));

    const QString messageLengthAutoEnabledTip = tr("Auto-detect payload symbol length from headers.");
    const QString messageLengthAutoDisabledTip = tr("Disabled in LoRa explicit-header mode. Payload length is decoded from the LoRa header.");
    ui->messageLengthAuto->setEnabled(!explicitHeaderMode);
    ui->messageLengthAuto->setToolTip(explicitHeaderMode ? messageLengthAutoDisabledTip : messageLengthAutoEnabledTip);

    const QString messageLengthDefaultTip = tr("Maximum payload symbol length when auto is disabled.");
    const QString messageLengthHeaderTip = tr("Maximum payload symbol clamp in LoRa explicit-header mode. Header still provides nominal payload length.");
    const QString messageLengthTip = explicitHeaderMode ? messageLengthHeaderTip : messageLengthDefaultTip;
    ui->messageLength->setToolTip(messageLengthTip);
    ui->messageLengthLabel->setToolTip(messageLengthTip);
    ui->messageLengthText->setToolTip(messageLengthTip);

    const bool headerControlsEnabled = !m_settings.m_hasHeader;
    ui->fecParity->setEnabled(headerControlsEnabled);
    ui->crc->setEnabled(headerControlsEnabled);
    ui->packetLength->setEnabled(headerControlsEnabled);

    const QString fecParityEnabledTip = tr("LoRa coding rate parity denominator (CR).");
    const QString fecParityDisabledTip = tr("Disabled in explicit-header mode. Coding rate is decoded from the LoRa header.");
    const QString fecParityTip = headerControlsEnabled ? fecParityEnabledTip : fecParityDisabledTip;
    ui->fecParity->setToolTip(fecParityTip);
    ui->fecParityLabel->setToolTip(fecParityTip);
    ui->fecParityText->setToolTip(fecParityTip);

    const QString crcEnabledTip = tr("Expect payload CRC.");
    const QString crcDisabledTip = tr("Disabled in explicit-header mode. CRC expectation is decoded from the LoRa header.");
    ui->crc->setToolTip(headerControlsEnabled ? crcEnabledTip : crcDisabledTip);

    const QString packetLengthEnabledTip = tr("Fixed packet length for implicit-header mode.");
    const QString packetLengthDisabledTip = tr("Disabled in explicit-header mode. Payload length is decoded from the LoRa header.");
    const QString packetLengthTip = headerControlsEnabled ? packetLengthEnabledTip : packetLengthDisabledTip;
    ui->packetLength->setToolTip(packetLengthTip);
    ui->packetLengthLabel->setToolTip(packetLengthTip);
    ui->packetLengthText->setToolTip(packetLengthTip);
}

void MeshtasticDemodGUI::displaySettings()
{
    int thisBW = MeshtasticDemodSettings::bandwidths[m_settings.m_bandwidthIndex];

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
    ui->invertRamps->setChecked(m_settings.m_invertRamps);

    displaySquelch();
    updateIndexLabel();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();

    if (m_meshKeysButton)
    {
        const bool hasCustomKeys = !m_settings.m_meshtasticKeySpecList.trimmed().isEmpty();
        m_meshKeysButton->setText(hasCustomKeys ? tr("Keys*") : tr("Keys..."));
        m_meshKeysButton->setToolTip(hasCustomKeys ?
            tr("Custom Meshtastic decode keys configured. Click to edit.") :
            tr("Open Meshtastic key manager."));
    }

    if (m_meshAutoSampleRateCheck)
    {
        m_meshControlsUpdating = true;
        m_meshAutoSampleRateCheck->setChecked(m_settings.m_meshtasticAutoSampleRate);
        m_meshControlsUpdating = false;
    }

    if (m_meshAutoLockButton)
    {
        m_meshAutoLockButton->blockSignals(true);
        m_meshAutoLockButton->setChecked(m_meshAutoLockActive);
        m_meshAutoLockButton->setText(m_meshAutoLockActive ? tr("Locking...") : tr("Auto Lock"));
        m_meshAutoLockButton->blockSignals(false);
    }

    if (m_meshRegionCombo && m_meshPresetCombo && m_meshChannelCombo)
    {
        m_meshControlsUpdating = true;

        int regionIndex = m_meshRegionCombo->findData(m_settings.m_meshtasticRegionCode);
        if (regionIndex < 0) {
            regionIndex = m_meshRegionCombo->findData("US");
        }
        if (regionIndex < 0) {
            regionIndex = 0;
        }
        m_meshRegionCombo->setCurrentIndex(regionIndex);

        int presetIndex = m_meshPresetCombo->findData(m_settings.m_meshtasticPresetName);
        if (presetIndex < 0) {
            presetIndex = m_meshPresetCombo->findData("LONG_FAST");
        }
        if (presetIndex < 0) {
            presetIndex = 0;
        }
        m_meshPresetCombo->setCurrentIndex(presetIndex);
        m_meshControlsUpdating = false;

        rebuildMeshtasticChannelOptions();

        m_meshControlsUpdating = true;
        int channelIndex = m_meshChannelCombo->findData(m_settings.m_meshtasticChannelIndex);
        if (channelIndex < 0) {
            channelIndex = 0;
        }
        m_meshChannelCombo->setCurrentIndex(channelIndex);
        m_meshControlsUpdating = false;
    }

    updateControlAvailabilityHints();
    blockApplySettings(false);
}

void MeshtasticDemodGUI::displaySquelch()
{
    ui->eomSquelch->setValue(m_settings.m_eomSquelchTenths);

    if (m_settings.m_eomSquelchTenths == ui->eomSquelch->maximum()) {
        ui->eomSquelchText->setText("---");
    } else {
        ui->eomSquelchText->setText(tr("%1").arg(m_settings.m_eomSquelchTenths / 10.0, 0, 'f', 1));
    }
}

void MeshtasticDemodGUI::displayLoRaStatus(int headerParityStatus, bool headerCRCStatus, int payloadParityStatus, bool payloadCRCStatus)
{
    if (m_settings.m_hasHeader && (headerParityStatus == (int) MeshtasticDemodSettings::ParityOK)) {
        ui->headerHammingStatus->setStyleSheet("QLabel { background-color : green; }");
    } else if (m_settings.m_hasHeader && (headerParityStatus == (int) MeshtasticDemodSettings::ParityError)) {
        ui->headerHammingStatus->setStyleSheet("QLabel { background-color : red; }");
    } else if (m_settings.m_hasHeader && (headerParityStatus == (int) MeshtasticDemodSettings::ParityCorrected)) {
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

    if (payloadParityStatus == (int) MeshtasticDemodSettings::ParityOK) {
        ui->payloadFECStatus->setStyleSheet("QLabel { background-color : green; }");
    } else if (payloadParityStatus == (int) MeshtasticDemodSettings::ParityError) {
        ui->payloadFECStatus->setStyleSheet("QLabel { background-color : red; }");
    } else if (payloadParityStatus == (int) MeshtasticDemodSettings::ParityCorrected) {
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

void MeshtasticDemodGUI::resetLoRaStatus()
{
    ui->headerHammingStatus->setStyleSheet("QLabel { background:rgb(79,79,79); }");
    ui->headerCRCStatus->setStyleSheet("QLabel { background:rgb(79,79,79); }");
    ui->payloadFECStatus->setStyleSheet("QLabel { background:rgb(79,79,79); }");
    ui->payloadCRCStatus->setStyleSheet("QLabel { background:rgb(79,79,79); }");
    ui->nbSymbolsText->setText("---");
    ui->nbCodewordsText->setText("---");
}

void MeshtasticDemodGUI::setBandwidths()
{
    int maxBandwidth = m_basebandSampleRate/MeshtasticDemodSettings::oversampling;
    int maxIndex = 0;

    for (; (maxIndex < MeshtasticDemodSettings::nbBandwidths) && (MeshtasticDemodSettings::bandwidths[maxIndex] <= maxBandwidth); maxIndex++)
    {}

    if (maxIndex != 0)
    {
        qDebug("MeshtasticDemodGUI::setBandwidths: avl: %d max: %d", maxBandwidth, MeshtasticDemodSettings::bandwidths[maxIndex-1]);
        ui->BW->setMaximum(maxIndex - 1);
        int index = ui->BW->value();
        ui->BWText->setText(QString("%1 Hz").arg(MeshtasticDemodSettings::bandwidths[index]));
    }
}

void MeshtasticDemodGUI::setupPipelineViews()
{
    if (!ui->textLayout || m_pipelineTabs) {
        return;
    }

    ui->textLayout->removeWidget(ui->messageText);
    ui->messageText->hide();

    m_pipelineTabs = new QTabWidget(this);
    m_pipelineTabs->setTabPosition(QTabWidget::West);
    m_pipelineTabs->setMovable(false);
    m_pipelineTabs->setDocumentMode(true);
    m_pipelineTabs->setMinimumHeight(ui->messageText->minimumHeight());
    ui->textLayout->addWidget(m_pipelineTabs);

    ensurePipelineView(-1, "All");
}

MeshtasticDemodGUI::PipelineView& MeshtasticDemodGUI::ensurePipelineView(int pipelineId, const QString& pipelineName)
{
    auto it = m_pipelineViews.find(pipelineId);

    if (it != m_pipelineViews.end()) {
        return it.value();
    }

    PipelineView view;
    view.tabWidget = new QWidget(m_pipelineTabs);
    QVBoxLayout *layout = new QVBoxLayout(view.tabWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    QSplitter *splitter = new QSplitter(Qt::Vertical, view.tabWidget);
    splitter->setChildrenCollapsible(false);

    view.logText = new QPlainTextEdit(splitter);
    view.logText->setReadOnly(true);
    view.logText->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont monoLog = view.logText->font();
    monoLog.setFamily("Liberation Mono");
    view.logText->setFont(monoLog);

    view.treeWidget = new QTreeWidget(splitter);
    view.treeWidget->setColumnCount(2);
    view.treeWidget->setHeaderLabels(QStringList() << "Field" << "Value");
    view.treeWidget->header()->setStretchLastSection(true);
    view.treeWidget->setAlternatingRowColors(true);
    view.treeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    QObject::connect(view.treeWidget, &QTreeWidget::itemSelectionChanged, this, &MeshtasticDemodGUI::onPipelineTreeSelectionChanged);
    QObject::connect(view.treeWidget, &QTreeWidget::itemClicked, this, &MeshtasticDemodGUI::onPipelineTreeSelectionChanged);
    QObject::connect(view.treeWidget, &QTreeWidget::currentItemChanged, this, [this, tree=view.treeWidget](QTreeWidgetItem*, QTreeWidgetItem*) {
        queueReplayForTree(tree);
    });

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter);

    const QString tabLabel = pipelineName.trimmed().isEmpty() ? QString("P%1").arg(pipelineId) : pipelineName;
    m_pipelineTabs->addTab(view.tabWidget, tabLabel);
    m_pipelineViews.insert(pipelineId, view);
    return m_pipelineViews[pipelineId];
}

void MeshtasticDemodGUI::clearPipelineViews()
{
    setDechirpInspectionMode(false);

    for (auto it = m_pipelineViews.begin(); it != m_pipelineViews.end(); ++it)
    {
        if (it.value().logText) {
            it.value().logText->clear();
        }
        if (it.value().treeWidget) {
            it.value().treeWidget->clear();
        }
    }

    if (ui->messageText) {
        ui->messageText->clear();
    }

    m_dechirpSnapshots.clear();
    m_dechirpSnapshotOrder.clear();
    m_dechirpSelectedMessageKey.clear();
    m_replayPendingMessageKey.clear();
    m_replayPendingHasSelection = false;
    m_replaySelectionQueued = false;
    m_pipelineMessageKeyByBase.clear();
    m_pipelinePendingMessageKeysByBase.clear();
    m_pipelineMessageSequence = 0;
}

QString MeshtasticDemodGUI::buildPipelineMessageBaseKey(int pipelineId, uint32_t frameId, const QString& timestamp) const
{
    if (frameId != 0U) {
        return QString("%1|frame:%2").arg(pipelineId).arg(frameId);
    }

    const QString ts = timestamp.trimmed().isEmpty()
        ? QStringLiteral("no-ts")
        : timestamp.trimmed();
    return QString("%1|%2").arg(pipelineId).arg(ts);
}

QString MeshtasticDemodGUI::allocatePipelineMessageKey(const QString& baseKey)
{
    ++m_pipelineMessageSequence;
    const QString key = QString("%1#%2").arg(baseKey).arg(m_pipelineMessageSequence);
    m_pipelineMessageKeyByBase[baseKey] = key;
    m_pipelinePendingMessageKeysByBase[baseKey].push_back(key);
    return key;
}

QString MeshtasticDemodGUI::resolvePipelineMessageKey(const QString& baseKey) const
{
    auto pendingIt = m_pipelinePendingMessageKeysByBase.constFind(baseKey);

    if ((pendingIt != m_pipelinePendingMessageKeysByBase.constEnd()) && !pendingIt.value().isEmpty()) {
        return pendingIt.value().front();
    }

    auto it = m_pipelineMessageKeyByBase.constFind(baseKey);
    return it == m_pipelineMessageKeyByBase.constEnd() ? QString() : it.value();
}

void MeshtasticDemodGUI::consumePipelineMessageKey(const QString& baseKey, const QString& key)
{
    auto pendingIt = m_pipelinePendingMessageKeysByBase.find(baseKey);

    if (pendingIt == m_pipelinePendingMessageKeysByBase.end()) {
        return;
    }

    QVector<QString>& pendingKeys = pendingIt.value();
    const int idx = pendingKeys.indexOf(key);

    if (idx >= 0) {
        pendingKeys.removeAt(idx);
    }

    if (pendingKeys.isEmpty()) {
        m_pipelinePendingMessageKeysByBase.erase(pendingIt);
    }
}

void MeshtasticDemodGUI::rememberLoRaDechirpSnapshot(
    const MeshtasticDemodMsg::MsgReportDecodeBytes& msg,
    const QString& messageKey
)
{
    const std::vector<std::vector<float>>& lines = msg.getDechirpedSpectrum();

    if (lines.empty()) {
        return;
    }

    if (messageKey.trimmed().isEmpty()) {
        return;
    }

    const QString key = messageKey;
    DechirpSnapshot snapshot;
    snapshot.fftSize = static_cast<int>(lines.front().size());
    snapshot.lines = lines;
    m_dechirpSnapshots[key] = snapshot;

    const int existingIndex = m_dechirpSnapshotOrder.indexOf(key);
    if (existingIndex >= 0) {
        m_dechirpSnapshotOrder.removeAt(existingIndex);
    }

    m_dechirpSnapshotOrder.push_back(key);
    static constexpr int kMaxStoredSnapshots = 256;

    while (m_dechirpSnapshotOrder.size() > kMaxStoredSnapshots)
    {
        const QString oldestKey = m_dechirpSnapshotOrder.front();
        m_dechirpSnapshotOrder.pop_front();
        m_dechirpSnapshots.remove(oldestKey);
        clearTreeMessageKeyReferences(oldestKey);

        if (oldestKey == m_dechirpSelectedMessageKey) {
            setDechirpInspectionMode(false);
        }
    }
}

void MeshtasticDemodGUI::setDechirpInspectionMode(bool enabled)
{
    if (m_dechirpInspectionActive == enabled)
    {
        updateDechirpModeUI();
        return;
    }

    m_dechirpInspectionActive = enabled;

    if (enabled)
    {
        if (m_spectrumVis) {
            m_spectrumVis->setGLSpectrum(nullptr);
        }
    }
    else
    {
        m_dechirpSelectedMessageKey.clear();

        if (m_spectrumVis && ui && ui->glSpectrum)
        {
            m_spectrumVis->setGLSpectrum(ui->glSpectrum);

            const SpectrumSettings spectrumSettings = m_spectrumVis->getSettings();
            ui->glSpectrum->setDisplayWaterfall(spectrumSettings.m_displayWaterfall);
            ui->glSpectrum->setDisplay3DSpectrogram(spectrumSettings.m_display3DSpectrogram);
        }
    }

    updateDechirpModeUI();
}

void MeshtasticDemodGUI::updateDechirpModeUI()
{
    if (!m_dechirpLiveFollowButton) {
        return;
    }

    m_dechirpLiveFollowButton->setEnabled(m_dechirpInspectionActive);
    m_dechirpLiveFollowButton->setText(tr("Live"));
    m_dechirpLiveFollowButton->setToolTip(m_dechirpInspectionActive
        ? tr("Return de-chirped spectrum to live follow mode.")
        : tr("Already in live follow mode."));
}

void MeshtasticDemodGUI::queueReplayForTree(QTreeWidget *treeWidget)
{
    m_replayPendingMessageKey.clear();
    m_replayPendingHasSelection = false;

    if (treeWidget)
    {
        const QList<QTreeWidgetItem*> selectedItems = treeWidget->selectedItems();
        QTreeWidgetItem *root = selectedItems.isEmpty() ? treeWidget->currentItem() : selectedItems.first();

        if (root)
        {
            while (root->parent()) {
                root = root->parent();
            }

            m_replayPendingHasSelection = true;
            m_replayPendingMessageKey = root->data(0, kTreeMessageKeyRole).toString();
        }
    }

    if (m_replaySelectionQueued) {
        return;
    }

    m_replaySelectionQueued = true;
    QMetaObject::invokeMethod(this, [this]() { processQueuedReplay(); }, Qt::QueuedConnection);
}

void MeshtasticDemodGUI::processQueuedReplay()
{
    m_replaySelectionQueued = false;
    const bool hasSelection = m_replayPendingHasSelection;
    const QString key = m_replayPendingMessageKey;
    m_replayPendingHasSelection = false;
    m_replayPendingMessageKey.clear();

    if (!hasSelection)
    {
        setDechirpInspectionMode(false);
        return;
    }

    if (key.isEmpty())
    {
        setDechirpInspectionMode(false);
        return;
    }

    auto it = m_dechirpSnapshots.constFind(key);

    if (it == m_dechirpSnapshots.constEnd())
    {
        setDechirpInspectionMode(false);
        return;
    }

    setDechirpInspectionMode(true);
    m_dechirpSelectedMessageKey = key;
    replayDechirpSnapshot(it.value());
}

void MeshtasticDemodGUI::hardResetDechirpDisplayBuffers()
{
    if (!ui || !ui->glSpectrum) {
        return;
    }

    bool wantWaterfall = true;
    bool want3DSpectrogram = false;

    if (m_spectrumVis)
    {
        const SpectrumSettings spectrumSettings = m_spectrumVis->getSettings();
        wantWaterfall = spectrumSettings.m_displayWaterfall;
        want3DSpectrogram = spectrumSettings.m_display3DSpectrogram;
    }

    if (!wantWaterfall && !want3DSpectrogram) {
        wantWaterfall = true;
    }

    ui->glSpectrum->setDisplayWaterfall(false);
    ui->glSpectrum->setDisplay3DSpectrogram(false);
    QCoreApplication::processEvents(
        QEventLoop::ExcludeUserInputEvents | QEventLoop::ExcludeSocketNotifiers,
        1
    );

    ui->glSpectrum->setDisplayWaterfall(wantWaterfall);
    ui->glSpectrum->setDisplay3DSpectrogram(want3DSpectrogram);
    QCoreApplication::processEvents(
        QEventLoop::ExcludeUserInputEvents | QEventLoop::ExcludeSocketNotifiers,
        1
    );
}

void MeshtasticDemodGUI::clearTreeMessageKeyReferences(const QString& messageKey)
{
    if (messageKey.trimmed().isEmpty()) {
        return;
    }

    for (auto it = m_pipelineViews.begin(); it != m_pipelineViews.end(); ++it)
    {
        QTreeWidget *treeWidget = it.value().treeWidget;

        if (!treeWidget) {
            continue;
        }

        for (int i = 0; i < treeWidget->topLevelItemCount(); ++i)
        {
            QTreeWidgetItem *item = treeWidget->topLevelItem(i);

            if (!item) {
                continue;
            }

            if (item->data(0, kTreeMessageKeyRole).toString() == messageKey)
            {
                item->setData(0, kTreeMessageKeyRole, QString());
                item->setToolTip(0, tr("Dechirp snapshot no longer available for this row."));
            }
        }
    }
}

void MeshtasticDemodGUI::replayDechirpSnapshot(const DechirpSnapshot& snapshot)
{
    if (!ui || !ui->glSpectrum || snapshot.lines.empty()) {
        return;
    }

    if (m_spectrumVis)
    {
        SpectrumSettings spectrumSettings = m_spectrumVis->getSettings();

        if (!spectrumSettings.m_displayWaterfall && !spectrumSettings.m_display3DSpectrogram)
        {
            spectrumSettings.m_displayWaterfall = true;
            SpectrumVis::MsgConfigureSpectrumVis *msg =
                SpectrumVis::MsgConfigureSpectrumVis::create(spectrumSettings, false);
            m_spectrumVis->getInputMessageQueue()->push(msg);
        }
    }

    const int fftSize = snapshot.fftSize > 0 ? snapshot.fftSize : static_cast<int>(snapshot.lines.front().size());

    if (fftSize <= 0) {
        return;
    }
    hardResetDechirpDisplayBuffers();

    std::vector<Real> line(static_cast<size_t>(fftSize), static_cast<Real>(-120.0f));
    const int spectrumHeight = (ui && ui->glSpectrum) ? ui->glSpectrum->height() : 0;
    const int clearLines = std::max(128, std::min(2048, spectrumHeight > 0 ? spectrumHeight + 64 : 768));
    const int replayChunkLines = 8;
    auto flushReplayChunk = [this]() {
        ui->glSpectrum->repaint();
        QCoreApplication::processEvents(
            QEventLoop::ExcludeUserInputEvents | QEventLoop::ExcludeSocketNotifiers,
            1
        );
    };

    auto feedLineToSpectrum = [&](const std::vector<float>* powers) {
        if (powers == nullptr)
        {
            std::fill(line.begin(), line.end(), static_cast<Real>(-120.0f));
        }
        else
        {
            std::fill(line.begin(), line.end(), static_cast<Real>(-120.0f));
            const int count = std::min(fftSize, static_cast<int>(powers->size()));

            for (int i = 0; i < count; ++i)
            {
                const float power = std::max((*powers)[static_cast<size_t>(i)], 1e-12f);
                line[static_cast<size_t>(i)] = static_cast<Real>(10.0f * std::log10(power));
            }
        }

        ui->glSpectrum->newSpectrum(line.data(), fftSize);
    };

    // Prime the GL spectrum so pending size/layout changes are applied before replay.
    flushReplayChunk();

    int lineCounter = 0;

    for (int i = 0; i < clearLines; ++i)
    {
        feedLineToSpectrum(nullptr);

        if ((++lineCounter % replayChunkLines) == 0) {
            flushReplayChunk();
        }
    }

    for (const std::vector<float>& powers : snapshot.lines)
    {
        feedLineToSpectrum(&powers);

        if ((++lineCounter % replayChunkLines) == 0) {
            flushReplayChunk();
        }
    }

    flushReplayChunk();
}

void MeshtasticDemodGUI::onPipelineTreeSelectionChanged()
{
    QTreeWidget *treeWidget = qobject_cast<QTreeWidget*>(sender());

    if (!treeWidget) {
        return;
    }

    queueReplayForTree(treeWidget);
}

void MeshtasticDemodGUI::appendPipelineLogLine(int pipelineId, const QString& pipelineName, const QString& line)
{
    auto appendLine = [&line](PipelineView& view) {
        if (!view.logText) {
            return;
        }

        view.logText->appendPlainText(line);
        alignTextViewToLatestLineLeft(view.logText);
    };

    PipelineView& targetView = ensurePipelineView(pipelineId, pipelineName);
    appendLine(targetView);

    if (pipelineId != -1)
    {
        PipelineView& allView = ensurePipelineView(-1, "All");

        if (allView.logText != targetView.logText) {
            appendLine(allView);
        }
    }
}

void MeshtasticDemodGUI::appendPipelineStatusLine(int pipelineId, const QString& pipelineName, const QString& status)
{
    appendPipelineLogLine(pipelineId, pipelineName, QString(">%1").arg(status));
}

void MeshtasticDemodGUI::appendPipelineBytes(int pipelineId, const QString& pipelineName, const QByteArray& bytes)
{
    QStringList lines;
    QString line;

    for (int i = 0; i < bytes.size(); ++i)
    {
        const unsigned int b = static_cast<unsigned int>(static_cast<unsigned char>(bytes.at(i)));

        if ((i % 16) == 0) {
            line = QString("%1|").arg(i, 3, 10, QChar('0'));
        }

        line += QString("%1").arg(b, 2, 16, QChar('0'));

        if ((i % 16) == 15)
        {
            lines.append(line);
        }
        else if ((i % 4) == 3)
        {
            line += "|";
        }
        else
        {
            line += " ";
        }
    }

    if ((bytes.size() % 16) != 0 && !line.isEmpty()) {
        lines.append(line);
    }

    for (const QString& l : lines) {
        appendPipelineLogLine(pipelineId, pipelineName, l);
    }
}

void MeshtasticDemodGUI::appendPipelineTreeFields(
    int pipelineId,
    const QString& pipelineName,
    const QString& messageTitle,
    const QVector<QPair<QString, QString>>& fields,
    const QString& messageKey
)
{
    auto fieldValue = [&fields](const QString& path) -> QString {
        for (const QPair<QString, QString>& field : fields)
        {
            if (field.first == path) {
                return field.second;
            }
        }
        return QString();
    };

    auto parseBool = [](const QString& value, bool& ok) -> bool {
        const QString lower = value.trimmed().toLower();

        if ((lower == "true") || (lower == "1") || (lower == "yes"))
        {
            ok = true;
            return true;
        }

        if ((lower == "false") || (lower == "0") || (lower == "no"))
        {
            ok = true;
            return false;
        }

        ok = false;
        return false;
    };

    const QString portName = fieldValue("data.port_name");
    const QString portNum = fieldValue("data.portnum");
    const QString payloadLength = fieldValue("data.payload_len");
    const QString decryptedValue = fieldValue("decode.decrypted");
    const QString payloadText = fieldValue("data.text");
    const QString payloadHex = fieldValue("data.payload_hex");
    const QString viaMqttValue = fieldValue("header.via_mqtt");

    QString messageType = portName;

    if (messageType.isEmpty() && !portNum.isEmpty()) {
        messageType = QString("PORT_%1").arg(portNum);
    }

    QString source;
    bool viaMqttOk = false;
    const bool viaMqtt = parseBool(viaMqttValue, viaMqttOk);

    if (viaMqttOk) {
        source = viaMqtt ? "mqtt" : "radio";
    }

    bool decryptedOk = false;
    const bool decrypted = parseBool(decryptedValue, decryptedOk);

    QString payloadPreview;

    if (decryptedOk && decrypted)
    {
        payloadPreview = !payloadText.isEmpty() ? payloadText : payloadHex;
        payloadPreview.replace('\n', ' ');
        payloadPreview.replace('\r', ' ');

        if (payloadPreview.size() > 96) {
            payloadPreview = payloadPreview.left(96) + "...";
        }
    }

    QStringList rootSummaryParts;

    if (!messageType.isEmpty()) {
        rootSummaryParts << QString("type=%1").arg(messageType);
    }
    if (!source.isEmpty()) {
        rootSummaryParts << QString("source=%1").arg(source);
    }
    if (!payloadLength.isEmpty()) {
        rootSummaryParts << QString("len=%1").arg(payloadLength);
    }
    if (decryptedOk) {
        rootSummaryParts << QString("decrypted=%1").arg(decrypted ? "yes" : "no");
    }
    if (!payloadPreview.isEmpty()) {
        rootSummaryParts << QString("payload=\"%1\"").arg(payloadPreview);
    }

    const QString rootSummary = rootSummaryParts.join(" ");
    auto compactSummaryValue = [](QString value, int maxLen = 64) -> QString {
        value.replace('\n', ' ');
        value.replace('\r', ' ');
        value = value.trimmed();

        if (value.size() > maxLen) {
            return value.left(maxLen) + "...";
        }

        return value;
    };

    auto splitCamelCase = [](const QString& token) -> QString {
        QString out;
        out.reserve(token.size() + 8);

        for (int i = 0; i < token.size(); ++i)
        {
            const QChar ch = token.at(i);
            const QChar prev = (i > 0) ? token.at(i - 1) : QChar();
            const bool breakBeforeUpper = (i > 0)
                && ch.isUpper()
                && (prev.isLower() || prev.isDigit());

            if (breakBeforeUpper) {
                out += ' ';
            }

            out += ch;
        }

        return out;
    };

    auto humanizeProtoField = [&](const QString& rawName) -> QString {
        static const QMap<QString, QString> kTokenMap = {
            {"id", "ID"},
            {"uid", "UID"},
            {"snr", "SNR"},
            {"rssi", "RSSI"},
            {"rx", "RX"},
            {"tx", "TX"},
            {"utc", "UTC"},
            {"gps", "GPS"},
            {"lat", "Latitude"},
            {"lon", "Longitude"},
            {"lng", "Longitude"},
            {"alt", "Altitude"},
            {"deg", "Degrees"},
            {"num", "Count"},
            {"secs", "Seconds"},
            {"hz", "Hz"}
        };

        QStringList prettyParts;
        const QStringList snakeParts = rawName.split('_', Qt::SkipEmptyParts);

        for (const QString& snakePart : snakeParts)
        {
            const QString camelSplit = splitCamelCase(snakePart);
            const QStringList words = camelSplit.split(' ', Qt::SkipEmptyParts);

            for (const QString& word : words)
            {
                const QString lower = word.toLower();
                const auto mapIt = kTokenMap.find(lower);

                if (mapIt != kTokenMap.end())
                {
                    prettyParts.append(mapIt.value());
                }
                else
                {
                    QString normalized = lower;

                    if (!normalized.isEmpty()) {
                        normalized[0] = normalized[0].toUpper();
                    }

                    prettyParts.append(normalized);
                }
            }
        }

        if (prettyParts.isEmpty()) {
            return rawName;
        }

        return prettyParts.join(' ');
    };

    auto formatFieldLabel = [&](const QString& rawName) -> QString {
        if (rawName.isEmpty()) {
            return rawName;
        }

        bool isIndex = false;
        rawName.toInt(&isIndex);

        if (isIndex) {
            return QString("Item (%1)").arg(rawName);
        }

        return QString("%1 (%2)").arg(humanizeProtoField(rawName), rawName);
    };

    auto formatLabeledValue = [&](const QString& rawName, const QString& value, int maxLen = 120) -> QString {
        const QString compactValue = compactSummaryValue(value, maxLen);
        const QString label = formatFieldLabel(rawName);

        if (label.isEmpty()) {
            return compactValue;
        }

        if (compactValue.isEmpty()) {
            return label;
        }

        return QString("%1: %2").arg(label, compactValue);
    };

    auto populateTree = [&](PipelineView& view) {
        if (!view.treeWidget) {
            return;
        }

        QTreeWidgetItem *root = nullptr;

        if (!messageKey.isEmpty())
        {
            for (int i = 0; i < view.treeWidget->topLevelItemCount(); ++i)
            {
                QTreeWidgetItem *candidate = view.treeWidget->topLevelItem(i);
                if (!candidate) {
                    continue;
                }

                if (candidate->data(0, kTreeMessageKeyRole).toString() == messageKey)
                {
                    root = candidate;
                    break;
                }
            }
        }

        if (!root) {
            root = new QTreeWidgetItem(view.treeWidget);
        } else {
            while (root->childCount() > 0) {
                delete root->takeChild(0);
            }
        }

        root->setText(0, messageTitle);
        root->setText(1, rootSummary);

        const bool snapshotKnown = messageKey.isEmpty() || m_dechirpSnapshots.contains(messageKey);

        if (!snapshotKnown)
        {
            // Keep row selectable for structured content but prevent stale replay lookup.
            root->setData(0, kTreeMessageKeyRole, QString());
            root->setToolTip(0, tr("No dechirp snapshot is available for this decoded row."));
        }
        else
        {
            root->setData(0, kTreeMessageKeyRole, messageKey);
        }

        for (const QPair<QString, QString>& field : fields)
        {
            QStringList pathParts = field.first.split('.', Qt::SkipEmptyParts);

            if (pathParts.isEmpty())
            {
                const QString fieldLabel = formatFieldLabel(field.first);
                const QString labeledValue = formatLabeledValue(field.first, field.second);
                QTreeWidgetItem *leaf = new QTreeWidgetItem(QStringList() << fieldLabel << labeledValue);
                leaf->setData(0, kTreeRawKeyRole, field.first);
                leaf->setData(0, kTreeDisplayLabelRole, fieldLabel);
                leaf->setData(1, kTreeRawValueRole, field.second);
                const QString formatted = formatLabeledValue(field.first, field.second, 120);
                leaf->setToolTip(0, formatted);
                leaf->setToolTip(1, formatted);
                root->addChild(leaf);
                continue;
            }

            QTreeWidgetItem *parent = root;

            for (int i = 0; i < pathParts.size(); ++i)
            {
                const QString& part = pathParts.at(i);
                QTreeWidgetItem *child = nullptr;

                for (int c = 0; c < parent->childCount(); ++c)
                {
                    QTreeWidgetItem *candidate = parent->child(c);
                    QString candidateRaw = candidate->data(0, kTreeRawKeyRole).toString();

                    if (candidateRaw.isEmpty()) {
                        candidateRaw = candidate->text(0);
                    }

                    if (candidateRaw == part)
                    {
                        child = candidate;
                        break;
                    }
                }

                if (!child)
                {
                    const QString fieldLabel = formatFieldLabel(part);
                    child = new QTreeWidgetItem(QStringList() << fieldLabel);
                    child->setData(0, kTreeRawKeyRole, part);
                    child->setData(0, kTreeDisplayLabelRole, fieldLabel);
                    parent->addChild(child);
                }

                if (i == pathParts.size() - 1)
                {
                    const QString fieldLabel = child->data(0, kTreeDisplayLabelRole).toString().isEmpty()
                        ? formatFieldLabel(part)
                        : child->data(0, kTreeDisplayLabelRole).toString();
                    const QString formatted = formatLabeledValue(part, field.second, 120);
                    child->setData(0, kTreeDisplayLabelRole, fieldLabel);
                    child->setData(1, kTreeRawValueRole, field.second);
                    child->setText(1, formatLabeledValue(part, field.second));
                    child->setToolTip(0, formatted);
                    child->setToolTip(1, formatted);
                }

                parent = child;
            }
        }

        std::function<QString(QTreeWidgetItem*)> computeNodeSummary = [&](QTreeWidgetItem *item) -> QString {
            if (!item) {
                return QString();
            }

            QStringList parts;
            const int maxParts = 4;

            for (int i = 0; i < item->childCount(); ++i)
            {
                QTreeWidgetItem *child = item->child(i);
                if (!child) {
                    continue;
                }

                QString key = child->data(0, kTreeDisplayLabelRole).toString().trimmed();
                if (key.isEmpty()) {
                    key = child->text(0).trimmed();
                }

                QString value = child->data(1, kTreeRawValueRole).toString().trimmed();
                if (value.isEmpty() && (child->childCount() == 0)) {
                    value = child->text(1).trimmed();
                }

                QString part;

                if (!value.isEmpty())
                {
                    part = key.isEmpty() ? compactSummaryValue(value) : QString("%1: %2").arg(key, compactSummaryValue(value));
                }
                else if (child->childCount() > 0)
                {
                    const QString nested = computeNodeSummary(child);
                    if (!nested.isEmpty()) {
                        part = key.isEmpty() ? nested : QString("%1: {%2}").arg(key, nested);
                    } else {
                        part = key;
                    }
                }
                else
                {
                    part = key;
                }

                if (!part.isEmpty()) {
                    parts.append(part);
                }

                if (parts.size() >= maxParts) {
                    break;
                }
            }

            if (item->childCount() > parts.size()) {
                parts.append("...");
            }

            return parts.join(" ");
        };

        std::function<void(QTreeWidgetItem*, bool)> applyNodeSummaries = [&](QTreeWidgetItem *item, bool isRoot) {
            if (!item) {
                return;
            }

            for (int i = 0; i < item->childCount(); ++i) {
                applyNodeSummaries(item->child(i), false);
            }

            if (item->childCount() == 0) {
                return;
            }

            const QString derivedSummary = computeNodeSummary(item);
            if (derivedSummary.isEmpty()) {
                return;
            }

            if (isRoot)
            {
                if (item->text(1).trimmed().isEmpty()) {
                    item->setText(1, derivedSummary);
                } else {
                    item->setToolTip(1, item->text(1));
                }
            }
            else if (item->text(1).trimmed().isEmpty())
            {
                item->setText(1, derivedSummary);
                item->setToolTip(1, derivedSummary);
            }
        };

        applyNodeSummaries(root, true);
        root->setExpanded(true);
        alignTreeViewToLatestEntryLeft(view.treeWidget, root);
    };

    PipelineView& targetView = ensurePipelineView(pipelineId, pipelineName);
    populateTree(targetView);

    if (pipelineId != -1)
    {
        PipelineView& allView = ensurePipelineView(-1, "All");

        if (allView.treeWidget != targetView.treeWidget) {
            populateTree(allView);
        }
    }
}

void MeshtasticDemodGUI::showLoRaMessage(const Message& message)
{
    const MeshtasticDemodMsg::MsgReportDecodeBytes& msg = (MeshtasticDemodMsg::MsgReportDecodeBytes&) message;
    const int pipelineId = msg.getPipelineId();
    const QString messageBaseKey = buildPipelineMessageBaseKey(pipelineId, msg.getFrameId(), msg.getMsgTimestamp());
    const QString messageKey = allocatePipelineMessageKey(messageBaseKey);
    rememberLoRaDechirpSnapshot(msg, messageKey);
    const QString pipelineName = msg.getPipelineName().trimmed().isEmpty()
        ? (pipelineId < 0 ? QString("Main") : QString("P%1").arg(pipelineId))
        : msg.getPipelineName();
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

        appendPipelineStatusLine(pipelineId, pipelineName, loRaStatus);
        displayLoRaStatus(msg.getHeaderParityStatus(), msg.getHeaderCRCStatus(), (int) MeshtasticDemodSettings::ParityUndefined, true);
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

        appendPipelineStatusLine(pipelineId, pipelineName, loRaHeader);
        appendPipelineBytes(pipelineId, pipelineName, bytes);

        QByteArray bytesCopy(bytes);
        bytesCopy.truncate(packetLength);
        bytesCopy.replace('\0', " ");
        QString str = QString(bytesCopy.toStdString().c_str());
        QString textHeader(tr("%1 (%2)").arg(dateStr).arg(syncWordStr));
        appendPipelineLogLine(pipelineId, pipelineName, QString("TXT|%1 %2").arg(textHeader, str));
        displayLoRaStatus(msg.getHeaderParityStatus(), msg.getHeaderCRCStatus(), msg.getPayloadParityStatus(), msg.getPayloadCRCStatus());
    }

    // Always create/update a selectable row per LoRa frame so every frame can
    // be selected for dechirp replay, even when higher-layer Meshtastic parsing
    // does not yield structured fields.
    QVector<QPair<QString, QString>> fallbackFields;
    const QByteArray payloadBytes = bytes.left(static_cast<int>(packetLength));
    const QString payloadHex = QString::fromLatin1(payloadBytes.toHex());

    fallbackFields.append(qMakePair(QString("header.via_mqtt"), QString("false")));
    fallbackFields.append(qMakePair(QString("data.port_name"), QString("LORA_FRAME")));
    fallbackFields.append(qMakePair(QString("decode.frame_id"), QString::number(msg.getFrameId())));
    fallbackFields.append(qMakePair(QString("decode.status"), msg.getEarlyEOM() ? QString("early_eom") : (msg.getPayloadCRCStatus() ? QString("ok") : QString("crc_error"))));
    fallbackFields.append(qMakePair(QString("decode.sync_word"), QString("0x%1").arg(msg.getSyncWord(), 2, 16, QChar('0'))));
    fallbackFields.append(qMakePair(QString("decode.signal_db"), QString::number(msg.getSingalDb(), 'f', 1)));
    fallbackFields.append(qMakePair(QString("decode.snr_db"), QString::number(msg.getSingalDb() - msg.getNoiseDb(), 'f', 1)));
    fallbackFields.append(qMakePair(QString("decode.header_parity"), getParityStr(msg.getHeaderParityStatus())));
    fallbackFields.append(qMakePair(QString("decode.header_crc"), msg.getHeaderCRCStatus() ? QString("ok") : QString("err")));
    fallbackFields.append(qMakePair(QString("decode.payload_parity"), getParityStr(msg.getPayloadParityStatus())));
    fallbackFields.append(qMakePair(QString("decode.payload_crc"), msg.getPayloadCRCStatus() ? QString("ok") : QString("err")));
    fallbackFields.append(qMakePair(QString("decode.early_eom"), msg.getEarlyEOM() ? QString("true") : QString("false")));
    fallbackFields.append(qMakePair(QString("decode.decrypted"), QString("false")));
    fallbackFields.append(qMakePair(QString("data.payload_len"), QString::number(packetLength)));
    fallbackFields.append(qMakePair(QString("data.payload_hex"), payloadHex));
    appendPipelineTreeFields(
        pipelineId,
        pipelineName,
        QString("%1 %2").arg(dateStr, pipelineName),
        fallbackFields,
        messageKey
    );

    ui->nbSymbolsText->setText(tr("%1").arg(msg.getNbSymbols()));
    ui->nbCodewordsText->setText(tr("%1").arg(msg.getNbCodewords()));
}

void MeshtasticDemodGUI::showTextMessage(const Message& message)
{
    const MeshtasticDemodMsg::MsgReportDecodeString& msg = (MeshtasticDemodMsg::MsgReportDecodeString&) message;
    const int pipelineId = msg.getPipelineId();
    const QString pipelineName = msg.getPipelineName().trimmed().isEmpty()
        ? (pipelineId < 0 ? QString("Main") : QString("P%1").arg(pipelineId))
        : msg.getPipelineName();

    QDateTime dt = QDateTime::currentDateTime();
    QString dateStr = dt.toString("HH:mm:ss");
    ui->sText->setText(tr("%1").arg(msg.getSingalDb(), 0, 'f', 1));
    ui->snrText->setText(tr("%1").arg(msg.getSingalDb() - msg.getNoiseDb(), 0, 'f', 1));

    QString status = tr("%1 S:%2 SN:%3")
        .arg(dateStr)
        .arg(msg.getSingalDb(), 0, 'f', 1)
        .arg(msg.getSingalDb() - msg.getNoiseDb(), 0, 'f', 1);

    appendPipelineStatusLine(pipelineId, pipelineName, status);
    appendPipelineLogLine(pipelineId, pipelineName, QString("TXT|%1").arg(msg.getString()));

    if (msg.hasStructuredFields())
    {
        const QString title = QString("%1 %2").arg(dateStr, pipelineName);
        const QString messageBaseKey = buildPipelineMessageBaseKey(pipelineId, msg.getFrameId(), msg.getMsgTimestamp());
        QString messageKey = resolvePipelineMessageKey(messageBaseKey);

        if (messageKey.isEmpty() && (m_settings.m_codingScheme != MeshtasticDemodSettings::CodingLoRa)) {
            messageKey = allocatePipelineMessageKey(messageBaseKey);
        }

        appendPipelineTreeFields(pipelineId, pipelineName, title, msg.getStructuredFields(), messageKey);

        if (!messageKey.isEmpty()) {
            consumePipelineMessageKey(messageBaseKey, messageKey);
        }
    }
}

void MeshtasticDemodGUI::displayText(const QString& text)
{
    if (m_pipelineTabs)
    {
        appendPipelineLogLine(-1, "All", QString("TXT|%1").arg(text));
        return;
    }

    QTextCursor cursor = ui->messageText->textCursor();
    cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    if (!ui->messageText->document()->isEmpty()) {
        cursor.insertText("\n");
    }
    cursor.insertText(tr("TXT|%1").arg(text));
    alignTextViewToLatestLineLeft(ui->messageText);
}

void MeshtasticDemodGUI::displayBytes(const QByteArray& bytes)
{
    if (m_pipelineTabs)
    {
        appendPipelineBytes(-1, "All", bytes);
        return;
    }

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

    alignTextViewToLatestLineLeft(ui->messageText);
}

void MeshtasticDemodGUI::displayStatus(const QString& status)
{
    if (m_pipelineTabs)
    {
        appendPipelineStatusLine(-1, "All", status);
        qInfo() << "MeshtasticDemodGUI::displayStatus:" << status;
        return;
    }

    QTextCursor cursor = ui->messageText->textCursor();
    cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);

    if (!ui->messageText->document()->isEmpty()) {
        cursor.insertText("\n");
    }

    cursor.insertText(tr(">%1").arg(status));
    alignTextViewToLatestLineLeft(ui->messageText);
    qInfo() << "MeshtasticDemodGUI::displayStatus:" << status;
}

QString MeshtasticDemodGUI::getParityStr(int parityStatus)
{
    if (parityStatus == (int) MeshtasticDemodSettings::ParityError) {
        return "err";
    } else if (parityStatus == (int) MeshtasticDemodSettings::ParityCorrected) {
        return "fix";
    } else if (parityStatus == (int) MeshtasticDemodSettings::ParityOK) {
        return "ok";
    } else {
        return "n/a";
    }
}

void MeshtasticDemodGUI::tick()
{
    handleMeshAutoLockSourceObservation();
    advanceMeshAutoLock();

    if (m_deviceUISet && m_deviceUISet->m_deviceAPI)
    {
        const bool isRemoteTcpInput = (m_deviceUISet->m_deviceAPI->getHardwareId() == "RemoteTCPInput");

        if (isRemoteTcpInput)
        {
            const bool running = (m_deviceUISet->m_deviceAPI->state(m_settings.m_streamIndex) == DeviceAPI::StRunning);

            if (running && !m_remoteTcpLastRunningState)
            {
                m_remoteTcpReconnectAutoApplyPending = true;
                m_remoteTcpReconnectAutoApplyWaitTicks = 0;
                qInfo() << "MeshtasticDemodGUI::tick: RemoteTCP input running - waiting for first DSP notification to reapply Meshtastic profile";
            }

            if (m_remoteTcpReconnectAutoApplyPending && running)
            {
                m_remoteTcpReconnectAutoApplyWaitTicks++;

                if (m_remoteTcpReconnectAutoApplyWaitTicks >= 20)
                {
                    m_remoteTcpReconnectAutoApplyPending = false;
                    m_remoteTcpReconnectAutoApplyWaitTicks = 0;
                    qInfo() << "MeshtasticDemodGUI::tick: RemoteTCP reconnect fallback timeout - reapplying Meshtastic profile";
                    QMetaObject::invokeMethod(this, &MeshtasticDemodGUI::applyMeshtasticProfileFromSelection, Qt::QueuedConnection);
                }
            }

            if (!running)
            {
                m_remoteTcpReconnectAutoApplyPending = false;
                m_remoteTcpReconnectAutoApplyWaitTicks = 0;
            }

            m_remoteTcpLastRunningState = running;
        }
        else
        {
            m_remoteTcpReconnectAutoApplyPending = false;
            m_remoteTcpReconnectAutoApplyWaitTicks = 0;
            m_remoteTcpLastRunningState = false;
        }
    }

    if (m_tickCount < 10)
    {
        m_tickCount++;
    }
    else
    {
        m_tickCount = 0;

        ui->nText->setText(tr("%1").arg(CalcDb::dbPower(m_meshtasticDemod->getCurrentNoiseLevel()), 0, 'f', 1));
        ui->channelPower->setText(tr("%1 dB").arg(CalcDb::dbPower(m_meshtasticDemod->getTotalPower()), 0, 'f', 1));

        if (m_meshtasticDemod->getDemodActive()) {
            ui->mute->setStyleSheet("QToolButton { background-color : green; }");
        } else {
            ui->mute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        }
    }
}

void MeshtasticDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &MeshtasticDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->BW, &QSlider::valueChanged, this, &MeshtasticDemodGUI::on_BW_valueChanged);
    QObject::connect(ui->Spread, &QSlider::valueChanged, this, &MeshtasticDemodGUI::on_Spread_valueChanged);
    QObject::connect(ui->deBits, &QSlider::valueChanged, this, &MeshtasticDemodGUI::on_deBits_valueChanged);
    QObject::connect(ui->fftWindow, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MeshtasticDemodGUI::on_fftWindow_currentIndexChanged);
    QObject::connect(ui->preambleChirps, &QSlider::valueChanged, this, &MeshtasticDemodGUI::on_preambleChirps_valueChanged);
    QObject::connect(ui->mute, &QToolButton::toggled, this, &MeshtasticDemodGUI::on_mute_toggled);
    QObject::connect(ui->clear, &QPushButton::clicked, this, &MeshtasticDemodGUI::on_clear_clicked);
    QObject::connect(ui->eomSquelch, &QDial::valueChanged, this, &MeshtasticDemodGUI::on_eomSquelch_valueChanged);
    QObject::connect(ui->messageLength, &QDial::valueChanged, this, &MeshtasticDemodGUI::on_messageLength_valueChanged);
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    QObject::connect(ui->udpSend, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state){ on_udpSend_stateChanged(static_cast<int>(state)); });
#else
    QObject::connect(ui->udpSend, &QCheckBox::stateChanged, this, &MeshtasticDemodGUI::on_udpSend_stateChanged);
#endif
    QObject::connect(ui->udpAddress, &QLineEdit::editingFinished, this, &MeshtasticDemodGUI::on_udpAddress_editingFinished);
    QObject::connect(ui->udpPort, &QLineEdit::editingFinished, this, &MeshtasticDemodGUI::on_udpPort_editingFinished);
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    QObject::connect(ui->invertRamps, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state){ on_invertRamps_stateChanged(static_cast<int>(state)); });
#else
    QObject::connect(ui->invertRamps, &QCheckBox::stateChanged, this, &MeshtasticDemodGUI::on_invertRamps_stateChanged);
#endif
}

void MeshtasticDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
