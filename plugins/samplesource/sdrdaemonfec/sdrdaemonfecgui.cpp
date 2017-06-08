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

#include "ui_sdrdaemonfecgui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "mainwindow.h"
#include "util/simpleserializer.h"

#include "sdrdaemonfecgui.h"

#include <device/devicesourceapi.h>
#include <dsp/filerecord.h>

SDRdaemonFECGui::SDRdaemonFECGui(DeviceSourceAPI *deviceAPI, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::SDRdaemonFECGui),
	m_deviceAPI(deviceAPI),
	m_sampleSource(NULL),
	m_acquisition(false),
	m_lastEngineState((DSPDeviceSourceEngine::State)-1),
	m_sampleRate(0),
	m_centerFrequency(0),
	m_framesDecodingStatus(0),
	m_bufferLengthInSecs(0.0),
    m_bufferGauge(-50),
	m_nbOriginalBlocks(128),
    m_nbFECBlocks(0),
    m_samplesCount(0),
    m_tickCount(0),
    m_address("127.0.0.1"),
    m_dataPort(9090),
    m_controlPort(9091),
    m_addressEdited(false),
    m_dataPortEdited(false),
    m_initSendConfiguration(false),
	m_countUnrecoverable(0),
	m_countRecovered(0),
    m_doApplySettings(true),
    m_forceSettings(true),
    m_dcBlock(false),
    m_iqCorrection(false)
{
	m_sender = nn_socket(AF_SP, NN_PAIR);
	assert(m_sender != -1);
	int millis = 500;
    int rc __attribute__((unused)) = nn_setsockopt (m_sender, NN_SOL_SOCKET, NN_SNDTIMEO, &millis, sizeof (millis));
    assert (rc == 0);

    m_paletteGreenText.setColor(QPalette::WindowText, Qt::green);
    m_paletteWhiteText.setColor(QPalette::WindowText, Qt::white);

	m_startingTimeStamp.tv_sec = 0;
	m_startingTimeStamp.tv_usec = 0;
	ui->setupUi(this);
	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
	ui->centerFrequency->setValueRange(7, 0, pow(10,7));

	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);
	connect(&(deviceAPI->getMainWindow()->getMasterTimer()), SIGNAL(timeout()), this, SLOT(tick()));
    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));

	m_sampleSource = new SDRdaemonFECInput(deviceAPI->getMainWindow()->getMasterTimer(), m_deviceAPI);
	connect(m_sampleSource->getOutputMessageQueueToGUI(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
	m_deviceAPI->setSource(m_sampleSource);

	displaySettings();

    char recFileNameCStr[30];
    sprintf(recFileNameCStr, "test_%d.sdriq", m_deviceAPI->getDeviceUID());
    m_fileSink = new FileRecord(std::string(recFileNameCStr));
    m_deviceAPI->addSink(m_fileSink);

    connect(m_deviceAPI->getDeviceOutputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleDSPMessages()), Qt::QueuedConnection);

    m_eventsTime.start();
    displayEventCounts();
    displayEventTimer();

    displaySettings();
    sendControl(true);
    sendSettings();
}

SDRdaemonFECGui::~SDRdaemonFECGui()
{
    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;
    delete m_sampleSource;
	delete ui;
}

void SDRdaemonFECGui::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void SDRdaemonFECGui::destroy()
{
	delete this;
}

void SDRdaemonFECGui::setName(const QString& name)
{
	setObjectName(name);
}

QString SDRdaemonFECGui::getName() const
{
	return objectName();
}

void SDRdaemonFECGui::resetToDefaults()
{
    blockApplySettings(true);
    m_settings.resetToDefaults();
    displaySettings();
    blockApplySettings(false);
    sendSettings();
}

QByteArray SDRdaemonFECGui::serialize() const
{
    return m_settings.serialize();
}

