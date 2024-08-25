///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2015-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2018 beta-tester <alpha-beta-release@gmx.net>                   //
// Copyright (C) 2018 Jason Gerecke <killertofu@gmail.com>                       //
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

#include "bfmdemodgui.h"

#include "device/deviceuiset.h"
#include <QDockWidget>
#include <QMainWindow>
#include <QDebug>
#include "boost/format.hpp"
#include <sstream>
#include <iostream>
#include <iomanip>

#include "dsp/dspengine.h"
#include "dsp/spectrumvis.h"
#include "dsp/dspcommands.h"
#include "gui/glspectrum.h"
#include "plugin/pluginapi.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/crightclickenabler.h"
#include "gui/audioselectdialog.h"
#include "gui/dialogpositioner.h"
#include "maincore.h"

#include "bfmdemodreport.h"
#include "bfmdemodsettings.h"
#include "bfmdemod.h"
#include "rdstmc.h"
#include "ui_bfmdemodgui.h"

BFMDemodGUI* BFMDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUIset, BasebandSampleSink *rxChannel)
{
	BFMDemodGUI* gui = new BFMDemodGUI(pluginAPI, deviceUIset, rxChannel);
	return gui;
}

void BFMDemodGUI::destroy()
{
	delete this;
}

void BFMDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();

    blockApplySettings(true);
    ui->g00AltFrequenciesBox->setEnabled(false);
    ui->g14MappedFrequencies->setEnabled(false);
    ui->g14AltFrequencies->setEnabled(false);
	blockApplySettings(false);

	applySettings();
}

