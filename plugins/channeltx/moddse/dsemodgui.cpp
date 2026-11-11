///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include <algorithm>

#include <QDateTime>
#include <QFileDialog>
#include <QLocale>
#include <QLineEdit>
#include <QRegularExpressionValidator>
#include <QTime>
#include <QDebug>

#include "gui/buttonswitch.h"
#include "dsemodaprssymboldialog.h"
#include "dsemodvocodermbelib.h"
#include "dsemodgui.h"
#include "dsemodsource.h"
#include "dsp/dv/dvangel61.h"

#include "device/deviceuiset.h"
#include "dsp/spectrumvis.h"
#include "plugin/pluginapi.h"
#include "util/db.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/cwkeyer.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "maincore.h"
#include "channel/channelapi.h"
#include "SWGChannelReport.h"
#include "SWGDSDDemodReport.h"
#include "SWGFeatureReport.h"
//#include "SWGDSGatewayReport.h"
#include "feature/feature.h"
#include "feature/featureset.h"
#include "ui_dsemodgui.h"

DSEModGUI* DSEModGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx)
{
    DSEModGUI* gui = new DSEModGUI(pluginAPI, deviceUISet, channelTx);
    return gui;
}

void DSEModGUI::destroy()
{
    delete this;
}

void DSEModGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
}

QByteArray DSEModGUI::serialize() const
{
    return m_settings.serialize();
}

bool DSEModGUI::deserialize(const QByteArray& data)
{
    bool ok = m_settings.deserialize(data);

    if (!ok) {
        m_settings.resetToDefaults();
    }

    m_settings.m_transmit = false; // never start transmitting from stored settings
    displaySettings();
    applySettings(QStringList(), true);
    return ok;
}