bool SDRdaemonFECGui::deserialize(const QByteArray& data)
{
    blockApplySettings(true);

    if(m_settings.deserialize(data))
    {
        displaySettings();
        configureUDPLink();
        blockApplySettings(false);
        sendControl(true);
        m_forceSettings = true;
        sendSettings();
        return true;
    }
    else
    {
        blockApplySettings(false);
        return false;
    }
}

qint64 SDRdaemonFECGui::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void SDRdaemonFECGui::setCenterFrequency(qint64 centerFrequency)
{
    m_settings.m_centerFrequency = centerFrequency;
    displaySettings();
    sendControl();
    sendSettings();
}

bool SDRdaemonFECGui::handleMessage(const Message& message)
{
	if (SDRdaemonFECInput::MsgReportSDRdaemonAcquisition::match(message))
	{
		m_acquisition = ((SDRdaemonFECInput::MsgReportSDRdaemonAcquisition&)message).getAcquisition();
		updateWithAcquisition();
		return true;
	}
	else if (SDRdaemonFECInput::MsgReportSDRdaemonFECStreamData::match(message))
	{
		m_sampleRate = ((SDRdaemonFECInput::MsgReportSDRdaemonFECStreamData&)message).getSampleRate();
		m_centerFrequency = ((SDRdaemonFECInput::MsgReportSDRdaemonFECStreamData&)message).getCenterFrequency();
		m_startingTimeStamp.tv_sec = ((SDRdaemonFECInput::MsgReportSDRdaemonFECStreamData&)message).get_tv_sec();
		m_startingTimeStamp.tv_usec = ((SDRdaemonFECInput::MsgReportSDRdaemonFECStreamData&)message).get_tv_usec();
		updateWithStreamData();
		return true;
	}
	else if (SDRdaemonFECInput::MsgReportSDRdaemonFECStreamTiming::match(message))
	{
		m_startingTimeStamp.tv_sec = ((SDRdaemonFECInput::MsgReportSDRdaemonFECStreamTiming&)message).get_tv_sec();
		m_startingTimeStamp.tv_usec = ((SDRdaemonFECInput::MsgReportSDRdaemonFECStreamTiming&)message).get_tv_usec();
		m_framesDecodingStatus = ((SDRdaemonFECInput::MsgReportSDRdaemonFECStreamTiming&)message).getFramesDecodingStatus();
		m_allBlocksReceived = ((SDRdaemonFECInput::MsgReportSDRdaemonFECStreamTiming&)message).allBlocksReceived();
		m_bufferLengthInSecs = ((SDRdaemonFECInput::MsgReportSDRdaemonFECStreamTiming&)message).getBufferLengthInSecs();
        m_bufferGauge = ((SDRdaemonFECInput::MsgReportSDRdaemonFECStreamTiming&)message).getBufferGauge();
        m_minNbBlocks = ((SDRdaemonFECInput::MsgReportSDRdaemonFECStreamTiming&)message).getMinNbBlocks();
        m_minNbOriginalBlocks = ((SDRdaemonFECInput::MsgReportSDRdaemonFECStreamTiming&)message).getMinNbOriginalBlocks();
        m_maxNbRecovery = ((SDRdaemonFECInput::MsgReportSDRdaemonFECStreamTiming&)message).getMaxNbRecovery();
        m_avgNbBlocks = ((SDRdaemonFECInput::MsgReportSDRdaemonFECStreamTiming&)message).getAvgNbBlocks();
        m_avgNbOriginalBlocks = ((SDRdaemonFECInput::MsgReportSDRdaemonFECStreamTiming&)message).getAvgNbOriginalBlocks();
        m_avgNbRecovery = ((SDRdaemonFECInput::MsgReportSDRdaemonFECStreamTiming&)message).getAvgNbRecovery();
        m_nbOriginalBlocks = ((SDRdaemonFECInput::MsgReportSDRdaemonFECStreamTiming&)message).getNbOriginalBlocksPerFrame();
        m_nbFECBlocks = ((SDRdaemonFECInput::MsgReportSDRdaemonFECStreamTiming&)message).getNbFECBlocksPerFrame();

		updateWithStreamTime();
		return true;
	}
	else
	{
		return false;
	}
}