QByteArray BFMDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool BFMDemodGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data)) {
        displaySettings();
        applySettings(true);
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

bool BFMDemodGUI::handleMessage(const Message& message)
{
    if (BFMDemodReport::MsgReportChannelSampleRateChanged::match(message))
    {
        BFMDemodReport::MsgReportChannelSampleRateChanged& report = (BFMDemodReport::MsgReportChannelSampleRateChanged&) message;
        m_rate = report.getSampleRate();
        qDebug("BFMDemodGUI::handleMessage: BFMDemodReport::MsgReportChannelSampleRateChanged: %d S/s", m_rate);
        ui->glSpectrum->setCenterFrequency(m_rate / 4);
        ui->glSpectrum->setSampleRate(m_rate / 2);
        return true;
    }
    else if (BFMDemod::MsgConfigureBFMDemod::match(message))
    {
        qDebug("BFMDemodGUI::handleMessage: BFMDemod::MsgConfigureBFMDemod");
        const BFMDemod::MsgConfigureBFMDemod& cfg = (BFMDemod::MsgConfigureBFMDemod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
		ui->spectrumGUI->updateSettings();
		m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        ui->deltaFrequency->setValueRange(false, 8, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();
        return true;
    }
    else
    {
        return false;
    }
}

void BFMDemodGUI::handleInputMessages()
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

void BFMDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void BFMDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void BFMDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings();
}

void BFMDemodGUI::on_rfBW_valueChanged(int value)
{
	ui->rfBWText->setText(QString("%1 kHz").arg(BFMDemodSettings::getRFBW(value) / 1000.0));
	m_channelMarker.setBandwidth(BFMDemodSettings::getRFBW(value));
	m_settings.m_rfBandwidth = BFMDemodSettings::getRFBW(value);
	applySettings();
}

void BFMDemodGUI::on_afBW_valueChanged(int value)
{
	ui->afBWText->setText(QString("%1 kHz").arg(value));
	m_settings.m_afBandwidth = value * 1000.0;
	applySettings();
}

void BFMDemodGUI::on_volume_valueChanged(int value)
{
	ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
	m_settings.m_volume = value / 10.0;
	applySettings();
}

void BFMDemodGUI::on_squelch_valueChanged(int value)
{
	ui->squelchText->setText(QString("%1 dB").arg(value));
	m_settings.m_squelch = value;
	applySettings();
}

void BFMDemodGUI::on_audioStereo_toggled(bool stereo)
{
	if (!stereo)
	{
		ui->audioStereo->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
	}

	m_settings.m_audioStereo = stereo;
	applySettings();
}

void BFMDemodGUI::on_lsbStereo_toggled(bool lsb)
{
    m_settings.m_lsbStereo = lsb;
	applySettings();
}

void BFMDemodGUI::on_showPilot_clicked()
{
    m_settings.m_showPilot = ui->showPilot->isChecked();
	applySettings();
}

void BFMDemodGUI::on_rds_clicked()
{
    m_settings.m_rdsActive = ui->rds->isChecked();
	applySettings();
}

void BFMDemodGUI::on_clearData_clicked(bool checked)
{
    (void) checked;
	if (ui->rds->isChecked())
	{
		if (m_bfmDemod->getRDSParser()) {
			m_bfmDemod->getRDSParser()->clearAllFields();
		}

		ui->g00ProgServiceName->clear();
		ui->go2Text->clear();
		ui->go2PrevText->clear();
		ui->g14ProgServiceNames->clear();
		ui->g14MappedFrequencies->clear();
		ui->g14AltFrequencies->clear();

		ui->g00AltFrequenciesBox->setEnabled(false);
		ui->g14MappedFrequencies->setEnabled(false);
		ui->g14AltFrequencies->setEnabled(false);

		rdsUpdate(true);
	}
}

void BFMDemodGUI::on_g14ProgServiceNames_currentIndexChanged(int _index)
{
	if (!m_bfmDemod->getRDSParser()) {
		return;
	}

    uint32_t index = _index & 0x7FFFFFF;

	if (index < m_g14ComboIndex.size())
	{
		unsigned int piKey = m_g14ComboIndex[index];
		RDSParser::freqs_map_t::const_iterator mIt = m_bfmDemod->getRDSParser()->m_g14_mapped_freqs.find(piKey);

		if (mIt != m_bfmDemod->getRDSParser()->m_g14_mapped_freqs.end())
		{
			ui->g14MappedFrequencies->clear();
			RDSParser::freqs_set_t::iterator sIt = (mIt->second).begin();
			const RDSParser::freqs_set_t::iterator sItEnd = (mIt->second).end();

			for (; sIt != sItEnd; ++sIt)
			{
				std::ostringstream os;
				os << std::fixed << std::showpoint << std::setprecision(2) << *sIt;
				ui->g14MappedFrequencies->addItem(os.str().c_str());
			}

			ui->g14MappedFrequencies->setEnabled(ui->g14MappedFrequencies->count() > 0);
		}

		mIt = m_bfmDemod->getRDSParser()->m_g14_alt_freqs.find(piKey);

		if (mIt != m_bfmDemod->getRDSParser()->m_g14_alt_freqs.end())
		{
			ui->g14AltFrequencies->clear();
			RDSParser::freqs_set_t::iterator sIt = (mIt->second).begin();
			const RDSParser::freqs_set_t::iterator sItEnd = (mIt->second).end();

			for (; sIt != sItEnd; ++sIt)
			{
				std::ostringstream os;
				os << std::fixed << std::showpoint << std::setprecision(2) << *sIt;
				ui->g14AltFrequencies->addItem(os.str().c_str());
			}

			ui->g14AltFrequencies->setEnabled(ui->g14AltFrequencies->count() > 0);
		}
	}
}

void BFMDemodGUI::on_g00AltFrequenciesBox_activated(int index)
{
    (void) index;
	qint64 f = (qint64) ((ui->g00AltFrequenciesBox->currentText()).toDouble() * 1e6);
	changeFrequency(f);
}

void BFMDemodGUI::on_go2ClearPrevText_clicked()
{
	ui->go2PrevText->clear();
}

void BFMDemodGUI::on_g14MappedFrequencies_activated(int index)
{
    (void) index;
	qint64 f = (qint64) ((ui->g14MappedFrequencies->currentText()).toDouble() * 1e6);
	changeFrequency(f);
}

void BFMDemodGUI::on_g14AltFrequencies_activated(int index)
{
    (void) index;
	qint64 f = (qint64) ((ui->g14AltFrequencies->currentText()).toDouble() * 1e6);
	changeFrequency(f);
}

void BFMDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings();
}

void BFMDemodGUI::onMenuDialogCalled(const QPoint &p)
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
            dialog.setNumberOfStreams(m_bfmDemod->getNumberOfDeviceStreams());
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

BFMDemodGUI::BFMDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
	ChannelGUI(parent),
	ui(new Ui::BFMDemodGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_basebandSampleRate(1),
	m_rdsTimerCount(0),
    m_radiotext_AB_flag(false),
	m_rate(625000)
{
	setAttribute(Qt::WA_DeleteOnClose, true);
	m_helpURL = "plugins/channelrx/demodbfm/readme.md";
    RollupContents *rollupContents = getRollupContents();
	ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
	connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 8, -99999999, 99999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    CRightClickEnabler *audioMuteRightClickEnabler = new CRightClickEnabler(ui->audioStereo);
    connect(audioMuteRightClickEnabler, SIGNAL(rightClick(const QPoint &)), this, SLOT(audioSelect(const QPoint &)));

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

	m_bfmDemod = (BFMDemod*) rxChannel;
    m_spectrumVis = m_bfmDemod->getSpectrumVis();
	m_spectrumVis->setGLSpectrum(ui->glSpectrum);
	m_bfmDemod->setMessageQueueToGUI(getInputMessageQueue());

	ui->spectrumGUI->setBuddies(m_spectrumVis, ui->glSpectrum);

    ui->glSpectrum->setCenterFrequency(m_rate / 4);
	ui->glSpectrum->setSampleRate(m_rate / 2);

    SpectrumSettings spectrumSettings = m_spectrumVis->getSettings();
    spectrumSettings.m_displayWaterfall = false;
    spectrumSettings.m_displayMaxHold = false;
    spectrumSettings.m_ssb = true;
    SpectrumVis::MsgConfigureSpectrumVis *msg = SpectrumVis::MsgConfigureSpectrumVis::create(spectrumSettings, false);
    m_spectrumVis->getInputMessageQueue()->push(msg);

	connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

	m_channelMarker.blockSignals(true);
	m_channelMarker.setColor(m_settings.m_rgbColor);
	m_channelMarker.setBandwidth(12500);
	m_channelMarker.setCenterFrequency(0);
	m_channelMarker.setTitle("Broadcast FM Demod");
	m_channelMarker.blockSignals(false);
	m_channelMarker.setVisible(true); // activate signal on the last setting only

	setTitleColor(m_channelMarker.getColor());

	m_settings.setChannelMarker(&m_channelMarker);
	m_settings.setSpectrumGUI(ui->spectrumGUI);
	m_settings.setRollupState(&m_rollupState);

	m_deviceUISet->addChannelMarker(&m_channelMarker);

	connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));

	ui->g00AltFrequenciesBox->setEnabled(false);
	ui->g14MappedFrequencies->setEnabled(false);
	ui->g14AltFrequencies->setEnabled(false);

#ifdef ANDROID
    // Currently a bit too wide for most Android screens
    ui->rdsContainer->setVisible(false);
#endif

	rdsUpdateFixedFields();
	rdsUpdate(true);
	displaySettings();
    makeUIConnections();
	applySettings(true);
    m_resizer.enableChildMouseTracking();
}