bool DSEModGUI::handleMessage(const Message& message)
{
    if (DSEMod::MsgReportFileSourceStreamData::match(message))
    {
        m_recordSampleRate = ((DSEMod::MsgReportFileSourceStreamData&)message).getSampleRate();
        m_recordLength = ((DSEMod::MsgReportFileSourceStreamData&)message).getRecordLength();
        m_samplesCount = 0;
        updateWithStreamData();
        return true;
    }
    else if (DSEMod::MsgReportFileSourceStreamTiming::match(message))
    {
        m_samplesCount = (qint64) ((DSEMod::MsgReportFileSourceStreamTiming&)message).getSamplesCount();
        updateWithStreamTime();
        return true;
    }
    else if (DSEMod::MsgReportAvailableAMBEFeatures::match(message))
    {
        DSEMod::MsgReportAvailableAMBEFeatures& report = (DSEMod::MsgReportAvailableAMBEFeatures&) message;
        m_availableAMBEFeatures = report.getFeatures();
        updateAMBEFeaturesList();
        return true;
    }
    else if (DSEModSource::MsgReportFileTransfer::match(message))
    {
        // How the file transfer is going (3.4)
        const DSEModSource::MsgReportFileTransfer& report = (DSEModSource::MsgReportFileTransfer&) message;
        static const char *states[] = { "idle", "sending", "waiting", "done", "failed" };
        int state = report.getState();
        QString text = QString("File: %1").arg((state >= 0) && (state < 5) ? states[state] : "?");

        if (report.getBlocksTotal() > 0)
        {
            // Blocks that have gone at least once, of the file's own count. Not "sent minus
            // pending": sent counts every block put on the air, repeats in later rounds
            // included, so at the start that read 0 - 1818 and the operator saw -1818/1818
            int done = report.getBlocksTotal() - report.getBlocksPending();
            text += QString(" %1/%2").arg(done).arg(report.getBlocksTotal());

            // The rate, averaged over the transfer. Two things here were wrong before and
            // both showed as the rate coming and going: it is measured from blocksSent, which
            // counts every block put on the air and never goes backwards, and not from the
            // progress above - a repair round re-queues the blocks the receiver missed, so the
            // progress *does* go backwards and that was read as a new transfer, restarting the
            // clock and suppressing the rate until it had run again. And once measured it stays
            // on screen: between rounds the transmitter is unkeyed and nothing new arrives
            if (report.getBlockSize() > 0)
            {
                qint64 now = QDateTime::currentMSecsSinceEpoch();
                int sent = report.getBlocksSent();

                if ((sent < m_fileRateSent) || (m_fileRateStart == 0))
                {
                    m_fileRateStart = now;   // a transfer that has just begun, or begun again
                    m_fileRate = 0.0;
                }

                m_fileRateSent = sent;
                qint64 elapsed = now - m_fileRateStart;

                if ((sent > 0) && (elapsed >= 1000)) {
                    m_fileRate = (double) sent * report.getBlockSize() * 1000.0 / (double) elapsed;
                }

                if (m_fileRate > 0.0)
                {
                    text += QString(", %1 B/s (%2 kbit/s)")
                        .arg(m_fileRate, 0, 'f', 0).arg(m_fileRate * 8.0 / 1000.0, 0, 'f', 1);

                    int left = report.getBlocksPending();

                    if ((left > 0) && (state == 1)) {   // sending
                        text += QString(", %1 s left")
                            .arg((double) left * report.getBlockSize() / m_fileRate, 0, 'f', 0);
                    }
                }
            }

            if (report.getRounds() > 0) {
                text += QString(", round %1").arg(report.getRounds());
            }
        }

        ui->angel61FileStatus->setText(text);
        return true;
    }
    else if (DSEMod::MsgReportAvailableDemodulators::match(message))
    {
        DSEMod::MsgReportAvailableDemodulators& report = (DSEMod::MsgReportAvailableDemodulators&) message;
        m_availableDemodulators = report.getDemodulators();
        updateDemodulatorsList();
        return true;
    }
    else if (DSEMod::MsgReportLinkState::match(message))
    {
        const DSEMod::MsgReportLinkState& report = (DSEMod::MsgReportLinkState&) message;

        if (report.isHeard()) {
            // The tier's constellation in this profile (ANGEL61.md 2.10)
            const DVAngel61::Profile profile = m_settings.m_mode == DSEModSettings::DSEModModeAngel61WB ? DVAngel61::ProfileWB
                : m_settings.m_mode == DSEModSettings::DSEModModeAngel61HFNB ? DVAngel61::ProfileHFNB
                : m_settings.m_mode == DSEModSettings::DSEModModeAngel61HFWB ? DVAngel61::ProfileHFWB : DVAngel61::ProfileNB;
            const bool qam16 = DVAngel61::constellationOf(DVAngel61::voiceType(profile, report.getRequestedTier())) == DVAngel61::QAM16;
            ui->angel61Link->setText(QString("Link: asked tier %1 (%2), asking %3").arg(report.getRequestedTier()).arg(qam16 ? "16-QAM" : "QPSK").arg(report.getSendTier()));
        } else {
            ui->angel61Link->setText("Link: none");
        }

        return true;
    }
    else if (DSEMod::MsgReportTunnelStatus::match(message))
    {
        const DSEMod::MsgReportTunnelStatus& report = (const DSEMod::MsgReportTunnelStatus&) message;
        m_tunnelStatus = report.getStatus();
        ui->angel61TunnelStatus->setText(m_tunnelStatus);
        return true;
    }
    else if (DSEMod::MsgReportTunnelTraffic::match(message))
    {
        const DSEMod::MsgReportTunnelTraffic& report = (const DSEMod::MsgReportTunnelTraffic&) message;
        m_tunnelTxBytes = report.getTxBytes();
        m_tunnelRxBytes = report.getRxBytes();
        m_tunnelRefusedText = report.getRefused();
        updateTunnelTrafficDisplay();
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        const DSPSignalNotification& notif = (const DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        ui->deltaFrequency->setValueRange(false, 7, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();
        return true;
    }
    else if (DSEMod::MsgConfigureDSEMod::match(message))
    {
        const DSEMod::MsgConfigureDSEMod& cfg = (DSEMod::MsgConfigureDSEMod&) message;

        if (cfg.getForce())
        {
            // The window's own (where it is, whether it is shown, its widgets' states) stay the
            // GUI's: the channel's copy of them is as it was when last sent, and a preset saved
            // afterwards took the window back there
            const int workspaceIndex = m_settings.m_workspaceIndex;
            const QByteArray geometryBytes = m_settings.m_geometryBytes;
            const bool hidden = m_settings.m_hidden;
            Serializable *channelMarker = m_settings.m_channelMarker;
            Serializable *spectrumGUI = m_settings.m_spectrumGUI;
            Serializable *cwKeyerGUI = m_settings.m_cwKeyerGUI;
            Serializable *rollupState = m_settings.m_rollupState;
            m_settings = cfg.getSettings();
            m_settings.m_workspaceIndex = workspaceIndex;
            m_settings.m_geometryBytes = geometryBytes;
            m_settings.m_hidden = hidden;
            m_settings.m_channelMarker = channelMarker;
            m_settings.m_spectrumGUI = spectrumGUI;
            m_settings.m_cwKeyerGUI = cwKeyerGUI;
            m_settings.m_rollupState = rollupState;
        }
        else
        {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        blockApplySettings(true);
        ui->spectrumGUI->updateSettings();
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (CWKeyer::MsgConfigureCWKeyer::match(message))
    {
        const CWKeyer::MsgConfigureCWKeyer& cfg = (CWKeyer::MsgConfigureCWKeyer&) message;
        ui->cwKeyerGUI->setSettings(cfg.getSettings());
        ui->cwKeyerGUI->displaySettings();
        return true;
    }
    else
    {
        return false;
    }
}

void DSEModGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings(QStringList("inputFrequencyOffset"));
}

void DSEModGUI::channelMarkerUpdate()
{
    m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
    displaySettings();
    applySettings(QStringList("rgbColor"));
}

void DSEModGUI::updateTunnelTrafficDisplay()
{
    const QLocale locale;
    ui->angel61TunnelTraffic->setText(tr("IP traffic: TX %1 B   RX %2 B%3")
        .arg(locale.toString(m_tunnelTxBytes))
        .arg(locale.toString(m_tunnelRxBytes))
        .arg(m_tunnelRefusedText.isEmpty() ? QString() : tr("   refused: %1").arg(m_tunnelRefusedText)));
}

void DSEModGUI::handleSourceMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void DSEModGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings(QStringList("inputFrequencyOffset"));
}

QWidget *DSEModGUI::modeTab(int mode)
{
    // The tab holding this mode's parameters: NXDN96 shares NXDN's and P25 Phase 2 shares P25's,
    // since the parameters are the same ones (the tab is named for whichever is selected)
    switch (mode)
    {
    case DSEModSettings::DSEModModeDMR: return ui->dmrPage;
    case DSEModSettings::DSEModModeYSF: return ui->ysfPage;
    case DSEModSettings::DSEModModeNXDN:
    case DSEModSettings::DSEModModeNXDN96: return ui->nxdnPage;
    case DSEModSettings::DSEModModeDPMR: return ui->dpmrPage;
    case DSEModSettings::DSEModModeP25:
    case DSEModSettings::DSEModModeP25P2: return ui->p25Page;
    case DSEModSettings::DSEModModeAngel61:
    case DSEModSettings::DSEModModeAngel61WB:
    case DSEModSettings::DSEModModeAngel61HFNB:     // the callsigns, path and message
    case DSEModSettings::DSEModModeAngel61HFWB: return ui->angel61Page;
    case DSEModSettings::DSEModModeM17: return ui->m17Page;
    case DSEModSettings::DSEModModeDStar:
    default: return ui->dstarPage;
    }
}

void DSEModGUI::updateModeControls()
{
    // M17 encodes with Codec2 and Angel61 with V1 whatever the vocoder setting says, so the
    // encoder, its gain and its options mean nothing there; and a linear or OFDM mode has no
    // symbol polarity to invert
    const bool angel61 = DSEModSettings::isAngel61(m_settings.m_mode);
    bool mbeMode = !angel61 && (m_settings.m_mode != DSEModSettings::DSEModModeM17);
    bool linear = angel61;
    bool feature = m_settings.m_vocoder == DSEModSettings::DSEModVocoderAMBEFeature;

    // Only three of the protocols carry a position: D-STAR as D-PRS slow data, DMR as a GPS
    // Info embedded LC and Angel61 in its position slot
    bool position = (m_settings.m_mode == DSEModSettings::DSEModModeDStar)
        || (m_settings.m_mode == DSEModSettings::DSEModModeDMR)
        || angel61;

    ui->sendPosition->setVisible(position);
    ui->invert->setVisible(!linear);
    ui->vocoderLabel->setVisible(mbeMode);
    ui->vocoder->setVisible(mbeMode);
    ui->gainLabel->setVisible(mbeMode && feature);      // the input gain of a DVSI device
    ui->vocoderGain->setVisible(mbeMode && feature);
    ui->toneDetect->setVisible((mbeMode && !feature) || angel61); // the mbevocoder encoder's, or Angel61's live DTMF (3.8)
    ui->noiseSuppress->setVisible(mbeMode && !feature);
    updateAngel61Rates();
}

void DSEModGUI::updateAngel61Rates()
{
    // The items carry the setting's value: tier 1 (medium) is the last but one value, as it was
    // added after tiers 0 and 2
    ui->angel61Rate->blockSignals(true);
    ui->angel61Rate->clear();
    ui->angel61Rate->addItem("Auto", (int) DSEModSettings::DSEModAngel61RateAuto);

    if (DSEModSettings::isAngel61(m_settings.m_mode))
    {
        // The tiers, V1 at the profile's codebooks, WB's tier 3 Opus (2.3)
        const char *labels[4][4] = {
            { "2.25k QPSK", "3.0k QPSK", "4.5k QPSK", "9.0k 16-QAM" },          // NB
            { "4.5k QPSK", "9.0k QPSK", "12k 16-QAM", "Opus 16-QAM" },          // WB
            { "1.5k QPSK", "2.25k 16-QAM", "3.0k 16-QAM", "4.5k 16-QAM" },      // HF-NB
            { "2.25k QPSK", "4.5k QPSK", "6.0k 16-QAM", "9.0k 16-QAM" } };      // HF-WB
        const int profile = m_settings.m_mode == DSEModSettings::DSEModModeAngel61WB ? 1
            : m_settings.m_mode == DSEModSettings::DSEModModeAngel61HFNB ? 2
            : m_settings.m_mode == DSEModSettings::DSEModModeAngel61HFWB ? 3 : 0;
        ui->angel61Rate->addItem(labels[profile][0], (int) DSEModSettings::DSEModAngel61RateLow);
        ui->angel61Rate->addItem(labels[profile][1], (int) DSEModSettings::DSEModAngel61RateMedium);
        ui->angel61Rate->addItem(labels[profile][2], (int) DSEModSettings::DSEModAngel61RateHigh);
        ui->angel61Rate->addItem(labels[profile][3], (int) DSEModSettings::DSEModAngel61RateTier3);
    }

    ui->angel61Rate->setCurrentIndex(std::max(0, ui->angel61Rate->findData((int) m_settings.m_angel61Rate)));
    ui->angel61Rate->blockSignals(false);
}

void DSEModGUI::updateFunctionTabs()
{
    // One strip holds the selected mode's parameters and everything this station can send. Only
    // the selected mode's tab is there, named for it; the four Angel61 functions are there only
    // in that mode; the audio file, DTMF and the keyer are of any mode. By widget rather than by
    // index, so that the strip does not depend on the order of the tabs
    bool angel61 = DSEModSettings::isAngel61(m_settings.m_mode);
    QWidget *mode = modeTab(m_settings.m_mode);
    QList<QWidget*> modeTabs = {
        ui->dstarPage, ui->dmrPage, ui->ysfPage, ui->nxdnPage,
        ui->dpmrPage, ui->p25Page, ui->angel61Page, ui->m17Page };
    QList<QWidget*> angel61Tabs = {
        ui->angel61MessageTab, ui->angel61APRSTab, ui->angel61FileTab, ui->angel61TunnelTab };
    bool modeAppeared = false;

    for (QWidget *tab : modeTabs)
    {
        int index = ui->functionsWidget->indexOf(tab);

        if (index < 0) {
            continue;
        }

        if (tab == mode)
        {
            // NXDN96 and P25 Phase 2 share a tab with the mode they are a rate or a phase of,
            // so the tab is named for the mode actually selected rather than for the tab
            ui->functionsWidget->setTabText(index, ui->mode->itemText((int) m_settings.m_mode));
            modeAppeared = !ui->functionsWidget->isTabVisible(index);
        }

        ui->functionsWidget->setTabVisible(index, tab == mode);
    }

    for (QWidget *tab : angel61Tabs)
    {
        int index = ui->functionsWidget->indexOf(tab);

        if (index >= 0) {
            ui->functionsWidget->setTabVisible(index, angel61);
        }
    }

    // A tab that has just gone leaves the operator looking at nothing in particular, so the new
    // mode's parameters take the front. A tab that was already there is left alone
    QWidget *current = ui->functionsWidget->currentWidget();

    if (modeAppeared || modeTabs.contains(current) || (!angel61 && angel61Tabs.contains(current))) {
        ui->functionsWidget->setCurrentWidget(mode);
    }
}

void DSEModGUI::on_mode_currentIndexChanged(int index)
{
    m_settings.m_mode = (DSEModSettings::DSEModMode) index;
    updateFunctionTabs();
    updateModeControls();
    applySettings(QStringList("mode"));
}

void DSEModGUI::on_invert_toggled(bool checked)
{
    m_settings.m_invert = checked;
    applySettings(QStringList("invert"));
}

void DSEModGUI::on_transmit_toggled(bool checked)
{
    m_settings.m_transmit = checked;
    applySettings(QStringList("transmit"));
}

void DSEModGUI::on_toneFrequency_valueChanged(int value)
{
    ui->toneFrequencyText->setText(QString("%1k").arg(value / 100.0, 0, 'f', 2));
    m_settings.m_toneFrequency = value * 10.0;
    applySettings(QStringList("toneFrequency"));
}

void DSEModGUI::on_volume_valueChanged(int value)
{
    ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
    m_settings.m_volumeFactor = value / 10.0;
    applySettings(QStringList("volumeFactor"));
}

void DSEModGUI::setInput(DSEModSettings::DSEModInputAF input, bool checked)
{
    m_settings.m_modAFInput = checked ? input : DSEModSettings::DSEModInputNone;
    displayInputSelection(true);
    applySettings(QStringList("modAFInput"));
}

void DSEModGUI::on_play_toggled(bool checked)
{
    setInput(DSEModSettings::DSEModInputFile, checked);
}

void DSEModGUI::on_tone_toggled(bool checked)
{
    setInput(DSEModSettings::DSEModInputTone, checked);
}

void DSEModGUI::on_morseKeyer_toggled(bool checked)
{
    setInput(DSEModSettings::DSEModInputCWTone, checked);
}

void DSEModGUI::on_mic_toggled(bool checked)
{
    setInput(DSEModSettings::DSEModInputAudio, checked);
}

void DSEModGUI::on_feedbackEnable_toggled(bool checked)
{
    m_settings.m_feedbackAudioEnable = checked;
    applySettings(QStringList("feedbackAudioEnable"));
}

void DSEModGUI::on_spectrumAudio_toggled(bool checked)
{
    m_settings.m_spectrumAudio = checked;
    updateSpectrumSource();
    applySettings(QStringList("spectrumAudio"));
}

// IQ: the 48 kS/s baseband, both sides of the carrier. Audio: the encoder input at its rate, real,
// shown from 0 Hz to half the rate
void DSEModGUI::updateSpectrumSource()
{
    bool audio = m_settings.m_spectrumAudio;
    int rate = audio ? m_dseMod->getPcmSampleRate() : DSEModSource::m_basebandSampleRate;

    if (rate <= 0) {
        rate = 8000;
    }

    m_spectrumRate = audio ? rate : -rate;
    ui->glSpectrum->setCenterFrequency(0);
    ui->glSpectrum->setSampleRate(rate);
    ui->glSpectrum->setSsbSpectrum(audio);
    ui->glSpectrum->setLsbDisplay(false);
    SpectrumSettings spectrumSettings = m_spectrumVis->getSettings();
    spectrumSettings.m_ssb = audio;
    SpectrumVis::MsgConfigureSpectrumVis *msg = SpectrumVis::MsgConfigureSpectrumVis::create(spectrumSettings, false);
    m_spectrumVis->getInputMessageQueue()->push(msg);
}

void DSEModGUI::on_feedbackVolume_valueChanged(int value)
{
    ui->feedbackVolumeText->setText(QString("%1").arg(value / 100.0, 0, 'f', 2));
    m_settings.m_feedbackVolumeFactor = (float) value / 100.0f;
    applySettings(QStringList("feedbackVolumeFactor"));
}

void DSEModGUI::on_playLoop_toggled(bool checked)
{
    m_settings.m_playLoop = checked;
    applySettings(QStringList("playLoop"));
}

void DSEModGUI::on_vocoder_currentIndexChanged(int index)
{
    if ((index < 0) || (index >= ui->vocoder->count())) {
        return;
    }

    int feature = ui->vocoder->itemData(index).toInt();

    if (feature >= 0)
    {
        m_settings.m_vocoder = DSEModSettings::DSEModVocoderAMBEFeature;
        m_settings.m_ambeFeatureIndex = feature;
        m_settings.m_connectAMBE = true;
    }
    else
    {
        m_settings.m_vocoder = DSEModSettings::DSEModVocoderMbelib;
        m_settings.m_connectAMBE = false;
    }

    updateModeControls();
    applySettings(QStringList({"vocoder", "ambeFeatureIndex", "connectAMBE"}));
}



void DSEModGUI::on_vocoderGain_valueChanged(int value)
{
    m_settings.m_vocoderGain = value;
    applySettings(QStringList("vocoderGain"));
}

void DSEModGUI::on_toneDetect_toggled(bool checked)
{
    m_settings.m_toneDetect = checked;
    applySettings(QStringList("toneDetect"));
}

void DSEModGUI::on_noiseSuppress_toggled(bool checked)
{
    m_settings.m_noiseSuppress = checked;
    applySettings(QStringList("noiseSuppress"));
}

void DSEModGUI::on_dstarMyCall_editingFinished()
{
    m_settings.m_dstarMyCall = ui->dstarMyCall->text().toUpper();
    ui->dstarMyCall->setText(m_settings.m_dstarMyCall);
    applySettings(QStringList("dstarMyCall"));
}

void DSEModGUI::on_dstarMySuffix_editingFinished()
{
    m_settings.m_dstarMySuffix = ui->dstarMySuffix->text().toUpper();
    ui->dstarMySuffix->setText(m_settings.m_dstarMySuffix);
    applySettings(QStringList("dstarMySuffix"));
}

void DSEModGUI::on_dstarUrCall_editingFinished()
{
    m_settings.m_dstarUrCall = ui->dstarUrCall->text().toUpper();
    ui->dstarUrCall->setText(m_settings.m_dstarUrCall);
    applySettings(QStringList("dstarUrCall"));
}

void DSEModGUI::on_dstarRpt1_editingFinished()
{
    m_settings.m_dstarRpt1 = ui->dstarRpt1->text().toUpper();
    ui->dstarRpt1->setText(m_settings.m_dstarRpt1);
    applySettings(QStringList("dstarRpt1"));
}

void DSEModGUI::on_dstarRpt2_editingFinished()
{
    m_settings.m_dstarRpt2 = ui->dstarRpt2->text().toUpper();
    ui->dstarRpt2->setText(m_settings.m_dstarRpt2);
    applySettings(QStringList("dstarRpt2"));
}

void DSEModGUI::on_dstarText_editingFinished()
{
    m_settings.m_dstarText = ui->dstarText->text();
    applySettings(QStringList("dstarText"));
}

void DSEModGUI::on_dtmf_editingFinished()
{
    m_settings.m_dtmfSequence = ui->dtmf->text().toUpper();
    applySettings(QStringList("dtmfSequence"));
}

void DSEModGUI::on_dtmfSend_clicked()
{
    m_dseMod->getInputMessageQueue()->push(DSEModSource::MsgSendDTMF::create(ui->dtmf->text().toUpper()));
}

void DSEModGUI::on_m17Source_editingFinished()
{
    m_settings.m_m17Source = ui->m17Source->text();
    applySettings(QStringList("m17Source"));
}

void DSEModGUI::on_m17Destination_editingFinished()
{
    m_settings.m_m17Destination = ui->m17Destination->text();
    applySettings(QStringList("m17Destination"));
}

void DSEModGUI::on_m17CAN_valueChanged(int value)
{
    m_settings.m_m17CAN = value;
    applySettings(QStringList("m17CAN"));
}

void DSEModGUI::on_m17Message_editingFinished()
{
    m_settings.m_m17Message = ui->m17Message->text();
    applySettings(QStringList("m17Message"));
}

void DSEModGUI::on_m17Send_clicked()
{
    // An SMS packet transmission of its own
    m_dseMod->getInputMessageQueue()->push(DSEModSource::MsgSendMessage::create(ui->m17Message->text()));
}

void DSEModGUI::on_angel61APRSSend_clicked()
{
    // An information field is a transmission of its own: content type 2, keyed up and down by
    // the channel. Nothing is kept in the settings, since it is one packet rather than a station
    // property - a text message is the thing an operator sends over and over
    QString info = ui->angel61APRSInfo->text().trimmed();

    if (!info.isEmpty()) {
        m_dseMod->getInputMessageQueue()->push(DSEModSource::MsgSendAPRS::create(info));
    }
}

void DSEModGUI::on_angel61Symbol_clicked()
{
    DSEModAPRSSymbolDialog dialog(m_settings.m_angel61Symbol, this);
    new DialogPositioner(&dialog, true);

    if (dialog.exec() == QDialog::Accepted)
    {
        m_settings.m_angel61Symbol = dialog.symbol();
        displayAPRSSymbol();
        applySettings(QStringList("angel61Symbol"));
    }
}

void DSEModGUI::displayAPRSSymbol()
{
    // The symbol's icon, with the two characters under it when the APRS feature that holds the
    // icons is not in this build
    QPixmap pixmap = DSEModAPRSSymbolDialog::symbolPixmap(m_settings.m_angel61Symbol);

    ui->angel61Symbol->setIcon(pixmap.isNull() ? QIcon() : QIcon(pixmap));
    ui->angel61Symbol->setText(pixmap.isNull()
        ? (m_settings.m_angel61Symbol.isEmpty() ? QString("...") : m_settings.m_angel61Symbol)
        : QString());
}

void DSEModGUI::on_angel61Tunnel_toggled(bool checked)
{
    m_settings.m_angel61Tunnel = checked;
    m_tunnelStatus = checked ? QStringLiteral("Tunnel: starting...") : QStringLiteral("Tunnel: off");
    ui->angel61TunnelStatus->setText(m_tunnelStatus);
    applySettings(QStringList("angel61Tunnel"));
}

void DSEModGUI::on_angel61TunnelPeer_editingFinished()
{
    m_settings.m_angel61TunnelPeer = ui->angel61TunnelPeer->text().toUpper();
    ui->angel61TunnelPeer->setText(m_settings.m_angel61TunnelPeer);
    applySettings(QStringList("angel61TunnelPeer"));
}

void DSEModGUI::on_angel61TunnelAddress_editingFinished()
{
    m_settings.m_angel61TunnelAddress = ui->angel61TunnelAddress->text();
    applySettings(QStringList("angel61TunnelAddress"));
}

void DSEModGUI::on_angel61TunnelAllow_editingFinished()
{
    m_settings.m_angel61TunnelAllow = ui->angel61TunnelAllow->text();
    applySettings(QStringList("angel61TunnelAllow"));
}

void DSEModGUI::on_angel61TunnelGroups_editingFinished()
{
    m_settings.m_angel61TunnelGroups = ui->angel61TunnelGroups->text();
    applySettings(QStringList("angel61TunnelGroups"));
}

void DSEModGUI::on_angel61TunnelRoutes_editingFinished()
{
    m_settings.m_angel61TunnelRoutes = ui->angel61TunnelRoutes->text();
    applySettings(QStringList("angel61TunnelRoutes"));
}

void DSEModGUI::on_angel61TunnelInterface_editingFinished()
{
    m_settings.m_angel61TunnelInterface = ui->angel61TunnelInterface->text();
    applySettings(QStringList("angel61TunnelInterface"));
}

void DSEModGUI::on_angel61TunnelMTU_valueChanged(int value)
{
    m_settings.m_angel61TunnelMTU = value;
    applySettings(QStringList("angel61TunnelMTU"));
}

void DSEModGUI::on_angel61FileName_editingFinished()
{
    m_settings.m_angel61FileName = ui->angel61FileName->text();
    applySettings(QStringList("angel61FileName"));
}

void DSEModGUI::on_angel61FileBrowse_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("File to send"), m_settings.m_angel61FileName);

    if (!fileName.isEmpty())
    {
        m_settings.m_angel61FileName = fileName;
        ui->angel61FileName->setText(fileName);
        applySettings(QStringList("angel61FileName"));
    }
}

