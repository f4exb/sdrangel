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

#ifdef _WIN32
#include <nn.h>
#include <pair.h>
#else
#include <nanomsg/nn.h>
#include <nanomsg/pair.h>
#endif

#include "ui_sdrdaemonsourcegui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "mainwindow.h"
#include "util/simpleserializer.h"

#include <device/devicesourceapi.h>
#include "device/deviceuiset.h"
#include "sdrdaemonsourcegui.h"

SDRdaemonSourceGui::SDRdaemonSourceGui(DeviceUISet *deviceUISet, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::SDRdaemonSourceGui),
	m_deviceUISet(deviceUISet),
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
    m_txDelay(0.0),
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
	ui->centerFrequency->setValueRange(7, 0, 9999999U);

    ui->freq->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->freq->setValueRange(7, 0, 9999999U);

    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(7, 32000U, 9999999U);

	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);
	connect(&(m_deviceUISet->m_deviceSourceAPI->getMasterTimer()), SIGNAL(timeout()), this, SLOT(tick()));
    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));

    m_sampleSource = (SDRdaemonSourceInput*) m_deviceUISet->m_deviceSourceAPI->getSampleSource();

	displaySettings();

	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);

    m_eventsTime.start();
    displayEventCounts();
    displayEventTimer();

    displaySettings();
    sendControl(true);
    sendSettings();
}

SDRdaemonSourceGui::~SDRdaemonSourceGui()
{
	delete ui;
}

void SDRdaemonSourceGui::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void SDRdaemonSourceGui::destroy()
{
	delete this;
}

void SDRdaemonSourceGui::setName(const QString& name)
{
	setObjectName(name);
}

QString SDRdaemonSourceGui::getName() const
{
	return objectName();
}

void SDRdaemonSourceGui::resetToDefaults()
{
    blockApplySettings(true);
    m_settings.resetToDefaults();
    displaySettings();
    blockApplySettings(false);
    sendSettings();
}

QByteArray SDRdaemonSourceGui::serialize() const
{
    return m_settings.serialize();
}

bool SDRdaemonSourceGui::deserialize(const QByteArray& data)
{
    blockApplySettings(true);

    if(m_settings.deserialize(data))
    {
        displaySettings();
        configureUDPLink();
        updateTxDelay();
        sendControl();
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

qint64 SDRdaemonSourceGui::getCenterFrequency() const
{
    return m_centerFrequency;
}

void SDRdaemonSourceGui::setCenterFrequency(qint64 centerFrequency)
{
    m_settings.m_centerFrequency = centerFrequency;
    displaySettings();
    sendControl();
    sendSettings();
}

bool SDRdaemonSourceGui::handleMessage(const Message& message)
{
	if (SDRdaemonSourceInput::MsgReportSDRdaemonAcquisition::match(message))
	{
		m_acquisition = ((SDRdaemonSourceInput::MsgReportSDRdaemonAcquisition&)message).getAcquisition();
		updateWithAcquisition();
		return true;
	}
	else if (SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamData::match(message))
	{
        int sampleRate = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamData&)message).getSampleRate();

        if (m_sampleRate != sampleRate)
        {
            m_sampleRate = sampleRate;
            updateTxDelay();
            sendControl();
        }

        m_centerFrequency = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamData&)message).getCenterFrequency();
        m_startingTimeStamp.tv_sec = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamData&)message).get_tv_sec();
        m_startingTimeStamp.tv_usec = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamData&)message).get_tv_usec();

        updateWithStreamData();
        return true;
	}
	else if (SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming::match(message))
	{
		m_startingTimeStamp.tv_sec = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).get_tv_sec();
		m_startingTimeStamp.tv_usec = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).get_tv_usec();
		m_framesDecodingStatus = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).getFramesDecodingStatus();
		m_allBlocksReceived = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).allBlocksReceived();
		m_bufferLengthInSecs = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).getBufferLengthInSecs();
        m_bufferGauge = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).getBufferGauge();
        m_minNbBlocks = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).getMinNbBlocks();
        m_minNbOriginalBlocks = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).getMinNbOriginalBlocks();
        m_maxNbRecovery = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).getMaxNbRecovery();
        m_avgNbBlocks = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).getAvgNbBlocks();
        m_avgNbOriginalBlocks = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).getAvgNbOriginalBlocks();
        m_avgNbRecovery = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).getAvgNbRecovery();
        m_nbOriginalBlocks = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).getNbOriginalBlocksPerFrame();

        int nbFECBlocks = ((SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming&)message).getNbFECBlocksPerFrame();

        if (m_nbFECBlocks != nbFECBlocks)
        {
            m_nbFECBlocks = nbFECBlocks;
            updateTxDelay();
            sendControl();
        }

		updateWithStreamTime();
		return true;
	}
	else
	{
		return false;
	}
}

void SDRdaemonSourceGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        //qDebug("SDRdaemonGui::handleInputMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_deviceSampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("SDRdaemonGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();
            DSPSignalNotification *fwd = new DSPSignalNotification(*notif);
            m_sampleSource->getInputMessageQueue()->push(fwd);

            delete message;
        }
        else
        {
            if (handleMessage(*message))
            {
                delete message;
            }
        }
    }
}

void SDRdaemonSourceGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_deviceSampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_deviceSampleRate / 1000));
}

void SDRdaemonSourceGui::updateTxDelay()
{
    if (m_sampleRate == 0) {
        m_txDelay = 0.0; // 0 value will not set the Tx delay
    } else {
        m_txDelay = ((127*127*m_settings.m_txDelay) / m_sampleRate)/(128 + m_nbFECBlocks);
    }

    ui->txDelayText->setToolTip(tr("%1 us").arg(QString::number(m_txDelay*1e6, 'f', 0)));
}

void SDRdaemonSourceGui::displaySettings()
{
    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
    ui->deviceRateText->setText(tr("%1k").arg(m_sampleRate / 1000.0));

    ui->freq->setValue(m_settings.m_centerFrequency / 1000);
    ui->decim->setCurrentIndex(m_settings.m_log2Decim);
    ui->fcPos->setCurrentIndex(m_settings.m_fcPos);
    ui->sampleRate->setValue(m_settings.m_sampleRate);
    ui->specificParms->setText(m_settings.m_specificParameters);
    ui->specificParms->setCursorPosition(0);
    ui->txDelayText->setText(tr("%1").arg(m_settings.m_txDelay*100));
    ui->nbFECBlocks->setValue(m_settings.m_nbFECBlocks);
    QString nstr = QString("%1").arg(m_settings.m_nbFECBlocks, 2, 10, QChar('0'));
    ui->nbFECBlocksText->setText(nstr);

    QString s0 = QString::number(128 + m_settings.m_nbFECBlocks, 'f', 0);
    ui->nominalNbBlocksText->setText(tr("%1/%2").arg(s0).arg(nstr));

    ui->address->setText(m_settings.m_address);
    ui->dataPort->setText(tr("%1").arg(m_settings.m_dataPort));
    ui->controlPort->setText(tr("%1").arg(m_settings.m_controlPort));
    ui->specificParms->setText(m_settings.m_specificParameters);

	ui->dcOffset->setChecked(m_settings.m_dcBlock);
	ui->iqImbalance->setChecked(m_settings.m_iqCorrection);
}