BFMDemodGUI::~BFMDemodGUI()
{
	delete ui;
}

void BFMDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void BFMDemodGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
        BFMDemod::MsgConfigureBFMDemod* msgConfig = BFMDemod::MsgConfigureBFMDemod::create( m_settings, force);
        m_bfmDemod->getInputMessageQueue()->push(msgConfig);
	}
}

void BFMDemodGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

    ui->rfBW->setValue(BFMDemodSettings::getRFBWIndex(m_settings.m_rfBandwidth));
    ui->rfBWText->setText(QString("%1 kHz").arg(m_settings.m_rfBandwidth / 1000.0));

    ui->afBW->setValue(m_settings.m_afBandwidth/1000.0);
    ui->afBWText->setText(QString("%1 kHz").arg(m_settings.m_afBandwidth/1000.0));

    ui->volume->setValue(m_settings.m_volume * 10.0);
    ui->volumeText->setText(QString("%1").arg(m_settings.m_volume, 0, 'f', 1));

    ui->squelch->setValue(m_settings.m_squelch);
    ui->squelchText->setText(QString("%1 dB").arg(m_settings.m_squelch));

    ui->audioStereo->setChecked(m_settings.m_audioStereo);
    ui->lsbStereo->setChecked(m_settings.m_lsbStereo);
    ui->showPilot->setChecked(m_settings.m_showPilot);
    ui->rds->setChecked(m_settings.m_rdsActive);

    updateIndexLabel();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void BFMDemodGUI::leaveEvent(QEvent* event)
{
	m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void BFMDemodGUI::enterEvent(EnterEventType* event)
{
	m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void BFMDemodGUI::audioSelect(const QPoint& p)
{
    qDebug("BFMDemodGUI::audioSelect");
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

void BFMDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_bfmDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    Real powDbAvg = CalcDb::dbPower(magsqAvg);
    Real powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));

	Real pilotPowDb =  CalcDb::dbPower(m_bfmDemod->getPilotLevel());
	QString pilotPowDbStr = QString("%1%2").arg(pilotPowDb < 0 ? '-' : '+').arg(pilotPowDb, 3, 'f', 1, QLatin1Char('0'));
	ui->pilotPower->setText(pilotPowDbStr);

    if (m_bfmDemod->getAudioSampleRate() < 0)
    {
        ui->audioStereo->setStyleSheet("QToolButton { background-color : red; }");
    }
    else
    {
        if (m_bfmDemod->getPilotLock())
        {
            if (ui->audioStereo->isChecked()) {
                ui->audioStereo->setStyleSheet("QToolButton { background-color : green; }");
            }
        }
        else
        {
            if (ui->audioStereo->isChecked()) {
                ui->audioStereo->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
            }
        }
    }

	if (ui->rds->isChecked() && (m_rdsTimerCount == 0))
	{
		rdsUpdate(false);
	}

	m_rdsTimerCount = (m_rdsTimerCount + 1) % 25;

	//qDebug() << "Pilot lock: " << m_bfmDemod->getPilotLock() << ":" << m_bfmDemod->getPilotLevel(); TODO: update a GUI item with status
}

