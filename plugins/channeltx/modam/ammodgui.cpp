///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include <QDockWidget>
#include <QMainWindow>

#include "ammodgui.h"

#include "device/devicesinkapi.h"
#include "dsp/upchannelizer.h"

#include "dsp/threadedbasebandsamplesource.h"
#include "ui_ammodgui.h"
#include "plugin/pluginapi.h"
#include "util/simpleserializer.h"
#include "util/db.h"
#include "gui/basicchannelsettingswidget.h"
#include "dsp/dspengine.h"
#include "mainwindow.h"

#include "ammod.h"

const QString AMModGUI::m_channelID = "sdrangel.channel.modam";

const int AMModGUI::m_rfBW[] = {
	5000, 6250, 8330, 10000, 12500, 15000, 20000, 25000, 40000
};

AMModGUI* AMModGUI::create(PluginAPI* pluginAPI, DeviceSinkAPI *deviceAPI)
{
	AMModGUI* gui = new AMModGUI(pluginAPI, deviceAPI);
	return gui;
}

void AMModGUI::destroy()
{
}

void AMModGUI::setName(const QString& name)
{
	setObjectName(name);
}

QString AMModGUI::getName() const
{
	return objectName();
}

qint64 AMModGUI::getCenterFrequency() const {
	return m_channelMarker.getCenterFrequency();
}

void AMModGUI::setCenterFrequency(qint64 centerFrequency)
{
	m_channelMarker.setCenterFrequency(centerFrequency);
	applySettings();
}

void AMModGUI::resetToDefaults()
{
	blockApplySettings(true);

	ui->rfBW->setValue(4);
	ui->afBW->setValue(3);
	ui->modPercent->setValue(20);
	ui->deltaFrequency->setValue(0);

	blockApplySettings(false);
	applySettings();
}

QByteArray AMModGUI::serialize() const
{
	SimpleSerializer s(1);
	s.writeS32(1, m_channelMarker.getCenterFrequency());
	s.writeS32(2, ui->rfBW->value());
	s.writeS32(3, ui->afBW->value());
	s.writeS32(4, ui->modPercent->value());
	s.writeU32(5, m_channelMarker.getColor().rgb());
	return s.final();
}

bool AMModGUI::deserialize(const QByteArray& data)
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
		qint32 tmp;

		blockApplySettings(true);
		m_channelMarker.blockSignals(true);

		d.readS32(1, &tmp, 0);
		m_channelMarker.setCenterFrequency(tmp);
		d.readS32(2, &tmp, 4);
		ui->rfBW->setValue(tmp);
		d.readS32(3, &tmp, 3);
		ui->afBW->setValue(tmp);
		d.readS32(4, &tmp, 20);
		ui->modPercent->setValue(tmp);

        if(d.readU32(5, &u32tmp))
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

bool AMModGUI::handleMessage(const Message& message)
{
	return false;
}

void AMModGUI::viewChanged()
{
	applySettings();
}

void AMModGUI::on_deltaMinus_toggled(bool minus)
{
	int deltaFrequency = m_channelMarker.getCenterFrequency();
	bool minusDelta = (deltaFrequency < 0);

	if (minus ^ minusDelta) // sign change
	{
		m_channelMarker.setCenterFrequency(-deltaFrequency);
	}
}

void AMModGUI::on_deltaFrequency_changed(quint64 value)
{
	if (ui->deltaMinus->isChecked()) {
		m_channelMarker.setCenterFrequency(-value);
	} else {
		m_channelMarker.setCenterFrequency(value);
	}
}

void AMModGUI::on_rfBW_valueChanged(int value)
{
	ui->rfBWText->setText(QString("%1 kHz").arg(m_rfBW[value] / 1000.0));
	m_channelMarker.setBandwidth(m_rfBW[value]);
	applySettings();
}

void AMModGUI::on_afBW_valueChanged(int value)
{
	ui->afBWText->setText(QString("%1 kHz").arg(value));
	applySettings();
}

void AMModGUI::on_modPercent_valueChanged(int value)
{
	ui->modPercentText->setText(QString("%1").arg(value));
	applySettings();
}

void AMModGUI::on_audioMute_toggled(bool checked)
{
	applySettings();
}

void AMModGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
}

void AMModGUI::onMenuDoubleClicked()
{
	if(!m_basicSettingsShown) {
		m_basicSettingsShown = true;
		BasicChannelSettingsWidget* bcsw = new BasicChannelSettingsWidget(&m_channelMarker, this);
		bcsw->show();
	}
}

AMModGUI::AMModGUI(PluginAPI* pluginAPI, DeviceSinkAPI *deviceAPI, QWidget* parent) :
	RollupWidget(parent),
	ui(new Ui::AMModGUI),
	m_pluginAPI(pluginAPI),
	m_deviceAPI(deviceAPI),
	m_channelMarker(this),
	m_basicSettingsShown(false),
	m_doApplySettings(true),
	m_channelPowerDbAvg(20,0)
{
	ui->setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);
	connect(this, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
	connect(this, SIGNAL(menuDoubleClickEvent()), this, SLOT(onMenuDoubleClicked()));

	m_amMod = new AMMod();
	m_channelizer = new UpChannelizer(m_amMod);
	m_threadedChannelizer = new ThreadedBasebandSampleSource(m_channelizer, this);
	//m_pluginAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addThreadedSource(m_threadedChannelizer);

	connect(&m_pluginAPI->getMainWindow()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));

	ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));

	//m_channelMarker = new ChannelMarker(this);
	m_channelMarker.setColor(Qt::yellow);
	m_channelMarker.setBandwidth(12500);
	m_channelMarker.setCenterFrequency(0);
	m_channelMarker.setVisible(true);

	connect(&m_channelMarker, SIGNAL(changed()), this, SLOT(viewChanged()));

	m_deviceAPI->registerChannelInstance(m_channelID, this);
    m_deviceAPI->addChannelMarker(&m_channelMarker);
    m_deviceAPI->addRollupWidget(this);

	applySettings();
}

AMModGUI::~AMModGUI()
{
    m_deviceAPI->removeChannelInstance(this);
	m_deviceAPI->removeThreadedSource(m_threadedChannelizer);
	delete m_threadedChannelizer;
	delete m_channelizer;
	delete m_amMod;
	//delete m_channelMarker;
	delete ui;
}

void AMModGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void AMModGUI::applySettings()
{
	if (m_doApplySettings)
	{
		setTitleColor(m_channelMarker.getColor());

		m_channelizer->configure(m_channelizer->getInputMessageQueue(),
			48000,
			m_channelMarker.getCenterFrequency());

		ui->deltaFrequency->setValue(abs(m_channelMarker.getCenterFrequency()));
		ui->deltaMinus->setChecked(m_channelMarker.getCenterFrequency() < 0);

		m_amMod->configure(m_amMod->getInputMessageQueue(),
			m_rfBW[ui->rfBW->value()],
			ui->afBW->value() * 1000.0,
			ui->modPercent->value() / 100.0f,
			ui->audioMute->isChecked());
	}
}

void AMModGUI::leaveEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(false);
	blockApplySettings(false);
}

void AMModGUI::enterEvent(QEvent*)
{
	blockApplySettings(true);
	m_channelMarker.setHighlighted(true);
	blockApplySettings(false);
}

void AMModGUI::tick()
{
	Real powDb = CalcDb::dbPower(m_amMod->getMagSq());
	m_channelPowerDbAvg.feed(powDb);
	ui->channelPower->setText(QString::number(m_channelPowerDbAvg.average(), 'f', 1));
}

