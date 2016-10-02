///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "../../channelrx/demodbfm/bfmdemodgui.h"

#include <QDockWidget>
#include <QMainWindow>
#include <QDebug>
#include "boost/format.hpp"
#include <sstream>
#include <iostream>
#include <iomanip>

#include "dsp/threadedsamplesink.h"
#include "dsp/channelizer.h"
#include "dsp/dspengine.h"
#include "dsp/spectrumvis.h"
#include "gui/glspectrum.h"
#include "plugin/pluginapi.h"
#include "device/deviceapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingswidget.h"
#include "mainwindow.h"

#include "../../channelrx/demodbfm/bfmdemod.h"
#include "../../channelrx/demodbfm/rdstmc.h"
#include "ui_bfmdemodgui.h"

const QString BFMDemodGUI::m_channelID = "sdrangel.channel.bfm";

const int BFMDemodGUI::m_rfBW[] = {
	80000, 100000, 120000, 140000, 160000, 180000, 200000, 220000, 250000
};

int requiredBW(int rfBW)
{
	if (rfBW <= 48000)
		return 48000;
	else if (rfBW < 100000)
		return 96000;
	else
		return 384000;
}

BFMDemodGUI* BFMDemodGUI::create(PluginAPI* pluginAPI, DeviceAPI *deviceAPI)
{
	BFMDemodGUI* gui = new BFMDemodGUI(pluginAPI, deviceAPI);
	return gui;
}

void BFMDemodGUI::destroy()
{
	delete this;
}

void BFMDemodGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString BFMDemodGUI::getName() const
{
	return objectName();
}

qint64 BFMDemodGUI::getCenterFrequency() const
{
	return m_channelMarker.getCenterFrequency();
}

void BFMDemodGUI::setCenterFrequency(qint64 centerFrequency)
{
	m_channelMarker.setCenterFrequency(centerFrequency);
	applySettings();
}

void BFMDemodGUI::resetToDefaults()
{
	blockApplySettings(true);

	ui->rfBW->setValue(4);
	ui->afBW->setValue(3);
	ui->volume->setValue(20);
	ui->squelch->setValue(-40);
	ui->deltaFrequency->setValue(0);

	blockApplySettings(false);
	applySettings();
}

QByteArray BFMDemodGUI::serialize() const
{
	SimpleSerializer s(1);
	s.writeS32(1, m_channelMarker.getCenterFrequency());
	s.writeS32(2, ui->rfBW->value());
	s.writeS32(3, ui->afBW->value());
	s.writeS32(4, ui->volume->value());
	s.writeS32(5, ui->squelch->value());
	s.writeU32(7, m_channelMarker.getColor().rgb());
	s.writeBlob(8, ui->spectrumGUI->serialize());
	s.writeBool(9, ui->audioStereo->isChecked());
	s.writeBool(10, ui->lsbStereo->isChecked());
	return s.final();
}

bool BFMDemodGUI::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if (!d.isValid())
	{
		resetToDefaults();
		return false;
	}

	if (d.getVersion() == 1)
	{
		QByteArray bytetmp;
		quint32 u32tmp;
		qint32 tmp;
		bool booltmp;

		blockApplySettings(true);
	    m_channelMarker.blockSignals(true);

		d.readS32(1, &tmp, 0);
		m_channelMarker.setCenterFrequency(tmp);

		d.readS32(2, &tmp, 4);
		ui->rfBW->setValue(tmp);
		ui->rfBWText->setText(QString("%1 kHz").arg(m_rfBW[tmp] / 1000.0));
		m_channelMarker.setBandwidth(m_rfBW[tmp]);

		d.readS32(3, &tmp, 3);
		ui->afBW->setValue(tmp);

		d.readS32(4, &tmp, 20);
		ui->volume->setValue(tmp);

		d.readS32(5, &tmp, -40);
		ui->squelch->setValue(tmp);

		/*
		if(d.readU32(7, &u32tmp))
		{
			m_channelMarker.setColor(u32tmp);
		}*/

		d.readBlob(8, &bytetmp);
		ui->spectrumGUI->deserialize(bytetmp);

		d.readBool(9, &booltmp, false);
		ui->audioStereo->setChecked(booltmp);

		d.readBool(10, &booltmp, false);
		ui->lsbStereo->setChecked(booltmp);

		blockApplySettings(false);
	    m_channelMarker.blockSignals(false);

		applySettings();
		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}

