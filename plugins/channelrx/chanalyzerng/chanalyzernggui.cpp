///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include "chanalyzernggui.h"

#include <device/devicesourceapi.h>
#include <dsp/downchannelizer.h>
#include <QDockWidget>
#include <QMainWindow>

#include "dsp/threadedbasebandsamplesink.h"
#include "ui_chanalyzernggui.h"
#include "dsp/spectrumscopengcombovis.h"
#include "dsp/spectrumvis.h"
#include "dsp/scopevis.h"
#include "gui/glspectrum.h"
#include "gui/glscopeng.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingswidget.h"
#include "dsp/dspengine.h"
#include "mainwindow.h"

#include "chanalyzerng.h"

const QString ChannelAnalyzerNGGUI::m_channelID = "sdrangel.channel.chanalyzerng";

ChannelAnalyzerNGGUI* ChannelAnalyzerNGGUI::create(PluginAPI* pluginAPI, DeviceSourceAPI *deviceAPI)
{
    ChannelAnalyzerNGGUI* gui = new ChannelAnalyzerNGGUI(pluginAPI, deviceAPI);
	return gui;
}

void ChannelAnalyzerNGGUI::destroy()
{
	delete this;
}

void ChannelAnalyzerNGGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString ChannelAnalyzerNGGUI::getName() const
{
	return objectName();
}

qint64 ChannelAnalyzerNGGUI::getCenterFrequency() const
{
	return m_channelMarker.getCenterFrequency();
}

void ChannelAnalyzerNGGUI::setCenterFrequency(qint64 centerFrequency)
{
	m_channelMarker.setCenterFrequency(centerFrequency);
	applySettings();
}

void ChannelAnalyzerNGGUI::resetToDefaults()
{
	blockApplySettings(true);

	ui->useRationalDownsampler->setChecked(false);
	ui->BW->setValue(30);
	ui->deltaFrequency->setValue(0);
	ui->spanLog2->setCurrentIndex(3);

	blockApplySettings(false);
	applySettings();
}

QByteArray ChannelAnalyzerNGGUI::serialize() const
{
	SimpleSerializer s(1);
	s.writeS32(1, m_channelMarker.getCenterFrequency());
	s.writeS32(2, ui->BW->value());
	s.writeBlob(3, ui->spectrumGUI->serialize());
	s.writeU32(4, m_channelMarker.getColor().rgb());
	s.writeS32(5, ui->lowCut->value());
	s.writeS32(6, ui->spanLog2->currentIndex());
	s.writeBool(7, ui->ssb->isChecked());
	s.writeBlob(8, ui->scopeGUI->serialize());
	s.writeU64(9, ui->channelSampleRate->getValueNew());
	return s.final();
}

bool ChannelAnalyzerNGGUI::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);

	if(!d.isValid())
    {
		resetToDefaults();
		return false;
	}

	if(d.getVersion() == 1)
    {
		QByteArray bytetmp;
		quint32 u32tmp;
		quint64 u64tmp;
		qint32 tmp, spanLog2, bw, lowCut;
		bool tmpBool;

		blockApplySettings(true);
	    m_channelMarker.blockSignals(true);

		d.readS32(1, &tmp, 0);
		m_channelMarker.setCenterFrequency(tmp);
		d.readS32(2, &bw, 30);
		d.readBlob(3, &bytetmp);
		ui->spectrumGUI->deserialize(bytetmp);

		if(d.readU32(4, &u32tmp))
		{
			m_channelMarker.setColor(u32tmp);
		}

		d.readS32(5, &lowCut, 3);
		d.readS32(6, &spanLog2, 3);
		d.readBool(7, &tmpBool, false);
		ui->ssb->setChecked(tmpBool);
		d.readBlob(8, &bytetmp);
		ui->scopeGUI->deserialize(bytetmp);
		d.readU64(9, &u64tmp, 2000U);
		ui->channelSampleRate->setValue(u64tmp);

		blockApplySettings(false);
	    m_channelMarker.blockSignals(false);

        ui->spanLog2->setCurrentIndex(spanLog2);
        setNewFinalRate(spanLog2);
		ui->BW->setValue(bw);
		ui->lowCut->setValue(lowCut); // does applySettings();

		return true;
	}
    else
    {
		resetToDefaults();
		return false;
	}
}

bool ChannelAnalyzerNGGUI::handleMessage(const Message& message __attribute__((unused)))
{
	return false;
}