void SDRdaemonFECGui::handleDSPMessages()
{
    Message* message;

    while ((message = m_deviceAPI->getDeviceOutputMessageQueue()->pop()) != 0)
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

void SDRdaemonFECGui::handleSourceMessages()
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

void SDRdaemonFECGui::updateSampleRateAndFrequency()
{
    m_deviceAPI->getSpectrum()->setSampleRate(m_deviceSampleRate);
    m_deviceAPI->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_deviceSampleRate / 1000));
}

void SDRdaemonFECGui::displaySettings()
{
    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
    ui->deviceRateText->setText(tr("%1k").arg(m_sampleRate / 1000.0));

    ui->freq->setText(QString::number(m_settings.m_centerFrequency / 1000));
    ui->decim->setCurrentIndex(m_settings.m_log2Decim);
    ui->fcPos->setCurrentIndex(m_settings.m_fcPos);
    ui->sampleRate->setText(QString::number(m_settings.m_sampleRate / 1000));
    ui->specificParms->setText(m_settings.m_specificParameters);
    ui->specificParms->setCursorPosition(0);


    QString s0 = QString::number(128 + m_settings.m_nbFECBlocks, 'f', 0);
    QString s1 = QString::number(m_settings.m_nbFECBlocks, 'f', 0);
    ui->nominalNbBlocksText->setText(tr("%1/%2").arg(s0).arg(s1));

    ui->address->setText(m_settings.m_address);
    ui->dataPort->setText(tr("%1").arg(m_settings.m_dataPort));
    ui->controlPort->setText(tr("%1").arg(m_settings.m_controlPort));
    ui->specificParms->setText(m_settings.m_specificParameters);

	ui->dcOffset->setChecked(m_settings.m_dcBlock);
	ui->iqImbalance->setChecked(m_settings.m_iqCorrection);

}