void DSEModGUI::on_angel61FileSend_clicked()
{
    if (m_settings.m_angel61FileName.isEmpty())
    {
        on_angel61FileBrowse_clicked();

        if (m_settings.m_angel61FileName.isEmpty()) {
            return;
        }
    }

    m_dseMod->getInputMessageQueue()->push(DSEModSource::MsgSendFile::create(m_settings.m_angel61FileName, QByteArray()));
}

void DSEModGUI::on_angel61Demod_currentIndexChanged(int index)
{
    // Index 0 is "Any": every DSD Demodulator in the instance
    m_settings.m_angel61Demod = index <= 0 ? QString() : ui->angel61Demod->itemData(index).toString();
    applySettings(QStringList("angel61Demod"));
    updateAngel61Destinations();
}

void DSEModGUI::on_angel61Rate_currentIndexChanged(int index)
{
    m_settings.m_angel61Rate = (DSEModSettings::DSEModAngel61Rate) ui->angel61Rate->itemData(index).toInt();
    applySettings(QStringList("angel61Rate"));
}

void DSEModGUI::on_angel61Source_editingFinished()
{
    m_settings.m_angel61Source = ui->angel61Source->text().trimmed().toUpper();
    ui->angel61Source->setText(m_settings.m_angel61Source);
    applySettings(QStringList("angel61Source"));
}