void ChannelAnalyzerNGGUI::viewChanged()
{
	applySettings();
}

void ChannelAnalyzerNGGUI::tick()
{
	double powDb = CalcDb::dbPower(m_channelAnalyzer->getMagSq());
	m_channelPowerDbAvg.feed(powDb);
	ui->channelPower->setText(tr("%1 dB").arg(m_channelPowerDbAvg.average(), 0, 'f', 1));
}

void ChannelAnalyzerNGGUI::channelizerInputSampleRateChanged()
{
    //ui->channelSampleRate->setValueRange(7, 2000U, m_channelAnalyzer->getInputSampleRate());
	setNewFinalRate(m_spanLog2);
	applySettings();
}

void ChannelAnalyzerNGGUI::on_channelSampleRate_changed(quint64 value)
{
    ui->channelSampleRate->setValueRange(7, 2000U, m_channelAnalyzer->getInputSampleRate());

    if (ui->useRationalDownsampler->isChecked())
    {
        qDebug("ChannelAnalyzerNGGUI::on_channelSampleRate_changed: %llu", value);
        setNewFinalRate(m_spanLog2);
        applySettings();
    }
}

void ChannelAnalyzerNGGUI::on_useRationalDownsampler_toggled(bool checked __attribute__((unused)))
{
    setNewFinalRate(m_spanLog2);
    applySettings();
}

int ChannelAnalyzerNGGUI::getRequestedChannelSampleRate()
{
    if (ui->useRationalDownsampler->isChecked()) {
        return ui->channelSampleRate->getValueNew();
    } else {
        return m_channelizer->getInputSampleRate();
    }
}

void ChannelAnalyzerNGGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
}

void ChannelAnalyzerNGGUI::on_BW_valueChanged(int value)
{
	m_channelMarker.setBandwidth(value * 100 * 2);

	if (ui->ssb->isChecked())
	{
		QString s = QString::number(value/10.0, 'f', 1);
	    ui->BWText->setText(tr("%1k").arg(s));
	}
	else
	{
	    QString s = QString::number(value/5.0, 'f', 1); // BW = value * 2
	    ui->BWText->setText(tr("%1k").arg(s));
	}

	displayBandwidth();
	on_lowCut_valueChanged(m_channelMarker.getLowCutoff()/100);
}

int ChannelAnalyzerNGGUI::getEffectiveLowCutoff(int lowCutoff)
{
	int ssbBW = m_channelMarker.getBandwidth() / 2;
	int effectiveLowCutoff = lowCutoff;
	const int guard = 100;

	if (ssbBW < 0) {
		if (effectiveLowCutoff < ssbBW + guard) {
			effectiveLowCutoff = ssbBW + guard;
		}
		if (effectiveLowCutoff > 0)	{
			effectiveLowCutoff = 0;
		}
	} else {
		if (effectiveLowCutoff > ssbBW - guard)	{
			effectiveLowCutoff = ssbBW - guard;
		}
		if (effectiveLowCutoff < 0)	{
			effectiveLowCutoff = 0;
		}
	}

	return effectiveLowCutoff;
}

void ChannelAnalyzerNGGUI::on_lowCut_valueChanged(int value)
{
	int lowCutoff = getEffectiveLowCutoff(value * 100);
	m_channelMarker.setLowCutoff(lowCutoff);
	QString s = QString::number(lowCutoff/1000.0, 'f', 1);
	ui->lowCutText->setText(tr("%1k").arg(s));
	ui->lowCut->setValue(lowCutoff/100);
	applySettings();
}

void ChannelAnalyzerNGGUI::on_spanLog2_currentIndexChanged(int index)
{
	if (setNewFinalRate(index)) {
		applySettings();
	}

}

void ChannelAnalyzerNGGUI::on_ssb_toggled(bool checked)
{
    //int bw = m_channelMarker.getBandwidth();

	if (checked)
	{
	    setFiltersUIBoundaries();

	    ui->BWLabel->setText("LP");
        QString s = QString::number(ui->BW->value()/10.0, 'f', 1); // bw/2
        ui->BWText->setText(tr("%1k").arg(s));

		on_lowCut_valueChanged(m_channelMarker.getLowCutoff()/100);
	}
	else
	{
        if (ui->BW->value() < 0) {
            ui->BW->setValue(-ui->BW->value());
        }

        setFiltersUIBoundaries();
        //m_channelMarker.setBandwidth(ui->BW->value() * 200.0);

        ui->BWLabel->setText("BP");
        QString s = QString::number(ui->BW->value()/5.0, 'f', 1); // bw
        ui->BWText->setText(tr("%1k").arg(s));

        ui->lowCut->setEnabled(false);
        ui->lowCut->setValue(0);
        ui->lowCutText->setText("0.0k");
	}

    applySettings();
    displayBandwidth();
}