bool BFMDemodGUI::handleMessage(const Message& message)
{
	return false;
}

void BFMDemodGUI::viewChanged()
{
	applySettings();
}

void BFMDemodGUI::on_deltaMinus_toggled(bool minus)
{
	int deltaFrequency = m_channelMarker.getCenterFrequency();
	bool minusDelta = (deltaFrequency < 0);

	if (minus ^ minusDelta) // sign change
	{
		m_channelMarker.setCenterFrequency(-deltaFrequency);
	}
}

void BFMDemodGUI::on_deltaFrequency_changed(quint64 value)
{
	if (ui->deltaMinus->isChecked())
	{
		m_channelMarker.setCenterFrequency(-value);
	}
	else
	{
		m_channelMarker.setCenterFrequency(value);
	}
}

void BFMDemodGUI::on_rfBW_valueChanged(int value)
{
	ui->rfBWText->setText(QString("%1 kHz").arg(m_rfBW[value] / 1000.0));
	m_channelMarker.setBandwidth(m_rfBW[value]);
	applySettings();
}

void BFMDemodGUI::on_afBW_valueChanged(int value)
{
	ui->afBWText->setText(QString("%1 kHz").arg(value));
	applySettings();
}

void BFMDemodGUI::on_volume_valueChanged(int value)
{
	ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
	applySettings();
}

void BFMDemodGUI::on_squelch_valueChanged(int value)
{
	ui->squelchText->setText(QString("%1 dB").arg(value));
	applySettings();
}

void BFMDemodGUI::on_audioStereo_toggled(bool stereo)
{
	if (!stereo)
	{
		ui->audioStereo->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
	}

	applySettings();
}

void BFMDemodGUI::on_lsbStereo_toggled(bool lsb)
{
	applySettings();
}

void BFMDemodGUI::on_showPilot_clicked()
{
	applySettings();
}

void BFMDemodGUI::on_rds_clicked()
{
	applySettings();
}

void BFMDemodGUI::on_clearData_clicked(bool checked)
{
	if (ui->rds->isChecked())
	{
		m_rdsParser.clearAllFields();

		ui->g14ProgServiceNames->clear();
		ui->g14MappedFrequencies->clear();
		ui->g14AltFrequencies->clear();

		ui->g00AltFrequenciesBox->setEnabled(false);
		ui->g14MappedFrequencies->setEnabled(false);
		ui->g14AltFrequencies->setEnabled(false);

		rdsUpdate(true);
	}
}

