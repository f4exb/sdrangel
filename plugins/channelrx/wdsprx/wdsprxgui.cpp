///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany     //
// written by Christian Daniel                                                       //
// Copyright (C) 2015-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>              //
// Copyright (C) 2021-2023 Jon Beniston, M7RCE <jon@beniston.com>                    //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
#include <QPixmap>

#include "wdsprxgui.h"

#include "device/deviceuiset.h"

#include "ui_wdsprxgui.h"
#include "dsp/spectrumvis.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "gui/glspectrum.h"
#include "gui/basicchannelsettingsdialog.h"
#include "plugin/pluginapi.h"
#include "util/db.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "maincore.h"
#include "wdsprx.h"
#include "wdsprxagcdialog.h"
#include "wdsprxdnbdialog.h"
#include "wdsprxdnrdialog.h"
#include "wdsprxamdialog.h"
#include "wdsprxfmdialog.h"
#include "wdsprxcwpeakdialog.h"
#include "wdsprxsquelchdialog.h"
#include "wdsprxeqdialog.h"
#include "wdsprxpandialog.h"

WDSPRxGUI* WDSPRxGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
	auto* gui = new WDSPRxGUI(pluginAPI, deviceUISet, rxChannel);
	return gui;
}

void WDSPRxGUI::resetToDefaults()
{
	m_settings.resetToDefaults();
}

QByteArray WDSPRxGUI::serialize() const
{
    return m_settings.serialize();
}

bool WDSPRxGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        ui->BW->setMaximum(480);
        ui->BW->setMinimum(-480);
        ui->lowCut->setMaximum(480);
        ui->lowCut->setMinimum(-480);
        displaySettings();
        applyBandwidths(m_settings.m_profiles[m_settings.m_profileIndex].m_spanLog2, true); // does applySettings(true)
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        ui->BW->setMaximum(480);
        ui->BW->setMinimum(-480);
        ui->lowCut->setMaximum(480);
        ui->lowCut->setMinimum(-480);
        displaySettings();
        applyBandwidths(m_settings.m_profiles[m_settings.m_profileIndex].m_spanLog2, true); // does applySettings(true)
        return false;
    }
}