void DSEModGUI::on_angel61Destination_editingFinished()
{
    // A talkgroup offered by a gateway is shown with its name: TG 9 is what is sent
    const QString text = ui->angel61Destination->currentText().trimmed();
    m_settings.m_angel61Destination = m_angel61DestinationCodes.value(text, text).toUpper();
    ui->angel61Destination->setEditText(m_settings.m_angel61Destination);
    applySettings(QStringList("angel61Destination"));
}

void DSEModGUI::on_angel61Path_editingFinished()
{
    m_settings.m_angel61Path = ui->angel61Path->text().trimmed().toUpper();
    ui->angel61Path->setText(m_settings.m_angel61Path);
    applySettings(QStringList("angel61Path"));
}

void DSEModGUI::on_sendPosition_toggled(bool checked)
{
    m_settings.m_sendPosition = checked;
    applySettings(QStringList("sendPosition"));
}

void DSEModGUI::on_angel61Message_editingFinished()
{
    m_settings.m_angel61Message = ui->angel61Message->text();
    applySettings(QStringList("angel61Message"));
}

void DSEModGUI::on_angel61Send_clicked()
{
    // The text as typed (the setting follows it too, so that a data only transmission can carry it)
    if (m_settings.m_angel61Message != ui->angel61Message->text())
    {
        m_settings.m_angel61Message = ui->angel61Message->text();
        applySettings(QStringList("angel61Message"));
    }

    m_dseMod->getInputMessageQueue()->push(DSEModSource::MsgSendMessage::create(ui->angel61Message->text()));
}


void DSEModGUI::on_dmrColorCode_valueChanged(int value)
{
    m_settings.m_dmrColorCode = value;
    applySettings(QStringList("dmrColorCode"));
}

void DSEModGUI::on_dmrSourceId_valueChanged(int value)
{
    m_settings.m_dmrSourceId = value;
    applySettings(QStringList("dmrSourceId"));
}

void DSEModGUI::on_dmrDestinationId_valueChanged(int value)
{
    m_settings.m_dmrDestinationId = value;
    applySettings(QStringList("dmrDestinationId"));
}

void DSEModGUI::on_dmrCallType_currentIndexChanged(int index)
{
    m_settings.m_dmrGroupCall = (index == 0);
    applySettings(QStringList("dmrGroupCall"));
}

void DSEModGUI::on_dmrSlot_currentIndexChanged(int index)
{
    m_settings.m_dmrSlot = index + 1;
    applySettings(QStringList("dmrSlot"));
}

void DSEModGUI::on_dmrDirect_currentIndexChanged(int index)
{
    m_settings.m_dmrDirect = index == 1;
    applySettings(QStringList("dmrDirect"));
}

void DSEModGUI::on_ysfCallsign_editingFinished()
{
    m_settings.m_ysfCallsign = ui->ysfCallsign->text().toUpper();
    ui->ysfCallsign->setText(m_settings.m_ysfCallsign);
    applySettings(QStringList("ysfCallsign"));
}

void DSEModGUI::on_ysfDestination_editingFinished()
{
    m_settings.m_ysfDestination = ui->ysfDestination->text().toUpper();
    ui->ysfDestination->setText(m_settings.m_ysfDestination);
    applySettings(QStringList("ysfDestination"));
}

void DSEModGUI::on_ysfDownlink_editingFinished()
{
    m_settings.m_ysfDownlink = ui->ysfDownlink->text().toUpper();
    ui->ysfDownlink->setText(m_settings.m_ysfDownlink);
    applySettings(QStringList("ysfDownlink"));
}

void DSEModGUI::on_ysfMode_currentIndexChanged(int index)
{
    m_settings.m_ysfMode = (DSEModSettings::DSEModYSFMode) std::max(0, std::min((int) DSEModSettings::DSEModYSFModeVW, index));
    applySettings(QStringList("ysfMode"));
}

void DSEModGUI::on_ysfUplink_editingFinished()
{
    m_settings.m_ysfUplink = ui->ysfUplink->text().toUpper();
    ui->ysfUplink->setText(m_settings.m_ysfUplink);
    applySettings(QStringList("ysfUplink"));
}

void DSEModGUI::on_nxdnRan_valueChanged(int value)
{
    m_settings.m_nxdnRan = value;
    applySettings(QStringList("nxdnRan"));
}

void DSEModGUI::on_nxdnSourceId_valueChanged(int value)
{
    m_settings.m_nxdnSourceId = value;
    applySettings(QStringList("nxdnSourceId"));
}

void DSEModGUI::on_nxdnDestinationId_valueChanged(int value)
{
    m_settings.m_nxdnDestinationId = value;
    applySettings(QStringList("nxdnDestinationId"));
}

void DSEModGUI::on_nxdnCallType_currentIndexChanged(int index)
{
    m_settings.m_nxdnGroupCall = (index == 0);
    applySettings(QStringList("nxdnGroupCall"));
}