void ChannelAnalyzerNGGUI::onWidgetRolled(QWidget* widget __attribute__((unused)), bool rollDown __attribute__((unused)))
{
	/*
	if((widget == ui->spectrumContainer) && (m_ssbDemod != NULL))
		m_ssbDemod->setSpectrum(m_threadedSampleSink->getMessageQueue(), rollDown);
	*/
}

void ChannelAnalyzerNGGUI::onMenuDoubleClicked()
{
	if(!m_basicSettingsShown) {
		m_basicSettingsShown = true;
		BasicChannelSettingsWidget* bcsw = new BasicChannelSettingsWidget(&m_channelMarker, this);
		bcsw->show();
	}
}

ChannelAnalyzerNGGUI::ChannelAnalyzerNGGUI(PluginAPI* pluginAPI, DeviceSourceAPI *deviceAPI, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::ChannelAnalyzerNGGUI),
	m_pluginAPI(pluginAPI),
	m_deviceAPI(deviceAPI),
	m_channelMarker(this),
	m_basicSettingsShown(false),
	m_doApplySettings(true),
	m_rate(6000),
	m_spanLog2(0),
	m_channelPowerDbAvg(40,0)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(menuDoubleClickEvent()), this, SLOT(onMenuDoubleClicked()));

	m_spectrumVis = new SpectrumVis(ui->glSpectrum);
	m_scopeVis = new ScopeVisNG(ui->glScope);
	m_spectrumScopeComboVis = new SpectrumScopeNGComboVis(m_spectrumVis, m_scopeVis);
	m_channelAnalyzer = new ChannelAnalyzerNG(m_spectrumScopeComboVis);
	m_channelizer = new DownChannelizer(m_channelAnalyzer);
	m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
	connect(m_channelizer, SIGNAL(inputSampleRateChanged()), this, SLOT(channelizerInputSampleRateChanged()));
	m_deviceAPI->addThreadedSink(m_threadedChannelizer);

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);

	ui->channelSampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
	ui->channelSampleRate->setValueRange(7, 2000U, 9999999U);

	ui->glSpectrum->setCenterFrequency(m_rate/2);
	ui->glSpectrum->setSampleRate(m_rate);
	ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);
	ui->glSpectrum->setSsbSpectrum(false);
    ui->glSpectrum->setLsbDisplay(false);
	ui->BWLabel->setText("BP");

	ui->glSpectrum->connectTimer(m_pluginAPI->getMainWindow()->getMasterTimer());
	ui->glScope->connectTimer(m_pluginAPI->getMainWindow()->getMasterTimer());
	connect(&m_pluginAPI->getMainWindow()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

	//m_channelMarker = new ChannelMarker(this);
	m_channelMarker.setColor(Qt::gray);
	m_channelMarker.setBandwidth(m_rate);
	m_channelMarker.setSidebands(ChannelMarker::usb);
	m_channelMarker.setCenterFrequency(0);
	m_channelMarker.setVisible(true);

	connect(&m_channelMarker, SIGNAL(changed()), this, SLOT(viewChanged()));

	m_deviceAPI->registerChannelInstance(m_channelID, this);
	m_deviceAPI->addChannelMarker(&m_channelMarker);
	m_deviceAPI->addRollupWidget(this);

	ui->spectrumGUI->setBuddies(m_spectrumVis->getInputMessageQueue(), m_spectrumVis, ui->glSpectrum);
	ui->scopeGUI->setBuddies(m_scopeVis->getInputMessageQueue(), m_scopeVis, ui->glScope);

	applySettings();
	setNewFinalRate(m_spanLog2);
}

ChannelAnalyzerNGGUI::~ChannelAnalyzerNGGUI()
{
    m_deviceAPI->removeChannelInstance(this);
	m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
	delete m_threadedChannelizer;
	delete m_channelizer;
	delete m_channelAnalyzer;
	delete m_spectrumVis;
	delete m_scopeVis;
	delete m_spectrumScopeComboVis;
	//delete m_channelMarker;
	delete ui;
}

