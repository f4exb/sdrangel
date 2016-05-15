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

#include <stdint.h>
#include <sstream>
#include <iostream>
#include <cassert>

#include <QDebug>
#include <QMessageBox>
#include <QTime>
#include <QDateTime>
#include <QString>
#include <QFileDialog>

#include <nanomsg/nn.h>
#include <nanomsg/pair.h>

#include "ui_sdrdaemongui.h"
#include "plugin/pluginapi.h"
#include "device/deviceapi.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/filesink.h"
#include "mainwindow.h"
#include "util/simpleserializer.h"

#include "sdrdaemongui.h"

SDRdaemonGui::SDRdaemonGui(PluginAPI* pluginAPI, DeviceAPI *deviceAPI, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::SDRdaemonGui),
	m_pluginAPI(pluginAPI),
	m_deviceAPI(deviceAPI),
	m_sampleSource(NULL),
	m_acquisition(false),
	m_lastEngineState((DSPDeviceEngine::State)-1),
	m_sampleRate(0),
	m_sampleRateStream(0),
	m_centerFrequency(0),
	m_syncLocked(false),
	m_frameSize(0),
	m_lz4(false),
	m_compressionRatio(1.0),
	m_nbLz4DataCRCOK(0),
	m_nbLz4SuccessfulDecodes(0),
	m_bufferLengthInSecs(0.0),
    m_bufferGauge(-50),
	m_samplesCount(0),
	m_tickCount(0),
	m_address("127.0.0.1"),
	m_dataPort(9090),
	m_controlPort(9091),
	m_addressEdited(false),
	m_dataPortEdited(false),
	m_initSendConfiguration(false),
	m_dcBlock(false),
	m_iqCorrection(false),
	m_autoFollowRate(false)
{
	m_sender = nn_socket(AF_SP, NN_PAIR);
	assert(m_sender != -1);
	int millis = 500;
    int rc = nn_setsockopt (m_sender, NN_SOL_SOCKET, NN_SNDTIMEO, &millis, sizeof (millis));
    assert (rc == 0);

	m_startingTimeStamp.tv_sec = 0;
	m_startingTimeStamp.tv_usec = 0;
	ui->setupUi(this);
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));
	ui->centerFrequency->setValueRange(7, 0, pow(10,7));

	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);
	connect(&(m_pluginAPI->getMainWindow()->getMasterTimer()), SIGNAL(timeout()), this, SLOT(tick()));

	m_sampleSource = new SDRdaemonInput(m_pluginAPI->getMainWindow()->getMasterTimer(), m_pluginAPI);
	connect(m_sampleSource->getOutputMessageQueueToGUI(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
	m_pluginAPI->setSource(m_sampleSource);

	displaySettings();
	ui->applyButton->setEnabled(false);
	ui->sendButton->setEnabled(false);

    char recFileNameCStr[30];
    sprintf(recFileNameCStr, "test_%d.sdriq", m_pluginAPI->getDeviceUID());
    m_fileSink = new FileSink(std::string(recFileNameCStr));
    m_deviceAPI->addSink(m_fileSink);

    connect(m_pluginAPI->getDeviceOutputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleDSPMessages()), Qt::QueuedConnection);
}

SDRdaemonGui::~SDRdaemonGui()
{
    m_pluginAPI->removeSink(m_fileSink);
    delete m_fileSink;
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
	m_remoteAddress = "127.0.0.1";
	m_dataPort = 9090;
	m_controlPort = 9091;
	m_dcBlock = false;
	m_iqCorrection = false;
	m_autoFollowRate = false;
	displaySettings();
}

QByteArray SDRdaemonGui::serialize() const
{
	bool ok;
	SimpleSerializer s(1);

	s.writeString(1, ui->address->text());
	uint32_t uintval = ui->dataPort->text().toInt(&ok);

	if((!ok) || (uintval < 1024) || (uintval > 65535)) {
		uintval = 9090;
	}

	s.writeU32(2, uintval);
	s.writeBool(3, m_dcBlock);
	s.writeBool(4, m_iqCorrection);
	s.writeBool(5, m_autoFollowRate);
    s.writeBool(6, m_autoCorrBuffer);

	uintval = ui->controlPort->text().toInt(&ok);

	if((!ok) || (uintval < 1024) || (uintval > 65535)) {
		uintval = 9091;
	}

	s.writeU32(7, uintval);

	uint32_t confFrequency = ui->freq->text().toInt(&ok);

	if (ok) {
		s.writeU32(8, confFrequency);
	}

	s.writeU32(9,  ui->decim->currentIndex());
	s.writeU32(10,  ui->fcPos->currentIndex());

	uint32_t sampleRate = ui->sampleRate->text().toInt(&ok);

	if (ok) {
		s.writeU32(11, sampleRate);
	}

	s.writeString(12, ui->specificParms->text());

	return s.final();
}

