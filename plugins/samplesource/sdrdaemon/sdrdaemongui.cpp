///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#include <QDebug>

#include <QTime>
#include <QDateTime>
#include <QString>
#include <QFileDialog>
#include <stdint.h>
#include "ui_sdrdaemongui.h"
#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "dsp/dspengine.h"
#include "mainwindow.h"
#include "util/simpleserializer.h"

#include "sdrdaemongui.h"

SDRdaemonGui::SDRdaemonGui(PluginAPI* pluginAPI, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::SDRdaemonGui),
	m_pluginAPI(pluginAPI),
	m_sampleSource(NULL),
	m_acquisition(false),
	m_sampleRate(0),
	m_centerFrequency(0),
	m_samplesCount(0),
	m_tickCount(0),
	m_address("127.0.0.1"),
	m_port(9090),
	m_dcBlock(false),
	m_iqCorrection(false)
{
	m_startingTimeStamp.tv_sec = 0;
	m_startingTimeStamp.tv_usec = 0;
	ui->setupUi(this);
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));
	ui->centerFrequency->setValueRange(7, 0, pow(10,7));
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	connect(&(m_pluginAPI->getMainWindow()->getMasterTimer()), SIGNAL(timeout()), this, SLOT(tick()));

	m_sampleSource = new SDRdaemonInput(m_pluginAPI->getMainWindow()->getMasterTimer());
	connect(m_sampleSource->getOutputMessageQueueToGUI(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
	DSPEngine::instance()->setSource(m_sampleSource);

	displaySettings();
}

SDRdaemonGui::~SDRdaemonGui()
{
	delete ui;
}

void SDRdaemonGui::destroy()
{
	delete this;
}

void SDRdaemonGui::setName(const QString& name)
{
	setObjectName(name);
}

QString SDRdaemonGui::getName() const
{
	return objectName();
}

void SDRdaemonGui::resetToDefaults()
{
	m_address = "127.0.0.1";
	m_port = 9090;
	m_dcBlock = false;
	m_iqCorrection = false;
	displaySettings();
}

QByteArray SDRdaemonGui::serialize() const
{
	bool ok;
	SimpleSerializer s(1);

	s.writeString(1, ui->address->text());
	uint32_t uintval = ui->port->text().toInt(&ok);

	if((!ok) || (uintval < 1024) || (uintval > 65535)) {
		uintval = 9090;
	}

	s.writeU32(2, uintval);
	s.writeBool(3, m_dcBlock);
	s.writeBool(4, m_iqCorrection);

	return s.final();
}

bool SDRdaemonGui::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);
	QString address;
	uint32_t uintval;
	quint16 port;

	if(!d.isValid()) {
		resetToDefaults();
		return false;
	}

	if(d.getVersion() == 1) {
		uint32_t uintval;
		d.readString(1, &m_address, "127.0.0.1");
		d.readU32(2, &uintval, 9090);

		if ((uintval > 1024) && (uintval < 65536)) {
			m_port = uintval;
		} else {
			m_port = 9090;
		}

		d.readBool(3, &m_dcBlock, false);
		d.readBool(4, &m_iqCorrection, false);

		return true;
	} else {
		resetToDefaults();
		return false;
	}

	displaySettings();
}

qint64 SDRdaemonGui::getCenterFrequency() const
{
	return m_centerFrequency;
}

void SDRdaemonGui::setCenterFrequency(qint64 centerFrequency)
{
	m_centerFrequency = centerFrequency;
	displaySettings();
}