bool WDSPRxGUI::handleMessage(const Message& message)
{
    if (WDSPRx::MsgConfigureWDSPRx::match(message))
    {
        qDebug("WDSPRxGUI::handleMessage: WDSPRx::MsgConfigureWDSPRx");
        auto& cfg = (const WDSPRx::MsgConfigureWDSPRx&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        ui->spectrumGUI->updateSettings();
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (DSPConfigureAudio::match(message))
    {
        qDebug("WDSPRxGUI::handleMessage: DSPConfigureAudio: %d", m_wdspRx->getAudioSampleRate());
        applyBandwidths(1 + ui->spanLog2->maximum() - ui->spanLog2->value()); // will update spectrum details with new sample rate
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        auto& notif = (const DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        qDebug("WDSPRxGUI::handleMessage: DSPSignalNotification: centerFrequency: %lld sampleRate: %d",
            m_deviceCenterFrequency, m_basebandSampleRate);
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

void WDSPRxGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != nullptr)
    {
        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

void WDSPRxGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void WDSPRxGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void WDSPRxGUI::on_audioBinaural_toggled(bool binaural)
{
	m_audioBinaural = binaural;
	m_settings.m_audioBinaural = binaural;
    m_settings.m_profiles[m_settings.m_profileIndex].m_audioBinaural = m_settings.m_audioBinaural;
	applySettings();
}

void WDSPRxGUI::on_audioFlipChannels_toggled(bool flip)
{
	m_audioFlipChannels = flip;
	m_settings.m_audioFlipChannels = flip;
    m_settings.m_profiles[m_settings.m_profileIndex].m_audioFlipChannels = m_settings.m_audioFlipChannels;
	applySettings();
}

void WDSPRxGUI::on_dsb_toggled(bool dsb)
{
    ui->flipSidebands->setEnabled(!dsb);
    applyBandwidths(1 + ui->spanLog2->maximum() - ui->spanLog2->value());
}

void WDSPRxGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency((int) value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void WDSPRxGUI::on_BW_valueChanged(int value)
{
    (void) value;
    qDebug("WDSPRxGUI::on_BW_valueChanged: ui->spanLog2: %d", ui->spanLog2->value());
    applyBandwidths(1 + ui->spanLog2->maximum() - ui->spanLog2->value());
}

void WDSPRxGUI::on_lowCut_valueChanged(int value)
{
    (void) value;
    applyBandwidths(1 + ui->spanLog2->maximum() - ui->spanLog2->value());
}

void WDSPRxGUI::on_volume_valueChanged(int value)
{
	ui->volumeText->setText(QString("%1").arg(value));
	m_settings.m_volume = (Real) CalcDb::powerFromdB(value);
	applySettings();
}

void WDSPRxGUI::on_agc_toggled(bool checked)
{
    m_settings.m_agc = checked;
    m_settings.m_profiles[m_settings.m_profileIndex].m_agc = m_settings.m_agc;
    applySettings();
}

void WDSPRxGUI::on_agcGain_valueChanged(int value)
{
    QString s = QString::number(value, 'f', 0);
    ui->agcGainText->setText(s);
    m_settings.m_agcGain = value;
    m_settings.m_profiles[m_settings.m_profileIndex].m_agcGain = m_settings.m_agcGain;
    applySettings();
}

void WDSPRxGUI::on_dnr_toggled(bool checked)
{
    m_settings.m_dnr = checked;
    m_settings.m_profiles[m_settings.m_profileIndex].m_dnr = m_settings.m_dnr;
    applySettings();
}

void WDSPRxGUI::on_dnb_toggled(bool checked)
{
    m_settings.m_dnb = checked;
    m_settings.m_profiles[m_settings.m_profileIndex].m_dnb = m_settings.m_dnb;
    applySettings();
}

void WDSPRxGUI::on_anf_toggled(bool checked)
{
    m_settings.m_anf = checked;
    m_settings.m_profiles[m_settings.m_profileIndex].m_anf = m_settings.m_anf;
    applySettings();
}

void WDSPRxGUI::on_cwPeaking_toggled(bool checked)
{
    m_settings.m_cwPeaking = checked;
    m_settings.m_profiles[m_settings.m_profileIndex].m_cwPeaking = m_settings.m_cwPeaking;
    applySettings();
}

void WDSPRxGUI::on_squelch_toggled(bool checked)
{
    m_settings.m_squelch = checked;
    m_settings.m_profiles[m_settings.m_profileIndex].m_squelch = m_settings.m_squelch;
    applySettings();
}

void WDSPRxGUI::on_squelchThreshold_valueChanged(int value)
{
    m_settings.m_squelchThreshold = value;
    m_settings.m_profiles[m_settings.m_profileIndex].m_squelchThreshold = m_settings.m_squelchThreshold;
    ui->squelchThresholdText->setText(tr("%1").arg(m_settings.m_squelchThreshold));
    applySettings();
}

void WDSPRxGUI::on_equalizer_toggled(bool checked)
{
    m_settings.m_equalizer = checked;
    m_settings.m_profiles[m_settings.m_profileIndex].m_equalizer = m_settings.m_equalizer;
    applySettings();
}

void WDSPRxGUI::on_rit_toggled(bool checked)
{
    m_settings.m_rit = checked;
    m_settings.m_profiles[m_settings.m_profileIndex].m_rit = m_settings.m_rit;
    m_channelMarker.setShift(checked ? (int) m_settings.m_ritFrequency: 0);
    applySettings();
}

void WDSPRxGUI::on_ritFrequency_valueChanged(int value)
{
    m_settings.m_ritFrequency = value;
    m_settings.m_profiles[m_settings.m_profileIndex].m_ritFrequency = m_settings.m_ritFrequency;
    ui->ritFrequencyText->setText(tr("%1").arg(value));
    m_channelMarker.setShift(m_settings.m_rit ? value: 0);
    applySettings();
}

void WDSPRxGUI::on_dbOrS_toggled(bool checked)
{
    ui->dbOrS->setText(checked ? "dB": "S");
    m_settings.m_dbOrS = checked;
    m_settings.m_profiles[m_settings.m_profileIndex].m_dbOrS = m_settings.m_dbOrS;
    ui->channelPowerMeter->setRange(WDSPRxSettings::m_minPowerThresholdDB, 0, !checked);
}

void WDSPRxGUI::on_audioMute_toggled(bool checked)
{
	m_audioMute = checked;
	m_settings.m_audioMute = checked;
	applySettings();
}

void WDSPRxGUI::on_spanLog2_valueChanged(int value)
{
    int s2max = spanLog2Max();

    if ((value < 0) || (value > s2max-1)) {
        return;
    }

    applyBandwidths(s2max - ui->spanLog2->value());
}

void WDSPRxGUI::on_flipSidebands_clicked(bool checked)
{
    (void) checked;
    int bwValue = ui->BW->value();
    int lcValue = ui->lowCut->value();
    ui->BW->setValue(-bwValue);
    ui->lowCut->setValue(-lcValue);
}

void WDSPRxGUI::on_fftWindow_currentIndexChanged(int index)
{
    m_settings.m_profiles[m_settings.m_profileIndex].m_fftWindow = index;
    applySettings();
}

void WDSPRxGUI::on_profileIndex_valueChanged(int value)
{
    if ((value < 0) || (value >= 10)) {
        return;
    }

    ui->profileIndexText->setText(tr("%1").arg(value));
    m_settings.m_profileIndex = value;
    // Bandwidth setup
    ui->BW->setMaximum(480);
    ui->BW->setMinimum(-480);
    ui->lowCut->setMaximum(480);
    ui->lowCut->setMinimum(-480);
    m_settings.m_demod = m_settings.m_profiles[m_settings.m_profileIndex].m_demod;
    m_settings.m_audioBinaural = m_settings.m_profiles[m_settings.m_profileIndex].m_audioBinaural;
    m_settings.m_audioFlipChannels = m_settings.m_profiles[m_settings.m_profileIndex].m_audioFlipChannels;
    m_settings.m_dsb = m_settings.m_profiles[m_settings.m_profileIndex].m_dsb;
    m_settings.m_dbOrS = m_settings.m_profiles[m_settings.m_profileIndex].m_dbOrS;
    // AGC setup
    m_settings.m_agc = m_settings.m_profiles[m_settings.m_profileIndex].m_agc;
    m_settings.m_agcGain = m_settings.m_profiles[m_settings.m_profileIndex].m_agcGain;
    // Noise blanking
    m_settings.m_dnb = m_settings.m_profiles[m_settings.m_profileIndex].m_dnb;
    m_settings.m_nbScheme = m_settings.m_profiles[m_settings.m_profileIndex].m_nbScheme;
    m_settings.m_nb2Mode = m_settings.m_profiles[m_settings.m_profileIndex].m_nb2Mode;
    m_settings.m_nbSlewTime = m_settings.m_profiles[m_settings.m_profileIndex].m_nbSlewTime;
    m_settings.m_nbLeadTime = m_settings.m_profiles[m_settings.m_profileIndex].m_nbLeadTime;
    m_settings.m_nbLagTime = m_settings.m_profiles[m_settings.m_profileIndex].m_nbLagTime;
    m_settings.m_nbThreshold = m_settings.m_profiles[m_settings.m_profileIndex].m_nbThreshold;
    m_settings.m_nbAvgTime = m_settings.m_profiles[m_settings.m_profileIndex].m_nbAvgTime;
    // Noise reduction
    m_settings.m_dnr = m_settings.m_profiles[m_settings.m_profileIndex].m_dnr;
    m_settings.m_snb = m_settings.m_profiles[m_settings.m_profileIndex].m_snb;
    m_settings.m_anf = m_settings.m_profiles[m_settings.m_profileIndex].m_anf;
    m_settings.m_nrScheme = m_settings.m_profiles[m_settings.m_profileIndex].m_nrScheme;
    m_settings.m_nr2Gain = m_settings.m_profiles[m_settings.m_profileIndex].m_nr2Gain;
    m_settings.m_nr2NPE = m_settings.m_profiles[m_settings.m_profileIndex].m_nr2NPE;
    m_settings.m_nrPosition = m_settings.m_profiles[m_settings.m_profileIndex].m_nrPosition;
    m_settings.m_nr2ArtifactReduction = m_settings.m_profiles[m_settings.m_profileIndex].m_nr2ArtifactReduction;
    // demod
    m_settings.m_demod = m_settings.m_profiles[m_settings.m_profileIndex].m_demod;
    m_settings.m_amFadeLevel = m_settings.m_profiles[m_settings.m_profileIndex].m_amFadeLevel;
    m_settings.m_cwPeaking = m_settings.m_profiles[m_settings.m_profileIndex].m_cwPeaking;
    m_settings.m_cwPeakFrequency = m_settings.m_profiles[m_settings.m_profileIndex].m_cwPeakFrequency;
    m_settings.m_cwBandwidth = m_settings.m_profiles[m_settings.m_profileIndex].m_cwBandwidth;
    m_settings.m_cwGain = m_settings.m_profiles[m_settings.m_profileIndex].m_cwGain;
    m_settings.m_fmDeviation = m_settings.m_profiles[m_settings.m_profileIndex].m_fmDeviation;
    m_settings.m_fmAFLow = m_settings.m_profiles[m_settings.m_profileIndex].m_fmAFLow;
    m_settings.m_fmAFHigh = m_settings.m_profiles[m_settings.m_profileIndex].m_fmAFHigh;
    m_settings.m_fmAFLimiter = m_settings.m_profiles[m_settings.m_profileIndex].m_fmAFLimiter;
    m_settings.m_fmAFLimiterGain = m_settings.m_profiles[m_settings.m_profileIndex].m_fmAFLimiterGain;
    m_settings.m_fmCTCSSNotch = m_settings.m_profiles[m_settings.m_profileIndex].m_fmCTCSSNotch;
    m_settings.m_fmCTCSSNotchFrequency = m_settings.m_profiles[m_settings.m_profileIndex].m_fmCTCSSNotchFrequency;
    // Squelch
    m_settings.m_squelch = m_settings.m_profiles[m_settings.m_profileIndex].m_squelch;
    m_settings.m_squelchThreshold = m_settings.m_profiles[m_settings.m_profileIndex].m_squelchThreshold;
    m_settings.m_squelchMode = m_settings.m_profiles[m_settings.m_profileIndex].m_squelchMode;
    m_settings.m_ssqlTauMute = m_settings.m_profiles[m_settings.m_profileIndex].m_ssqlTauMute;
    m_settings.m_ssqlTauUnmute = m_settings.m_profiles[m_settings.m_profileIndex].m_ssqlTauUnmute;
    m_settings.m_amsqMaxTail = m_settings.m_profiles[m_settings.m_profileIndex].m_amsqMaxTail;
    // Equalizer
    m_settings.m_equalizer = m_settings.m_profiles[m_settings.m_profileIndex].m_equalizer;
    m_settings.m_eqF = m_settings.m_profiles[m_settings.m_profileIndex].m_eqF;
    m_settings.m_eqG = m_settings.m_profiles[m_settings.m_profileIndex].m_eqG;
    // RIT
    m_settings.m_rit = m_settings.m_profiles[m_settings.m_profileIndex].m_rit;
    m_settings.m_ritFrequency = m_settings.m_profiles[m_settings.m_profileIndex].m_ritFrequency;
    displaySettings();
    applyBandwidths(m_settings.m_profiles[m_settings.m_profileIndex].m_spanLog2, true); // does applySettings(true)
}

void WDSPRxGUI::on_demod_currentIndexChanged(int index)
{
    auto demod = (WDSPRxProfile::WDSPRxDemod) index;

    if ((m_settings.m_demod != WDSPRxProfile::DemodSSB) && (demod == WDSPRxProfile::DemodSSB)) {
        m_settings.m_dsb = false;
    }

    m_settings.m_demod = demod;
    m_settings.m_profiles[m_settings.m_profileIndex].m_demod = m_settings.m_demod;

    switch(m_settings.m_demod)
    {
    case WDSPRxProfile::DemodSSB:
    case WDSPRxProfile::DemodSAM:
        break;
    case WDSPRxProfile::DemodAM:
    case WDSPRxProfile::DemodFMN:
        m_settings.m_dsb = true;
        break;
    default:
        break;
    }

    displaySettings();
    applyBandwidths(m_settings.m_profiles[m_settings.m_profileIndex].m_spanLog2, true); // does applySettings(true)
}

void WDSPRxGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_wdspRx->getNumberOfDeviceStreams());
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

void WDSPRxGUI::onWidgetRolled(const QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

WDSPRxGUI::WDSPRxGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
	ChannelGUI(parent),
	ui(new Ui::WDSPRxGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
	m_doApplySettings(true),
    m_spectrumRate(6000),
	m_audioBinaural(false),
	m_audioFlipChannels(false),
    m_audioMute(false),
	m_squelchOpen(false),
    m_audioSampleRate(-1),
    m_agcDialog(nullptr),
    m_dnbDialog(nullptr),
    m_dnrDialog(nullptr),
    m_amDialog(nullptr),
    m_fmDialog(nullptr),
    m_cwPeakDialog(nullptr),
    m_squelchDialog(nullptr)
{
	setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/wdsprx/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

	m_wdspRx = (WDSPRx*) rxChannel;
    m_spectrumVis = m_wdspRx->getSpectrumVis();
	m_spectrumVis->setGLSpectrum(ui->glSpectrum);
	m_wdspRx->setMessageQueueToGUI(WDSPRxGUI::getInputMessageQueue());

    m_audioMuteRightClickEnabler = new CRightClickEnabler(ui->audioMute);
    connect(m_audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect(const QPoint &)));

    m_agcRightClickEnabler = new CRightClickEnabler(ui->agc);
    connect(m_agcRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(agcSetupDialog(const QPoint &)));

    m_dnbRightClickEnabler = new CRightClickEnabler(ui->dnb);
    connect(m_dnbRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(dnbSetupDialog(const QPoint &)));

    m_dnrRightClickEnabler = new CRightClickEnabler(ui->dnr);
    connect(m_dnrRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(dnrSetupDialog(const QPoint &)));

    m_cwPeakRightClickEnabler = new CRightClickEnabler(ui->cwPeaking);
    connect(m_cwPeakRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(cwPeakSetupDialog(const QPoint &)));

    m_squelchRightClickEnabler = new CRightClickEnabler(ui->squelch);
    connect(m_squelchRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(squelchSetupDialog(const QPoint &)));

    m_equalizerRightClickEnabler = new CRightClickEnabler(ui->equalizer);
    connect(m_equalizerRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(equalizerSetupDialog(const QPoint &)));

    m_panRightClickEnabler = new CRightClickEnabler(ui->audioBinaural);
    connect(m_panRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(panSetupDialog(const QPoint &)));

    m_demodRightClickEnabler = new CRightClickEnabler(ui->demod);
    connect(m_demodRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(demodSetupDialog(const QPoint &)));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
	ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

	ui->spectrumGUI->setBuddies(m_spectrumVis, ui->glSpectrum);

    ui->glSpectrum->setCenterFrequency(m_spectrumRate/2);
    ui->glSpectrum->setSampleRate(m_spectrumRate);

    SpectrumSettings spectrumSettings = m_spectrumVis->getSettings();
    spectrumSettings.m_displayWaterfall = true;
    spectrumSettings.m_ssb = true;
    SpectrumVis::MsgConfigureSpectrumVis *msg = SpectrumVis::MsgConfigureSpectrumVis::create(spectrumSettings, false);
    m_spectrumVis->getInputMessageQueue()->push(msg);

	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

	m_channelMarker.blockSignals(true);
	m_channelMarker.setColor(Qt::green);
    m_channelMarker.setBandwidth(6000);
    m_channelMarker.setCenterFrequency(0);
    m_channelMarker.setTitle("SSB Demodulator");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    setTitleColor(m_channelMarker.getColor());

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setSpectrumGUI(ui->spectrumGUI);
    m_settings.setRollupState(&m_rollupState);

	m_deviceUISet->addChannelMarker(&m_channelMarker);
	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(WDSPRxGUI::getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));


	m_iconDSBUSB.addPixmap(QPixmap("://dsb.png"), QIcon::Normal, QIcon::On);
    m_iconDSBUSB.addPixmap(QPixmap("://usb.png"), QIcon::Normal, QIcon::Off);
	m_iconDSBLSB.addPixmap(QPixmap("://dsb.png"), QIcon::Normal, QIcon::On);
    m_iconDSBLSB.addPixmap(QPixmap("://lsb.png"), QIcon::Normal, QIcon::Off);

    ui->BW->setMaximum(480);
    ui->BW->setMinimum(-480);
    ui->lowCut->setMaximum(480);
    ui->lowCut->setMinimum(-480);
	displaySettings();
    makeUIConnections();

	applyBandwidths(m_settings.m_profiles[m_settings.m_profileIndex].m_spanLog2, true); // does applySettings(true)
    DialPopup::addPopupsToChildDials(this);
    m_resizer.enableChildMouseTracking();
}

WDSPRxGUI::~WDSPRxGUI()
{
    qDebug("WDSPRxGUI::~WDSPRxGUI");
	delete ui;
    delete m_audioMuteRightClickEnabler;
    delete m_agcRightClickEnabler;
    delete m_dnbRightClickEnabler;
    delete m_dnrRightClickEnabler;
    delete m_cwPeakRightClickEnabler;
    delete m_squelchRightClickEnabler;
    delete m_equalizerRightClickEnabler;
    delete m_panRightClickEnabler;
    delete m_demodRightClickEnabler;
    qDebug("WDSPRxGUI::~WDSPRxGUI: end");
}

bool WDSPRxGUI::blockApplySettings(bool block)
{
    bool ret = !m_doApplySettings;
    m_doApplySettings = !block;
    return ret;
}

void WDSPRxGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
        WDSPRx::MsgConfigureWDSPRx* message = WDSPRx::MsgConfigureWDSPRx::create( m_settings, force);
        m_wdspRx->getInputMessageQueue()->push(message);
	}
}

uint32_t WDSPRxGUI::getValidAudioSampleRate() const
{
    // When not running, m_wdspRx->getAudioSampleRate() will return 0, but we
    // want a valid value to initialise the GUI, to allow a user to preselect settings
    int sr = m_wdspRx->getAudioSampleRate();
    if (sr == 0)
    {
        if (m_audioSampleRate > 0) {
            sr = m_audioSampleRate;
        } else {
            sr = 48000;
        }
    }
    return sr;
}

unsigned int WDSPRxGUI::spanLog2Max() const
{
    unsigned int spanLog2 = 0;
    for (; getValidAudioSampleRate() / (1<<spanLog2) >= 1000; spanLog2++);
    return spanLog2 == 0 ? 0 : spanLog2-1;
}

void WDSPRxGUI::applyBandwidths(unsigned int spanLog2, bool force)
{
    unsigned int s2max = spanLog2Max();
    spanLog2 = spanLog2 > s2max ? s2max : spanLog2;
    unsigned int limit = s2max < 1 ? 0 : s2max - 1;
    ui->spanLog2->setMaximum(limit);
    bool dsb = ui->dsb->isChecked();
    m_spectrumRate = getValidAudioSampleRate() / (1<<spanLog2);
    int bw = ui->BW->value();
    int lw = ui->lowCut->value();
    int bwMax = getValidAudioSampleRate() / (100*(1<<spanLog2));
    int tickInterval = m_spectrumRate / 1200;
    tickInterval = tickInterval == 0 ? 1 : tickInterval;

    qDebug() << "WDSPRxGUI::applyBandwidths:"
            << " s2max:" << s2max
            << " dsb: " << dsb
            << " spanLog2: " << spanLog2
            << " m_spectrumRate: " << m_spectrumRate
            << " bw: " << bw
            << " lw: " << lw
            << " bwMax: " << bwMax
            << " tickInterval: " << tickInterval;

    ui->BW->setTickInterval(tickInterval);
    ui->lowCut->setTickInterval(tickInterval);

    bw = bw < -bwMax ? -bwMax : bw > bwMax ? bwMax : bw;

    if (bw < 0) {
        lw = lw < bw+1 ? bw+1 : lw < 0 ? lw : 0;
    } else if (bw > 0) {
        lw = lw > bw-1 ? bw-1 : lw < 0 ? 0 : lw;
    } else {
        lw = 0;
    }

    if (dsb)
    {
        bw = bw < 0 ? -bw : bw;
        lw = 0;
    }

    QString spanStr = QString::number(bwMax/10.0, 'f', 1);
    QString bwStr   = QString::number(bw/10.0, 'f', 1);
    QString lwStr   = QString::number(lw/10.0, 'f', 1);

    SpectrumSettings spectrumSettings = m_spectrumVis->getSettings();

    if (dsb)
    {
        ui->BWText->setText(tr("%1%2k").arg(QChar(0xB1, 0x00)).arg(bwStr));
        ui->spanText->setText(tr("%1%2k").arg(QChar(0xB1, 0x00)).arg(spanStr));
        ui->scaleMinus->setText("0");
        ui->scaleCenter->setText("");
        ui->scalePlus->setText(tr("%1").arg(QChar(0xB1, 0x00)));
        ui->lsbLabel->setText("");
        ui->usbLabel->setText("");
        ui->glSpectrum->setCenterFrequency(0);
        ui->glSpectrum->setSampleRate(2*m_spectrumRate);
        spectrumSettings.m_ssb = false;
        ui->glSpectrum->setLsbDisplay(false);
        ui->glSpectrum->setSsbSpectrum(false);
    }
    else
    {
        ui->BWText->setText(tr("%1k").arg(bwStr));
        ui->spanText->setText(tr("%1k").arg(spanStr));
        ui->scaleMinus->setText("-");
        ui->scaleCenter->setText("0");
        ui->scalePlus->setText("+");
        ui->lsbLabel->setText("LSB");
        ui->usbLabel->setText("USB");
        ui->glSpectrum->setCenterFrequency(0);
        ui->glSpectrum->setSampleRate(2*m_spectrumRate);
        spectrumSettings.m_ssb = true;
        ui->glSpectrum->setLsbDisplay(bw < 0);
        ui->glSpectrum->setSsbSpectrum(true);
    }

    SpectrumVis::MsgConfigureSpectrumVis *msg = SpectrumVis::MsgConfigureSpectrumVis::create(spectrumSettings, false);
    m_spectrumVis->getInputMessageQueue()->push(msg);

    ui->lowCutText->setText(tr("%1k").arg(lwStr));

    ui->BW->blockSignals(true);
    ui->lowCut->blockSignals(true);

    ui->BW->setMaximum(bwMax);
    ui->BW->setMinimum(dsb ? 0 : -bwMax);
    ui->BW->setValue(bw);

    ui->lowCut->setMaximum(dsb ? 0 :  bwMax);
    ui->lowCut->setMinimum(dsb ? 0 : -bwMax);
    ui->lowCut->setValue(lw);

    ui->lowCut->blockSignals(false);
    ui->BW->blockSignals(false);

    m_settings.m_dsb = dsb;
    m_settings.m_profiles[m_settings.m_profileIndex].m_dsb = dsb;
    m_settings.m_profiles[m_settings.m_profileIndex].m_spanLog2 = spanLog2;
    m_settings.m_profiles[m_settings.m_profileIndex].m_highCutoff = (Real) (bw * 100);
    m_settings.m_profiles[m_settings.m_profileIndex].m_lowCutoff = (Real) (lw * 100);

    applySettings(force);

    bool wasBlocked = blockApplySettings(true);
    m_channelMarker.setBandwidth(bw * 200);
    m_channelMarker.setSidebands(dsb ? ChannelMarker::dsb : bw < 0 ? ChannelMarker::lsb : ChannelMarker::usb);
    ui->dsb->setIcon(bw < 0 ? m_iconDSBLSB: m_iconDSBUSB);

    if (!dsb) {
        m_channelMarker.setLowCutoff(lw * 100);
    }

    blockApplySettings(wasBlocked);
}

void WDSPRxGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth((int) (m_settings.m_profiles[m_settings.m_profileIndex].m_highCutoff * 2));
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.setLowCutoff((int) m_settings.m_profiles[m_settings.m_profileIndex].m_lowCutoff);
    int shift = m_settings.m_profiles[m_settings.m_profileIndex].m_rit ?
        (int) m_settings.m_profiles[m_settings.m_profileIndex].m_ritFrequency :
        0;
    m_channelMarker.setShift(shift);

    if (m_deviceUISet->m_deviceMIMOEngine)
    {
        m_channelMarker.clearStreamIndexes();
        m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
    }

    ui->flipSidebands->setEnabled(!m_settings.m_dsb);

    if (m_settings.m_dsb)
    {
        m_channelMarker.setSidebands(ChannelMarker::dsb);
    }
    else
    {
        if (m_settings.m_profiles[m_settings.m_profileIndex].m_highCutoff < 0)
        {
            m_channelMarker.setSidebands(ChannelMarker::lsb);
            ui->dsb->setIcon(m_iconDSBLSB);
        }
        else
        {
            m_channelMarker.setSidebands(ChannelMarker::usb);
            ui->dsb->setIcon(m_iconDSBUSB);
        }
    }

    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->demod->setCurrentIndex(m_settings.m_demod);
    ui->agc->setChecked(m_settings.m_agc);
    ui->agcGain->setValue(m_settings.m_agcGain);
    QString s = QString::number((ui->agcGain->value()), 'f', 0);
    ui->agcGainText->setText(s);
    ui->dnr->setChecked(m_settings.m_dnr);
    ui->dnb->setChecked(m_settings.m_dnb);
    ui->anf->setChecked(m_settings.m_anf);
    ui->cwPeaking->setChecked(m_settings.m_cwPeaking);
    ui->squelch->setChecked(m_settings.m_squelch);
    ui->squelchThreshold->setValue(m_settings.m_squelchThreshold);
    ui->squelchThresholdText->setText(tr("%1").arg(m_settings.m_squelchThreshold));
    ui->equalizer->setChecked(m_settings.m_equalizer);
    ui->rit->setChecked(m_settings.m_rit);
    ui->ritFrequency->setValue((int) m_settings.m_ritFrequency);
    ui->ritFrequencyText->setText(tr("%1").arg((int) m_settings.m_ritFrequency));
    ui->audioBinaural->setChecked(m_settings.m_audioBinaural);
    ui->audioFlipChannels->setChecked(m_settings.m_audioFlipChannels);
    ui->audioMute->setChecked(m_settings.m_audioMute);
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    ui->fftWindow->setCurrentIndex(m_settings.m_profiles[m_settings.m_profileIndex].m_fftWindow);

    // DSB enable/disable
    switch(m_settings.m_demod)
    {
    case WDSPRxProfile::DemodSSB:
    case WDSPRxProfile::DemodSAM:
        ui->dsb->setEnabled(true);
        break;
    case WDSPRxProfile::DemodAM:
    case WDSPRxProfile::DemodFMN:
        ui->dsb->setEnabled(false);
        break;
    default:
        ui->dsb->setEnabled(true);
        break;
    }

    // Prevent uncontrolled triggering of applyBandwidths
    ui->spanLog2->blockSignals(true);
    ui->dsb->blockSignals(true);
    ui->BW->blockSignals(true);
    ui->profileIndex->blockSignals(true);

    ui->profileIndex->setValue(m_settings.m_profileIndex);
    ui->profileIndexText->setText(tr("%1").arg(m_settings.m_profileIndex));

    ui->dsb->setChecked(m_settings.m_dsb);
    ui->spanLog2->setValue(1 + ui->spanLog2->maximum() - m_settings.m_profiles[m_settings.m_profileIndex].m_spanLog2);

    ui->BW->setValue((int) (m_settings.m_profiles[m_settings.m_profileIndex].m_highCutoff / 100.0));
    s = QString::number(m_settings.m_profiles[m_settings.m_profileIndex].m_highCutoff/1000.0, 'f', 1);

    if (m_settings.m_dsb) {
        ui->BWText->setText(tr("%1%2k").arg(QChar(0xB1, 0x00)).arg(s));
    } else {
        ui->BWText->setText(tr("%1k").arg(s));
    }

    ui->dbOrS->setText(m_settings.m_dbOrS ? "dB": "S");
    ui->channelPowerMeter->setRange(WDSPRxSettings::m_minPowerThresholdDB, 0, !m_settings.m_dbOrS);

    ui->spanLog2->blockSignals(false);
    ui->dsb->blockSignals(false);
    ui->BW->blockSignals(false);
    ui->profileIndex->blockSignals(false);

    // The only one of the four signals triggering applyBandwidths will trigger it once only with all other values
    // set correctly and therefore validate the settings and apply them to dependent widgets
    ui->lowCut->setValue((int) (m_settings.m_profiles[m_settings.m_profileIndex].m_lowCutoff / 100.0));
    ui->lowCutText->setText(tr("%1k").arg(m_settings.m_profiles[m_settings.m_profileIndex].m_lowCutoff / 1000.0));

    auto volume = (int) CalcDb::dbPower(m_settings.m_volume);
    ui->volume->setValue(volume);
    ui->volumeText->setText(QString("%1").arg(volume));


    updateIndexLabel();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void WDSPRxGUI::leaveEvent(QEvent* event)
{
	m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void WDSPRxGUI::enterEvent(EnterEventType* event)
{
	m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void WDSPRxGUI::audioSelect(const QPoint& p)
{
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_audioDeviceName);
    audioSelect.move(p);
    new DialogPositioner(&audioSelect, false);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_audioDeviceName = audioSelect.m_audioDeviceName;
        applySettings();
    }
}

void WDSPRxGUI::agcSetupDialog(const QPoint& p)
{
    m_agcDialog = new WDSPRxAGCDialog();
    m_agcDialog->move(p);
    m_agcDialog->setAGCMode(m_settings.m_agcMode);
    m_agcDialog->setAGCSlope(m_settings.m_agcSlope);
    m_agcDialog->setAGCHangThreshold(m_settings.m_agcHangThreshold);
    QObject::connect(m_agcDialog, &WDSPRxAGCDialog::valueChanged, this, &WDSPRxGUI::agcSetup);
    m_agcDialog->exec();
    QObject::disconnect(m_agcDialog, &WDSPRxAGCDialog::valueChanged, this, &WDSPRxGUI::agcSetup);
    m_agcDialog->deleteLater();
    m_agcDialog = nullptr;
}

void WDSPRxGUI::agcSetup(int iValueChanged)
{
    if (!m_agcDialog) {
        return;
    }

    auto valueChanged = (WDSPRxAGCDialog::ValueChanged) iValueChanged;

    switch (valueChanged)
    {
    case WDSPRxAGCDialog::ValueChanged::ChangedMode:
        m_settings.m_agcMode = m_agcDialog->getAGCMode();
        m_settings.m_profiles[m_settings.m_profileIndex].m_agcMode = m_settings.m_agcMode;
        applySettings();
        break;
    case WDSPRxAGCDialog::ValueChanged::ChangedSlope:
        m_settings.m_agcSlope = m_agcDialog->getAGCSlope();
        m_settings.m_profiles[m_settings.m_profileIndex].m_agcSlope = m_settings.m_agcSlope;
        applySettings();
        break;
    case WDSPRxAGCDialog::ValueChanged::ChangedHangThreshold:
        m_settings.m_agcHangThreshold = m_agcDialog->getAGCHangThreshold();
        m_settings.m_profiles[m_settings.m_profileIndex].m_agcHangThreshold = m_settings.m_agcHangThreshold;
        applySettings();
        break;
    default:
        break;
    }
}

void WDSPRxGUI::dnbSetupDialog(const QPoint& p)
{
    m_dnbDialog = new WDSPRxDNBDialog();
    m_dnbDialog->move(p);
    m_dnbDialog->setNBScheme(m_settings.m_nbScheme);
    m_dnbDialog->setNB2Mode(m_settings.m_nb2Mode);
    m_dnbDialog->setNBSlewTime(m_settings.m_nbSlewTime);
    m_dnbDialog->setNBLeadTime(m_settings.m_nbLeadTime);
    m_dnbDialog->setNBLagTime(m_settings.m_nbLagTime);
    m_dnbDialog->setNBThreshold(m_settings.m_nbThreshold);
    m_dnbDialog->setNBAvgTime(m_settings.m_nbAvgTime);
    QObject::connect(m_dnbDialog, &WDSPRxDNBDialog::valueChanged, this, &WDSPRxGUI::dnbSetup);
    m_dnbDialog->exec();
    QObject::disconnect(m_dnbDialog, &WDSPRxDNBDialog::valueChanged, this, &WDSPRxGUI::dnbSetup);
    m_dnbDialog->deleteLater();
    m_dnbDialog = nullptr;
}

void WDSPRxGUI::dnbSetup(int32_t iValueChanged)
{
    if (!m_dnbDialog) {
        return;
    }

    auto valueChanged = (WDSPRxDNBDialog::ValueChanged) iValueChanged;

    switch (valueChanged)
    {
    case WDSPRxDNBDialog::ValueChanged::ChangedNB:
        m_settings.m_nbScheme = m_dnbDialog->getNBScheme();
        m_settings.m_profiles[m_settings.m_profileIndex].m_nbScheme = m_settings.m_nbScheme;
        applySettings();
        break;
    case WDSPRxDNBDialog::ValueChanged::ChangedNB2Mode:
        m_settings.m_nb2Mode = m_dnbDialog->getNB2Mode();
        m_settings.m_profiles[m_settings.m_profileIndex].m_nb2Mode = m_settings.m_nb2Mode;
        applySettings();
        break;
    case WDSPRxDNBDialog::ValueChanged::ChangedNBSlewTime:
        m_settings.m_nbSlewTime = m_dnbDialog->getNBSlewTime();
        m_settings.m_profiles[m_settings.m_profileIndex].m_nbSlewTime = m_settings.m_nbSlewTime;
        applySettings();
        break;
    case WDSPRxDNBDialog::ValueChanged::ChangedNBLeadTime:
        m_settings.m_nbLeadTime = m_dnbDialog->getNBLeadTime();
        m_settings.m_profiles[m_settings.m_profileIndex].m_nbLeadTime = m_settings.m_nbLeadTime;
        applySettings();
        break;
    case WDSPRxDNBDialog::ValueChanged::ChangedNBLagTime:
        m_settings.m_nbLagTime = m_dnbDialog->getNBLagTime();
        m_settings.m_profiles[m_settings.m_profileIndex].m_nbLagTime = m_settings.m_nbLagTime;
        applySettings();
        break;
    case WDSPRxDNBDialog::ValueChanged::ChangedNBThreshold:
        m_settings.m_nbThreshold = m_dnbDialog->getNBThreshold();
        m_settings.m_profiles[m_settings.m_profileIndex].m_nbThreshold = m_settings.m_nbThreshold;
        applySettings();
        break;
    case WDSPRxDNBDialog::ValueChanged::ChangedNBAvgTime:
        m_settings.m_nbAvgTime = m_dnbDialog->getNBAvgTime();
        m_settings.m_profiles[m_settings.m_profileIndex].m_nbAvgTime = m_settings.m_nbAvgTime;
        applySettings();
        break;
    default:
        break;
    }
}

void WDSPRxGUI::dnrSetupDialog(const QPoint& p)
{
    m_dnrDialog = new WDSPRxDNRDialog();
    m_dnrDialog->move(p);
    m_dnrDialog->setSNB(m_settings.m_snb);
    m_dnrDialog->setNRScheme(m_settings.m_nrScheme);
    m_dnrDialog->setNR2Gain(m_settings.m_nr2Gain);
    m_dnrDialog->setNR2NPE(m_settings.m_nr2NPE);
    m_dnrDialog->setNRPosition(m_settings.m_nrPosition);
    m_dnrDialog->setNR2ArtifactReduction(m_settings.m_nr2ArtifactReduction);
    QObject::connect(m_dnrDialog, &WDSPRxDNRDialog::valueChanged, this, &WDSPRxGUI::dnrSetup);
    m_dnrDialog->exec();
    QObject::disconnect(m_dnrDialog, &WDSPRxDNRDialog::valueChanged, this, &WDSPRxGUI::dnrSetup);
    m_dnrDialog->deleteLater();
    m_dnrDialog = nullptr;
}

void WDSPRxGUI::dnrSetup(int32_t iValueChanged)
{
    if (!m_dnrDialog) {
        return;
    }

    auto valueChanged = (WDSPRxDNRDialog::ValueChanged) iValueChanged;

    switch (valueChanged)
    {
    case WDSPRxDNRDialog::ValueChanged::ChangedSNB:
        m_settings.m_snb = m_dnrDialog->getSNB();
        m_settings.m_profiles[m_settings.m_profileIndex].m_snb = m_settings.m_snb;
        applySettings();
        break;
    case WDSPRxDNRDialog::ValueChanged::ChangedNR:
        m_settings.m_nrScheme = m_dnrDialog->getNRScheme();
        m_settings.m_profiles[m_settings.m_profileIndex].m_nrScheme = m_settings.m_nrScheme;
        applySettings();
        break;
    case WDSPRxDNRDialog::ValueChanged::ChangedNR2Gain:
        m_settings.m_nr2Gain = m_dnrDialog->getNR2Gain();
        m_settings.m_profiles[m_settings.m_profileIndex].m_nr2Gain = m_settings.m_nr2Gain;
        applySettings();
        break;
    case WDSPRxDNRDialog::ValueChanged::ChangedNR2NPE:
        m_settings.m_nr2NPE = m_dnrDialog->getNR2NPE();
        m_settings.m_profiles[m_settings.m_profileIndex].m_nr2NPE = m_settings.m_nr2NPE;
        applySettings();
        break;
    case WDSPRxDNRDialog::ValueChanged::ChangedNRPosition:
        m_settings.m_nrPosition = m_dnrDialog->getNRPosition();
        m_settings.m_profiles[m_settings.m_profileIndex].m_nrPosition = m_settings.m_nrPosition;
        applySettings();
        break;
    case WDSPRxDNRDialog::ValueChanged::ChangedNR2Artifacts:
        m_settings.m_nr2ArtifactReduction = m_dnrDialog->getNR2ArtifactReduction();
        m_settings.m_profiles[m_settings.m_profileIndex].m_nr2ArtifactReduction = m_settings.m_nr2ArtifactReduction;
        applySettings();
        break;
    default:
        break;
    }
}

void WDSPRxGUI::cwPeakSetupDialog(const QPoint& p)
{
    m_cwPeakDialog = new WDSPRxCWPeakDialog();
    m_cwPeakDialog->move(p);
    m_cwPeakDialog->setCWPeakFrequency(m_settings.m_cwPeakFrequency);
    m_cwPeakDialog->setCWBandwidth(m_settings.m_cwBandwidth);
    m_cwPeakDialog->setCWGain(m_settings.m_cwGain);
    QObject::connect(m_cwPeakDialog, &WDSPRxCWPeakDialog::valueChanged, this, &WDSPRxGUI::cwPeakSetup);
    m_cwPeakDialog->exec();
    QObject::disconnect(m_cwPeakDialog, &WDSPRxCWPeakDialog::valueChanged, this, &WDSPRxGUI::cwPeakSetup);
    m_cwPeakDialog->deleteLater();
    m_cwPeakDialog = nullptr;
}

void WDSPRxGUI::cwPeakSetup(int iValueChanged)
{
    if (!m_cwPeakDialog) {
        return;
    }

    auto valueChanged = (WDSPRxCWPeakDialog::ValueChanged) iValueChanged;

    switch (valueChanged)
    {
    case WDSPRxCWPeakDialog::ChangedCWPeakFrequency:
        m_settings.m_cwPeakFrequency = m_cwPeakDialog->getCWPeakFrequency();
        m_settings.m_profiles[m_settings.m_profileIndex].m_cwPeakFrequency = m_settings.m_cwPeakFrequency;
        applySettings();
        break;
    case WDSPRxCWPeakDialog::ChangedCWBandwidth:
        m_settings.m_cwBandwidth = m_cwPeakDialog->getCWBandwidth();
        m_settings.m_profiles[m_settings.m_profileIndex].m_cwBandwidth = m_settings.m_cwBandwidth;
        applySettings();
        break;
    case WDSPRxCWPeakDialog::ChangedCWGain:
        m_settings.m_cwGain = m_cwPeakDialog->getCWGain();
        m_settings.m_profiles[m_settings.m_profileIndex].m_cwGain = m_settings.m_cwGain;
        applySettings();
        break;
    default:
        break;
    }
}


void WDSPRxGUI::demodSetupDialog(const QPoint& p)
{
    if ((m_settings.m_demod == WDSPRxProfile::DemodAM) || (m_settings.m_demod == WDSPRxProfile::DemodSAM))
    {
        m_amDialog = new WDSPRxAMDialog();
        m_amDialog->move(p);
        m_amDialog->setFadeLevel(m_settings.m_amFadeLevel);
        QObject::connect(m_amDialog, &WDSPRxAMDialog::valueChanged, this, &WDSPRxGUI::amSetup);
        m_amDialog->exec();
        QObject::disconnect(m_amDialog, &WDSPRxAMDialog::valueChanged, this, &WDSPRxGUI::amSetup);
        m_amDialog->deleteLater();
        m_amDialog = nullptr;
    }
    else if (m_settings.m_demod == WDSPRxProfile::DemodFMN)
    {
        m_fmDialog = new WDSPRxFMDialog();
        m_fmDialog->move(p);
        m_fmDialog->setDeviation(m_settings.m_fmDeviation);
        m_fmDialog->setAFLow(m_settings.m_fmAFLow);
        m_fmDialog->setAFHigh(m_settings.m_fmAFHigh);
        m_fmDialog->setAFLimiter(m_settings.m_fmAFLimiter);
        m_fmDialog->setAFLimiterGain(m_settings.m_fmAFLimiterGain);
        m_fmDialog->setCTCSSNotch(m_settings.m_fmCTCSSNotch);
        m_fmDialog->setCTCSSNotchFrequency(m_settings.m_fmCTCSSNotchFrequency);
        QObject::connect(m_fmDialog, &WDSPRxFMDialog::valueChanged, this, &WDSPRxGUI::fmSetup);
        m_fmDialog->exec();
        QObject::disconnect(m_fmDialog, &WDSPRxFMDialog::valueChanged, this, &WDSPRxGUI::fmSetup);
        m_fmDialog->deleteLater();
        m_fmDialog = nullptr;
    }
}

void WDSPRxGUI::amSetup(int iValueChanged)
{
    if (!m_amDialog) {
        return;
    }

    auto valueChanged = (WDSPRxAMDialog::ValueChanged) iValueChanged;

    if (valueChanged == WDSPRxAMDialog::ChangedFadeLevel)
    {
        m_settings.m_amFadeLevel = m_amDialog->getFadeLevel();
        m_settings.m_profiles[m_settings.m_profileIndex].m_amFadeLevel = m_settings.m_amFadeLevel;
        applySettings();
    }
}

void WDSPRxGUI::fmSetup(int iValueChanged)
{
    if (!m_fmDialog) {
        return;
    }

    auto valueChanged = (WDSPRxFMDialog::ValueChanged) iValueChanged;

    switch (valueChanged)
    {
    case WDSPRxFMDialog::ChangedDeviation:
        m_settings.m_fmDeviation = m_fmDialog->getDeviation();
        m_settings.m_profiles[m_settings.m_profileIndex].m_fmDeviation = m_settings.m_fmDeviation;
        applySettings();
        break;
    case WDSPRxFMDialog::ChangedAFLow:
        m_settings.m_fmAFLow = m_fmDialog->getAFLow();
        m_settings.m_profiles[m_settings.m_profileIndex].m_fmAFLow = m_settings.m_fmAFLow;
        applySettings();
        break;
    case WDSPRxFMDialog::ChangedAFHigh:
        m_settings.m_fmAFHigh = m_fmDialog->getAFHigh();
        m_settings.m_profiles[m_settings.m_profileIndex].m_fmAFHigh = m_settings.m_fmAFHigh;
        applySettings();
        break;
    case WDSPRxFMDialog::ChangedAFLimiter:
        m_settings.m_fmAFLimiter = m_fmDialog->getAFLimiter();
        m_settings.m_profiles[m_settings.m_profileIndex].m_fmAFLimiter = m_settings.m_fmAFLimiter;
        applySettings();
        break;
    case WDSPRxFMDialog::ChangedAFLimiterGain:
        m_settings.m_fmAFLimiterGain = m_fmDialog->getAFLimiterGain();
        m_settings.m_profiles[m_settings.m_profileIndex].m_fmAFLimiterGain = m_settings.m_fmAFLimiterGain;
        applySettings();
        break;
    case WDSPRxFMDialog::ChangedCTCSSNotch:
        m_settings.m_fmCTCSSNotch = m_fmDialog->getCTCSSNotch();
        m_settings.m_profiles[m_settings.m_profileIndex].m_fmCTCSSNotch = m_settings.m_fmCTCSSNotch;
        applySettings();
        break;
    case WDSPRxFMDialog::ChangedCTCSSNotchFrequency:
        m_settings.m_fmCTCSSNotchFrequency = m_fmDialog->getCTCSSNotchFrequency();
        m_settings.m_profiles[m_settings.m_profileIndex].m_fmCTCSSNotchFrequency = m_settings.m_fmCTCSSNotchFrequency;
        applySettings();
        break;
    default:
        break;
    }
}

void WDSPRxGUI::squelchSetupDialog(const QPoint& p)
{
    m_squelchDialog = new WDSPRxSquelchDialog();
    m_squelchDialog->move(p);
    m_squelchDialog->setMode(m_settings.m_squelchMode);
    m_squelchDialog->setSSQLTauMute(m_settings.m_ssqlTauMute);
    m_squelchDialog->setSSQLTauUnmute(m_settings.m_ssqlTauUnmute);
    m_squelchDialog->setAMSQMaxTail(m_settings.m_amsqMaxTail);
    QObject::connect(m_squelchDialog, &WDSPRxSquelchDialog::valueChanged, this, &WDSPRxGUI::squelchSetup);
    m_squelchDialog->exec();
    QObject::disconnect(m_squelchDialog, &WDSPRxSquelchDialog::valueChanged, this, &WDSPRxGUI::squelchSetup);
    m_squelchDialog->deleteLater();
    m_squelchDialog = nullptr;
}

void WDSPRxGUI::squelchSetup(int iValueChanged)
{
    if (!m_squelchDialog) {
        return;
    }

    auto valueChanged = (WDSPRxSquelchDialog::ValueChanged) iValueChanged;

    switch (valueChanged)
    {
    case WDSPRxSquelchDialog::ChangedMode:
        m_settings.m_squelchMode = m_squelchDialog->getMode();
        m_settings.m_profiles[m_settings.m_profileIndex].m_squelchMode = m_settings.m_squelchMode;
        applySettings();
        break;
    case WDSPRxSquelchDialog::ChangedSSQLTauMute:
        m_settings.m_ssqlTauMute = m_squelchDialog->getSSQLTauMute();
        m_settings.m_profiles[m_settings.m_profileIndex].m_ssqlTauMute = m_settings.m_ssqlTauMute;
        applySettings();
        break;
    case WDSPRxSquelchDialog::ChangedSSQLTauUnmute:
        m_settings.m_ssqlTauUnmute = m_squelchDialog->getSSQLTauUnmute();
        m_settings.m_profiles[m_settings.m_profileIndex].m_ssqlTauUnmute = m_settings.m_ssqlTauUnmute;
        applySettings();
        break;
    case WDSPRxSquelchDialog::ChangedAMSQMaxTail:
        m_settings.m_amsqMaxTail = m_squelchDialog->getAMSQMaxTail();
        m_settings.m_profiles[m_settings.m_profileIndex].m_amsqMaxTail = m_settings.m_amsqMaxTail;
        applySettings();
        break;
    default:
        break;
    }
}

void WDSPRxGUI::equalizerSetupDialog(const QPoint& p)
{
    m_equalizerDialog = new WDSPRxEqDialog();
    m_equalizerDialog->move(p);
    m_equalizerDialog->setEqF(m_settings.m_eqF);
    m_equalizerDialog->setEqG(m_settings.m_eqG);
    QObject::connect(m_equalizerDialog, &WDSPRxEqDialog::valueChanged, this, &WDSPRxGUI::equalizerSetup);
    m_equalizerDialog->exec();
    QObject::disconnect(m_equalizerDialog, &WDSPRxEqDialog::valueChanged, this, &WDSPRxGUI::equalizerSetup);
    m_equalizerDialog->deleteLater();
    m_equalizerDialog = nullptr;
}

void WDSPRxGUI::equalizerSetup(int iValueChanged)
{
    if (!m_equalizerDialog) {
        return;
    }

    auto valueChanged = (WDSPRxEqDialog::ValueChanged) iValueChanged;

    switch (valueChanged)
    {
    case WDSPRxEqDialog::ChangedFrequency:
        m_settings.m_eqF = m_equalizerDialog->getEqF();
        m_settings.m_profiles[m_settings.m_profileIndex].m_eqF = m_settings.m_eqF;
        applySettings();
        break;
    case WDSPRxEqDialog::ChangedGain:
        m_settings.m_eqG = m_equalizerDialog->getEqG();
        m_settings.m_profiles[m_settings.m_profileIndex].m_eqG = m_settings.m_eqG;
        applySettings();
        break;
    default:
        break;
    }
}

void WDSPRxGUI::panSetupDialog(const QPoint& p)
{
    m_panDialog = new WDSPRxPanDialog();
    m_panDialog->move(p);
    m_panDialog->setPan(m_settings.m_audioPan);
    QObject::connect(m_panDialog, &WDSPRxPanDialog::valueChanged, this, &WDSPRxGUI::panSetup);
    m_panDialog->exec();
    QObject::disconnect(m_panDialog, &WDSPRxPanDialog::valueChanged, this, &WDSPRxGUI::panSetup);
    m_panDialog->deleteLater();
    m_panDialog = nullptr;
}

void WDSPRxGUI::panSetup(int iValueChanged)
{
    if (!m_panDialog) {
        return;
    }

    auto valueChanged = (WDSPRxPanDialog::ValueChanged) iValueChanged;

    if (valueChanged == WDSPRxPanDialog::ChangedPan)
    {
        m_settings.m_audioPan = m_panDialog->getPan();
        m_settings.m_profiles[m_settings.m_profileIndex].m_audioPan = m_settings.m_audioPan;
        applySettings();
    }
}

void WDSPRxGUI::tick()
{
    double powDbAvg;
    double powDbPeak;
    int nbMagsqSamples;
    m_wdspRx->getMagSqLevels(powDbAvg, powDbPeak, nbMagsqSamples); // powers directly in dB

    ui->channelPowerMeter->levelChanged(
            (WDSPRxSettings::m_mminPowerThresholdDBf + powDbAvg) / WDSPRxSettings::m_mminPowerThresholdDBf,
            (WDSPRxSettings::m_mminPowerThresholdDBf + powDbPeak) / WDSPRxSettings::m_mminPowerThresholdDBf,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(tr("%1 dB").arg(powDbAvg, 0, 'f', 1));
    }

    int audioSampleRate = m_wdspRx->getAudioSampleRate();
    bool squelchOpen = m_wdspRx->getAudioActive();

    if ((audioSampleRate != m_audioSampleRate) || (squelchOpen != m_squelchOpen))
    {
        if (audioSampleRate < 0) {
            ui->audioMute->setStyleSheet("QToolButton { background-color : red; }");
        } else if (squelchOpen) {
            ui->audioMute->setStyleSheet("QToolButton { background-color : green; }");
        } else {
            ui->audioMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        }

        m_audioSampleRate = audioSampleRate;
		m_squelchOpen = squelchOpen;
    }

    m_tickCount++;
}

void WDSPRxGUI::makeUIConnections() const
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &WDSPRxGUI::on_deltaFrequency_changed);
    QObject::connect(ui->audioBinaural, &QToolButton::toggled, this, &WDSPRxGUI::on_audioBinaural_toggled);
    QObject::connect(ui->audioFlipChannels, &QToolButton::toggled, this, &WDSPRxGUI::on_audioFlipChannels_toggled);
    QObject::connect(ui->dsb, &QToolButton::toggled, this, &WDSPRxGUI::on_dsb_toggled);
    QObject::connect(ui->BW, &TickedSlider::valueChanged, this, &WDSPRxGUI::on_BW_valueChanged);
    QObject::connect(ui->lowCut, &TickedSlider::valueChanged, this, &WDSPRxGUI::on_lowCut_valueChanged);
    QObject::connect(ui->volume, &QDial::valueChanged, this, &WDSPRxGUI::on_volume_valueChanged);
    QObject::connect(ui->agc, &ButtonSwitch::toggled, this, &WDSPRxGUI::on_agc_toggled);
    QObject::connect(ui->dnr, &ButtonSwitch::toggled, this, &WDSPRxGUI::on_dnr_toggled);
    QObject::connect(ui->dnb, &ButtonSwitch::toggled, this, &WDSPRxGUI::on_dnb_toggled);
    QObject::connect(ui->anf, &ButtonSwitch::toggled, this, &WDSPRxGUI::on_anf_toggled);
    QObject::connect(ui->agcGain, &QDial::valueChanged, this, &WDSPRxGUI::on_agcGain_valueChanged);
    QObject::connect(ui->audioMute, &QToolButton::toggled, this, &WDSPRxGUI::on_audioMute_toggled);
    QObject::connect(ui->spanLog2, &QSlider::valueChanged, this, &WDSPRxGUI::on_spanLog2_valueChanged);
    QObject::connect(ui->flipSidebands, &QPushButton::clicked, this, &WDSPRxGUI::on_flipSidebands_clicked);
    QObject::connect(ui->fftWindow, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WDSPRxGUI::on_fftWindow_currentIndexChanged);
    QObject::connect(ui->profileIndex, &QDial::valueChanged, this, &WDSPRxGUI::on_profileIndex_valueChanged);
    QObject::connect(ui->demod, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &WDSPRxGUI::on_demod_currentIndexChanged);
    QObject::connect(ui->cwPeaking, &ButtonSwitch::toggled, this, &WDSPRxGUI::on_cwPeaking_toggled);
    QObject::connect(ui->squelch, &ButtonSwitch::toggled, this, &WDSPRxGUI::on_squelch_toggled);
    QObject::connect(ui->squelchThreshold, &QDial::valueChanged, this, &WDSPRxGUI::on_squelchThreshold_valueChanged);
    QObject::connect(ui->equalizer, &ButtonSwitch::toggled, this, &WDSPRxGUI::on_equalizer_toggled);
    QObject::connect(ui->rit, &ButtonSwitch::toggled, this, &WDSPRxGUI::on_rit_toggled);
    QObject::connect(ui->ritFrequency, &QDial::valueChanged, this, &WDSPRxGUI::on_ritFrequency_valueChanged);
    QObject::connect(ui->dbOrS, &QToolButton::toggled, this, &WDSPRxGUI::on_dbOrS_toggled);
}

void WDSPRxGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