bool SDRdaemonGui::deserialize(const QByteArray& data)
{
	SimpleDeserializer d(data);
	QString address;
	uint32_t uintval;
	quint16 dataPort;
	bool dcBlock;
	bool iqCorrection;
	bool autoFollowRate;
    bool autoCorrBuffer;
    uint32_t confFrequency;
    uint32_t confSampleRate;
    uint32_t confDecim;
    uint32_t confFcPos;
    QString confSpecificParms;

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
			dataPort = uintval;
		} else {
			dataPort = 9090;
		}

		d.readBool(3, &dcBlock, false);
		d.readBool(4, &iqCorrection, false);
		d.readBool(5, &autoFollowRate, false);
        d.readBool(6, &autoCorrBuffer, false);
		d.readU32(7, &uintval, 9091);

		if ((uintval > 1024) && (uintval < 65536)) {
			m_controlPort = uintval;
		} else {
			m_controlPort = 9091;
		}

		d.readU32(8, &confFrequency, 435000);
		d.readU32(9, &confDecim, 3);
		d.readU32(10, &confFcPos, 2);
		d.readU32(11, &confSampleRate, 1000);
		d.readString(12, &confSpecificParms, "");

		if ((address != m_address) || (dataPort != m_dataPort))
		{
			m_address = address;
			m_dataPort = dataPort;
			configureUDPLink();
		}

		if ((dcBlock != m_dcBlock) || (iqCorrection != m_iqCorrection))
		{
			m_dcBlock = dcBlock;
			m_iqCorrection = iqCorrection;
			configureAutoCorrections();
		}

        if ((m_autoFollowRate != autoFollowRate) || (m_autoCorrBuffer != autoCorrBuffer))
        {
			m_autoFollowRate = autoFollowRate;
            m_autoCorrBuffer = autoCorrBuffer;
            configureAutoFollowPolicy();
		}

		displaySettings();
		displayConfigurationParameters(confFrequency, confDecim, confFcPos, confSampleRate, confSpecificParms);
		m_initSendConfiguration = true;
		return true;
	}
	else
	{
		resetToDefaults();
		displaySettings();
		QString defaultSpecificParameters("");
		displayConfigurationParameters(435000, 3, 2, 1000, defaultSpecificParameters);
		m_initSendConfiguration = true;
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
		m_bufferLengthInSecs = ((SDRdaemonInput::MsgReportSDRdaemonStreamTiming&)message).getBufferLengthInSecs();
        m_bufferGauge = ((SDRdaemonInput::MsgReportSDRdaemonStreamTiming&)message).getBufferGauge();

		updateWithStreamTime();
		return true;
	}
	else
	{
		return false;
	}
}

void SDRdaemonGui::handleDSPMessages()
{
    Message* message;

    while ((message = m_pluginAPI->getDeviceOutputMessageQueue()->pop()) != 0)
    {
        qDebug("SDRdaemonGui::handleDSPMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_deviceSampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("SDRdaemonGui::handleDSPMessages: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();
            m_fileSink->handleMessage(*notif); // forward to file sink

            delete message;
        }
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

void SDRdaemonGui::updateSampleRateAndFrequency()
{
    m_pluginAPI->getSpectrum()->setSampleRate(m_deviceSampleRate);
    m_pluginAPI->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_deviceSampleRate / 1000));
}

void SDRdaemonGui::displaySettings()
{
	ui->address->setText(m_address);
	ui->dataPort->setText(QString::number(m_dataPort));
	ui->controlPort->setText(QString::number(m_controlPort));
	ui->dcOffset->setChecked(m_dcBlock);
	ui->iqImbalance->setChecked(m_iqCorrection);
	ui->autoFollowRate->setChecked(m_autoFollowRate);
    ui->autoCorrBuffer->setChecked(m_autoCorrBuffer);
}