void BFMDemodGUI::rdsUpdateFixedFields()
{
	if (!m_bfmDemod->getRDSParser()) {
		return;
	}

	ui->g00Label->setText(m_bfmDemod->getRDSParser()->rds_group_acronym_tags[0].c_str());
	ui->g01Label->setText(m_bfmDemod->getRDSParser()->rds_group_acronym_tags[1].c_str());
	ui->g02Label->setText(m_bfmDemod->getRDSParser()->rds_group_acronym_tags[2].c_str());
	ui->g03Label->setText(m_bfmDemod->getRDSParser()->rds_group_acronym_tags[3].c_str());
	ui->g04Label->setText(m_bfmDemod->getRDSParser()->rds_group_acronym_tags[4].c_str());
	//ui->g05Label->setText(m_bfmDemod->getRDSParser()->rds_group_acronym_tags[5].c_str());
	//ui->g06Label->setText(m_bfmDemod->getRDSParser()->rds_group_acronym_tags[6].c_str());
	//ui->g07Label->setText(m_bfmDemod->getRDSParser()->rds_group_acronym_tags[7].c_str());
	ui->g08Label->setText(m_bfmDemod->getRDSParser()->rds_group_acronym_tags[8].c_str());
	ui->g09Label->setText(m_bfmDemod->getRDSParser()->rds_group_acronym_tags[9].c_str());
	ui->g14Label->setText(m_bfmDemod->getRDSParser()->rds_group_acronym_tags[14].c_str());

	ui->g00CountLabel->setText(m_bfmDemod->getRDSParser()->rds_group_acronym_tags[0].c_str());
	ui->g01CountLabel->setText(m_bfmDemod->getRDSParser()->rds_group_acronym_tags[1].c_str());
	ui->g02CountLabel->setText(m_bfmDemod->getRDSParser()->rds_group_acronym_tags[2].c_str());
	ui->g03CountLabel->setText(m_bfmDemod->getRDSParser()->rds_group_acronym_tags[3].c_str());
	ui->g04CountLabel->setText(m_bfmDemod->getRDSParser()->rds_group_acronym_tags[4].c_str());
	ui->g05CountLabel->setText(m_bfmDemod->getRDSParser()->rds_group_acronym_tags[5].c_str());
	ui->g06CountLabel->setText(m_bfmDemod->getRDSParser()->rds_group_acronym_tags[6].c_str());
	ui->g07CountLabel->setText(m_bfmDemod->getRDSParser()->rds_group_acronym_tags[7].c_str());
	ui->g08CountLabel->setText(m_bfmDemod->getRDSParser()->rds_group_acronym_tags[8].c_str());
	ui->g09CountLabel->setText(m_bfmDemod->getRDSParser()->rds_group_acronym_tags[9].c_str());
	ui->g14CountLabel->setText(m_bfmDemod->getRDSParser()->rds_group_acronym_tags[14].c_str());
}

