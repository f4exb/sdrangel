#include "../../channelrx/demodssb/ssbdemodgui.h"
#include "../../channelrx/demodssb/ssbdemodgui.h"

#include <device/devicesourceapi.h>
#include <dsp/downchannelizer.h>
#include <QDockWidget>
#include <QMainWindow>

#include "../../../sdrbase/dsp/threadedbasebandsamplesink.h"
#include "ui_ssbdemodgui.h"
#include "ui_ssbdemodgui.h"
#include "dsp/spectrumvis.h"
#include "gui/glspectrum.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingswidget.h"
#include "dsp/dspengine.h"
#include "mainwindow.h"
#include "../../channelrx/demodssb/ssbdemod.h"

const QString SSBDemodGUI::m_channelID = "de.maintech.sdrangelove.channel.ssb";

SSBDemodGUI* SSBDemodGUI::create(PluginAPI* pluginAPI, DeviceSourceAPI *deviceAPI)
{
	SSBDemodGUI* gui = new SSBDemodGUI(pluginAPI, deviceAPI);
	return gui;
}

void SSBDemodGUI::destroy()
{
	delete this;
}

void SSBDemodGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString SSBDemodGUI::getName() const
{
	return objectName();
}

qint64 SSBDemodGUI::getCenterFrequency() const
{
	return m_channelMarker.getCenterFrequency();
}

void SSBDemodGUI::setCenterFrequency(qint64 centerFrequency)
{
	m_channelMarker.setCenterFrequency(centerFrequency);
	applySettings();
}

void SSBDemodGUI::resetToDefaults()
{
	blockApplySettings(true);

	ui->BW->setValue(30);
	ui->volume->setValue(40);
	ui->deltaFrequency->setValue(0);
	ui->spanLog2->setValue(3);

	blockApplySettings(false);
	applySettings();
}

QByteArray SSBDemodGUI::serialize() const
{
	SimpleSerializer s(1);
	s.writeS32(1, m_channelMarker.getCenterFrequency());
	s.writeS32(2, ui->BW->value());
	s.writeS32(3, ui->volume->value());
	s.writeBlob(4, ui->spectrumGUI->serialize());
	s.writeU32(5, m_channelMarker.getColor().rgb());
	s.writeS32(6, ui->lowCut->value());
	s.writeS32(7, ui->spanLog2->value());
	s.writeBool(8, m_audioBinaural);
	s.writeBool(9, m_audioFlipChannels);
	s.writeBool(10, m_dsb);
	return s.final();
}

bool SSBDemodGUI::deserialize(const QByteArray& data)
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

		blockApplySettings(true);
	    m_channelMarker.blockSignals(true);

		d.readS32(1, &tmp, 0);
		m_channelMarker.setCenterFrequency(tmp);
		d.readS32(2, &tmp, 30);
		ui->BW->setValue(tmp);
		d.readS32(3, &tmp, 20);
		ui->volume->setValue(tmp);
		d.readBlob(4, &bytetmp);
		ui->spectrumGUI->deserialize(bytetmp);
		if(d.readU32(5, &u32tmp))
			m_channelMarker.setColor(u32tmp);
		d.readS32(6, &tmp, 3);
		ui->lowCut->setValue(tmp);
		d.readS32(7, &tmp, 20);
		ui->spanLog2->setValue(tmp);
		setNewRate(tmp);
		d.readBool(8, &m_audioBinaural);
		ui->audioBinaural->setChecked(m_audioBinaural);
		d.readBool(9, &m_audioFlipChannels);
		ui->audioFlipChannels->setChecked(m_audioFlipChannels);
		d.readBool(10, &m_dsb);
		ui->dsb->setChecked(m_dsb);

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

bool SSBDemodGUI::handleMessage(const Message& message __attribute__((unused)))
{
	return false;
}

void SSBDemodGUI::viewChanged()
{
	applySettings();
}

void SSBDemodGUI::on_audioBinaural_toggled(bool binaural)
{
	m_audioBinaural = binaural;
	applySettings();
}

void SSBDemodGUI::on_audioFlipChannels_toggled(bool flip)
{
	m_audioFlipChannels = flip;
	applySettings();
}

