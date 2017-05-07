///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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
#include <QMessageBox>

#include "ui_limesdroutputgui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/devicesinkapi.h"
#include "dsp/filerecord.h"
#include "limesdroutputgui.h"

LimeSDROutputGUI::LimeSDROutputGUI(DeviceSinkAPI *deviceAPI, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::LimeSDROutputGUI),
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_sampleSink(0),
    m_sampleRate(0),
    m_lastEngineState((DSPDeviceSinkEngine::State)-1),
    m_doApplySettings(true),
    m_statusCounter(0)
{
    m_limeSDROutput = new LimeSDROutput(m_deviceAPI);
    m_sampleSink = (DeviceSampleSink *) m_limeSDROutput;
    m_deviceAPI->setSink(m_sampleSink);

    ui->setupUi(this);

    float minF, maxF, stepF;

    m_limeSDROutput->getLORange(minF, maxF, stepF);
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));
    ui->centerFrequency->setValueRange(7, ((uint32_t) minF)/1000, ((uint32_t) maxF)/1000); // frequency dial is in kHz

    m_limeSDROutput->getSRRange(minF, maxF, stepF);
    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::ReverseGreenYellow));
    ui->sampleRate->setValueRange(8, (uint32_t) minF, (uint32_t) maxF);

    m_limeSDROutput->getLPRange(minF, maxF, stepF);
    ui->lpf->setColorMapper(ColorMapper(ColorMapper::ReverseGold));
    ui->lpf->setValueRange(6, (minF/1000)+1, maxF/1000);

    ui->lpFIR->setColorMapper(ColorMapper(ColorMapper::ReverseGold));
    ui->lpFIR->setValueRange(5, 1U, 56000U);

    ui->ncoFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));

    ui->channelNumberText->setText(tr("#%1").arg(m_limeSDROutput->getChannelIndex()));

    ui->hwInterpLabel->setText(QString::fromUtf8("H\u2191"));
    ui->swInterpLabel->setText(QString::fromUtf8("S\u2191"));

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    displaySettings();

    char recFileNameCStr[30];
    sprintf(recFileNameCStr, "test_%d.sdriq", m_deviceAPI->getDeviceUID());
    m_fileSink = new FileRecord(std::string(recFileNameCStr));

    connect(m_deviceAPI->getDeviceOutputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleMessagesToGUI()), Qt::QueuedConnection);
}

LimeSDROutputGUI::~LimeSDROutputGUI()
{
    delete m_fileSink;
    delete m_sampleSink; // Valgrind memcheck
    delete ui;
}

void LimeSDROutputGUI::destroy()
{
    delete this;
}

void LimeSDROutputGUI::setName(const QString& name)
{
    setObjectName(name);
}

QString LimeSDROutputGUI::getName() const
{
    return objectName();
}

void LimeSDROutputGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    sendSettings();
}

qint64 LimeSDROutputGUI::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void LimeSDROutputGUI::setCenterFrequency(qint64 centerFrequency)
{
    m_settings.m_centerFrequency = centerFrequency;
    displaySettings();
    sendSettings();
}

QByteArray LimeSDROutputGUI::serialize() const
{
    return m_settings.serialize();
}

bool LimeSDROutputGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        displaySettings();
        sendSettings();
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool LimeSDROutputGUI::handleMessage(const Message& message) // TODO: does not seem to be really useful in any of the source (+sink?) plugins
{
    return false;
}

