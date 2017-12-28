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

#include <QDebug>

#include <QTime>
#include <QDateTime>
#include <QString>
#include <QFileDialog>
#include <QMessageBox>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <nanomsg/nn.h>
#include <nanomsg/pair.h>

#include "ui_sdrdaemonsinkgui.h"
#include "plugin/pluginapi.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"

#include "mainwindow.h"

#include "device/devicesinkapi.h"
#include "device/deviceuiset.h"
#include "sdrdaemonsinkgui.h"

SDRdaemonSinkGui::SDRdaemonSinkGui(DeviceUISet *deviceUISet, QWidget* parent) :
	QWidget(parent),
	ui(new Ui::SDRdaemonSinkGui),
	m_deviceUISet(deviceUISet),
	m_settings(),
	m_deviceSampleSink(0),
	m_sampleRate(0),
	m_samplesCount(0),
	m_tickCount(0),
	m_nbSinceLastFlowCheck(0),
	m_lastEngineState((DSPDeviceSinkEngine::State)-1),
	m_doApplySettings(true),
	m_forceSettings(true)
{
    m_nnSender = nn_socket(AF_SP, NN_PAIR);
    assert(m_nnSender != -1);
    int millis = 500;
    int rc = nn_setsockopt(m_nnSender, NN_SOL_SOCKET, NN_SNDTIMEO, &millis, sizeof (millis));

    if (rc != 0) {
        qCritical("SDRdaemonSinkGui::SDRdaemonSinkGui: nn_setsockopt failed with rc %d", rc);
    }

    m_countUnrecoverable = 0;
    m_countRecovered = 0;

    m_paletteGreenText.setColor(QPalette::WindowText, Qt::green);
    m_paletteRedText.setColor(QPalette::WindowText, Qt::red);
    m_paletteWhiteText.setColor(QPalette::WindowText, Qt::white);

    ui->setupUi(this);

	ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
	ui->centerFrequency->setValueRange(7, 0, pow(10,7));

    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(7, 32000U, 9000000U);

	connect(&(m_deviceUISet->m_deviceSinkAPI->getMasterTimer()), SIGNAL(timeout()), this, SLOT(tick()));
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
	connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	m_statusTimer.start(500);

	m_deviceSampleSink = (SDRdaemonSinkOutput*) m_deviceUISet->m_deviceSinkAPI->getSampleSink();

	connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);

    m_time.start();
    displayEventCounts();
    displayEventTimer();

    displaySettings();
    sendControl(true);
    sendSettings();
}

SDRdaemonSinkGui::~SDRdaemonSinkGui()
{
	delete ui;
}

void SDRdaemonSinkGui::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void SDRdaemonSinkGui::destroy()
{
	delete this;
}

void SDRdaemonSinkGui::setName(const QString& name)
{
	setObjectName(name);
}

QString SDRdaemonSinkGui::getName() const
{
	return objectName();
}

void SDRdaemonSinkGui::resetToDefaults()
{
    blockApplySettings(true);
	m_settings.resetToDefaults();
	displaySettings();
	blockApplySettings(false);
	sendSettings();
}

qint64 SDRdaemonSinkGui::getCenterFrequency() const
{
	return m_settings.m_centerFrequency;
}

void SDRdaemonSinkGui::setCenterFrequency(qint64 centerFrequency)
{
    m_settings.m_centerFrequency = centerFrequency;
	displaySettings();
	sendControl();
	sendSettings();
}

QByteArray SDRdaemonSinkGui::serialize() const
{
	return m_settings.serialize();
}

bool SDRdaemonSinkGui::deserialize(const QByteArray& data)
{
    blockApplySettings(true);

	if(m_settings.deserialize(data))
	{
		displaySettings();
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

bool SDRdaemonSinkGui::handleMessage(const Message& message)
{
    if (SDRdaemonSinkOutput::MsgConfigureSDRdaemonSink::match(message))
    {
        const SDRdaemonSinkOutput::MsgConfigureSDRdaemonSink& cfg = (SDRdaemonSinkOutput::MsgConfigureSDRdaemonSink&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (SDRdaemonSinkOutput::MsgReportSDRdaemonSinkStreamTiming::match(message))
	{
		m_samplesCount = ((SDRdaemonSinkOutput::MsgReportSDRdaemonSinkStreamTiming&)message).getSamplesCount();
		updateWithStreamTime();
		return true;
	}
    else if (SDRdaemonSinkOutput::MsgStartStop::match(message))
    {
        SDRdaemonSinkOutput::MsgStartStop& notif = (SDRdaemonSinkOutput::MsgStartStop&) message;
        blockApplySettings(true);
        ui->startStop->setChecked(notif.getStartStop());
        blockApplySettings(false);
        return true;
    }
	else
	{
		return false;
	}
}

void SDRdaemonSinkGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        qDebug("SDRdaemonSinkGui::handleInputMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            qDebug("SDRdaemonSinkGui::handleInputMessages: DSPSignalNotification: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            updateSampleRateAndFrequency();

            delete message;
        }
        else
        {
            if (handleMessage(*message)) {
                delete message;
            }
        }
    }
}

void SDRdaemonSinkGui::updateSampleRateAndFrequency()
{
    m_deviceUISet->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceUISet->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)(m_sampleRate*(1<<m_settings.m_log2Interp)) / 1000));
}