bool ChannelAnalyzerNGGUI::setNewFinalRate(int spanLog2)
{
	qDebug("ChannelAnalyzerNGGUI::setNewRate");

	if ((spanLog2 < 0) || (spanLog2 > 6)) {
		return false;
	}

	m_spanLog2 = spanLog2;
	//m_rate = 48000 / (1<<spanLog2);
	//m_rate = m_channelizer->getInputSampleRate() / (1<<spanLog2);
    m_rate = getRequestedChannelSampleRate() / (1<<spanLog2);

    if (m_rate == 0) {
        m_rate = 6000;
    }

	setFiltersUIBoundaries();

	QString s = QString::number(m_rate/1000.0, 'f', 1);
	ui->spanText->setText(tr("%1 kS/s").arg(s));

	displayBandwidth();

	ui->glScope->setSampleRate(m_rate);
	m_scopeVis->setSampleRate(m_rate);

	return true;
}

void ChannelAnalyzerNGGUI::displayBandwidth()
{
    if (ui->ssb->isChecked())
    {
        if (ui->BW->value() < 0)
        {
            m_channelMarker.setSidebands(ChannelMarker::lsb);
            ui->glSpectrum->setLsbDisplay(true);
        }
        else
        {
            m_channelMarker.setSidebands(ChannelMarker::usb);
            ui->glSpectrum->setLsbDisplay(false);
        }

        ui->glSpectrum->setCenterFrequency(m_rate/4);
        ui->glSpectrum->setSampleRate(m_rate/2);
        ui->glSpectrum->setSsbSpectrum(true);
    }
    else
    {
        m_channelMarker.setSidebands(ChannelMarker::dsb);

        ui->glSpectrum->setCenterFrequency(0);
        ui->glSpectrum->setSampleRate(m_rate);
        ui->glSpectrum->setLsbDisplay(false);
        ui->glSpectrum->setSsbSpectrum(false);
    }


}

void ChannelAnalyzerNGGUI::setFiltersUIBoundaries()
{
    if (ui->BW->value() < -m_rate/200) {
        ui->BW->setValue(-m_rate/200);
        m_channelMarker.setBandwidth(-m_rate*2);
    } else if (ui->BW->value() > m_rate/200) {
        ui->BW->setValue(m_rate/200);
        m_channelMarker.setBandwidth(m_rate*2);
    }

    if (ui->lowCut->value() < -m_rate/200) {
        ui->lowCut->setValue(-m_rate/200);
        m_channelMarker.setLowCutoff(-m_rate);
    } else if (ui->lowCut->value() > m_rate/200) {
        ui->lowCut->setValue(m_rate/200);
        m_channelMarker.setLowCutoff(m_rate);
    }

    if (ui->ssb->isChecked()) {
        ui->BW->setMinimum(-m_rate/200);
        ui->lowCut->setMinimum(-m_rate/200);
    } else {
        ui->BW->setMinimum(0);
        ui->lowCut->setMinimum(-m_rate/200);
        ui->lowCut->setValue(0);
    }

    ui->BW->setMaximum(m_rate/200);
    ui->lowCut->setMaximum(m_rate/200);
}

void ChannelAnalyzerNGGUI::blockApplySettings(bool block)
{
    ui->glScope->blockSignals(block);
    ui->glSpectrum->blockSignals(block);
    m_doApplySettings = !block;
}

void ChannelAnalyzerNGGUI::applySettings()
{
	if (m_doApplySettings)
	{
		setTitleColor(m_channelMarker.getColor());
		ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

		m_channelizer->configure(m_channelizer->getInputMessageQueue(),
			//m_channelizer->getInputSampleRate(),
            getRequestedChannelSampleRate(),
			m_channelMarker.getCenterFrequency());

		m_channelAnalyzer->configure(m_channelAnalyzer->getInputMessageQueue(),
			//m_channelizer->getInputSampleRate(), // TODO: specify required channel sample rate
            getRequestedChannelSampleRate(), // TODO: specify required channel sample rate
			ui->BW->value() * 100.0,
			ui->lowCut->value() * 100.0,
			m_spanLog2,
			ui->ssb->isChecked());
	}
}

void ChannelAnalyzerNGGUI::leaveEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(false);
	blockApplySettings(false);
}

void ChannelAnalyzerNGGUI::enterEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(true);
	blockApplySettings(false);
}