bool SDRdaemonGui::handleMessage(const Message& message)
{
	if (SDRdaemonInput::MsgReportSDRdaemonAcquisition::match(message))
	{
		m_acquisition = ((SDRdaemonInput::MsgReportSDRdaemonAcquisition&)message).getAcquisition();
		updateWithAcquisition();
		return true;
	}
	else if (SDRdaemonInput::MsgReportSDRdaemonStreamData::match(message))
	{
		m_sampleRate = ((SDRdaemonInput::MsgReportSDRdaemonStreamData&)message).getSampleRate();
		m_centerFrequency = ((SDRdaemonInput::MsgReportSDRdaemonStreamData&)message).getCenterFrequency();
		m_startingTimeStamp.tv_sec = ((SDRdaemonInput::MsgReportSDRdaemonStreamData&)message).get_tv_sec();
		m_startingTimeStamp.tv_usec = ((SDRdaemonInput::MsgReportSDRdaemonStreamData&)message).get_tv_usec();
		updateWithStreamData();
		return true;
	}
	else if (SDRdaemonInput::MsgReportSDRdaemonStreamTiming::match(message))
	{
		m_startingTimeStamp.tv_sec = ((SDRdaemonInput::MsgReportSDRdaemonStreamTiming&)message).get_tv_sec();
		m_startingTimeStamp.tv_usec = ((SDRdaemonInput::MsgReportSDRdaemonStreamTiming&)message).get_tv_usec();
		updateWithStreamTime();
		return true;
	}
	else
	{
		return false;
	}
}

void SDRdaemonGui::handleSourceMessages()
{
	Message* message;

	while ((message = m_sampleSource->getOutputMessageQueueToGUI()->pop()) != 0)
	{
		//qDebug("SDRdaemonGui::handleSourceMessages: message: %s", message->getIdentifier());

		if (handleMessage(*message))
		{
			delete message;
		}
	}
}

void SDRdaemonGui::displaySettings()
{
	ui->address->setText(m_address);
	ui->port->setText(QString::number(m_port));
	ui->dcOffset->setChecked(m_dcBlock);
	ui->iqImbalance->setChecked(m_iqCorrection);
}

void SDRdaemonGui::on_applyButton_clicked(bool checked)
{
	configureUDPLink();
}

void SDRdaemonGui::on_dcOffset_toggled(bool checked)
{
	if (m_dcBlock != checked)
	{
		m_dcBlock = checked;
		configureAutoCorrections();
	}
}

void SDRdaemonGui::on_iqImbalance_toggled(bool checked)
{
	if (m_iqCorrection != checked)
	{
		m_iqCorrection = checked;
		configureAutoCorrections();
	}
}

void SDRdaemonGui::configureUDPLink()
{
	bool ok;
	QString udpAddress = ui->address->text();
	int udpPort = ui->port->text().toInt(&ok);

	if((!ok) || (udpPort < 1024) || (udpPort > 65535))
	{
		udpPort = 9090;
	}

	qDebug() << "SDRdaemonGui::configureUDPLink: " << udpAddress.toStdString().c_str()
			<< " : " << udpPort;

	SDRdaemonInput::MsgConfigureSDRdaemonUDPLink* message = SDRdaemonInput::MsgConfigureSDRdaemonUDPLink::create(udpAddress, udpPort);
	m_sampleSource->getInputMessageQueue()->push(message);
}

void SDRdaemonGui::configureAutoCorrections()
{
	SDRdaemonInput::MsgConfigureSDRdaemonAutoCorr* message = SDRdaemonInput::MsgConfigureSDRdaemonAutoCorr::create(m_dcBlock, m_iqCorrection);
	m_sampleSource->getInputMessageQueue()->push(message);
}

void SDRdaemonGui::updateWithAcquisition()
{
}

void SDRdaemonGui::updateWithStreamData()
{
	ui->centerFrequency->setValue(m_centerFrequency);
	QString s = QString::number(m_sampleRate/1000.0, 'f', 0);
	ui->sampleRateText->setText(tr("%1k").arg(s));
	updateWithStreamTime(); // TODO: remove when time data is implemented
}

void SDRdaemonGui::updateWithStreamTime()
{
	quint64 startingTimeStampMsec = (m_startingTimeStamp.tv_sec * 1000) + (m_startingTimeStamp.tv_usec / 1000);
	QDateTime dt = QDateTime::fromMSecsSinceEpoch(startingTimeStampMsec);
	QString s_date = dt.toString("yyyy-MM-dd  hh:mm:ss.zzz");
	ui->absTimeText->setText(s_date);
}

void SDRdaemonGui::tick()
{
	if ((++m_tickCount & 0xf) == 0) {
		SDRdaemonInput::MsgConfigureSDRdaemonStreamTiming* message = SDRdaemonInput::MsgConfigureSDRdaemonStreamTiming::create();
		m_sampleSource->getInputMessageQueue()->push(message);
	}
}