void SDRdaemonSinkGui::updateTxDelayTooltip()
{
    double delay = ((127*127*m_settings.m_txDelay) / m_settings.m_sampleRate)/(128 + m_settings.m_nbFECBlocks);
    ui->txDelayText->setToolTip(tr("%1 us").arg(QString::number(delay*1e6, 'f', 0)));
}

void SDRdaemonSinkGui::displaySettings()
{
    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
    ui->sampleRate->setValue(m_settings.m_sampleRate);
    ui->deviceRateText->setText(tr("%1k").arg((float)(m_sampleRate*(1<<m_settings.m_log2Interp)) / 1000));
    ui->interp->setCurrentIndex(m_settings.m_log2Interp);
    ui->txDelay->setValue(m_settings.m_txDelay*100);
    ui->txDelayText->setText(tr("%1").arg(m_settings.m_txDelay*100));
    ui->nbFECBlocks->setValue(m_settings.m_nbFECBlocks);

    QString s0 = QString::number(128 + m_settings.m_nbFECBlocks, 'f', 0);
    QString s1 = QString::number(m_settings.m_nbFECBlocks, 'f', 0);
    ui->nominalNbBlocksText->setText(tr("%1/%2").arg(s0).arg(s1));

    ui->address->setText(m_settings.m_address);
    ui->dataPort->setText(tr("%1").arg(m_settings.m_dataPort));
    ui->controlPort->setText(tr("%1").arg(m_settings.m_controlPort));
    ui->specificParms->setText(m_settings.m_specificParameters);
}

void SDRdaemonSinkGui::sendControl(bool force)
{
    if ((m_settings.m_address != m_controlSettings.m_address) ||
        (m_settings.m_controlPort != m_controlSettings.m_controlPort) || force)
    {
        int rc = nn_shutdown(m_nnSender, 0);

        if (rc < 0) {
            qDebug() << "SDRdaemonSinkGui::sendControl: disconnection failed";
        } else {
            qDebug() << "SDRdaemonSinkGui::sendControl: disconnection successful";
        }

        std::ostringstream os;
        os << "tcp://" << m_settings.m_address.toStdString() << ":" << m_settings.m_controlPort;
        std::string addrstrng = os.str();
        rc = nn_connect(m_nnSender, addrstrng.c_str());

        if (rc < 0)
        {
            qDebug() << "SDRdaemonSinkGui::sendControl: connection to " << addrstrng.c_str() << " failed";
            QMessageBox::information(this, tr("Message"), tr("Cannot connect to remote control port"));
            return;
        }
        else
        {
            qDebug() << "SDRdaemonSinkGui::sendControl: connection to " << addrstrng.c_str() << " successful";
            force = true;
        }
    }

    std::ostringstream os;
    int nbArgs = 0;

    if ((m_settings.m_centerFrequency != m_controlSettings.m_centerFrequency) || force)
    {
        os << "freq=" << m_settings.m_centerFrequency;
        nbArgs++;
    }

    if ((m_settings.m_sampleRate != m_controlSettings.m_sampleRate) || (m_settings.m_log2Interp != m_controlSettings.m_log2Interp) || force)
    {
        if (nbArgs > 0) os << ",";
        os << "srate=" << m_settings.m_sampleRate * (1<<m_settings.m_log2Interp);
        nbArgs++;

        if ((m_settings.m_log2Interp != m_controlSettings.m_log2Interp) || force)
        {
            os << ",interp=" << m_settings.m_log2Interp;
            nbArgs++;
        }
    }

    if ((m_settings.m_specificParameters != m_controlSettings.m_specificParameters) || force)
    {
        if (nbArgs > 0) os << ",";
        os << m_settings.m_specificParameters.toStdString();
        nbArgs++;
    }

    if (nbArgs > 0)
    {
        int config_size = os.str().size();
        int rc = nn_send(m_nnSender, (void *) os.str().c_str(), config_size, 0);

        if (rc != config_size)
        {
            //QMessageBox::information(this, tr("Message"), tr("Cannot send message to remote control port"));
            qDebug() << "SDRdaemonSinkGui::sendControl: Cannot send message to remote control port."
                << " remoteAddress: " << m_settings.m_address
                << " remotePort: " << m_settings.m_controlPort
                << " message: " << os.str().c_str();
        }
        else
        {
            qDebug() << "SDRdaemonSinkGui::sendControl:"
                << "remoteAddress:" << m_settings.m_address
                << "remotePort:" << m_settings.m_controlPort
                << "message:" << os.str().c_str();
        }
    }

    m_controlSettings.m_address = m_settings.m_address;
    m_controlSettings.m_controlPort = m_settings.m_controlPort;
    m_controlSettings.m_centerFrequency = m_settings.m_centerFrequency;
    m_controlSettings.m_sampleRate = m_settings.m_sampleRate;
    m_controlSettings.m_log2Interp = m_settings.m_log2Interp;
    m_controlSettings.m_specificParameters = m_settings.m_specificParameters;
}