void DSEModGUI::on_dpmrColorCode_valueChanged(int value)
{
    m_settings.m_dpmrColorCode = value;
    applySettings(QStringList("dpmrColorCode"));
}

void DSEModGUI::on_dpmrOwnId_valueChanged(int value)
{
    m_settings.m_dpmrOwnId = value;
    applySettings(QStringList("dpmrOwnId"));
}

void DSEModGUI::on_dpmrCalledId_valueChanged(int value)
{
    m_settings.m_dpmrCalledId = value;
    applySettings(QStringList("dpmrCalledId"));
}

void DSEModGUI::on_p25Nac_valueChanged(int value)
{
    m_settings.m_p25Nac = value;
    applySettings(QStringList("p25Nac"));
}

void DSEModGUI::on_p25TalkGroup_valueChanged(int value)
{
    m_settings.m_p25TalkGroup = value;
    applySettings(QStringList("p25TalkGroup"));
}

void DSEModGUI::on_p25SourceId_valueChanged(int value)
{
    m_settings.m_p25SourceId = value;
    applySettings(QStringList("p25SourceId"));
}

void DSEModGUI::on_navTimeSlider_valueChanged(int value)
{
    if (m_enableNavTime && ((value >= 0) && (value <= 100)))
    {
        DSEMod::MsgConfigureFileSourceSeek* message = DSEMod::MsgConfigureFileSourceSeek::create(value);
        m_dseMod->getInputMessageQueue()->push(message);
    }
}

void DSEModGUI::on_showFileDialog_clicked(bool checked)
{
    (void) checked;
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Open audio file"), ".", tr("Audio files (*.wav *.raw);;WAV files (*.wav);;Raw audio files (*.raw)"), 0, QFileDialog::DontUseNativeDialog);

    if (fileName != "")
    {
        m_settings.m_audioFileName = fileName;
        ui->recordFileText->setText(fileName);
        ui->play->setEnabled(true);
        applySettings(QStringList("audioFileName"));
    }
}

void DSEModGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings(QStringList());
}

void DSEModGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_dseMod->getNumberOfDeviceStreams());
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

        applySettings(QStringList({
            "rgbColor",
            "title",
            "useReverseAPI",
            "reverseAPIAddress",
            "reverseAPIPort",
            "reverseAPIDeviceIndex",
            "reverseAPIChannelIndex",
            "streamIndex"
        }));
    }

    resetContextMenuType();
}

DSEModGUI::DSEModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::DSEModGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
    m_doApplySettings(true),
    m_recordLength(0),
    m_recordSampleRate(8000),
    m_samplesCount(0),
    m_audioSampleRate(-1),
    m_tickCount(0),
    m_enableNavTime(false)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channeltx/moddse/readme.md";
    RollupContents *rollupContents = getRollupContents();
    ui->setupUi(rollupContents);
    ui->dtmf->setValidator(new QRegularExpressionValidator(QRegularExpression("[0-9A-Da-d*#]*"), this));
    ui->angel61Destination->lineEdit()->setMaxLength(80); // a talkgroup's name as the list shows it

    // Numbers applied when they are entered, not at each digit typed (an ID's first digits were
    // applied as IDs of their own; an MTU's, as a tunnel restarted at each)
    for (QSpinBox *box : { ui->dmrSourceId, ui->dmrDestinationId, ui->nxdnSourceId, ui->nxdnDestinationId,
        ui->dpmrOwnId, ui->dpmrCalledId, ui->p25Nac, ui->p25TalkGroup, ui->p25SourceId, ui->angel61TunnelMTU }) {
        box->setKeyboardTracking(false);
    }
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
    connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_dseMod = (DSEMod*) channelTx;
    m_spectrumVis = m_dseMod->getSpectrumVis();
    m_spectrumVis->setGLSpectrum(ui->glSpectrum);
    m_dseMod->setMessageQueueToGUI(getInputMessageQueue());

    resetToDefaults();

    ui->spectrumGUI->setBuddies(m_spectrumVis, ui->glSpectrum);
    ui->glSpectrum->setCenterFrequency(0);
    ui->glSpectrum->setSampleRate(DSEModSource::m_basebandSampleRate);

    SpectrumSettings spectrumSettings = m_spectrumVis->getSettings();
    spectrumSettings.m_displayWaterfall = true;
    spectrumSettings.m_displayMaxHold = false;
    spectrumSettings.m_ssb = false;
    SpectrumVis::MsgConfigureSpectrumVis *msg = SpectrumVis::MsgConfigureSpectrumVis::create(spectrumSettings, false);
    m_spectrumVis->getInputMessageQueue()->push(msg);

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

    CRightClickEnabler *audioMuteRightClickEnabler = new CRightClickEnabler(ui->mic);
    connect(audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect(const QPoint &)));

    CRightClickEnabler *feedbackRightClickEnabler = new CRightClickEnabler(ui->feedbackEnable);
    connect(feedbackRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioFeedbackSelect(const QPoint &)));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

    m_channelMarker.setSourceOrSinkStream(false);
    m_channelMarker.setVisible(true);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));

    ui->cwKeyerGUI->setCWKeyer(m_dseMod->getCWKeyer());

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setSpectrumGUI(ui->spectrumGUI);
    m_settings.setCWKeyerGUI(ui->cwKeyerGUI);
    m_settings.setRollupState(&m_rollupState);

    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
    m_dseMod->setLevelMeter(ui->volumeMeter);

    displaySettings();
    makeUIConnections();
    m_resizer.enableChildMouseTracking();
    applySettings(QStringList(), true);
    DialPopup::addPopupsToChildDials(this);

    m_dseMod->getInputMessageQueue()->push(DSEMod::MsgQueryAvailableAMBEFeatures::create());
    m_dseMod->getInputMessageQueue()->push(DSEMod::MsgQueryAvailableDemodulators::create());
}

DSEModGUI::~DSEModGUI()
{
    delete ui;
}

bool DSEModGUI::blockApplySettings(bool block)
{
    bool ret = !m_doApplySettings;
    m_doApplySettings = !block;
    return ret;
}

void DSEModGUI::applySettings(const QStringList& settingsKeys, bool force)
{
    if (m_doApplySettings)
    {
        DSEMod::MsgConfigureDSEMod *msg = DSEMod::MsgConfigureDSEMod::create(settingsKeys, m_settings, force);
        m_dseMod->getInputMessageQueue()->push(msg);
    }
}

void DSEModGUI::displayInputSelection(bool inputChanged)
{
    // The inputs are one choice, so they behave as one: clicking another takes the selection
    // rather than doing nothing until the first is clicked off. The signals are blocked while
    // the buttons are set, or unchecking the old one would report "no input" over the new one
    DSEModSettings::DSEModInputAF input = m_settings.m_modAFInput;
    QList<ButtonSwitch*> buttons = { ui->tone, ui->mic, ui->play, ui->morseKeyer };

    for (ButtonSwitch *button : buttons) {
        button->blockSignals(true);
    }

    ui->tone->setChecked(input == DSEModSettings::DSEModInputTone);
    ui->mic->setChecked(input == DSEModSettings::DSEModInputAudio);
    ui->play->setChecked(input == DSEModSettings::DSEModInputFile);
    ui->morseKeyer->setChecked(input == DSEModSettings::DSEModInputCWTone);

    for (ButtonSwitch *button : buttons)
    {
        button->setEnabled(true);
        button->blockSignals(false);
    }

    // The tone's frequency belongs to the tone
    bool tone = input == DSEModSettings::DSEModInputTone;
    ui->toneFrequency->setVisible(tone);
    ui->toneFrequencyText->setVisible(tone);

    // The file is sought while it is not playing (whatever set the input: a preset, the API)
    bool file = input == DSEModSettings::DSEModInputFile;
    ui->navTimeSlider->setEnabled(!file);
    m_enableNavTime = !file;

    // And the tab of whatever is going to be sent comes to the front when it is chosen (not at
    // every settings update, taking the operator from the tab they were at)
    if (inputChanged && file) {
        ui->functionsWidget->setCurrentWidget(ui->audioFileTab);
    } else if (inputChanged && (input == DSEModSettings::DSEModInputCWTone)) {
        ui->functionsWidget->setCurrentWidget(ui->cwKeyerTab);
    }
}

void DSEModGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor);

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());
    updateIndexLabel();

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_settings.m_inputFrequencyOffset);
    ui->mode->setCurrentIndex((int) m_settings.m_mode);
    updateFunctionTabs();
    updateModeControls();
    ui->invert->setChecked(m_settings.m_invert);
    ui->transmit->setChecked(m_settings.m_transmit);
    ui->playLoop->setChecked(m_settings.m_playLoop);

    ui->toneFrequency->setValue(roundf(m_settings.m_toneFrequency / 10.0));
    ui->toneFrequencyText->setText(QString("%1k").arg(m_settings.m_toneFrequency / 1000.0, 0, 'f', 2));

    ui->volume->setValue((int) roundf(m_settings.m_volumeFactor * 10.0f));
    ui->volumeText->setText(QString("%1").arg(m_settings.m_volumeFactor, 0, 'f', 1));

    displayInputSelection();

    ui->vocoderGain->setValue(m_settings.m_vocoderGain);
    ui->toneDetect->setChecked(m_settings.m_toneDetect);
    ui->noiseSuppress->setChecked(m_settings.m_noiseSuppress);
    updateAMBEFeaturesList();

    ui->dstarMyCall->setText(m_settings.m_dstarMyCall);
    ui->dstarMySuffix->setText(m_settings.m_dstarMySuffix);
    ui->dstarUrCall->setText(m_settings.m_dstarUrCall);
    ui->dstarRpt1->setText(m_settings.m_dstarRpt1);
    ui->dstarRpt2->setText(m_settings.m_dstarRpt2);
    ui->dstarText->setText(m_settings.m_dstarText);
    ui->feedbackEnable->setChecked(m_settings.m_feedbackAudioEnable);
    ui->spectrumAudio->setChecked(m_settings.m_spectrumAudio);
    updateSpectrumSource();
    ui->feedbackVolume->setValue((int) roundf(m_settings.m_feedbackVolumeFactor * 100.0f));
    ui->feedbackVolumeText->setText(QString("%1").arg(m_settings.m_feedbackVolumeFactor, 0, 'f', 2));

    ui->dtmf->setText(m_settings.m_dtmfSequence);

    ui->dmrColorCode->setValue(m_settings.m_dmrColorCode);
    ui->dmrSourceId->setValue(m_settings.m_dmrSourceId);
    ui->dmrDestinationId->setValue(m_settings.m_dmrDestinationId);
    ui->dmrCallType->setCurrentIndex(m_settings.m_dmrGroupCall ? 0 : 1);
    ui->dmrSlot->setCurrentIndex(m_settings.m_dmrSlot == 2 ? 1 : 0);
    ui->dmrDirect->setCurrentIndex(m_settings.m_dmrDirect ? 1 : 0);

    ui->ysfCallsign->setText(m_settings.m_ysfCallsign);
    ui->ysfDestination->setText(m_settings.m_ysfDestination);
    ui->ysfDownlink->setText(m_settings.m_ysfDownlink);
    ui->ysfUplink->setText(m_settings.m_ysfUplink);
    ui->ysfMode->setCurrentIndex((int) m_settings.m_ysfMode);
    ui->nxdnRan->setValue(m_settings.m_nxdnRan);
    ui->nxdnSourceId->setValue(m_settings.m_nxdnSourceId);
    ui->nxdnDestinationId->setValue(m_settings.m_nxdnDestinationId);
    ui->nxdnCallType->setCurrentIndex(m_settings.m_nxdnGroupCall ? 0 : 1);
    ui->dpmrColorCode->setValue(m_settings.m_dpmrColorCode);
    ui->dpmrOwnId->setValue(m_settings.m_dpmrOwnId);
    ui->dpmrCalledId->setValue(m_settings.m_dpmrCalledId);
    ui->p25Nac->setValue(m_settings.m_p25Nac);
    ui->p25TalkGroup->setValue(m_settings.m_p25TalkGroup);
    ui->p25SourceId->setValue(m_settings.m_p25SourceId);
    updateAngel61Rates();
    displayAPRSSymbol();
    ui->m17Source->setText(m_settings.m_m17Source);
    ui->m17Destination->setText(m_settings.m_m17Destination);
    ui->m17CAN->setValue(m_settings.m_m17CAN);
    ui->m17Message->setText(m_settings.m_m17Message);
    ui->angel61FileName->setText(m_settings.m_angel61FileName);
    ui->angel61Tunnel->blockSignals(true);     // (its handler sets the status: the tunnel's own report is kept)
    ui->angel61Tunnel->setChecked(m_settings.m_angel61Tunnel);
    ui->angel61Tunnel->blockSignals(false);
    ui->angel61TunnelPeer->setText(m_settings.m_angel61TunnelPeer);
    ui->angel61TunnelAddress->setText(m_settings.m_angel61TunnelAddress);
    ui->angel61TunnelRoutes->setText(m_settings.m_angel61TunnelRoutes);
    ui->angel61TunnelAllow->setText(m_settings.m_angel61TunnelAllow);
    ui->angel61TunnelGroups->setText(m_settings.m_angel61TunnelGroups);
    ui->angel61TunnelInterface->setText(m_settings.m_angel61TunnelInterface);
    ui->angel61TunnelMTU->setValue(m_settings.m_angel61TunnelMTU);
    if (!m_settings.m_angel61Tunnel) {
        m_tunnelStatus = QStringLiteral("Tunnel: off");
    } else if (m_tunnelStatus == QStringLiteral("Tunnel: off")) {
        m_tunnelStatus = QStringLiteral("Tunnel: starting...");
    }
    ui->angel61TunnelStatus->setText(m_tunnelStatus);
    updateDemodulatorsList();
    ui->angel61Source->setText(m_settings.m_angel61Source);
    ui->angel61Destination->setEditText(m_settings.m_angel61Destination);
    ui->angel61Path->setText(m_settings.m_angel61Path);
    ui->sendPosition->setChecked(m_settings.m_sendPosition);
    ui->angel61Message->setText(m_settings.m_angel61Message);
    ui->recordFileText->setText(m_settings.m_audioFileName.isEmpty() ? QString("...") : m_settings.m_audioFileName);

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void DSEModGUI::updateAMBEFeaturesList()
{
    // One list of encoders: the software one when it is built in, then every AMBE feature the
    // instance has. Choosing a feature is what connects to it - there is nothing else to say
    ui->vocoder->blockSignals(true);
    ui->vocoder->clear();

    if (DSEModVocoderMbelib::isAvailable()) {
        ui->vocoder->addItem("mbevocoder", -1);   // -1: the software encoder rather than a feature
    }

    for (int i = 0; i < m_availableAMBEFeatures.count(); i++)
    {
        ui->vocoder->addItem(tr("AMBE F:%1").arg(m_availableAMBEFeatures[i].m_featureIndex),
            m_availableAMBEFeatures[i].m_featureIndex);
    }

    bool any = ui->vocoder->count() > 0;
    int select = -1;

    if (m_settings.m_vocoder == DSEModSettings::DSEModVocoderAMBEFeature)
    {
        select = ui->vocoder->findData(m_settings.m_ambeFeatureIndex);

        // A feature not here now (removed, or a preset loaded before it) is shown as asked for, not
        // replaced: the channel connects it again when it comes back
        if (select < 0)
        {
            ui->vocoder->addItem(tr("AMBE F:%1 (absent)").arg(m_settings.m_ambeFeatureIndex), m_settings.m_ambeFeatureIndex);
            select = ui->vocoder->count() - 1;
        }
    }
    else if (DSEModVocoderMbelib::isAvailable())
    {
        select = ui->vocoder->findData(-1);
    }

    if (ui->vocoder->count() == 0) {
        ui->vocoder->addItem(tr("none"), -2);   // nothing to encode with: silence
    }

    ui->vocoder->setEnabled(any);

    ui->vocoder->setCurrentIndex(select >= 0 ? select : 0);
    ui->vocoder->blockSignals(false);

    // The software encoder asked for but not built in falls back to what the list actually offers
    if ((select < 0) && any) {
        on_vocoder_currentIndexChanged(0);
    }

    updateModeControls();   // a feature added or removed changes what the gain and options mean
}


void DSEModGUI::updateDemodulatorsList()
{
    // "Any" and the DSD Demodulators of the instance; a setting naming one that is not there
    // (a preset loaded before its receiver) is kept as an item of its own so it is not lost
    ui->angel61Demod->blockSignals(true);
    ui->angel61Demod->clear();
    ui->angel61Demod->addItem("Any", QString());
    int selected = 0;

    for (const AvailableChannelOrFeature& demodulator : m_availableDemodulators)
    {
        ui->angel61Demod->addItem(demodulator.getLongId(), demodulator.getId());

        if (demodulator.getId() == m_settings.m_angel61Demod) {
            selected = ui->angel61Demod->count() - 1;
        }
    }

    if (!m_settings.m_angel61Demod.isEmpty() && (selected == 0))
    {
        ui->angel61Demod->addItem(m_settings.m_angel61Demod, m_settings.m_angel61Demod);
        selected = ui->angel61Demod->count() - 1;
    }

    ui->angel61Demod->setCurrentIndex(selected);
    ui->angel61Demod->blockSignals(false);
    updateAngel61Destinations();
}