void SDRdaemonGui::displayConfigurationParameters(uint32_t freq,
		uint32_t log2Decim,
		uint32_t fcPos,
		uint32_t sampleRate,
		QString& specParms)
{
	ui->freq->setText(QString::number(freq));
	ui->decim->setCurrentIndex(log2Decim);
	ui->fcPos->setCurrentIndex(fcPos);
	ui->sampleRate->setText(QString::number(sampleRate));
	ui->specificParms->setText(specParms);
	ui->specificParms->setCursorPosition(0);
}

void SDRdaemonGui::on_applyButton_clicked(bool checked)
{
	bool dataOk, ctlOk;
	QString udpAddress = ui->address->text();
	int udpDataPort = ui->dataPort->text().toInt(&dataOk);
	int tcpCtlPort = ui->controlPort->text().toInt(&ctlOk);

	if((!dataOk) || (udpDataPort < 1024) || (udpDataPort > 65535))
	{
		udpDataPort = 9090;
	}

	if((!ctlOk) || (tcpCtlPort < 1024) || (tcpCtlPort > 65535))
	{
		tcpCtlPort = 9091;
	}

	m_address = udpAddress;
	m_dataPort = udpDataPort;
	m_controlPort = tcpCtlPort;

	if (m_addressEdited || m_dataPortEdited)
	{
		configureUDPLink();
		m_addressEdited = false;
		m_dataPortEdited = false;
	}

	ui->applyButton->setEnabled(false);
}

void SDRdaemonGui::on_sendButton_clicked(bool checked)
{
	sendConfiguration();
	ui->specificParms->setCursorPosition(0);
	ui->sendButton->setEnabled(false);
}

void SDRdaemonGui::sendConfiguration()
{
	QString remoteAddress;
	((SDRdaemonInput *) m_sampleSource)->getRemoteAddress(remoteAddress);

	if (remoteAddress != m_remoteAddress)
	{
		m_remoteAddress = remoteAddress;
		std::ostringstream os;
		os << "tcp://" << m_remoteAddress.toStdString() << ":" << m_controlPort;
		std::string addrstrng = os.str();
	    int rc = nn_connect(m_sender, addrstrng.c_str());

	    if (rc < 0) {
	    	QMessageBox::information(this, tr("Message"), tr("Cannot connect to remote control port"));
	    } else {
	    	qDebug() << "SDRdaemonGui::sendConfiguration: connexion to " << addrstrng.c_str() << " successful";
	    }
	}

	std::ostringstream os;
	bool ok;

	os << "decim=" << ui->decim->currentIndex()
	   << ",fcpos=" << ui->fcPos->currentIndex();

	uint64_t freq = ui->freq->text().toInt(&ok);

	if (ok) {
		os << ",freq=" << freq*1000LL;
	} else {
		QMessageBox::information(this, tr("Message"), tr("Invalid frequency"));
	}

	uint32_t srate = ui->sampleRate->text().toInt(&ok);

	if (ok) {
		os << ",srate=" << srate*1000;
	} else {
		QMessageBox::information(this,  tr("Message"), tr("invalid sample rate"));
	}

	if ((ui->specificParms->text()).size() > 0) {
		os << "," << ui->specificParms->text().toStdString();
	}

    int config_size = os.str().size();
    int rc = nn_send(m_sender, (void *) os.str().c_str(), config_size, 0);

    if (rc != config_size)
    {
    	QMessageBox::information(this, tr("Message"), tr("Cannot send message to remote control port"));
    }
    else
    {
    	qDebug() << "SDRdaemonGui::sendConfiguration:"
    		<< " remoteAddress: " << remoteAddress
    		<< " message: " << os.str().c_str();
    }
}

void SDRdaemonGui::on_address_textEdited(const QString& arg1)
{
	ui->applyButton->setEnabled(true);
	m_addressEdited = true;
}

void SDRdaemonGui::on_dataPort_textEdited(const QString& arg1)
{
	ui->applyButton->setEnabled(true);
	m_dataPortEdited = true;
}

void SDRdaemonGui::on_controlPort_textEdited(const QString& arg1)
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

void SDRdaemonGui::on_autoFollowRate_toggled(bool checked)
{
	if (m_autoFollowRate != checked) {
		m_autoFollowRate = checked;
        configureAutoFollowPolicy();
	}
}