void SDRdaemonSinkGui::sendSettings()
{
    if(!m_updateTimer.isActive())
        m_updateTimer.start(100);
}


void SDRdaemonSinkGui::updateHardware()
{
    qDebug() << "SDRdaemonSinkGui::updateHardware";
    SDRdaemonSinkOutput::MsgConfigureSDRdaemonSink* message = SDRdaemonSinkOutput::MsgConfigureSDRdaemonSink::create(m_settings, m_forceSettings);
    m_deviceSampleSink->getInputMessageQueue()->push(message);
    m_forceSettings = false;
    m_updateTimer.stop();
}

void SDRdaemonSinkGui::updateStatus()
{
    int state = m_deviceUISet->m_deviceSinkAPI->state();

    if(m_lastEngineState != state)
    {
        switch(state)
        {
            case DSPDeviceSinkEngine::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case DSPDeviceSinkEngine::StIdle:
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case DSPDeviceSinkEngine::StRunning:
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case DSPDeviceSinkEngine::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceSinkAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastEngineState = state;
    }
}

void SDRdaemonSinkGui::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    sendControl();
    sendSettings();
}

void SDRdaemonSinkGui::on_sampleRate_changed(quint64 value)
{
    m_settings.m_sampleRate = value;
    updateTxDelayTooltip();
    sendControl();
    sendSettings();
}

void SDRdaemonSinkGui::on_interp_currentIndexChanged(int index)
{
    if (index < 0) {
        return;
    }

    m_settings.m_log2Interp = index;
    updateSampleRateAndFrequency();
    sendControl();
}

void SDRdaemonSinkGui::on_txDelay_valueChanged(int value)
{
    m_settings.m_txDelay = value / 100.0;
    ui->txDelayText->setText(tr("%1").arg(value));
    updateTxDelayTooltip();
    sendSettings();
}

void SDRdaemonSinkGui::on_nbFECBlocks_valueChanged(int value)
{
    m_settings.m_nbFECBlocks = value;
    int nbOriginalBlocks = 128;
    int nbFECBlocks = value;
    QString s = QString::number(nbOriginalBlocks + nbFECBlocks, 'f', 0);
    QString s1 = QString::number(nbFECBlocks, 'f', 0);
    ui->nominalNbBlocksText->setText(tr("%1/%2").arg(s).arg(s1));
    updateTxDelayTooltip();
    sendSettings();
}

void SDRdaemonSinkGui::on_address_returnPressed()
{
    m_settings.m_address = ui->address->text();
    sendControl();
    sendSettings();
}

void SDRdaemonSinkGui::on_dataPort_returnPressed()
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

    sendSettings();
}

void SDRdaemonSinkGui::on_controlPort_returnPressed()
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

void SDRdaemonSinkGui::on_specificParms_returnPressed()
{
    m_settings.m_specificParameters = ui->specificParms->text();
    sendControl();
}

void SDRdaemonSinkGui::on_applyButton_clicked(bool checked __attribute__((unused)))
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
}

void SDRdaemonSinkGui::on_sendButton_clicked(bool checked __attribute__((unused)))
{
    sendControl(true);
}

void SDRdaemonSinkGui::on_startStop_toggled(bool checked)
{
    if (m_doApplySettings)
    {
        SDRdaemonSinkOutput::MsgStartStop *message = SDRdaemonSinkOutput::MsgStartStop::create(checked);
        m_deviceSampleSink->getInputMessageQueue()->push(message);
    }
}

void SDRdaemonSinkGui::on_eventCountsReset_clicked(bool checked __attribute__((unused)))
{
    m_countUnrecoverable = 0;
    m_countRecovered = 0;
    m_time.start();
    displayEventCounts();
    displayEventTimer();
}

void SDRdaemonSinkGui::displayEventCounts()
{
    QString nstr = QString("%1").arg(m_countUnrecoverable, 3, 10, QChar('0'));
    ui->eventUnrecText->setText(nstr);
    nstr = QString("%1").arg(m_countRecovered, 3, 10, QChar('0'));
    ui->eventRecText->setText(nstr);
}