void DSEModGUI::updateAngel61Destinations()
{
    QStringList options{QStringLiteral("CQ")};

    for (const AvailableChannelOrFeature& demodulator : m_availableDemodulators)
    {
        if (!m_settings.m_angel61Demod.isEmpty() && (demodulator.getId() != m_settings.m_angel61Demod)) {
            continue;
        }

        ChannelAPI *channel = qobject_cast<ChannelAPI *>(demodulator.m_object);
        if (!channel) {
            continue;
        }

        SWGSDRangel::SWGChannelReport response;
        QString errorMessage;

        if ((channel->webapiReportGet(response, errorMessage) != 200) || !response.getDsdDemodReport()) {
            continue;
        }

        QList<QString*> *callsigns = response.getDsdDemodReport()->getAngel61HeardCallsigns();
        if (!callsigns) {
            continue;
        }

        for (const QString *callsign : *callsigns)
        {
            if (callsign && !callsign->isEmpty() && !options.contains(*callsign, Qt::CaseInsensitive)) {
                options.append(callsign->toUpper());
            }
        }
    }

    // The talkgroups of the server of every DSGateway that transmits through this modulator: only
    // the gateways know the server (ANGEL61.md 2.6)
    QHash<QString, QString> codes;
    const QString channelId = QString("T%1:%2").arg(m_dseMod->getDeviceSetIndex()).arg(m_dseMod->getIndexInDeviceSet());

    /*
    for (FeatureSet *featureSet : MainCore::instance()->getFeatureeSets())
    {
        for (int i = 0; featureSet && (i < featureSet->getNumberOfFeatures()); i++)
        {
            Feature *feature = featureSet->getFeatureAt(i);

            if (!feature || (feature->getURI() != "sdrangel.feature.dsgateway")) {
                continue;
            }

            SWGSDRangel::SWGFeatureReport report;
            QString errorMessage;

            if ((feature->webapiReportGet(report, errorMessage) != 200) || !report.getDsGatewayReport()
                || !report.getDsGatewayReport()->getTxChannelId()
                || (report.getDsGatewayReport()->getTxChannelId()->section(' ', 0, 0) != channelId)) {
                continue;
            }

            for (const QJsonValue& value : report.getDsGatewayReport()->getAngel61Talkgroups())
            {
                const QJsonObject group = value.toObject();
                const QString code = QString("TG %1").arg(quint32(group["id"].toDouble()));
                const QString name = group["name"].toString();
                const QString text = name.isEmpty() ? code : QString("%1 - %2").arg(code, name);

                if (!options.contains(text) && !options.contains(code)) {
                    options.append(text);
                    codes.insert(text, code);
                }
            }
        }
    }
    */

    m_angel61DestinationCodes = codes;

    if (options == m_angel61DestinationOptions) {
        return;
    }

    const QString current = ui->angel61Destination->currentText();
    ui->angel61Destination->blockSignals(true);
    ui->angel61Destination->clear();
    ui->angel61Destination->addItems(options);
    ui->angel61Destination->setEditText(current);
    ui->angel61Destination->blockSignals(false);
    m_angel61DestinationOptions = options;
}

void DSEModGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void DSEModGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void DSEModGUI::audioFeedbackSelect(const QPoint& p)
{
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_feedbackAudioDeviceName, true); // true for output
    audioSelect.move(p);
    new DialogPositioner(&audioSelect, false);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_feedbackAudioDeviceName = audioSelect.m_audioDeviceName;
        applySettings(QStringList("feedbackAudioDeviceName"));
    }
}

void DSEModGUI::audioSelect(const QPoint& p)
{
    qDebug("DSEModGUI::audioSelect");
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName, true); // true for input
    audioSelect.move(p);
    new DialogPositioner(&audioSelect, false);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        applySettings(QStringList("audioDeviceName"));
    }
}

void DSEModGUI::tick()
{
    if (m_settings.m_spectrumAudio && (m_dseMod->getPcmSampleRate() > 0) && (m_spectrumRate != m_dseMod->getPcmSampleRate())) {
        updateSpectrumSource();
    }

    double powDb = CalcDb::dbPower(m_dseMod->getMagSq());
    m_channelPowerDbAvg(powDb);
    ui->channelPower->setText(tr("%1 dB").arg(m_channelPowerDbAvg.asDouble(), 0, 'f', 1));

    int audioSampleRate = m_dseMod->getAudioSampleRate();

    if (audioSampleRate != m_audioSampleRate)
    {
        if (audioSampleRate < 0) {
            ui->mic->setColor(QColor("red"));
        } else {
            ui->mic->resetColor();
        }

        m_audioSampleRate = audioSampleRate;
    }

    if ((++m_tickCount & 0xf) == 0)
    {
        updateAngel61Destinations();

        if (m_settings.m_modAFInput == DSEModSettings::DSEModInputFile)
        {
            DSEMod::MsgConfigureFileSourceStreamTiming* message = DSEMod::MsgConfigureFileSourceStreamTiming::create();
            m_dseMod->getInputMessageQueue()->push(message);
        }

        QString status;

        if (m_dseMod->isTransmitting())
        {
            status = tr("TX %1 frames, %2 enc, %3 under, %4 over, %5 trim")
                .arg(m_dseMod->getTxFrameCount())
                .arg(m_dseMod->getEncodedFrameCount())
                .arg(m_dseMod->getUnderrunCount())
                .arg(m_dseMod->getOverrunCount())
                .arg(m_dseMod->getTrimCount());

            if (m_settings.m_modAFInput == DSEModSettings::DSEModInputAudio) {
                status += tr(", mic -%1 +%2").arg(m_dseMod->getAudioUnderrunCount()).arg(m_dseMod->getAudioOverrunCount());
            }
        }
        else
        {
            status = m_dseMod->isVocoderReady() ? tr("Idle (vocoder ready)") : tr("Idle (vocoder unavailable)");
        }

        ui->vocoderStatus->setText(status);
    }
}

void DSEModGUI::updateWithStreamData()
{
    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(m_recordLength);
    QString s_time = recordLength.toString("HH:mm:ss");
    ui->recordLengthText->setText(s_time);
    updateWithStreamTime();
}

void DSEModGUI::updateWithStreamTime()
{
    int t_sec = 0;
    int t_msec = 0;

    if (m_recordSampleRate > 0)
    {
        t_msec = (int) (((m_samplesCount * 1000) / m_recordSampleRate) % 1000);
        t_sec = (int) (m_samplesCount / m_recordSampleRate);
    }

    QTime t(0, 0, 0, 0);
    t = t.addSecs(t_sec);
    t = t.addMSecs(t_msec);
    QString s_timems = t.toString("HH:mm:ss.zzz");
    ui->relTimeText->setText(s_timems);

    if (!m_enableNavTime && (m_recordLength > 0))
    {
        float posRatio = (float) t_sec / (float) m_recordLength;
        ui->navTimeSlider->setValue((int) (posRatio * 100.0));
    }
}

void DSEModGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &DSEModGUI::on_deltaFrequency_changed);
    QObject::connect(ui->mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DSEModGUI::on_mode_currentIndexChanged);
    QObject::connect(ui->invert, &ButtonSwitch::toggled, this, &DSEModGUI::on_invert_toggled);
    QObject::connect(ui->transmit, &ButtonSwitch::toggled, this, &DSEModGUI::on_transmit_toggled);
    QObject::connect(ui->volume, &QDial::valueChanged, this, &DSEModGUI::on_volume_valueChanged);
    QObject::connect(ui->tone, &ButtonSwitch::toggled, this, &DSEModGUI::on_tone_toggled);
    QObject::connect(ui->toneFrequency, &QDial::valueChanged, this, &DSEModGUI::on_toneFrequency_valueChanged);
    QObject::connect(ui->mic, &ButtonSwitch::toggled, this, &DSEModGUI::on_mic_toggled);
    QObject::connect(ui->play, &ButtonSwitch::toggled, this, &DSEModGUI::on_play_toggled);
    QObject::connect(ui->playLoop, &ButtonSwitch::toggled, this, &DSEModGUI::on_playLoop_toggled);
    QObject::connect(ui->morseKeyer, &ButtonSwitch::toggled, this, &DSEModGUI::on_morseKeyer_toggled);
    QObject::connect(ui->feedbackEnable, &QToolButton::toggled, this, &DSEModGUI::on_feedbackEnable_toggled);
    QObject::connect(ui->spectrumAudio, &ButtonSwitch::toggled, this, &DSEModGUI::on_spectrumAudio_toggled);
    QObject::connect(ui->feedbackVolume, &QDial::valueChanged, this, &DSEModGUI::on_feedbackVolume_valueChanged);
    QObject::connect(ui->vocoder, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DSEModGUI::on_vocoder_currentIndexChanged);
    QObject::connect(ui->vocoderGain, QOverload<int>::of(&QSpinBox::valueChanged), this, &DSEModGUI::on_vocoderGain_valueChanged);
    QObject::connect(ui->toneDetect, &ButtonSwitch::toggled, this, &DSEModGUI::on_toneDetect_toggled);
    QObject::connect(ui->noiseSuppress, &ButtonSwitch::toggled, this, &DSEModGUI::on_noiseSuppress_toggled);
    QObject::connect(ui->dstarMyCall, &QLineEdit::editingFinished, this, &DSEModGUI::on_dstarMyCall_editingFinished);
    QObject::connect(ui->dstarMySuffix, &QLineEdit::editingFinished, this, &DSEModGUI::on_dstarMySuffix_editingFinished);
    QObject::connect(ui->dstarUrCall, &QLineEdit::editingFinished, this, &DSEModGUI::on_dstarUrCall_editingFinished);
    QObject::connect(ui->dstarRpt1, &QLineEdit::editingFinished, this, &DSEModGUI::on_dstarRpt1_editingFinished);
    QObject::connect(ui->dstarRpt2, &QLineEdit::editingFinished, this, &DSEModGUI::on_dstarRpt2_editingFinished);
    QObject::connect(ui->dstarText, &QLineEdit::editingFinished, this, &DSEModGUI::on_dstarText_editingFinished);
    QObject::connect(ui->dtmf, &QLineEdit::editingFinished, this, &DSEModGUI::on_dtmf_editingFinished);
    QObject::connect(ui->dtmfSend, &QPushButton::clicked, this, &DSEModGUI::on_dtmfSend_clicked);
    QObject::connect(ui->dmrColorCode, QOverload<int>::of(&QSpinBox::valueChanged), this, &DSEModGUI::on_dmrColorCode_valueChanged);
    QObject::connect(ui->dmrSourceId, QOverload<int>::of(&QSpinBox::valueChanged), this, &DSEModGUI::on_dmrSourceId_valueChanged);
    QObject::connect(ui->dmrDestinationId, QOverload<int>::of(&QSpinBox::valueChanged), this, &DSEModGUI::on_dmrDestinationId_valueChanged);
    QObject::connect(ui->dmrCallType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DSEModGUI::on_dmrCallType_currentIndexChanged);
    QObject::connect(ui->dmrSlot, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DSEModGUI::on_dmrSlot_currentIndexChanged);
    QObject::connect(ui->dmrDirect, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DSEModGUI::on_dmrDirect_currentIndexChanged);
    QObject::connect(ui->ysfCallsign, &QLineEdit::editingFinished, this, &DSEModGUI::on_ysfCallsign_editingFinished);
    QObject::connect(ui->ysfDestination, &QLineEdit::editingFinished, this, &DSEModGUI::on_ysfDestination_editingFinished);
    QObject::connect(ui->ysfDownlink, &QLineEdit::editingFinished, this, &DSEModGUI::on_ysfDownlink_editingFinished);
    QObject::connect(ui->ysfUplink, &QLineEdit::editingFinished, this, &DSEModGUI::on_ysfUplink_editingFinished);
    QObject::connect(ui->ysfMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DSEModGUI::on_ysfMode_currentIndexChanged);
    QObject::connect(ui->nxdnRan, QOverload<int>::of(&QSpinBox::valueChanged), this, &DSEModGUI::on_nxdnRan_valueChanged);
    QObject::connect(ui->nxdnSourceId, QOverload<int>::of(&QSpinBox::valueChanged), this, &DSEModGUI::on_nxdnSourceId_valueChanged);
    QObject::connect(ui->nxdnDestinationId, QOverload<int>::of(&QSpinBox::valueChanged), this, &DSEModGUI::on_nxdnDestinationId_valueChanged);
    QObject::connect(ui->nxdnCallType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DSEModGUI::on_nxdnCallType_currentIndexChanged);
    QObject::connect(ui->dpmrColorCode, QOverload<int>::of(&QSpinBox::valueChanged), this, &DSEModGUI::on_dpmrColorCode_valueChanged);
    QObject::connect(ui->dpmrOwnId, QOverload<int>::of(&QSpinBox::valueChanged), this, &DSEModGUI::on_dpmrOwnId_valueChanged);
    QObject::connect(ui->dpmrCalledId, QOverload<int>::of(&QSpinBox::valueChanged), this, &DSEModGUI::on_dpmrCalledId_valueChanged);
    QObject::connect(ui->p25Nac, QOverload<int>::of(&QSpinBox::valueChanged), this, &DSEModGUI::on_p25Nac_valueChanged);
    QObject::connect(ui->angel61Rate, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DSEModGUI::on_angel61Rate_currentIndexChanged);
    QObject::connect(ui->angel61Demod, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DSEModGUI::on_angel61Demod_currentIndexChanged);
    QObject::connect(ui->angel61Symbol, &QToolButton::clicked, this, &DSEModGUI::on_angel61Symbol_clicked);
    QObject::connect(ui->angel61APRSInfo, &QLineEdit::returnPressed, this, &DSEModGUI::on_angel61APRSSend_clicked);
    QObject::connect(ui->angel61APRSSend, &QPushButton::clicked, this, &DSEModGUI::on_angel61APRSSend_clicked);
    QObject::connect(ui->m17Source, &QLineEdit::editingFinished, this, &DSEModGUI::on_m17Source_editingFinished);
    QObject::connect(ui->m17Destination, &QLineEdit::editingFinished, this, &DSEModGUI::on_m17Destination_editingFinished);
    QObject::connect(ui->m17CAN, QOverload<int>::of(&QSpinBox::valueChanged), this, &DSEModGUI::on_m17CAN_valueChanged);
    QObject::connect(ui->m17Message, &QLineEdit::editingFinished, this, &DSEModGUI::on_m17Message_editingFinished);
    QObject::connect(ui->m17Send, &QPushButton::clicked, this, &DSEModGUI::on_m17Send_clicked);
    QObject::connect(ui->angel61FileName, &QLineEdit::editingFinished, this, &DSEModGUI::on_angel61FileName_editingFinished);
    QObject::connect(ui->angel61Tunnel, &QCheckBox::toggled, this, &DSEModGUI::on_angel61Tunnel_toggled);
    QObject::connect(ui->angel61TunnelPeer, &QLineEdit::editingFinished, this, &DSEModGUI::on_angel61TunnelPeer_editingFinished);
    QObject::connect(ui->angel61TunnelAddress, &QLineEdit::editingFinished, this, &DSEModGUI::on_angel61TunnelAddress_editingFinished);
    QObject::connect(ui->angel61TunnelRoutes, &QLineEdit::editingFinished, this, &DSEModGUI::on_angel61TunnelRoutes_editingFinished);
    QObject::connect(ui->angel61TunnelAllow, &QLineEdit::editingFinished, this, &DSEModGUI::on_angel61TunnelAllow_editingFinished);
    QObject::connect(ui->angel61TunnelGroups, &QLineEdit::editingFinished, this, &DSEModGUI::on_angel61TunnelGroups_editingFinished);
    QObject::connect(ui->angel61TunnelInterface, &QLineEdit::editingFinished, this, &DSEModGUI::on_angel61TunnelInterface_editingFinished);
    QObject::connect(ui->angel61TunnelMTU, QOverload<int>::of(&QSpinBox::valueChanged), this, &DSEModGUI::on_angel61TunnelMTU_valueChanged);
    QObject::connect(ui->angel61FileBrowse, &QPushButton::clicked, this, &DSEModGUI::on_angel61FileBrowse_clicked);
    QObject::connect(ui->angel61FileSend, &QPushButton::clicked, this, &DSEModGUI::on_angel61FileSend_clicked);
    QObject::connect(ui->angel61Source, &QLineEdit::editingFinished, this, &DSEModGUI::on_angel61Source_editingFinished);
    QObject::connect(ui->angel61Destination->lineEdit(), &QLineEdit::editingFinished, this, &DSEModGUI::on_angel61Destination_editingFinished);
    QObject::connect(ui->angel61Destination, QOverload<int>::of(&QComboBox::activated), this, [this](int) { on_angel61Destination_editingFinished(); });
    QObject::connect(ui->angel61Path, &QLineEdit::editingFinished, this, &DSEModGUI::on_angel61Path_editingFinished);
    QObject::connect(ui->sendPosition, &ButtonSwitch::toggled, this, &DSEModGUI::on_sendPosition_toggled);
    QObject::connect(ui->angel61Message, &QLineEdit::editingFinished, this, &DSEModGUI::on_angel61Message_editingFinished);
    QObject::connect(ui->angel61Send, &QPushButton::clicked, this, &DSEModGUI::on_angel61Send_clicked);
    QObject::connect(ui->p25TalkGroup, QOverload<int>::of(&QSpinBox::valueChanged), this, &DSEModGUI::on_p25TalkGroup_valueChanged);
    QObject::connect(ui->p25SourceId, QOverload<int>::of(&QSpinBox::valueChanged), this, &DSEModGUI::on_p25SourceId_valueChanged);
    QObject::connect(ui->navTimeSlider, &QSlider::valueChanged, this, &DSEModGUI::on_navTimeSlider_valueChanged);
    QObject::connect(ui->showFileDialog, &QPushButton::clicked, this, &DSEModGUI::on_showFileDialog_clicked);
}

void DSEModGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