void SDRdaemonGui::on_autoCorrBuffer_toggled(bool checked)
{
    if (m_autoCorrBuffer != checked) {
        m_autoCorrBuffer = checked;
        configureAutoFollowPolicy();
    }
}

void SDRdaemonGui::on_resetIndexes_clicked(bool checked)
{
    SDRdaemonInput::MsgConfigureSDRdaemonResetIndexes* message = SDRdaemonInput::MsgConfigureSDRdaemonResetIndexes::create();
    m_sampleSource->getInputMessageQueue()->push(message);
}

void SDRdaemonGui::on_freq_textEdited(const QString& arg1)
{
	ui->sendButton->setEnabled(true);
}

void SDRdaemonGui::on_sampleRate_textEdited(const QString& arg1)
{
	ui->sendButton->setEnabled(true);
}

void SDRdaemonGui::on_specificParms_textEdited(const QString& arg1)
{
	ui->sendButton->setEnabled(true);
}

void SDRdaemonGui::on_decim_currentIndexChanged(int index)
{
	ui->sendButton->setEnabled(true);
}

void SDRdaemonGui::on_fcPos_currentIndexChanged(int index)
{
	ui->sendButton->setEnabled(true);
}

void SDRdaemonGui::on_startStop_toggled(bool checked)
{
    if (checked)
    {
        if (m_pluginAPI->initAcquisition())
        {
            m_pluginAPI->startAcquisition();
            DSPEngine::instance()->startAudio();
        }
    }
    else
    {
        m_pluginAPI->stopAcquistion();
        DSPEngine::instance()->stopAudio();
    }
}

void SDRdaemonGui::on_record_toggled(bool checked)
{
    if (checked)
    {
        ui->record->setStyleSheet("QToolButton { background-color : red; }");
        m_fileSink->startRecording();
    }
    else
    {
        ui->record->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        m_fileSink->stopRecording();
    }
}

void SDRdaemonGui::configureUDPLink()
{
	qDebug() << "SDRdaemonGui::configureUDPLink: " << m_address.toStdString().c_str()
			<< " : " << m_dataPort;

	SDRdaemonInput::MsgConfigureSDRdaemonUDPLink* message = SDRdaemonInput::MsgConfigureSDRdaemonUDPLink::create(m_address, m_dataPort);
	m_sampleSource->getInputMessageQueue()->push(message);
}

void SDRdaemonGui::configureAutoCorrections()
{
	SDRdaemonInput::MsgConfigureSDRdaemonAutoCorr* message = SDRdaemonInput::MsgConfigureSDRdaemonAutoCorr::create(m_dcBlock, m_iqCorrection);
	m_sampleSource->getInputMessageQueue()->push(message);
}

void SDRdaemonGui::configureAutoFollowPolicy()
{
    SDRdaemonInput::MsgConfigureSDRdaemonAutoFollowPolicy* message = SDRdaemonInput::MsgConfigureSDRdaemonAutoFollowPolicy::create(m_autoFollowRate, m_autoCorrBuffer);
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
	if (m_initSendConfiguration)
	{
		sendConfiguration();
		m_initSendConfiguration = false;
	}

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

	s = QString::number(m_bufferLengthInSecs, 'f', 1);
	ui->bufferLenSecsText->setText(tr("%1").arg(s));

	s = QString::number((m_bufferGauge < 0 ? -50 - m_bufferGauge : 50 - m_bufferGauge), 'f', 0);
	ui->bufferRWBalanceText->setText(tr("%1").arg(s));

    ui->bufferGaugeNegative->setValue((m_bufferGauge < 0 ? 50 + m_bufferGauge : 0));
    ui->bufferGaugePositive->setValue((m_bufferGauge < 0 ? 0 : 50 - m_bufferGauge));
}

void SDRdaemonGui::updateStatus()
{
    int state = m_pluginAPI->state();

    if(m_lastEngineState != state)
    {
        switch(state)
        {
            case DSPDeviceEngine::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case DSPDeviceEngine::StIdle:
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case DSPDeviceEngine::StRunning:
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case DSPDeviceEngine::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_pluginAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastEngineState = state;
    }
}

void SDRdaemonGui::tick()
{
	if ((++m_tickCount & 0xf) == 0) {
		SDRdaemonInput::MsgConfigureSDRdaemonStreamTiming* message = SDRdaemonInput::MsgConfigureSDRdaemonStreamTiming::create();
		m_sampleSource->getInputMessageQueue()->push(message);
	}
}