void SDRdaemonSourceGui::sendControl(bool force)
{
    QString remoteAddress;
    ((SDRdaemonSourceInput *) m_sampleSource)->getRemoteAddress(remoteAddress);

    if ((remoteAddress != m_remoteAddress) ||
        (m_settings.m_controlPort != m_controlSettings.m_controlPort) || force)
    {
        m_remoteAddress = remoteAddress;

        int rc = nn_shutdown(m_sender, 0);

        if (rc < 0) {
            qDebug() << "SDRdaemonSourceGui::sendControl: disconnection failed";
        } else {
            qDebug() << "SDRdaemonSourceGui::sendControl: disconnection successful";
        }

        std::ostringstream os;
        os << "tcp://" << m_remoteAddress.toStdString() << ":" << m_settings.m_controlPort;
        std::string addrstrng = os.str();
        rc = nn_connect(m_sender, addrstrng.c_str());

        if (rc < 0) {
            qDebug() << "SDRdaemonSourceGui::sendConfiguration: connexion to " << addrstrng.c_str() << " failed";
            QMessageBox::information(this, tr("Message"), tr("Cannot connect to remote control port"));
        } else {
            qDebug() << "SDRdaemonSourceGui::sendConfiguration: connexion to " << addrstrng.c_str() << " successful";
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

    if ((m_settings.m_nbFECBlocks != m_controlSettings.m_nbFECBlocks) || force)
    {
        if (nbArgs > 0) os << ",";
        os << "fecblk=" << m_settings.m_nbFECBlocks;
        nbArgs++;
    }

    if (m_txDelay != 0.0)
    {
        if (nbArgs > 0) os << ",";
        os << "txdelay=" << (int) (m_txDelay*1e6);
        nbArgs++;
        m_txDelay = 0.0;
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
            qDebug() << "SDRdaemonSourceGui::sendControl: Cannot send message to remote control port."
                << " remoteAddress: " << m_remoteAddress
                << " remotePort: " << m_settings.m_controlPort
                << " message: " << os.str().c_str();
        }
        else
        {
            qDebug() << "SDRdaemonSourceGui::sendControl:"
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
    m_controlSettings.m_nbFECBlocks = m_settings.m_nbFECBlocks;
    m_controlSettings.m_specificParameters = m_settings.m_specificParameters;
}

void SDRdaemonSourceGui::sendSettings()
{
    if(!m_updateTimer.isActive())
        m_updateTimer.start(100);
}

void SDRdaemonSourceGui::on_applyButton_clicked(bool checked __attribute__((unused)))
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

void SDRdaemonSourceGui::on_sendButton_clicked(bool checked __attribute__((unused)))
{
    updateTxDelay();
    sendControl(true);
	ui->specificParms->setCursorPosition(0);
}

void SDRdaemonSourceGui::on_address_returnPressed()
{
    m_settings.m_address = ui->address->text();
    configureUDPLink();
}

void SDRdaemonSourceGui::on_dataPort_returnPressed()
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

void SDRdaemonSourceGui::on_controlPort_returnPressed()
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

void SDRdaemonSourceGui::on_dcOffset_toggled(bool checked)
{
	if (m_dcBlock != checked)
	{
		m_dcBlock = checked;
		configureAutoCorrections();
	}
}

void SDRdaemonSourceGui::on_iqImbalance_toggled(bool checked)
{
	if (m_iqCorrection != checked)
	{
		m_iqCorrection = checked;
		configureAutoCorrections();
	}
}

void SDRdaemonSourceGui::on_freq_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    sendControl();
}

void SDRdaemonSourceGui::on_sampleRate_changed(quint64 value)
{
    m_settings.m_sampleRate = value;
    sendControl();
}

void SDRdaemonSourceGui::on_specificParms_returnPressed()
{
    if ((ui->specificParms->text()).size() > 0) {
        m_settings.m_specificParameters = ui->specificParms->text();
        sendControl();
    }
}

void SDRdaemonSourceGui::on_decim_currentIndexChanged(int index __attribute__((unused)))
{
    m_settings.m_log2Decim = ui->decim->currentIndex();
    sendControl();
}

void SDRdaemonSourceGui::on_fcPos_currentIndexChanged(int index __attribute__((unused)))
{
    m_settings.m_fcPos = ui->fcPos->currentIndex();
    sendControl();
}

void SDRdaemonSourceGui::on_txDelay_valueChanged(int value)
{
    m_settings.m_txDelay = value / 100.0;
    ui->txDelayText->setText(tr("%1").arg(value));
    updateTxDelay();
    sendControl();
}

void SDRdaemonSourceGui::on_nbFECBlocks_valueChanged(int value)
{
    m_settings.m_nbFECBlocks = value;
    QString nstr = QString("%1").arg(m_settings.m_nbFECBlocks, 2, 10, QChar('0'));
    ui->nbFECBlocksText->setText(nstr);
    sendControl();
}

void SDRdaemonSourceGui::on_startStop_toggled(bool checked)
{
    if (checked)
    {
        if (m_deviceUISet->m_deviceSourceAPI->initAcquisition())
        {
            m_deviceUISet->m_deviceSourceAPI->startAcquisition();
            DSPEngine::instance()->startAudioOutput();
        }
    }
    else
    {
        m_deviceUISet->m_deviceSourceAPI->stopAcquisition();
        DSPEngine::instance()->stopAudioOutput();
    }
}

void SDRdaemonSourceGui::on_record_toggled(bool checked)
{
    if (checked) {
        ui->record->setStyleSheet("QToolButton { background-color : red; }");
    } else {
        ui->record->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
    }

    SDRdaemonSourceInput::MsgFileRecord* message = SDRdaemonSourceInput::MsgFileRecord::create(checked);
    m_sampleSource->getInputMessageQueue()->push(message);
}

void SDRdaemonSourceGui::on_eventCountsReset_clicked(bool checked __attribute__((unused)))
{
    m_countUnrecoverable = 0;
    m_countRecovered = 0;
    m_eventsTime.start();
    displayEventCounts();
    displayEventTimer();
}

void SDRdaemonSourceGui::displayEventCounts()
{
    QString nstr = QString("%1").arg(m_countUnrecoverable, 3, 10, QChar('0'));
    ui->eventUnrecText->setText(nstr);
    nstr = QString("%1").arg(m_countRecovered, 3, 10, QChar('0'));
    ui->eventRecText->setText(nstr);
}

void SDRdaemonSourceGui::displayEventTimer()
{
    int elapsedTimeMillis = m_eventsTime.elapsed();
    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(elapsedTimeMillis/1000);
    QString s_time = recordLength.toString("hh:mm:ss");
    ui->eventCountsTimeText->setText(s_time);
}

void SDRdaemonSourceGui::configureUDPLink()
{
	qDebug() << "SDRdaemonGui::configureUDPLink: " << m_settings.m_address.toStdString().c_str()
			<< " : " << m_settings.m_dataPort;

	SDRdaemonSourceInput::MsgConfigureSDRdaemonUDPLink* message = SDRdaemonSourceInput::MsgConfigureSDRdaemonUDPLink::create(m_settings.m_address, m_settings.m_dataPort);
	m_sampleSource->getInputMessageQueue()->push(message);
}

void SDRdaemonSourceGui::configureAutoCorrections()
{
	SDRdaemonSourceInput::MsgConfigureSDRdaemonAutoCorr* message = SDRdaemonSourceInput::MsgConfigureSDRdaemonAutoCorr::create(m_dcBlock, m_iqCorrection);
	m_sampleSource->getInputMessageQueue()->push(message);
}

void SDRdaemonSourceGui::updateWithAcquisition()
{
}

void SDRdaemonSourceGui::updateWithStreamData()
{
	ui->centerFrequency->setValue(m_centerFrequency / 1000);
	updateWithStreamTime();
}

void SDRdaemonSourceGui::updateWithStreamTime()
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

    s = QString("%1").arg(m_maxNbRecovery, 2, 10, QChar('0'));
    ui->maxNbRecoveryText->setText(tr("%1").arg(s));

    s = QString::number(m_nbOriginalBlocks + m_nbFECBlocks, 'f', 0);
    QString s1 = QString("%1").arg(m_nbFECBlocks, 2, 10, QChar('0'));
    ui->nominalNbBlocksText->setText(tr("%1/%2").arg(s).arg(s1));

    if (updateEventCounts)
    {
        displayEventCounts();
    }

    displayEventTimer();
}

void SDRdaemonSourceGui::updateHardware()
{
    qDebug() << "SDRdaemonSinkGui::updateHardware";
    SDRdaemonSourceInput::MsgConfigureSDRdaemonSource* message = SDRdaemonSourceInput::MsgConfigureSDRdaemonSource::create(m_settings, m_forceSettings);
    m_sampleSource->getInputMessageQueue()->push(message);
    m_forceSettings = false;
    m_updateTimer.stop();
}

void SDRdaemonSourceGui::updateStatus()
{
    int state = m_deviceUISet->m_deviceSourceAPI->state();

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
                QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceSourceAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastEngineState = state;
    }
}

void SDRdaemonSourceGui::tick()
{
	if ((++m_tickCount & 0xf) == 0) {
		SDRdaemonSourceInput::MsgConfigureSDRdaemonStreamTiming* message = SDRdaemonSourceInput::MsgConfigureSDRdaemonStreamTiming::create();
		m_sampleSource->getInputMessageQueue()->push(message);
	}
}