void SSBDemodGUI::on_dsb_toggled(bool dsb)
{
	m_dsb = dsb;

	if (!m_dsb)
	{
		if (ui->BW->value() < 0) {
			m_channelMarker.setSidebands(ChannelMarker::lsb);
		} else {
			m_channelMarker.setSidebands(ChannelMarker::usb);
		}

		ui->glSpectrum->setCenterFrequency(m_rate/4);
		ui->glSpectrum->setSampleRate(m_rate/2);
		ui->glSpectrum->setSsbSpectrum(true);

		on_lowCut_valueChanged(m_channelMarker.getLowCutoff()/100);
	}
	else
	{
		m_channelMarker.setSidebands(ChannelMarker::dsb);

		ui->glSpectrum->setCenterFrequency(0);
		ui->glSpectrum->setSampleRate(m_rate);
		ui->glSpectrum->setSsbSpectrum(false);
	}

	applySettings();
	setNewRate(m_spanLog2);
}

void SSBDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
}

void SSBDemodGUI::on_BW_valueChanged(int value)
{
	QString s = QString::number(value/10.0, 'f', 1);
	ui->BWText->setText(tr("%1k").arg(s));
	m_channelMarker.setBandwidth(value * 100 * 2);

	if (!m_dsb)
	{
		if (value < 0)
		{
			m_channelMarker.setSidebands(ChannelMarker::lsb);
		}
		else
		{
			m_channelMarker.setSidebands(ChannelMarker::usb);
		}
	}
	else
	{
		m_channelMarker.setSidebands(ChannelMarker::dsb);
	}


	on_lowCut_valueChanged(m_channelMarker.getLowCutoff()/100);
}

int SSBDemodGUI::getEffectiveLowCutoff(int lowCutoff)
{
	int ssbBW = m_channelMarker.getBandwidth() / 2;
	int effectiveLowCutoff = lowCutoff;
	const int guard = 100;

	if (ssbBW < 0)
	{
		if (effectiveLowCutoff < ssbBW + guard)
		{
			effectiveLowCutoff = ssbBW + guard;
		}
		if (effectiveLowCutoff > 0)
		{
			effectiveLowCutoff = 0;
		}
	}
	else
	{
		if (effectiveLowCutoff > ssbBW - guard)
		{
			effectiveLowCutoff = ssbBW - guard;
		}
		if (effectiveLowCutoff < 0)
		{
			effectiveLowCutoff = 0;
		}
	}

	return effectiveLowCutoff;
}

void SSBDemodGUI::on_lowCut_valueChanged(int value)
{
	int lowCutoff = getEffectiveLowCutoff(value * 100);
	m_channelMarker.setLowCutoff(lowCutoff);
	QString s = QString::number(lowCutoff/1000.0, 'f', 1);
	ui->lowCutText->setText(tr("%1k").arg(s));
	ui->lowCut->setValue(lowCutoff/100);
	applySettings();
}

void SSBDemodGUI::on_volume_valueChanged(int value)
{
	ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
	applySettings();
}

void SSBDemodGUI::on_audioMute_toggled(bool checked)
{
	m_audioMute = checked;
	applySettings();
}

void SSBDemodGUI::on_spanLog2_valueChanged(int value)
{
	if (setNewRate(value))
	{
		applySettings();
	}

}

void SSBDemodGUI::onWidgetRolled(QWidget* widget __attribute__((unused)), bool rollDown __attribute__((unused)))
{
	/*
	if((widget == ui->spectrumContainer) && (m_ssbDemod != NULL))
		m_ssbDemod->setSpectrum(m_threadedSampleSink->getMessageQueue(), rollDown);
	*/
}

void SSBDemodGUI::onMenuDoubleClicked()
{
	if(!m_basicSettingsShown)
	{
		m_basicSettingsShown = true;
		BasicChannelSettingsWidget* bcsw = new BasicChannelSettingsWidget(&m_channelMarker, this);
		bcsw->show();
	}
}