void LimeSDROutputGUI::handleMessagesToGUI()
{
    Message* message;

    while ((message = m_deviceAPI->getDeviceOutputMessageQueue()->pop()) != 0)
    {
        if (DSPSignalNotification::match(*message))
        {
            qDebug("LimeSDROutputGUI::handleMessagesToGUI: message: %s", message->getIdentifier());
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("LimeSDROutputGUI::handleMessagesToGUI: SampleRate: %d, CenterFrequency: %llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();
            m_fileSink->handleMessage(*notif); // forward to file sink

            delete message;
        }
        else if (LimeSDROutput::MsgReportLimeSDRToGUI::match(*message))
        {
            qDebug("LimeSDROutputGUI::handleMessagesToGUI: message: %s", message->getIdentifier());
            LimeSDROutput::MsgReportLimeSDRToGUI *report = (LimeSDROutput::MsgReportLimeSDRToGUI *) message;

            m_settings.m_centerFrequency = report->getCenterFrequency();
            m_settings.m_devSampleRate = report->getSampleRate();
            m_settings.m_log2HardInterp = report->getLog2HardInterp();

            blockApplySettings(true);
            displaySettings();
            blockApplySettings(false);

            LimeSDROutput::MsgSetReferenceConfig* conf = LimeSDROutput::MsgSetReferenceConfig::create(m_settings);
            m_sampleSink->getInputMessageQueue()->push(conf);

            delete message;
        }
        else if (DeviceLimeSDRShared::MsgCrossReportToGUI::match(*message))
        {
            DeviceLimeSDRShared::MsgCrossReportToGUI *report = (DeviceLimeSDRShared::MsgCrossReportToGUI *) message;
            m_settings.m_devSampleRate = report->getSampleRate();

            blockApplySettings(true);
            displaySettings();
            blockApplySettings(false);

            LimeSDROutput::MsgSetReferenceConfig* conf = LimeSDROutput::MsgSetReferenceConfig::create(m_settings);
            m_sampleSink->getInputMessageQueue()->push(conf);

            delete message;
        }
        else if (LimeSDROutput::MsgReportStreamInfo::match(*message))
        {
            LimeSDROutput::MsgReportStreamInfo *report = (LimeSDROutput::MsgReportStreamInfo *) message;

            if (report->getSuccess())
            {
                ui->streamStatusLabel->setStyleSheet("QLabel { background-color : green; }");
                ui->streamLinkRateText->setText(tr("%1 MB/s").arg(QString::number(report->getLinkRate() / 1000000.0f, 'f', 3)));

                if (report->getUnderrun() > 0) {
                    ui->underrunLabel->setStyleSheet("QLabel { background-color : red; }");
                } else {
                    ui->underrunLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
                }

                if (report->getOverrun() > 0) {
                    ui->overrunLabel->setStyleSheet("QLabel { background-color : red; }");
                } else {
                    ui->overrunLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
                }

                if (report->getDroppedPackets() > 0) {
                    ui->droppedLabel->setStyleSheet("QLabel { background-color : red; }");
                } else {
                    ui->droppedLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
                }

                ui->fifoBar->setMaximum(report->getFifoSize());
                ui->fifoBar->setValue(report->getFifoFilledCount());
                ui->fifoBar->setToolTip(tr("FIFO fill %1/%2 samples").arg(QString::number(report->getFifoFilledCount())).arg(QString::number(report->getFifoSize())));
            }
            else
            {
                ui->streamStatusLabel->setStyleSheet("QLabel { background:rgb(79,79,79); }");
            }
        }
    }
}

void LimeSDROutputGUI::updateSampleRateAndFrequency()
{
    m_deviceAPI->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceAPI->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateLabel->setText(tr("%1k").arg(QString::number(m_sampleRate / 1000.0f, 'g', 5)));
}

void LimeSDROutputGUI::displaySettings()
{
    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
    ui->sampleRate->setValue(m_settings.m_devSampleRate);

    ui->hwInterp->setCurrentIndex(m_settings.m_log2HardInterp);
    ui->swInterp->setCurrentIndex(m_settings.m_log2SoftInterp);

    ui->lpf->setValue(m_settings.m_lpfBW / 1000);

    ui->lpFIREnable->setChecked(m_settings.m_lpfFIREnable);
    ui->lpFIR->setValue(m_settings.m_lpfFIRBW / 1000);

    ui->gain->setValue(m_settings.m_gain);
    ui->gainText->setText(tr("%1dB").arg(m_settings.m_gain));

    ui->antenna->setCurrentIndex((int) m_settings.m_antennaPath);

    setNCODisplay();
}

void LimeSDROutputGUI::setNCODisplay()
{
    int ncoHalfRange = (m_settings.m_devSampleRate * (1<<(m_settings.m_log2HardInterp)))/2;
    ui->ncoFrequency->setValueRange(7,
            (m_settings.m_centerFrequency - ncoHalfRange)/1000,
            (m_settings.m_centerFrequency + ncoHalfRange)/1000); // frequency dial is in kHz
    ui->ncoFrequency->setValue((m_settings.m_centerFrequency + m_settings.m_ncoFrequency)/1000);
}

void LimeSDROutputGUI::sendSettings()
{
    if(!m_updateTimer.isActive())
        m_updateTimer.start(100);
}

void LimeSDROutputGUI::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "LimeSDROutputGUI::updateHardware";
        LimeSDROutput::MsgConfigureLimeSDR* message = LimeSDROutput::MsgConfigureLimeSDR::create(m_settings);
        m_sampleSink->getInputMessageQueue()->push(message);
        m_updateTimer.stop();
    }
}