void SDRdaemonFECGui::sendControl(bool force)
{
    QString remoteAddress;
    ((SDRdaemonFECInput *) m_sampleSource)->getRemoteAddress(remoteAddress);

    if ((remoteAddress != m_remoteAddress) ||
        (m_settings.m_controlPort != m_controlSettings.m_controlPort) || force)
    {
        m_remoteAddress = remoteAddress;

        int rc = nn_shutdown(m_sender, 0);

        if (rc < 0) {
            qDebug() << "SDRdaemonFECGui::sendControl: disconnection failed";
        } else {
            qDebug() << "SDRdaemonFECGui::sendControl: disconnection successful";
        }

        std::ostringstream os;
        os << "tcp://" << m_remoteAddress.toStdString() << ":" << m_settings.m_controlPort;
        std::string addrstrng = os.str();
        rc = nn_connect(m_sender, addrstrng.c_str());

        if (rc < 0) {
            qDebug() << "SDRdaemonFECGui::sendConfiguration: connexion to " << addrstrng.c_str() << " failed";
            QMessageBox::information(this, tr("Message"), tr("Cannot connect to remote control port"));
        } else {
            qDebug() << "SDRdaemonFECGui::sendConfiguration: connexion to " << addrstrng.c_str() << " successful";
        }
    }

    std::ostringstream os;
    int nbArgs = 0;

    if ((m_settings.m_centerFrequency != m_controlSettings.m_centerFrequency) || force)
    {
        os << "freq=" << m_settings.m_centerFrequency;
        nbArgs++;
    }

    if ((m_settings.m_sampleRate != m_controlSettings.m_sampleRate) || (m_settings.m_log2Decim != m_controlSettings.m_log2Decim) || force)
    {
        if (nbArgs > 0) os << ",";
        os << "srate=" << m_settings.m_sampleRate;
        nbArgs++;
    }

    if ((m_settings.m_log2Decim != m_controlSettings.m_log2Decim) || force)
    {
        if (nbArgs > 0) os << ",";
        os << "decim=" << m_settings.m_log2Decim;
        nbArgs++;
    }

    if ((m_settings.m_fcPos != m_controlSettings.m_fcPos) || force)
    {
        if (nbArgs > 0) os << ",";
        os << "fcpos=" << m_settings.m_fcPos;
        nbArgs++;
    }

    if ((m_settings.m_specificParameters != m_controlSettings.m_specificParameters) || force)
    {
        if (m_settings.m_specificParameters.size() > 0)
        {
            if (nbArgs > 0) os << ",";
            os << m_settings.m_specificParameters.toStdString();
            nbArgs++;
        }
    }

    if (nbArgs > 0)
    {
        int config_size = os.str().size();
        int rc = nn_send(m_sender, (void *) os.str().c_str(), config_size, 0);

        if (rc != config_size)
        {
            //QMessageBox::information(this, tr("Message"), tr("Cannot send message to remote control port"));
            qDebug() << "SDRdaemonFECGui::sendControl: Cannot send message to remote control port."
                << " remoteAddress: " << m_remoteAddress
                << " remotePort: " << m_settings.m_controlPort
                << " message: " << os.str().c_str();
        }
        else
        {
            qDebug() << "SDRdaemonFECGui::sendControl:"
                << "remoteAddress:" << m_remoteAddress
                << "remotePort:" << m_settings.m_controlPort
                << "message:" << os.str().c_str();
        }
    }

    m_controlSettings.m_address = m_settings.m_address;
    m_controlSettings.m_controlPort = m_settings.m_controlPort;
    m_controlSettings.m_centerFrequency = m_settings.m_centerFrequency;
    m_controlSettings.m_sampleRate = m_settings.m_sampleRate;
    m_controlSettings.m_log2Decim = m_settings.m_log2Decim;
    m_controlSettings.m_fcPos = m_settings.m_fcPos;
    m_controlSettings.m_specificParameters = m_settings.m_specificParameters;
}

void SDRdaemonFECGui::sendSettings()
{
    if(!m_updateTimer.isActive())
        m_updateTimer.start(100);
}

void SDRdaemonFECGui::on_applyButton_clicked(bool checked __attribute__((unused)))
{
    m_settings.m_address = ui->address->text();

    bool ctlOk;
    int udpCtlPort = ui->controlPort->text().toInt(&ctlOk);

    if((ctlOk) && (udpCtlPort >= 1024) && (udpCtlPort < 65535))
    {
        m_settings.m_controlPort = udpCtlPort;
    }

    bool dataOk;
    int udpDataPort = ui->dataPort->text().toInt(&dataOk);

    if((dataOk) && (udpDataPort >= 1024) && (udpDataPort < 65535))
    {
        m_settings.m_dataPort = udpDataPort;
    }

    configureUDPLink();
}

void SDRdaemonFECGui::on_sendButton_clicked(bool checked __attribute__((unused)))
{
    sendControl(true);
	ui->specificParms->setCursorPosition(0);
}

void SDRdaemonFECGui::on_address_returnPressed()
{
    m_settings.m_address = ui->address->text();
    configureUDPLink();
}

void SDRdaemonFECGui::on_dataPort_returnPressed()
{
    bool dataOk;
    int udpDataPort = ui->dataPort->text().toInt(&dataOk);

    if((!dataOk) || (udpDataPort < 1024) || (udpDataPort > 65535))
    {
        return;
    }
    else
    {
        m_settings.m_dataPort = udpDataPort;
    }

    configureUDPLink();
}

void SDRdaemonFECGui::on_controlPort_returnPressed()
{
    bool ctlOk;
    int udpCtlPort = ui->controlPort->text().toInt(&ctlOk);

    if((!ctlOk) || (udpCtlPort < 1024) || (udpCtlPort > 65535))
    {
        return;
    }
    else
    {
        m_settings.m_controlPort = udpCtlPort;
    }

    sendControl();
}