void SDRdaemonSinkGui::displayEventTimer()
{
    int elapsedTimeMillis = m_time.elapsed();
    QTime recordLength(0, 0, 0, 0);
    recordLength = recordLength.addSecs(elapsedTimeMillis/1000);
    QString s_time = recordLength.toString("hh:mm:ss");
    ui->eventCountsTimeText->setText(s_time);
}

void SDRdaemonSinkGui::updateWithStreamTime()
{
	int t_sec = 0;
	int t_msec = 0;

	if (m_settings.m_sampleRate > 0){
		t_msec = ((m_samplesCount * 1000) / m_settings.m_sampleRate) % 1000;
		t_sec = m_samplesCount / m_settings.m_sampleRate;
	}

	QTime t(0, 0, 0, 0);
	t = t.addSecs(t_sec);
	t = t.addMSecs(t_msec);
	QString s_timems = t.toString("hh:mm:ss.zzz");
	//ui->relTimeText->setText(s_timems); TODO with absolute time
}

void SDRdaemonSinkGui::tick()
{
	if ((++m_tickCount & 0xf) == 0) // 16*50ms ~800ms
	{
	    void *msgBuf = 0;

		SDRdaemonSinkOutput::MsgConfigureSDRdaemonSinkStreamTiming* message = SDRdaemonSinkOutput::MsgConfigureSDRdaemonSinkStreamTiming::create();
		m_deviceSampleSink->getInputMessageQueue()->push(message);

		int len = nn_recv(m_nnSender, &msgBuf, NN_MSG, NN_DONTWAIT);

        if ((len > 0) && msgBuf)
        {
            std::string msg((char *) msgBuf, len);
            std::vector<std::string> strs;
            boost::split(strs, msg, boost::is_any_of(":"));
            unsigned int nbTokens = strs.size();
            unsigned int status = 0;
            bool updateEventCounts = false;

            if (nbTokens > 0) // at least the queue length is given
            {
                try
                {
                    int queueLength = boost::lexical_cast<int>(strs[0]);
                    ui->queueLengthText->setText(QString::fromStdString(strs[0]));
                    m_nbSinceLastFlowCheck++;
                    int samplesCorr = 0;
                    bool quickStart = false;

                    if (queueLength < 2)
                    {
                        samplesCorr = 127*8;
                        quickStart = true;
                    }
                    else if (queueLength < 8)
                    {
                        samplesCorr = ((8 - queueLength)*16)/m_nbSinceLastFlowCheck;
                    }
                    else if (queueLength > 8)
                    {
                        samplesCorr = ((8 - queueLength)*16)/m_nbSinceLastFlowCheck;
                    }
                    else if (queueLength > 16)
                    {
                        samplesCorr = -127*16;
                        quickStart = true;
                    }

                    if (samplesCorr != 0)
                    {
                        samplesCorr = quickStart ? samplesCorr : samplesCorr <= -50 ? -50 : samplesCorr >= 50 ? 50 : samplesCorr;
                        SDRdaemonSinkOutput::MsgConfigureSDRdaemonSinkChunkCorrection* message = SDRdaemonSinkOutput::MsgConfigureSDRdaemonSinkChunkCorrection::create(samplesCorr);
                        m_deviceSampleSink->getInputMessageQueue()->push(message);
                        m_nbSinceLastFlowCheck = 0;
                    }
                }
                catch(const boost::bad_lexical_cast &)
                {
                    qDebug("SDRdaemonSinkGui::tick: queue length invalid: %s", strs[0].c_str());
                }
            }

            if (nbTokens > 1) // the quality status is given also
            {
                if (strs[1] == "2")
                {
                    status = 2;
                }
                else if (strs[1] == "1")
                {
                    status = 1;
                    if (m_countUnrecoverable < 999) m_countUnrecoverable++;
                    updateEventCounts = true;
                    qDebug("SDRdaemonSinkGui::tick: %s", msg.c_str());
                }
                else
                {
                    if (m_countRecovered < 999) m_countRecovered++;
                    updateEventCounts = true;
                    qDebug("SDRdaemonSinkGui::tick: %s", msg.c_str());
                }
            }

            if (nbTokens > 2) // the quality indicator message is given also
            {
                ui->qualityStatusText->setText(QString::fromStdString(strs[2]));
            }

            if (status == 2) { // all OK
                ui->allFramesDecoded->setStyleSheet("QToolButton { background-color : green; }");
            } else if (status == 1) { // unrecoverable errors
                ui->allFramesDecoded->setStyleSheet("QToolButton { background-color : red; }");
            } else { // recoverable errors or unknown status
                ui->allFramesDecoded->setStyleSheet("QToolButton { background:rgb(56,56,56); }");
            }

            if (updateEventCounts)
            {
                displayEventCounts();
            }
        }

        displayEventTimer();
	}
}
