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
	m_sampleRateStream(0),
	m_centerFrequency(0),
	m_syncLocked(false),
	m_frameSize(0),
	m_lz4(false),
	m_compressionRatio(1.0),
	m_nbLz4DataCRCOK(0),
	m_nbLz4SuccessfulDecodes(0),
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
	ui->applyButton->setEnabled(false);
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
	bool dcBlock;
	bool iqCorrection;

	if (!d.isValid())
	{
		resetToDefaults();
		displaySettings();
		return false;
	}

	if (d.getVersion() == 1)
	{
		uint32_t uintval;
		d.readString(1, &address, "127.0.0.1");
		d.readU32(2, &uintval, 9090);

		if ((uintval > 1024) && (uintval < 65536)) {
			port = uintval;
		} else {
			port = 9090;
		}

		d.readBool(3, &dcBlock, false);
		d.readBool(4, &iqCorrection, false);

		if ((address != m_address) || (port != m_port))
		{
			m_address = address;
			m_port = port;
			configureUDPLink();
		}

		if ((dcBlock != m_dcBlock) || (iqCorrection != m_iqCorrection))
		{
			m_dcBlock = dcBlock;
			m_iqCorrection = iqCorrection;
			configureAutoCorrections();
		}

		displaySettings();
		return true;
	}
	else
	{
		resetToDefaults();
		displaySettings();
		return false;
	}
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
		m_sampleRateStream = ((SDRdaemonInput::MsgReportSDRdaemonStreamData&)message).getSampleRateStream();
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
		m_syncLocked = ((SDRdaemonInput::MsgReportSDRdaemonStreamTiming&)message).getSyncLock();
		m_frameSize = ((SDRdaemonInput::MsgReportSDRdaemonStreamTiming&)message).getFrameSize();
		m_lz4 = ((SDRdaemonInput::MsgReportSDRdaemonStreamTiming&)message).getLz4Compression();

		if (m_lz4) {
			m_compressionRatio = ((SDRdaemonInput::MsgReportSDRdaemonStreamTiming&)message).getLz4CompressionRatio();
		} else {
			m_compressionRatio = 1.0;
		}

		m_nbLz4DataCRCOK = ((SDRdaemonInput::MsgReportSDRdaemonStreamTiming&)message).getLz4DataCRCOK();
		m_nbLz4SuccessfulDecodes = ((SDRdaemonInput::MsgReportSDRdaemonStreamTiming&)message).getLz4SuccessfulDecodes();

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
	bool ok;
	QString udpAddress = ui->address->text();
	int udpPort = ui->port->text().toInt(&ok);

	if((!ok) || (udpPort < 1024) || (udpPort > 65535))
	{
		udpPort = 9090;
	}

	m_address = udpAddress;
	m_port = udpPort;

	configureUDPLink();

	ui->applyButton->setEnabled(false);
}

void SDRdaemonGui::on_address_textEdited(const QString& arg1)
{
	ui->applyButton->setEnabled(true);
}

void SDRdaemonGui::on_port_textEdited(const QString& arg1)
{
	ui->applyButton->setEnabled(true);
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
	qDebug() << "SDRdaemonGui::configureUDPLink: " << m_address.toStdString().c_str()
			<< " : " << m_port;

	SDRdaemonInput::MsgConfigureSDRdaemonUDPLink* message = SDRdaemonInput::MsgConfigureSDRdaemonUDPLink::create(m_address, m_port);
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
	ui->centerFrequency->setValue(m_centerFrequency / 1000);
	QString s0 = QString::number(m_sampleRateStream/1000.0, 'f', 2);
	ui->sampleRateStreamText->setText(tr("%1").arg(s0));
	QString s1 = QString::number(m_sampleRate/1000.0, 'f', 3);
	ui->sampleRateText->setText(tr("%1").arg(s1));
	float skewPerCent = (float) ((m_sampleRate - m_sampleRateStream) * 100) / (float) m_sampleRateStream;
	QString s2 = QString::number(skewPerCent, 'f', 2);
	ui->skewRateText->setText(tr("%1").arg(s2));
	updateWithStreamTime();
}

void SDRdaemonGui::updateWithStreamTime()
{
    quint64 startingTimeStampMsec = ((quint64) m_startingTimeStamp.tv_sec * 1000LL) + ((quint64) m_startingTimeStamp.tv_usec / 1000LL);
    QDateTime dt = QDateTime::fromMSecsSinceEpoch(startingTimeStampMsec);
    QString s_date = dt.toString("yyyy-MM-dd  hh:mm:ss.zzz");
	ui->absTimeText->setText(s_date);

	if (m_syncLocked) {
		ui->streamLocked->setStyleSheet("QToolButton { background-color : green; }");
	} else {
		ui->streamLocked->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
	}

	QString s = QString::number(m_frameSize / 1024.0, 'f', 0);
	ui->frameSizeText->setText(tr("%1").arg(s));

	if (m_lz4) {
		ui->lz4Compressed->setStyleSheet("QToolButton { background-color : green; }");
	} else {
		ui->lz4Compressed->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
	}

	s = QString::number(m_compressionRatio, 'f', 2);
	ui->compressionRatioText->setText(tr("%1").arg(s));

	s = QString::number(m_nbLz4DataCRCOK, 'f', 0);
	ui->dataCRCOKText->setText(tr("%1").arg(s));

	s = QString::number(m_nbLz4SuccessfulDecodes, 'f', 0);
	ui->lz4DecodesOKText->setText(tr("%1").arg(s));
}

void SDRdaemonGui::tick()
{
	if ((++m_tickCount & 0xf) == 0) {
		SDRdaemonInput::MsgConfigureSDRdaemonStreamTiming* message = SDRdaemonInput::MsgConfigureSDRdaemonStreamTiming::create();
		m_sampleSource->getInputMessageQueue()->push(message);
	}
}