void SDRdaemonFECGui::on_dcOffset_toggled(bool checked)
{
	if (m_dcBlock != checked)
	{
		m_dcBlock = checked;
		configureAutoCorrections();
	}
}

void SDRdaemonFECGui::on_iqImbalance_toggled(bool checked)
{
	if (m_iqCorrection != checked)
	{
		m_iqCorrection = checked;
		configureAutoCorrections();
	}
}

void SDRdaemonFECGui::on_freq_returnPressed()
{
    bool ok;
    uint64_t freq = ui->freq->text().toInt(&ok);

    if (ok)
    {
        m_settings.m_centerFrequency = freq * 1000;
        sendControl();
    }
}

void SDRdaemonFECGui::on_sampleRate_returnPressed()
{
    bool ok;
    uint32_t srate = ui->sampleRate->text().toInt(&ok);

    if (ok)
    {
        m_settings.m_sampleRate = srate * 1000;
        sendControl();
    }
}

void SDRdaemonFECGui::on_specificParms_returnPressed()
{
    if ((ui->specificParms->text()).size() > 0) {
        m_settings.m_specificParameters = ui->specificParms->text();
        sendControl();
    }
}

void SDRdaemonFECGui::on_decim_currentIndexChanged(int index __attribute__((unused)))
{
    m_settings.m_log2Decim = ui->decim->currentIndex();
    sendControl();
}

void SDRdaemonFECGui::on_fcPos_currentIndexChanged(int index __attribute__((unused)))
{
    m_settings.m_fcPos = ui->fcPos->currentIndex();
    sendControl();
}

void SDRdaemonFECGui::on_startStop_toggled(bool checked)
{
    if (checked)
    {
        if (m_deviceAPI->initAcquisition())
        {
            m_deviceAPI->startAcquisition();
            DSPEngine::instance()->startAudioOutput();
        }
    }
    else
    {
        m_deviceAPI->stopAcquisition();
        DSPEngine::instance()->stopAudioOutput();
    }
}

void SDRdaemonFECGui::on_record_toggled(bool checked)
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

void SDRdaemonFECGui::on_eventCountsReset_clicked(bool checked __attribute__((unused)))
{
    m_countUnrecoverable = 0;
    m_countRecovered = 0;
    m_eventsTime.start();
    displayEventCounts();
    displayEventTimer();
}

void SDRdaemonFECGui::displayEventCounts()
{
    QString nstr = QString("%1").arg(m_countUnrecoverable, 3, 10, QChar('0'));
    ui->eventUnrecText->setText(nstr);
    nstr = QString("%1").arg(m_countRecovered, 3, 10, QChar('0'));
    ui->eventRecText->setText(nstr);
}

void SDRdaemonFECGui::displayEventTimer()
{
    int elapsedTimeMillis = m_eventsTime.elapsed();
    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(elapsedTimeMillis/1000);
    QString s_time = recordLength.toString("hh:mm:ss");
    ui->eventCountsTimeText->setText(s_time);
}

void SDRdaemonFECGui::configureUDPLink()
{
	qDebug() << "SDRdaemonGui::configureUDPLink: " << m_settings.m_address.toStdString().c_str()
			<< " : " << m_settings.m_dataPort;

	SDRdaemonFECInput::MsgConfigureSDRdaemonUDPLink* message = SDRdaemonFECInput::MsgConfigureSDRdaemonUDPLink::create(m_settings.m_address, m_settings.m_dataPort);
	m_sampleSource->getInputMessageQueue()->push(message);
}

void SDRdaemonFECGui::configureAutoCorrections()
{
	SDRdaemonFECInput::MsgConfigureSDRdaemonAutoCorr* message = SDRdaemonFECInput::MsgConfigureSDRdaemonAutoCorr::create(m_dcBlock, m_iqCorrection);
	m_sampleSource->getInputMessageQueue()->push(message);
}

