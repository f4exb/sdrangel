#include "ssbdemodgui.h"
#include "ssbdemodgui.h"

#include <device/devicesourceapi.h>
#include "device/deviceuiset.h"
#include <QDockWidget>
#include <QMainWindow>

#include "ui_ssbdemodgui.h"
#include "dsp/spectrumvis.h"
#include "gui/glspectrum.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingswidget.h"
#include "dsp/dspengine.h"
#include "mainwindow.h"
#include "ssbdemod.h"

const QString SSBDemodGUI::m_channelID = "de.maintech.sdrangelove.channel.ssb";

SSBDemodGUI* SSBDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet)
{
	SSBDemodGUI* gui = new SSBDemodGUI(pluginAPI, deviceUISet);
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
	m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
	applySettings();
}

void SSBDemodGUI::resetToDefaults()
{
	m_settings.resetToDefaults();
}

QByteArray SSBDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool SSBDemodGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        displaySettings();
        applySettings(true); // will have true
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        displaySettings();
        applySettings(true); // will have true
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
	m_settings.m_audioBinaural = binaural;
	applySettings();
}

void SSBDemodGUI::on_audioFlipChannels_toggled(bool flip)
{
	m_audioFlipChannels = flip;
	m_settings.m_audioFlipChannels = flip;
	applySettings();
}

void SSBDemodGUI::on_dsb_toggled(bool dsb)
{
	m_dsb = dsb;
	m_settings.m_dsb = dsb;

	if (m_dsb)
	{
        if (ui->BW->value() < 0) {
            ui->BW->setValue(-ui->BW->value());
        }

        m_channelMarker.setSidebands(ChannelMarker::dsb);

        QString bwStr = QString::number(ui->BW->value()/10.0, 'f', 1);
        ui->BWText->setText(tr("%1%2k").arg(QChar(0xB1, 0x00)).arg(bwStr));
        ui->lowCut->setValue(0);
        ui->lowCut->setEnabled(false);

        applySettings();
	}
	else
	{
        if (ui->BW->value() < 0) {
            m_channelMarker.setSidebands(ChannelMarker::lsb);
        } else {
            m_channelMarker.setSidebands(ChannelMarker::usb);
        }

        QString bwStr = QString::number(ui->BW->value()/10.0, 'f', 1);
        ui->BWText->setText(tr("%1k").arg(bwStr));
        ui->lowCut->setEnabled(true);

        on_lowCut_valueChanged(m_channelMarker.getLowCutoff()/100);
	}

	setNewRate(m_spanLog2);
}

void SSBDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings();
}

void SSBDemodGUI::on_BW_valueChanged(int value)
{
	QString s = QString::number(value/10.0, 'f', 1);
	m_channelMarker.setBandwidth(value * 100 * 2);

	if (m_dsb)
	{
        ui->BWText->setText(tr("%1%2k").arg(QChar(0xB1, 0x00)).arg(s));
	}
	else
	{
        ui->BWText->setText(tr("%1k").arg(s));
	}

	m_settings.m_rfBandwidth = value * 100;
	on_lowCut_valueChanged(m_channelMarker.getLowCutoff()/100);
	setNewRate(m_spanLog2);
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
	m_settings.m_lowCutoff = lowCutoff;
	applySettings();
}

void SSBDemodGUI::on_volume_valueChanged(int value)
{
	ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
	m_settings.m_volume = value / 10.0;
	applySettings();
}

void SSBDemodGUI::on_agc_toggled(bool checked)
{
    m_settings.m_agc = checked;
    applySettings();
}

void SSBDemodGUI::on_agcClamping_toggled(bool checked)
{
    m_settings.m_agcClamping = checked;
    applySettings();
}

void SSBDemodGUI::on_agcTimeLog2_valueChanged(int value)
{
    QString s = QString::number((1<<value), 'f', 0);
    ui->agcTimeText->setText(s);
    m_settings.m_agcTimeLog2 = value;
    applySettings();
}

void SSBDemodGUI::on_agcPowerThreshold_valueChanged(int value)
{
    displayAGCPowerThreshold(value);
    m_settings.m_agcPowerThreshold = value;
    applySettings();
}