void BFMDemodGUI::on_g14ProgServiceNames_currentIndexChanged(int index)
{
	if (index < m_g14ComboIndex.size())
	{
		unsigned int piKey = m_g14ComboIndex[index];
		RDSParser::freqs_map_t::const_iterator mIt = m_rdsParser.m_g14_mapped_freqs.find(piKey);

		if (mIt != m_rdsParser.m_g14_mapped_freqs.end())
		{
			ui->g14MappedFrequencies->clear();
			RDSParser::freqs_set_t::iterator sIt = (mIt->second).begin();
			const RDSParser::freqs_set_t::iterator sItEnd = (mIt->second).end();

			for (sIt; sIt != sItEnd; ++sIt)
			{
				std::ostringstream os;
				os << std::fixed << std::showpoint << std::setprecision(2) << *sIt;
				ui->g14MappedFrequencies->addItem(os.str().c_str());
			}

			ui->g14MappedFrequencies->setEnabled(ui->g14MappedFrequencies->count() > 0);
		}

		mIt = m_rdsParser.m_g14_alt_freqs.find(piKey);

		if (mIt != m_rdsParser.m_g14_alt_freqs.end())
		{
			ui->g14AltFrequencies->clear();
			RDSParser::freqs_set_t::iterator sIt = (mIt->second).begin();
			const RDSParser::freqs_set_t::iterator sItEnd = (mIt->second).end();

			for (sIt; sIt != sItEnd; ++sIt)
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
	qint64 f = (qint64) ((ui->g00AltFrequenciesBox->currentText()).toDouble() * 1e6);
	changeFrequency(f);
}

void BFMDemodGUI::on_g14MappedFrequencies_activated(int index)
{
	qint64 f = (qint64) ((ui->g14MappedFrequencies->currentText()).toDouble() * 1e6);
	changeFrequency(f);
}

void BFMDemodGUI::on_g14AltFrequencies_activated(int index)
{
	qint64 f = (qint64) ((ui->g14AltFrequencies->currentText()).toDouble() * 1e6);
	changeFrequency(f);
}


void BFMDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
}

void BFMDemodGUI::onMenuDoubleClicked()
{
	if(!m_basicSettingsShown)
	{
		m_basicSettingsShown = true;
		BasicChannelSettingsWidget* bcsw = new BasicChannelSettingsWidget(&m_channelMarker, this);
		bcsw->show();
	}
}

BFMDemodGUI::BFMDemodGUI(PluginAPI* pluginAPI, DeviceAPI *deviceAPI, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::BFMDemodGUI),
	m_pluginAPI(pluginAPI),
	m_deviceAPI(deviceAPI),
	m_channelMarker(this),
	m_basicSettingsShown(false),
	m_rdsTimerCount(0),
	m_channelPowerDbAvg(20,0),
	m_rate(625000)
{
	ui->setupUi(this);
	ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));
	ui->deltaFrequency->setValueRange(7, 0U, 9999999U);

	setAttribute(Qt::WA_DeleteOnClose, true);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(menuDoubleClickEvent()), this, SLOT(onMenuDoubleClicked()));

	m_spectrumVis = new SpectrumVis(ui->glSpectrum);
	m_bfmDemod = new BFMDemod(m_spectrumVis, &m_rdsParser);
	m_channelizer = new Channelizer(m_bfmDemod);
	m_threadedChannelizer = new ThreadedSampleSink(m_channelizer, this);
	connect(m_channelizer, SIGNAL(inputSampleRateChanged()), this, SLOT(channelSampleRateChanged()));
	m_deviceAPI->addThreadedSink(m_threadedChannelizer);

	ui->glSpectrum->setCenterFrequency(m_rate / 4);
	ui->glSpectrum->setSampleRate(m_rate / 2);
	ui->glSpectrum->setDisplayWaterfall(false);
	ui->glSpectrum->setDisplayMaxHold(false);
	ui->glSpectrum->setSsbSpectrum(true);
	m_spectrumVis->configure(m_spectrumVis->getInputMessageQueue(), 64, 10, FFTWindow::BlackmanHarris);
	connect(&m_pluginAPI->getMainWindow()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

	//m_channelMarker = new ChannelMarker(this);
	//m_channelMarker.setColor(Qt::blue);
	m_channelMarker.setColor(QColor(80, 120, 228));
	m_channelMarker.setBandwidth(12500);
	m_channelMarker.setCenterFrequency(0);
	m_channelMarker.setVisible(true);

	connect(&m_channelMarker, SIGNAL(changed()), this, SLOT(viewChanged()));

	m_deviceAPI->registerChannelInstance(m_channelID, this);
	m_deviceAPI->addChannelMarker(&m_channelMarker);
	m_deviceAPI->addRollupWidget(this);

	ui->spectrumGUI->setBuddies(m_spectrumVis->getInputMessageQueue(), m_spectrumVis, ui->glSpectrum);

	ui->g00AltFrequenciesBox->setEnabled(false);
	ui->g14MappedFrequencies->setEnabled(false);
	ui->g14AltFrequencies->setEnabled(false);

	rdsUpdateFixedFields();
	rdsUpdate(true);

	applySettings();
}

BFMDemodGUI::~BFMDemodGUI()
{
    m_deviceAPI->removeChannelInstance(this);
	m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
	delete m_threadedChannelizer;
	delete m_channelizer;
	delete m_bfmDemod;
	//delete m_channelMarker;
	delete ui;
}

void BFMDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void BFMDemodGUI::applySettings()
{
	if (m_doApplySettings)
	{
		setTitleColor(m_channelMarker.getColor());

		m_channelizer->configure(m_channelizer->getInputMessageQueue(),
			requiredBW(m_rfBW[ui->rfBW->value()]), // TODO: this is where requested sample rate is specified
			m_channelMarker.getCenterFrequency());

		ui->deltaFrequency->setValue(abs(m_channelMarker.getCenterFrequency()));
		ui->deltaMinus->setChecked(m_channelMarker.getCenterFrequency() < 0);

		m_bfmDemod->configure(m_bfmDemod->getInputMessageQueue(),
			m_rfBW[ui->rfBW->value()],
			ui->afBW->value() * 1000.0,
			ui->volume->value() / 10.0,
			ui->squelch->value(),
			ui->audioStereo->isChecked(),
			ui->lsbStereo->isChecked(),
			ui->showPilot->isChecked(),
			ui->rds->isChecked());
	}
}