void SDRdaemonFECGui::updateWithAcquisition()
{
}

void SDRdaemonFECGui::updateWithStreamData()
{
	ui->centerFrequency->setValue(m_centerFrequency / 1000);
	updateWithStreamTime();
}

void SDRdaemonFECGui::updateWithStreamTime()
{
	bool updateEventCounts = false;
    quint64 startingTimeStampMsec = ((quint64) m_startingTimeStamp.tv_sec * 1000LL) + ((quint64) m_startingTimeStamp.tv_usec / 1000LL);
    QDateTime dt = QDateTime::fromMSecsSinceEpoch(startingTimeStampMsec);
    QString s_date = dt.toString("yyyy-MM-dd  hh:mm:ss.zzz");
	ui->absTimeText->setText(s_date);

	if (m_framesDecodingStatus == 2)
	{
		ui->allFramesDecoded->setStyleSheet("QToolButton { background-color : green; }");
	}
	else if (m_framesDecodingStatus == 1)
	{
        if (m_countRecovered < 999) m_countRecovered++;
        updateEventCounts = true;
        ui->allFramesDecoded->setStyleSheet("QToolButton { background:rgb(56,56,56); }");
	}
	else
	{
        if (m_countUnrecoverable < 999) m_countUnrecoverable++;
        updateEventCounts = true;
        ui->allFramesDecoded->setStyleSheet("QToolButton { background-color : red; }");
	}

	QString s = QString::number(m_bufferLengthInSecs, 'f', 1);
	ui->bufferLenSecsText->setText(tr("%1").arg(s));

    s = QString::number(m_bufferGauge, 'f', 0);
	ui->bufferRWBalanceText->setText(tr("%1").arg(s));

    ui->bufferGaugeNegative->setValue((m_bufferGauge < 0 ? -m_bufferGauge : 0));
    ui->bufferGaugePositive->setValue((m_bufferGauge < 0 ? 0 : m_bufferGauge));

    s = QString::number(m_minNbBlocks, 'f', 0);
    ui->minNbBlocksText->setText(tr("%1").arg(s));

    s = QString::number(m_maxNbRecovery, 'f', 0);
    ui->maxNbRecoveryText->setText(tr("%1").arg(s));

    s = QString::number(m_nbOriginalBlocks + m_nbFECBlocks, 'f', 0);
    QString s1 = QString::number(m_nbFECBlocks, 'f', 0);
    ui->nominalNbBlocksText->setText(tr("%1/%2").arg(s).arg(s1));

    if (updateEventCounts)
    {
        displayEventCounts();
    }

    displayEventTimer();
}

void SDRdaemonFECGui::updateHardware()
{
    qDebug() << "SDRdaemonSinkGui::updateHardware";
    SDRdaemonFECInput::MsgConfigureSDRdaemonFEC* message = SDRdaemonFECInput::MsgConfigureSDRdaemonFEC::create(m_settings, m_forceSettings);
    m_sampleSource->getInputMessageQueue()->push(message);
    m_forceSettings = false;
    m_updateTimer.stop();
}

void SDRdaemonFECGui::updateStatus()
{
    int state = m_deviceAPI->state();

    if(m_lastEngineState != state)
    {
        switch(state)
        {
            case DSPDeviceSourceEngine::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case DSPDeviceSourceEngine::StIdle:
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case DSPDeviceSourceEngine::StRunning:
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case DSPDeviceSourceEngine::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_deviceAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastEngineState = state;
    }
}

void SDRdaemonFECGui::tick()
{
	if ((++m_tickCount & 0xf) == 0) {
		SDRdaemonFECInput::MsgConfigureSDRdaemonStreamTiming* message = SDRdaemonFECInput::MsgConfigureSDRdaemonStreamTiming::create();
		m_sampleSource->getInputMessageQueue()->push(message);
	}
}