void BFMDemodGUI::rdsUpdate(bool force)
{
	if (!m_bfmDemod->getRDSParser()) {
		return;
	}

	// Quality metrics
	ui->demodQText->setText(QString("%1 %").arg(m_bfmDemod->getDemodQua(), 0, 'f', 0));
	ui->decoderQText->setText(QString("%1 %").arg(m_bfmDemod->getDecoderQua(), 0, 'f', 0));
	Real accDb = CalcDb::dbPower(std::fabs(m_bfmDemod->getDemodAcc()));
	ui->accumText->setText(QString("%1 dB").arg(accDb, 0, 'f', 1));
	ui->fclkText->setText(QString("%1 Hz").arg(m_bfmDemod->getDemodFclk(), 0, 'f', 2));

	if (m_bfmDemod->getDecoderSynced()) {
		ui->decoderQLabel->setStyleSheet("QLabel { background-color : green; }");
	} else {
		ui->decoderQLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
	}

	// PI group
	if (m_bfmDemod->getRDSParser()->m_pi_updated || force)
	{
		ui->piLabel->setStyleSheet("QLabel { background-color : green; }");
		ui->piCountText->setNum((int) m_bfmDemod->getRDSParser()->m_pi_count);
		QString pistring(str(boost::format("%04X") % m_bfmDemod->getRDSParser()->m_pi_program_identification).c_str());
		ui->piText->setText(pistring);

		if (m_bfmDemod->getRDSParser()->m_pi_traffic_program) {
			ui->piTPIndicator->setStyleSheet("QLabel { background-color : green; }");
		} else {
			ui->piTPIndicator->setStyleSheet("QLabel { background:rgb(79,79,79); }");
		}

		ui->piType->setText(QString(m_bfmDemod->getRDSParser()->pty_table[m_bfmDemod->getRDSParser()->m_pi_program_type].c_str()));
		ui->piCoverage->setText(QString(m_bfmDemod->getRDSParser()->coverage_area_codes[m_bfmDemod->getRDSParser()->m_pi_area_coverage_index].c_str()));
	}
	else
	{
		ui->piLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
	}

	// G0 group
	if (m_bfmDemod->getRDSParser()->m_g0_updated || force)
	{
		ui->g00Label->setStyleSheet("QLabel { background-color : green; }");
		ui->g00CountText->setNum((int) m_bfmDemod->getRDSParser()->m_g0_count);

		if (m_bfmDemod->getRDSParser()->m_g0_psn_bitmap == 0b1111) {
			ui->g00ProgServiceName->setText(QString(m_bfmDemod->getRDSParser()->m_g0_program_service_name));
		}

		if (m_bfmDemod->getRDSParser()->m_g0_traffic_announcement) {
			ui->g00TrafficAnnouncement->setStyleSheet("QLabel { background-color : green; }");
		} else {
			ui->g00TrafficAnnouncement->setStyleSheet("QLabel { background:rgb(79,79,79); }");
		}

		ui->g00MusicSpeech->setText(QString((m_bfmDemod->getRDSParser()->m_g0_music_speech ? "Music" : "Speech")));
		ui->g00MonoStereo->setText(QString((m_bfmDemod->getRDSParser()->m_g0_mono_stereo ? "Mono" : "Stereo")));

		if (m_bfmDemod->getRDSParser()->m_g0_af_updated)
		{
			ui->g00AltFrequenciesBox->clear();

			for (std::set<double>::iterator it = m_bfmDemod->getRDSParser()->m_g0_alt_freq.begin(); it != m_bfmDemod->getRDSParser()->m_g0_alt_freq.end(); ++it)
			{
				if (*it > 76.0)
				{
					std::ostringstream os;
					os << std::fixed << std::showpoint << std::setprecision(2) << *it;
					ui->g00AltFrequenciesBox->addItem(QString(os.str().c_str()));
				}
			}

			ui->g00AltFrequenciesBox->setEnabled(ui->g00AltFrequenciesBox->count() > 0);
		}
	}
	else
	{
		ui->g00Label->setStyleSheet("QLabel { background:rgb(79,79,79); }");
	}

	// G1 group
	if (m_bfmDemod->getRDSParser()->m_g1_updated || force)
	{
		ui->g01Label->setStyleSheet("QLabel { background-color : green; }");
		ui->g01CountText->setNum((int) m_bfmDemod->getRDSParser()->m_g1_count);

		if ((m_bfmDemod->getRDSParser()->m_g1_country_page_index >= 0) && (m_bfmDemod->getRDSParser()->m_g1_country_index >= 0)) {
			ui->g01CountryCode->setText(QString((m_bfmDemod->getRDSParser()->pi_country_codes[m_bfmDemod->getRDSParser()->m_g1_country_page_index][m_bfmDemod->getRDSParser()->m_g1_country_index]).c_str()));
		}

		if (m_bfmDemod->getRDSParser()->m_g1_language_index >= 0) {
			ui->g01Language->setText(QString(m_bfmDemod->getRDSParser()->language_codes[m_bfmDemod->getRDSParser()->m_g1_language_index].c_str()));
		}

		ui->g01DHM->setText(QString(str(boost::format("%id:%i:%i") % m_bfmDemod->getRDSParser()->m_g1_pin_day % m_bfmDemod->getRDSParser()->m_g1_pin_hour % m_bfmDemod->getRDSParser()->m_g1_pin_minute).c_str()));
	}
	else
	{
		ui->g01Label->setStyleSheet("QLabel { background:rgb(79,79,79); }");
	}

	// G2 group
	if (m_bfmDemod->getRDSParser()->m_g2_updated || force)
	{
		ui->g02Label->setStyleSheet("QLabel { background-color : green; }");
		ui->g02CountText->setNum((int) m_bfmDemod->getRDSParser()->m_g2_count);
        bool radiotext_AB_flag = m_bfmDemod->getRDSParser()->m_radiotext_AB_flag;

        if (!m_radiotext_AB_flag && radiotext_AB_flag) // B -> A transiition is start of new text
        {
            QString oldText = ui->go2Text->text();
            ui->go2PrevText->setText(oldText);
        }

		ui->go2Text->setText(QString(m_bfmDemod->getRDSParser()->m_g2_radiotext));
        m_radiotext_AB_flag = radiotext_AB_flag;
	}
	else
	{
		ui->g02Label->setStyleSheet("QLabel { background:rgb(79,79,79); }");
	}

	// G3 group
	if (m_bfmDemod->getRDSParser()->m_g3_updated || force)
	{
		ui->g03Label->setStyleSheet("QLabel { background-color : green; }");
		ui->g03CountText->setNum((int) m_bfmDemod->getRDSParser()->m_g3_count);
		std::string g3str = str(boost::format("%02X%c %04X %04X") % m_bfmDemod->getRDSParser()->m_g3_appGroup % (m_bfmDemod->getRDSParser()->m_g3_groupB ? 'B' : 'A') % m_bfmDemod->getRDSParser()->m_g3_message % m_bfmDemod->getRDSParser()->m_g3_aid);
		ui->g03Data->setText(QString(g3str.c_str()));
	}
	else
	{
		ui->g03Label->setStyleSheet("QLabel { background:rgb(79,79,79); }");
	}

	// G4 group
	if (m_bfmDemod->getRDSParser()->m_g4_updated || force)
	{
		ui->g04Label->setStyleSheet("QLabel { background-color : green; }");
		ui->g04CountText->setNum((int) m_bfmDemod->getRDSParser()->m_g4_count);
		std::string time = str(boost::format("%4i-%02i-%02i %02i:%02i (%+.1fh)")\
			% (1900 + m_bfmDemod->getRDSParser()->m_g4_year) % m_bfmDemod->getRDSParser()->m_g4_month % m_bfmDemod->getRDSParser()->m_g4_day % m_bfmDemod->getRDSParser()->m_g4_hours % m_bfmDemod->getRDSParser()->m_g4_minutes % m_bfmDemod->getRDSParser()->m_g4_local_time_offset);
	    ui->g04Time->setText(QString(time.c_str()));
	}
	else
	{
		ui->g04Label->setStyleSheet("QLabel { background:rgb(79,79,79); }");
	}

	// G5 group
	if (m_bfmDemod->getRDSParser()->m_g5_updated || force)
	{
		ui->g05CountText->setNum((int) m_bfmDemod->getRDSParser()->m_g5_count);
	}

	// G6 group
	if (m_bfmDemod->getRDSParser()->m_g6_updated || force)
	{
		ui->g06CountText->setNum((int) m_bfmDemod->getRDSParser()->m_g6_count);
	}

	// G7 group
	if (m_bfmDemod->getRDSParser()->m_g7_updated || force)
	{
		ui->g07CountText->setNum((int) m_bfmDemod->getRDSParser()->m_g7_count);
	}

	// G8 group
	if (m_bfmDemod->getRDSParser()->m_g8_updated || force)
	{
		ui->g08Label->setStyleSheet("QLabel { background-color : green; }");
		ui->g08CountText->setNum((int) m_bfmDemod->getRDSParser()->m_g8_count);

		std::ostringstream os;
		os << (m_bfmDemod->getRDSParser()->m_g8_sign ? "-" : "+") << m_bfmDemod->getRDSParser()->m_g8_extent + 1;
		ui->g08Extent->setText(QString(os.str().c_str()));
		int event_line = RDSTMC::get_tmc_event_code_index(m_bfmDemod->getRDSParser()->m_g8_event, 1);
		ui->g08TMCEvent->setText(QString(RDSTMC::get_tmc_events(event_line, 1).c_str()));
		QString pistring(str(boost::format("%04X") % m_bfmDemod->getRDSParser()->m_g8_location).c_str());
		ui->g08Location->setText(pistring);

		if (m_bfmDemod->getRDSParser()->m_g8_label_index >= 0) {
			ui->g08Description->setText(QString(m_bfmDemod->getRDSParser()->label_descriptions[m_bfmDemod->getRDSParser()->m_g8_label_index].c_str()));
		}

		ui->g08Content->setNum(m_bfmDemod->getRDSParser()->m_g8_content);
	}
	else
	{
		ui->g08Label->setStyleSheet("QLabel { background:rgb(79,79,79); }");
	}

	// G9 group
	if (m_bfmDemod->getRDSParser()->m_g9_updated || force)
	{
		ui->g09Label->setStyleSheet("QLabel { background-color : green; }");
		ui->g09CountText->setNum((int) m_bfmDemod->getRDSParser()->m_g9_count);
		std::string g9str = str(boost::format("%02X %04X %04X %02X %04X") % m_bfmDemod->getRDSParser()->m_g9_varA % m_bfmDemod->getRDSParser()->m_g9_cA % m_bfmDemod->getRDSParser()->m_g9_dA % m_bfmDemod->getRDSParser()->m_g9_varB % m_bfmDemod->getRDSParser()->m_g9_dB);
		ui->g09Data->setText(QString(g9str.c_str()));
	}
	else
	{
		ui->g09Label->setStyleSheet("QLabel { background:rgb(79,79,79); }");
	}

	// G14 group
	if (m_bfmDemod->getRDSParser()->m_g14_updated || force)
	{
		ui->g14CountText->setNum((int) m_bfmDemod->getRDSParser()->m_g14_count);

		if (m_bfmDemod->getRDSParser()->m_g14_data_available)
		{
			ui->g14Label->setStyleSheet("QLabel { background-color : green; }");
			m_g14ComboIndex.clear();
			ui->g14ProgServiceNames->clear();

			RDSParser::psns_map_t::iterator it = m_bfmDemod->getRDSParser()->m_g14_program_service_names.begin();
			const RDSParser::psns_map_t::iterator itEnd = m_bfmDemod->getRDSParser()->m_g14_program_service_names.end();
			int i = 0;

			for (; it != itEnd; ++it, i++)
			{
				m_g14ComboIndex.push_back(it->first);
				QString pistring(str(boost::format("%04X:%s") % it->first % it->second).c_str());
				ui->g14ProgServiceNames->addItem(pistring);
			}
		}
		else
		{
			ui->g14Label->setStyleSheet("QLabel { background:rgb(79,79,79); }");
		}
	}

	m_bfmDemod->getRDSParser()->clearUpdateFlags();
}