void LimeSDROutputGUI::updateStatus()
{
    int state = m_deviceAPI->state();

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
                QMessageBox::information(this, tr("Message"), m_deviceAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastEngineState = state;
    }

    if (m_statusCounter < 1)
    {
        m_statusCounter++;
    }
    else
    {
        LimeSDROutput::MsgGetStreamInfo* message = LimeSDROutput::MsgGetStreamInfo::create();
        m_sampleSink->getInputMessageQueue()->push(message);
        m_statusCounter = 0;
    }
}

void LimeSDROutputGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void LimeSDROutputGUI::on_startStop_toggled(bool checked)
{
    if (checked)
    {
        if (m_deviceAPI->initGeneration())
        {
            m_deviceAPI->startGeneration();
            DSPEngine::instance()->startAudioInput();
        }
    }
    else
    {
        m_deviceAPI->stopGeneration();
        DSPEngine::instance()->stopAudioInput();
    }
}

void LimeSDROutputGUI::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    setNCODisplay();
    sendSettings();
}

void LimeSDROutputGUI::on_ncoFrequency_changed(quint64 value)
{
    m_settings.m_ncoFrequency = (int64_t) value - (int64_t) m_settings.m_centerFrequency/1000;
    m_settings.m_ncoFrequency *= 1000;
    sendSettings();
}

void LimeSDROutputGUI::on_ncoEnable_toggled(bool checked)
{
    m_settings.m_ncoEnable = checked;
    sendSettings();
}

void LimeSDROutputGUI::on_ncoReset_clicked(bool checked)
{
    m_settings.m_ncoFrequency = 0;
    ui->ncoFrequency->setValue(m_settings.m_centerFrequency/1000);
    sendSettings();
}

void LimeSDROutputGUI::on_sampleRate_changed(quint64 value)
{
    m_settings.m_devSampleRate = value;
    setNCODisplay();
    sendSettings();}

void LimeSDROutputGUI::on_hwInterp_currentIndexChanged(int index)
{
    if ((index <0) || (index > 5))
        return;
    m_settings.m_log2HardInterp = index;
    setNCODisplay();
    sendSettings();
}

void LimeSDROutputGUI::on_swInterp_currentIndexChanged(int index)
{
    if ((index <0) || (index > 5))
        return;
    m_settings.m_log2SoftInterp = index;
    sendSettings();
}

void LimeSDROutputGUI::on_lpf_changed(quint64 value)
{
    m_settings.m_lpfBW = value * 1000;
    sendSettings();
}

void LimeSDROutputGUI::on_lpFIREnable_toggled(bool checked)
{
    m_settings.m_lpfFIREnable = checked;
    sendSettings();
}

void LimeSDROutputGUI::on_lpFIR_changed(quint64 value)
{
    m_settings.m_lpfFIRBW = value * 1000;

    if (m_settings.m_lpfFIREnable) { // do not send the update if the FIR is disabled
        sendSettings();
    }
}

void LimeSDROutputGUI::on_gain_valueChanged(int value)
{
    m_settings.m_gain = value;
    ui->gainText->setText(tr("%1dB").arg(m_settings.m_gain));
    sendSettings();
}

void LimeSDROutputGUI::on_antenna_currentIndexChanged(int index)
{
    m_settings.m_antennaPath = (LimeSDROutputSettings::PathRFE) index;
    sendSettings();
}
