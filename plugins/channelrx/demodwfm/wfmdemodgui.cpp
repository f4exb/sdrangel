#include "wfmdemodgui.h"

#include <device/devicesourceapi.h>
#include <dsp/downchannelizer.h>
#include <QDockWidget>
#include <QMainWindow>
#include <QDebug>

#include "dsp/threadedbasebandsamplesink.h"
#include "ui_wfmdemodgui.h"
#include "dsp/dspengine.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingswidget.h"
#include "mainwindow.h"

#include "wfmdemod.h"

const QString WFMDemodGUI::m_channelID = "de.maintech.sdrangelove.channel.wfm";

const int WFMDemodGUI::m_rfBW[] = {
        12500, 25000, 40000, 60000, 75000, 80000, 100000, 125000, 140000, 160000, 180000, 200000, 220000, 250000
};
const int WFMDemodGUI::m_nbRfBW = 14;

WFMDemodGUI* WFMDemodGUI::create(PluginAPI* pluginAPI, DeviceSourceAPI *deviceAPI)
{
	WFMDemodGUI* gui = new WFMDemodGUI(pluginAPI, deviceAPI);
	return gui;
}

void WFMDemodGUI::destroy()
{
	delete this;
}

void WFMDemodGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString WFMDemodGUI::getName() const
{
	return objectName();
}

qint64 WFMDemodGUI::getCenterFrequency() const
{
	return m_channelMarker.getCenterFrequency();
}

void WFMDemodGUI::setCenterFrequency(qint64 centerFrequency)
{
	m_channelMarker.setCenterFrequency(centerFrequency);
	applySettings();
}

void WFMDemodGUI::resetToDefaults()
{
	blockApplySettings(true);

	ui->rfBW->setCurrentIndex(6);
	ui->afBW->setValue(3);
	ui->volume->setValue(20);
	ui->squelch->setValue(-40);
	ui->deltaFrequency->setValue(0);

	blockApplySettings(false);
	applySettings();
}

QByteArray WFMDemodGUI::serialize() const
{
	SimpleSerializer s(1);
	s.writeS32(1, m_channelMarker.getCenterFrequency());
	s.writeS32(2, ui->rfBW->currentIndex());
	s.writeS32(3, ui->afBW->value());
	s.writeS32(4, ui->volume->value());
	s.writeS32(5, ui->squelch->value());
	s.writeU32(7, m_channelMarker.getColor().rgb());
	return s.final();
}

bool WFMDemodGUI::deserialize(const QByteArray& data)
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

		d.readS32(2, &tmp, 6);
		ui->rfBW->setCurrentIndex(tmp);
		m_channelMarker.setBandwidth(m_rfBW[tmp]);

		d.readS32(3, &tmp, 3);
		ui->afBW->setValue(tmp);

		d.readS32(4, &tmp, 20);
		ui->volume->setValue(tmp);

		d.readS32(5, &tmp, -40);
		ui->squelch->setValue(tmp);

		if(d.readU32(7, &u32tmp))
		{
			m_channelMarker.setColor(u32tmp);
		}

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

bool WFMDemodGUI::handleMessage(const Message& message)
{
	return false;
}

void WFMDemodGUI::viewChanged()
{
	applySettings();
}

void WFMDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
}

void WFMDemodGUI::on_rfBW_currentIndexChanged(int index)
{
    m_channelMarker.setBandwidth(m_rfBW[index]);
	applySettings();
}

void WFMDemodGUI::on_afBW_valueChanged(int value)
{
	ui->afBWText->setText(QString("%1 kHz").arg(value));
	applySettings();
}

void WFMDemodGUI::on_volume_valueChanged(int value)
{
	ui->volumeText->setText(QString("%1").arg(value / 10.0, 0, 'f', 1));
	applySettings();
}

void WFMDemodGUI::on_squelch_valueChanged(int value)
{
	ui->squelchText->setText(QString("%1 dB").arg(value));
	applySettings();
}