void BFMDemodGUI::leaveEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(false);
	blockApplySettings(false);
}

void BFMDemodGUI::enterEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(true);
	blockApplySettings(false);
}

void BFMDemodGUI::tick()
{
	Real powDb = CalcDb::dbPower(m_bfmDemod->getMagSq());
	m_channelPowerDbAvg.feed(powDb);
	ui->channelPower->setText(QString::number(m_channelPowerDbAvg.average(), 'f', 1));

	Real pilotPowDb =  CalcDb::dbPower(m_bfmDemod->getPilotLevel());
	QString pilotPowDbStr;
	pilotPowDbStr.sprintf("%+02.1f", pilotPowDb);
	ui->pilotPower->setText(pilotPowDbStr);

	if (m_bfmDemod->getPilotLock())
	{
		if (ui->audioStereo->isChecked())
		{
			ui->audioStereo->setStyleSheet("QToolButton { background-color : green; }");
		}
	}
	else
	{
		if (ui->audioStereo->isChecked())
		{
			ui->audioStereo->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
		}
	}

	if (ui->rds->isChecked() && (m_rdsTimerCount == 0))
	{
		rdsUpdate(false);
	}

	m_rdsTimerCount = (m_rdsTimerCount + 1) % 25;

	//qDebug() << "Pilot lock: " << m_bfmDemod->getPilotLock() << ":" << m_bfmDemod->getPilotLevel(); TODO: update a GUI item with status
}

void BFMDemodGUI::channelSampleRateChanged()
{
	m_rate = m_bfmDemod->getSampleRate();
	ui->glSpectrum->setCenterFrequency(m_rate / 4);
	ui->glSpectrum->setSampleRate(m_rate / 2);
}

void BFMDemodGUI::rdsUpdateFixedFields()
{
	ui->g00Label->setText(m_rdsParser.rds_group_acronym_tags[0].c_str());
	ui->g01Label->setText(m_rdsParser.rds_group_acronym_tags[1].c_str());
	ui->g02Label->setText(m_rdsParser.rds_group_acronym_tags[2].c_str());
	ui->g03Label->setText(m_rdsParser.rds_group_acronym_tags[3].c_str());
	ui->g04Label->setText(m_rdsParser.rds_group_acronym_tags[4].c_str());
	//ui->g05Label->setText(m_rdsParser.rds_group_acronym_tags[5].c_str());
	//ui->g06Label->setText(m_rdsParser.rds_group_acronym_tags[6].c_str());
	//ui->g07Label->setText(m_rdsParser.rds_group_acronym_tags[7].c_str());
	ui->g08Label->setText(m_rdsParser.rds_group_acronym_tags[8].c_str());
	ui->g09Label->setText(m_rdsParser.rds_group_acronym_tags[9].c_str());
	ui->g14Label->setText(m_rdsParser.rds_group_acronym_tags[14].c_str());

	ui->g00CountLabel->setText(m_rdsParser.rds_group_acronym_tags[0].c_str());
	ui->g01CountLabel->setText(m_rdsParser.rds_group_acronym_tags[1].c_str());
	ui->g02CountLabel->setText(m_rdsParser.rds_group_acronym_tags[2].c_str());
	ui->g03CountLabel->setText(m_rdsParser.rds_group_acronym_tags[3].c_str());
	ui->g04CountLabel->setText(m_rdsParser.rds_group_acronym_tags[4].c_str());
	ui->g05CountLabel->setText(m_rdsParser.rds_group_acronym_tags[5].c_str());
	ui->g06CountLabel->setText(m_rdsParser.rds_group_acronym_tags[6].c_str());
	ui->g07CountLabel->setText(m_rdsParser.rds_group_acronym_tags[7].c_str());
	ui->g08CountLabel->setText(m_rdsParser.rds_group_acronym_tags[8].c_str());
	ui->g09CountLabel->setText(m_rdsParser.rds_group_acronym_tags[9].c_str());
	ui->g14CountLabel->setText(m_rdsParser.rds_group_acronym_tags[14].c_str());
}