void BFMDemodGUI::changeFrequency(qint64 f)
{
	qint64 df = m_channelMarker.getCenterFrequency();
	qDebug() << "BFMDemodGUI::changeFrequency: " << f - df;
}

void BFMDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &BFMDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &BFMDemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->afBW, &QSlider::valueChanged, this, &BFMDemodGUI::on_afBW_valueChanged);
    QObject::connect(ui->volume, &QSlider::valueChanged, this, &BFMDemodGUI::on_volume_valueChanged);
    QObject::connect(ui->squelch, &QSlider::valueChanged, this, &BFMDemodGUI::on_squelch_valueChanged);
    QObject::connect(ui->audioStereo, &QToolButton::toggled, this, &BFMDemodGUI::on_audioStereo_toggled);
    QObject::connect(ui->lsbStereo, &ButtonSwitch::toggled, this, &BFMDemodGUI::on_lsbStereo_toggled);
    QObject::connect(ui->showPilot, &ButtonSwitch::clicked, this, &BFMDemodGUI::on_showPilot_clicked);
    QObject::connect(ui->rds, &ButtonSwitch::clicked, this, &BFMDemodGUI::on_rds_clicked);
    QObject::connect(ui->clearData, &QPushButton::clicked, this, &BFMDemodGUI::on_clearData_clicked);
    QObject::connect(ui->go2ClearPrevText, &QPushButton::clicked, this, &BFMDemodGUI::on_go2ClearPrevText_clicked);
}

void BFMDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