void WFMDemodGUI::on_audioMute_toggled(bool checked)
{
    m_audioMute = checked;
    applySettings();
}

void WFMDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
}

void WFMDemodGUI::onMenuDoubleClicked()
{
	if(!m_basicSettingsShown)
	{
		m_basicSettingsShown = true;
		BasicChannelSettingsWidget* bcsw = new BasicChannelSettingsWidget(&m_channelMarker, this);
		bcsw->show();
	}
}

WFMDemodGUI::WFMDemodGUI(PluginAPI* pluginAPI, DeviceSourceAPI *deviceAPI, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::WFMDemodGUI),
	m_pluginAPI(pluginAPI),
	m_deviceAPI(deviceAPI),
	m_channelMarker(this),
	m_basicSettingsShown(false),
	m_channelPowerDbAvg(20,0)
{
	ui->setupUi(this);
    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

	setAttribute(Qt::WA_DeleteOnClose, true);

    blockApplySettings(true);
    ui->rfBW->clear();
    for (int i = 0; i < m_nbRfBW; i++) {
        ui->rfBW->addItem(QString("%1").arg(m_rfBW[i] / 1000.0, 0, 'f', 2));
    }
    ui->rfBW->setCurrentIndex(6);
    blockApplySettings(false);

	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(menuDoubleClickEvent()), this, SLOT(onMenuDoubleClicked()));

	m_wfmDemod = new WFMDemod(0);
	m_channelizer = new DownChannelizer(m_wfmDemod);
	m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
	m_deviceAPI->addThreadedSink(m_threadedChannelizer);

	connect(&m_pluginAPI->getMainWindow()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

	//m_channelMarker = new ChannelMarker(this);
	m_channelMarker.setColor(Qt::blue);
	m_channelMarker.setBandwidth(m_rfBW[4]);
	m_channelMarker.setCenterFrequency(0);
	m_channelMarker.setVisible(true);

	connect(&m_channelMarker, SIGNAL(changed()), this, SLOT(viewChanged()));

	m_deviceAPI->registerChannelInstance(m_channelID, this);
	m_deviceAPI->addChannelMarker(&m_channelMarker);
	m_deviceAPI->addRollupWidget(this);

	applySettings();
}

WFMDemodGUI::~WFMDemodGUI()
{
    m_deviceAPI->removeChannelInstance(this);
	m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
	delete m_threadedChannelizer;
	delete m_channelizer;
	delete m_wfmDemod;
	//delete m_channelMarker;
	delete ui;
}

void WFMDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void WFMDemodGUI::applySettings()
{
	if (m_doApplySettings)
	{
		setTitleColor(m_channelMarker.getColor());

		m_channelizer->configure(m_channelizer->getInputMessageQueue(),
			requiredBW(m_rfBW[ui->rfBW->currentIndex()]), // TODO: this is where requested sample rate is specified
			m_channelMarker.getCenterFrequency());

		ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());

		m_wfmDemod->configure(m_wfmDemod->getInputMessageQueue(),
		    m_rfBW[ui->rfBW->currentIndex()],
			ui->afBW->value() * 1000.0,
			ui->volume->value() / 10.0,
			ui->squelch->value(),
			ui->audioMute->isChecked());
	}
}

void WFMDemodGUI::leaveEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(false);
	blockApplySettings(false);
}

void WFMDemodGUI::enterEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(true);
	blockApplySettings(false);
}

void WFMDemodGUI::tick()
{
//	Real powDb = CalcDb::dbPower(m_wfmDemod->getMagSq());
//	m_channelPowerDbAvg.feed(powDb);
//	ui->channelPower->setText(QString::number(m_channelPowerDbAvg.average(), 'f', 1));

    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_wfmDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);

    ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));
    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    bool squelchOpen = m_wfmDemod->getSquelchOpen();

    if (squelchOpen != m_squelchOpen)
    {
        if (squelchOpen) {
            ui->audioMute->setStyleSheet("QToolButton { background-color : green; }");
        } else {
            ui->audioMute->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        }

        m_squelchOpen = squelchOpen;
    }
}