void BFMDemodGUI::rdsUpdate(bool force)
{
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
	if (m_rdsParser.m_pi_updated || force)
	{
		ui->piLabel->setStyleSheet("QLabel { background-color : green; }");
		ui->piCountText->setNum((int) m_rdsParser.m_pi_count);
		QString pistring(str(boost::format("%04X") % m_rdsParser.m_pi_program_identification).c_str());
		ui->piText->setText(pistring);

		if (m_rdsParser.m_pi_traffic_program) {
			ui->piTPIndicator->setStyleSheet("QLabel { background-color : green; }");
		} else {
			ui->piTPIndicator->setStyleSheet("QLabel { background:rgb(79,79,79); }");
		}

		ui->piType->setText(QString(m_rdsParser.pty_table[m_rdsParser.m_pi_program_type].c_str()));
		ui->piCoverage->setText(QString(m_rdsParser.coverage_area_codes[m_rdsParser.m_pi_area_coverage_index].c_str()));
	}
	else
	{
		ui->piLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
	}

	// G0 group
	if (m_rdsParser.m_g0_updated || force)
	{
		ui->g00Label->setStyleSheet("QLabel { background-color : green; }");
		ui->g00CountText->setNum((int) m_rdsParser.m_g0_count);

		if (m_rdsParser.m_g0_psn_bitmap == 0b1111) {
			ui->g00ProgServiceName->setText(QString(m_rdsParser.m_g0_program_service_name));
		}

		if (m_rdsParser.m_g0_traffic_announcement) {
			ui->g00TrafficAnnouncement->setStyleSheet("QLabel { background-color : green; }");
		} else {
			ui->g00TrafficAnnouncement->setStyleSheet("QLabel { background:rgb(79,79,79); }");
		}

		ui->g00MusicSpeech->setText(QString((m_rdsParser.m_g0_music_speech ? "Music" : "Speech")));
		ui->g00MonoStereo->setText(QString((m_rdsParser.m_g0_mono_stereo ? "Mono" : "Stereo")));

		if (m_rdsParser.m_g0_af_updated)
		{
			ui->g00AltFrequenciesBox->clear();

			for (std::set<double>::iterator it = m_rdsParser.m_g0_alt_freq.begin(); it != m_rdsParser.m_g0_alt_freq.end(); ++it)
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
	if (m_rdsParser.m_g1_updated || force)
	{
		ui->g01Label->setStyleSheet("QLabel { background-color : green; }");
		ui->g01CountText->setNum((int) m_rdsParser.m_g1_count);

		if ((m_rdsParser.m_g1_country_page_index >= 0) && (m_rdsParser.m_g1_country_index >= 0)) {
			ui->g01CountryCode->setText(QString((m_rdsParser.pi_country_codes[m_rdsParser.m_g1_country_page_index][m_rdsParser.m_g1_country_index]).c_str()));
		}

		if (m_rdsParser.m_g1_language_index >= 0) {
			ui->g01Language->setText(QString(m_rdsParser.language_codes[m_rdsParser.m_g1_language_index].c_str()));
		}

		ui->g01DHM->setText(QString(str(boost::format("%id:%i:%i") % m_rdsParser.m_g1_pin_day % m_rdsParser.m_g1_pin_hour % m_rdsParser.m_g1_pin_minute).c_str()));
	}
	else
	{
		ui->g01Label->setStyleSheet("QLabel { background:rgb(79,79,79); }");
	}

	// G2 group
	if (m_rdsParser.m_g2_updated || force)
	{
		ui->g02Label->setStyleSheet("QLabel { background-color : green; }");
		ui->g02CountText->setNum((int) m_rdsParser.m_g2_count);
		ui->go2Text->setText(QString(m_rdsParser.m_g2_radiotext));
	}
	else
	{
		ui->g02Label->setStyleSheet("QLabel { background:rgb(79,79,79); }");
	}

	// G3 group
	if (m_rdsParser.m_g3_updated || force)
	{
		ui->g03Label->setStyleSheet("QLabel { background-color : green; }");
		ui->g03CountText->setNum((int) m_rdsParser.m_g3_count);
		std::string g3str = str(boost::format("%02X%c %04X %04X") % m_rdsParser.m_g3_appGroup % (m_rdsParser.m_g3_groupB ? 'B' : 'A') % m_rdsParser.m_g3_message % m_rdsParser.m_g3_aid);
		ui->g03Data->setText(QString(g3str.c_str()));
	}
	else
	{
		ui->g03Label->setStyleSheet("QLabel { background:rgb(79,79,79); }");
	}

	// G4 group
	if (m_rdsParser.m_g4_updated || force)
	{
		ui->g04Label->setStyleSheet("QLabel { background-color : green; }");
		ui->g04CountText->setNum((int) m_rdsParser.m_g4_count);
		std::string time = str(boost::format("%02i.%02i.%4i, %02i:%02i (%+.1fh)")\
			% m_rdsParser.m_g4_day % m_rdsParser.m_g4_month % (1900 + m_rdsParser.m_g4_year) % m_rdsParser.m_g4_hours % m_rdsParser.m_g4_minutes % m_rdsParser.m_g4_local_time_offset);
	    ui->g04Time->setText(QString(time.c_str()));
	}
	else
	{
		ui->g04Label->setStyleSheet("QLabel { background:rgb(79,79,79); }");
	}

	// G5 group
	if (m_rdsParser.m_g5_updated || force)
	{
		ui->g05CountText->setNum((int) m_rdsParser.m_g5_count);
	}

	// G6 group
	if (m_rdsParser.m_g6_updated || force)
	{
		ui->g06CountText->setNum((int) m_rdsParser.m_g6_count);
	}

	// G7 group
	if (m_rdsParser.m_g7_updated || force)
	{
		ui->g07CountText->setNum((int) m_rdsParser.m_g7_count);
	}

	// G8 group
	if (m_rdsParser.m_g8_updated || force)
	{
		ui->g08Label->setStyleSheet("QLabel { background-color : green; }");
		ui->g08CountText->setNum((int) m_rdsParser.m_g8_count);

		std::ostringstream os;
		os << (m_rdsParser.m_g8_sign ? "-" : "+") << m_rdsParser.m_g8_extent + 1;
		ui->g08Extent->setText(QString(os.str().c_str()));
		int event_line = RDSTMC::get_tmc_event_code_index(m_rdsParser.m_g8_event, 1);
		ui->g08TMCEvent->setText(QString(RDSTMC::get_tmc_events(event_line, 1).c_str()));
		QString pistring(str(boost::format("%04X") % m_rdsParser.m_g8_location).c_str());
		ui->g08Location->setText(pistring);

		if (m_rdsParser.m_g8_label_index >= 0) {
			ui->g08Description->setText(QString(m_rdsParser.label_descriptions[m_rdsParser.m_g8_label_index].c_str()));
		}

		ui->g08Content->setNum(m_rdsParser.m_g8_content);
	}
	else
	{
		ui->g08Label->setStyleSheet("QLabel { background:rgb(79,79,79); }");
	}

	// G9 group
	if (m_rdsParser.m_g9_updated || force)
	{
		ui->g09Label->setStyleSheet("QLabel { background-color : green; }");
		ui->g09CountText->setNum((int) m_rdsParser.m_g9_count);
		std::string g9str = str(boost::format("%02X %04X %04X %02X %04X") % m_rdsParser.m_g9_varA % m_rdsParser.m_g9_cA % m_rdsParser.m_g9_dA % m_rdsParser.m_g9_varB % m_rdsParser.m_g9_dB);
		ui->g09Data->setText(QString(g9str.c_str()));
	}
	else
	{
		ui->g09Label->setStyleSheet("QLabel { background:rgb(79,79,79); }");
	}

	// G14 group
	if (m_rdsParser.m_g14_updated || force)
	{
		ui->g14CountText->setNum((int) m_rdsParser.m_g14_count);

		if (m_rdsParser.m_g14_data_available)
		{
			ui->g14Label->setStyleSheet("QLabel { background-color : green; }");
			m_g14ComboIndex.clear();
			ui->g14ProgServiceNames->clear();

			RDSParser::psns_map_t::iterator it = m_rdsParser.m_g14_program_service_names.begin();
			const RDSParser::psns_map_t::iterator itEnd = m_rdsParser.m_g14_program_service_names.end();
			int i = 0;

			for (it; it != itEnd; ++it, i++)
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

	m_rdsParser.clearUpdateFlags();
}

void BFMDemodGUI::changeFrequency(qint64 f)
{
	qint64 df = m_channelMarker.getCenterFrequency();
	qDebug() << "BFMDemodGUI::changeFrequency: " << f - df;
	// TODO: in the future it should be able to set the center frequency of the sample source this channel plugin is linked to
}