SSBDemodGUI::SSBDemodGUI(PluginAPI* pluginAPI, DeviceSourceAPI *deviceAPI, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::SSBDemodGUI),
	m_pluginAPI(pluginAPI),
	m_deviceAPI(deviceAPI),
	m_channelMarker(this),
	m_basicSettingsShown(false),
	m_doApplySettings(true),
	m_rate(6000),
	m_spanLog2(3),
	m_audioBinaural(false),
	m_audioFlipChannels(false),
	m_dsb(false),
    m_audioMute(false),
	m_channelPowerDbAvg(20,0)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(menuDoubleClickEvent()), this, SLOT(onMenuDoubleClicked()));

	m_spectrumVis = new SpectrumVis(ui->glSpectrum);
	m_ssbDemod = new SSBDemod(m_spectrumVis);
	m_channelizer = new DownChannelizer(m_ssbDemod);
	m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
	m_deviceAPI->addThreadedSink(m_threadedChannelizer);

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
	ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

	ui->glSpectrum->setCenterFrequency(m_rate/2);
	ui->glSpectrum->setSampleRate(m_rate);
	ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);
	ui->glSpectrum->setSsbSpectrum(true);
	ui->glSpectrum->connectTimer(m_pluginAPI->getMainWindow()->getMasterTimer());

	connect(&m_pluginAPI->getMainWindow()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

	//m_channelMarker = new ChannelMarker(this);
	m_channelMarker.setColor(Qt::green);
	m_channelMarker.setBandwidth(m_rate);
	m_channelMarker.setSidebands(ChannelMarker::usb);
	m_channelMarker.setCenterFrequency(0);
	m_channelMarker.setVisible(true);

	connect(&m_channelMarker, SIGNAL(changed()), this, SLOT(viewChanged()));

	m_deviceAPI->registerChannelInstance(m_channelID, this);
	m_deviceAPI->addChannelMarker(&m_channelMarker);
	m_deviceAPI->addRollupWidget(this);

	ui->spectrumGUI->setBuddies(m_spectrumVis->getInputMessageQueue(), m_spectrumVis, ui->glSpectrum);

	applySettings();
	setNewRate(m_spanLog2);
}

SSBDemodGUI::~SSBDemodGUI()
{
    m_deviceAPI->removeChannelInstance(this);
	m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
	delete m_threadedChannelizer;
	delete m_channelizer;
	delete m_ssbDemod;
	delete m_spectrumVis;
	//delete m_channelMarker;
	delete ui;
}

bool SSBDemodGUI::setNewRate(int spanLog2)
{
	if ((spanLog2 < 1) || (spanLog2 > 5))
	{
		return false;
	}

	m_spanLog2 = spanLog2;
	m_rate = 48000 / (1<<spanLog2);

	if (ui->BW->value() < -m_rate/100)
	{
		ui->BW->setValue(-m_rate/100);
		m_channelMarker.setBandwidth(-m_rate*2);
	}
	else if (ui->BW->value() > m_rate/100)
	{
		ui->BW->setValue(m_rate/100);
		m_channelMarker.setBandwidth(m_rate*2);
	}

	if (ui->lowCut->value() < -m_rate/100)
	{
		ui->lowCut->setValue(-m_rate/100);
		m_channelMarker.setLowCutoff(-m_rate);
	}
	else if (ui->lowCut->value() > m_rate/100)
	{
		ui->lowCut->setValue(m_rate/100);
		m_channelMarker.setLowCutoff(m_rate);
	}

	ui->BW->setMinimum(-m_rate/100);
	ui->lowCut->setMinimum(-m_rate/100);
	ui->BW->setMaximum(m_rate/100);
	ui->lowCut->setMaximum(m_rate/100);

	QString s = QString::number(m_rate/1000.0, 'f', 1);
	ui->spanText->setText(tr("%1k").arg(s));

	//ui->glSpectrum->setCenterFrequency(m_rate/2);
	//ui->glSpectrum->setSampleRate(m_rate);
	if (!m_dsb)
	{
		if (ui->BW->value() < 0) {
			m_channelMarker.setSidebands(ChannelMarker::lsb);
		} else {
			m_channelMarker.setSidebands(ChannelMarker::usb);
		}

		ui->glSpectrum->setCenterFrequency(m_rate/2);
		ui->glSpectrum->setSampleRate(m_rate);
		ui->glSpectrum->setSsbSpectrum(true);
	}
	else
	{
		m_channelMarker.setSidebands(ChannelMarker::dsb);

		ui->glSpectrum->setCenterFrequency(0);
		ui->glSpectrum->setSampleRate(2*m_rate);
		ui->glSpectrum->setSsbSpectrum(false);
	}

	return true;
}

void SSBDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void SSBDemodGUI::applySettings()
{
	if (m_doApplySettings)
	{
		setTitleColor(m_channelMarker.getColor());
		ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

		m_channelizer->configure(m_channelizer->getInputMessageQueue(),
			48000,
			m_channelMarker.getCenterFrequency());

		m_ssbDemod->configure(m_ssbDemod->getInputMessageQueue(),
			ui->BW->value() * 100.0,
			ui->lowCut->value() * 100.0,
			ui->volume->value() / 10.0,
			m_spanLog2,
			m_audioBinaural,
			m_audioFlipChannels,
			m_dsb,
			ui->audioMute->isChecked());
	}
}

void SSBDemodGUI::leaveEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(false);
	blockApplySettings(false);
}

void SSBDemodGUI::enterEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(true);
	blockApplySettings(false);
}

void SSBDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_ssbDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    ui->channelPower->setText(tr("%1 dB").arg(powDbAvg, 0, 'f', 1));
}