void SSBDemodGUI::on_agcThresholdGate_valueChanged(int value)
{
    QString s = QString::number(value, 'f', 0);
    ui->agcThresholdGateText->setText(s);
    m_settings.m_agcThresholdGate = value;
    applySettings();
}

void SSBDemodGUI::on_audioMute_toggled(bool checked)
{
	m_audioMute = checked;
	m_settings.m_audioMute = checked;
	applySettings();
}

void SSBDemodGUI::on_spanLog2_valueChanged(int value)
{
	if (setNewRate(value))
	{
	    m_settings.m_spanLog2 = value;
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

SSBDemodGUI::SSBDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::SSBDemodGUI),
	m_pluginAPI(pluginAPI),
	m_deviceUISet(deviceUISet),
	m_channelMarker(this),
	m_basicSettingsShown(false),
	m_doApplySettings(true),
	m_rate(6000),
	m_spanLog2(3),
	m_audioBinaural(false),
	m_audioFlipChannels(false),
	m_dsb(false),
    m_audioMute(false),
	m_squelchOpen(false)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(menuDoubleClickEvent()), this, SLOT(onMenuDoubleClicked()));

	m_spectrumVis = new SpectrumVis(ui->glSpectrum);
	m_ssbDemod = new SSBDemod(m_deviceUISet->m_deviceSourceAPI);
	m_ssbDemod->setMessageQueueToGUI(getInputMessageQueue());
	m_ssbDemod->setSampleSink(m_spectrumVis);

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
	ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

	ui->glSpectrum->setDisplayWaterfall(true);
	ui->glSpectrum->setDisplayMaxHold(true);
	ui->glSpectrum->connectTimer(MainWindow::getInstance()->getMasterTimer());

	connect(&MainWindow::getInstance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

	m_channelMarker.setVisible(true);

    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setSpectrumGUI(ui->spectrumGUI);

	connect(&m_channelMarker, SIGNAL(changed()), this, SLOT(viewChanged()));

	m_deviceUISet->registerChannelInstance(m_channelID, this);
	m_deviceUISet->addChannelMarker(&m_channelMarker);
	m_deviceUISet->addRollupWidget(this);

	ui->spectrumGUI->setBuddies(m_spectrumVis->getInputMessageQueue(), m_spectrumVis, ui->glSpectrum);

	displaySettings();
	applySettings(true);
	setNewRate(m_spanLog2);
}

SSBDemodGUI::~SSBDemodGUI()
{
    m_deviceUISet->removeChannelInstance(this);
	delete m_ssbDemod;
	delete m_spectrumVis;
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

	QString s = QString::number(m_rate/1000.0, 'f', 1);

	if (m_dsb)
	{
        ui->BW->setMinimum(0);
        ui->BW->setMaximum(m_rate/100);
        ui->lowCut->setMinimum(0);
        ui->lowCut->setMaximum(m_rate/100);

        m_channelMarker.setSidebands(ChannelMarker::dsb);

        ui->spanText->setText(tr("%1%2k").arg(QChar(0xB1, 0x00)).arg(s));
        ui->glSpectrum->setCenterFrequency(0);
        ui->glSpectrum->setSampleRate(2*m_rate);
        ui->glSpectrum->setSsbSpectrum(false);
        ui->glSpectrum->setLsbDisplay(false);
	}
	else
	{
        ui->BW->setMinimum(-m_rate/100);
        ui->BW->setMaximum(m_rate/100);
        ui->lowCut->setMinimum(-m_rate/100);
        ui->lowCut->setMaximum(m_rate/100);

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

        ui->spanText->setText(tr("%1k").arg(s));
        ui->glSpectrum->setCenterFrequency(m_rate/2);
        ui->glSpectrum->setSampleRate(m_rate);
        ui->glSpectrum->setSsbSpectrum(true);
	}

	return true;
}

void SSBDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void SSBDemodGUI::applySettings(bool force)
{
	if (m_doApplySettings)
	{
		setTitleColor(m_channelMarker.getColor());
		ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

        SSBDemod::MsgConfigureChannelizer* channelConfigMsg = SSBDemod::MsgConfigureChannelizer::create(
                48000, m_channelMarker.getCenterFrequency());
        m_ssbDemod->getInputMessageQueue()->push(channelConfigMsg);

        SSBDemod::MsgConfigureSSBDemod* message = SSBDemod::MsgConfigureSSBDemod::create( m_settings, force);
        m_ssbDemod->getInputMessageQueue()->push(message);
	}
}

void SSBDemodGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth * 2);
    m_channelMarker.setLowCutoff(m_settings.m_lowCutoff);
    m_channelMarker.setColor(m_settings.m_rgbColor);
    setTitleColor(m_settings.m_rgbColor);

    if (m_settings.m_dsb) {
        m_channelMarker.setSidebands(ChannelMarker::dsb);
    } else {
        if (m_settings.m_rfBandwidth < 0) {
            m_channelMarker.setSidebands(ChannelMarker::lsb);
        } else {
            m_channelMarker.setSidebands(ChannelMarker::usb);
        }
    }

    m_channelMarker.blockSignals(false);

    setWindowTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->agc->setChecked(m_settings.m_agc);
    ui->agcClamping->setChecked(m_settings.m_agcClamping);
    ui->audioBinaural->setChecked(m_settings.m_audioBinaural);
    ui->audioFlipChannels->setChecked(m_settings.m_audioFlipChannels);
    ui->audioMute->setChecked(m_settings.m_audioMute);
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    ui->dsb->setChecked(m_settings.m_dsb);
    ui->spanLog2->setValue(m_settings.m_spanLog2);

    ui->BW->setValue(m_settings.m_rfBandwidth / 100.0);
    QString s = QString::number(m_settings.m_rfBandwidth/1000.0, 'f', 1);

    if (m_settings.m_dsb)
    {
        ui->BWText->setText(tr("%1%2k").arg(QChar(0xB1, 0x00)).arg(s));
    }
    else
    {
        ui->BWText->setText(tr("%1k").arg(s));
    }

    ui->lowCut->setValue(m_settings.m_lowCutoff / 100.0);
    ui->lowCutText->setText(tr("%1k").arg(m_settings.m_lowCutoff / 1000.0));

    ui->volume->setValue(m_settings.m_volume * 10.0);
    ui->volumeText->setText(QString("%1").arg(m_settings.m_volume, 0, 'f', 1));

    ui->agcTimeLog2->setValue(m_settings.m_agcTimeLog2);
    s = QString::number((1<<ui->agcTimeLog2->value()), 'f', 0);
    ui->agcTimeText->setText(s);

    ui->agcPowerThreshold->setValue(m_settings.m_agcPowerThreshold);
    displayAGCPowerThreshold(ui->agcPowerThreshold->value());

    ui->agcThresholdGate->setValue(m_settings.m_agcThresholdGate);
    s = QString::number(ui->agcThresholdGate->value(), 'f', 0);
    ui->agcThresholdGateText->setText(s);

    blockApplySettings(false);
}

void SSBDemodGUI::displayUDPAddress()
{
    //TODO: ui->copyAudioToUDP->setToolTip(QString("Copy audio output to UDP %1:%2").arg(m_channelMarker.getUDPAddress()).arg(m_channelMarker.getUDPSendPort()));
}

void SSBDemodGUI::displayAGCPowerThreshold(int value)
{
    if (value == -99)
    {
        ui->agcPowerThresholdText->setText("---");
    }
    else
    {
        QString s = QString::number(value, 'f', 0);
        ui->agcPowerThresholdText->setText(s);
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

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(tr("%1 dB").arg(powDbAvg, 0, 'f', 1));
    }

    bool squelchOpen = m_ssbDemod->getAudioActive();

    if (squelchOpen != m_squelchOpen)
    {
        if (squelchOpen) {
            ui->audioMute->setStyleSheet("QToolButton { background-color : green; }");
        } else {
            ui->audioMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        }

        m_squelchOpen = squelchOpen;
    }

    m_tickCount++;
}
