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

#include "limesdrinputgui.h"

#include <QDebug>
#include <QMessageBox>

#include "ui_limesdrinputgui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/devicesourceapi.h"
#include "dsp/filerecord.h"

LimeSDRInputGUI::LimeSDRInputGUI(DeviceSourceAPI *deviceAPI, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::LimeSDRInputGUI),
    m_deviceAPI(deviceAPI),
    m_settings(),
    m_sampleSource(0),
    m_sampleRate(0),
    m_lastEngineState((DSPDeviceSourceEngine::State)-1),
    m_doApplySettings(true),
    m_statusCounter(0)
{
    m_limeSDRInput = new LimeSDRInput(m_deviceAPI);
    m_sampleSource = (DeviceSampleSource *) m_limeSDRInput;
    m_deviceAPI->setSource(m_sampleSource);

    ui->setupUi(this);

    float minF, maxF, stepF;

    m_limeSDRInput->getLORange(minF, maxF, stepF);
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->centerFrequency->setValueRange(7, ((uint32_t) minF)/1000, ((uint32_t) maxF)/1000); // frequency dial is in kHz

    m_limeSDRInput->getSRRange(minF, maxF, stepF);
    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(8, (uint32_t) minF, (uint32_t) maxF);

    m_limeSDRInput->getLPRange(minF, maxF, stepF);
    ui->lpf->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->lpf->setValueRange(6, (minF/1000)+1, maxF/1000);

    ui->lpFIR->setColorMapper(ColorMapper(ColorMapper::GrayYellow));
    ui->lpFIR->setValueRange(5, 1U, 56000U);

    ui->ncoFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));

    ui->channelNumberText->setText(tr("#%1").arg(m_limeSDRInput->getChannelIndex()));

    ui->hwDecimLabel->setText(QString::fromUtf8("H\u2193"));
    ui->swDecimLabel->setText(QString::fromUtf8("S\u2193"));

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    displaySettings();

    char recFileNameCStr[30];
    sprintf(recFileNameCStr, "test_%d.sdriq", m_deviceAPI->getDeviceUID());
    m_fileSink = new FileRecord(std::string(recFileNameCStr));
    m_deviceAPI->addSink(m_fileSink);

    connect(m_deviceAPI->getDeviceOutputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleMessagesToGUI()), Qt::QueuedConnection);
}

LimeSDRInputGUI::~LimeSDRInputGUI()
{
    m_deviceAPI->removeSink(m_fileSink);
    delete m_fileSink;
    delete m_sampleSource; // Valgrind memcheck
    delete ui;
}

void LimeSDRInputGUI::destroy()
{
    delete this;
}

void LimeSDRInputGUI::setName(const QString& name)
{
    setObjectName(name);
}

QString LimeSDRInputGUI::getName() const
{
    return objectName();
}

void LimeSDRInputGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    sendSettings();
}

qint64 LimeSDRInputGUI::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void LimeSDRInputGUI::setCenterFrequency(qint64 centerFrequency)
{
    m_settings.m_centerFrequency = centerFrequency;
    displaySettings();
    sendSettings();
}

QByteArray LimeSDRInputGUI::serialize() const
{
    return m_settings.serialize();
}

bool LimeSDRInputGUI::deserialize(const QByteArray& data)
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

bool LimeSDRInputGUI::handleMessage(const Message& message __attribute__((unused))) // TODO: does not seem to be really useful in any of the source (+sink?) plugins
{
    return false;
}

void LimeSDRInputGUI::handleMessagesToGUI()
{
    Message* message;

    while ((message = m_deviceAPI->getDeviceOutputMessageQueue()->pop()) != 0)
    {
        if (DSPSignalNotification::match(*message))
        {
            qDebug("LimeSDRInputGUI::handleMessagesToGUI: message: %s", message->getIdentifier());
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("LimeSDRInputGUI::handleMessagesToGUI: SampleRate: %d, CenterFrequency: %llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();
            m_fileSink->handleMessage(*notif); // forward to file sink

            delete message;
        }
        else if (LimeSDRInput::MsgReportLimeSDRToGUI::match(*message))
        {
            qDebug("LimeSDRInputGUI::handleMessagesToGUI: message: %s", message->getIdentifier());
            LimeSDRInput::MsgReportLimeSDRToGUI *report = (LimeSDRInput::MsgReportLimeSDRToGUI *) message;

            m_settings.m_centerFrequency = report->getCenterFrequency();
            m_settings.m_devSampleRate = report->getSampleRate();
            m_settings.m_log2HardDecim = report->getLog2HardDecim();

            blockApplySettings(true);
            displaySettings();
            blockApplySettings(false);

            LimeSDRInput::MsgSetReferenceConfig* conf = LimeSDRInput::MsgSetReferenceConfig::create(m_settings);
            m_sampleSource->getInputMessageQueue()->push(conf);

            delete message;
        }
        else if (DeviceLimeSDRShared::MsgCrossReportToGUI::match(*message))
        {
            DeviceLimeSDRShared::MsgCrossReportToGUI *report = (DeviceLimeSDRShared::MsgCrossReportToGUI *) message;
            m_settings.m_devSampleRate = report->getSampleRate();

            blockApplySettings(true);
            displaySettings();
            blockApplySettings(false);

            LimeSDRInput::MsgSetReferenceConfig* conf = LimeSDRInput::MsgSetReferenceConfig::create(m_settings);
            m_sampleSource->getInputMessageQueue()->push(conf);

            delete message;
        }
        else if (LimeSDRInput::MsgReportStreamInfo::match(*message))
        {
            LimeSDRInput::MsgReportStreamInfo *report = (LimeSDRInput::MsgReportStreamInfo *) message;

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

void LimeSDRInputGUI::updateSampleRateAndFrequency()
{
    m_deviceAPI->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceAPI->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateLabel->setText(tr("%1k").arg(QString::number(m_sampleRate / 1000.0f, 'g', 5)));
}

void LimeSDRInputGUI::displaySettings()
{
    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);
    ui->sampleRate->setValue(m_settings.m_devSampleRate);

    ui->dcOffset->setChecked(m_settings.m_dcBlock);
    ui->iqImbalance->setChecked(m_settings.m_iqCorrection);

    ui->hwDecim->setCurrentIndex(m_settings.m_log2HardDecim);
    ui->swDecim->setCurrentIndex(m_settings.m_log2SoftDecim);

    ui->lpf->setValue(m_settings.m_lpfBW / 1000);

    ui->lpFIREnable->setChecked(m_settings.m_lpfFIREnable);
    ui->lpFIR->setValue(m_settings.m_lpfFIRBW / 1000);

    ui->gain->setValue(m_settings.m_gain);
    ui->gainText->setText(tr("%1dB").arg(m_settings.m_gain));

    ui->antenna->setCurrentIndex((int) m_settings.m_antennaPath);

    setNCODisplay();

    ui->ncoEnable->setChecked(m_settings.m_ncoEnable);
}

void LimeSDRInputGUI::setNCODisplay()
{
    int ncoHalfRange = (m_settings.m_devSampleRate * (1<<(m_settings.m_log2HardDecim)))/2;
    ui->ncoFrequency->setValueRange(7,
            (m_settings.m_centerFrequency - ncoHalfRange)/1000,
            (m_settings.m_centerFrequency + ncoHalfRange)/1000); // frequency dial is in kHz
    ui->ncoFrequency->setValue((m_settings.m_centerFrequency + m_settings.m_ncoFrequency)/1000);
}

void LimeSDRInputGUI::sendSettings()
{
    if(!m_updateTimer.isActive())
        m_updateTimer.start(100);
}

void LimeSDRInputGUI::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug() << "LimeSDRInputGUI::updateHardware";
        LimeSDRInput::MsgConfigureLimeSDR* message = LimeSDRInput::MsgConfigureLimeSDR::create(m_settings);
        m_sampleSource->getInputMessageQueue()->push(message);
        m_updateTimer.stop();
    }
}

void LimeSDRInputGUI::updateStatus()
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

    if (m_statusCounter < 1)
    {
        m_statusCounter++;
    }
    else
    {
        LimeSDRInput::MsgGetStreamInfo* message = LimeSDRInput::MsgGetStreamInfo::create();
        m_sampleSource->getInputMessageQueue()->push(message);
        m_statusCounter = 0;
    }
}

void LimeSDRInputGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void LimeSDRInputGUI::on_startStop_toggled(bool checked)
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

void LimeSDRInputGUI::on_record_toggled(bool checked)
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

void LimeSDRInputGUI::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    setNCODisplay();
    sendSettings();
}

void LimeSDRInputGUI::on_ncoFrequency_changed(quint64 value)
{
    m_settings.m_ncoFrequency = (int64_t) value - (int64_t) m_settings.m_centerFrequency/1000;
    m_settings.m_ncoFrequency *= 1000;
    sendSettings();
}

void LimeSDRInputGUI::on_ncoEnable_toggled(bool checked)
{
    m_settings.m_ncoEnable = checked;
    sendSettings();
}

void LimeSDRInputGUI::on_ncoReset_clicked(bool checked __attribute__((unused)))
{
    m_settings.m_ncoFrequency = 0;
    ui->ncoFrequency->setValue(m_settings.m_centerFrequency/1000);
    sendSettings();
}

void LimeSDRInputGUI::on_dcOffset_toggled(bool checked)
{
    m_settings.m_dcBlock = checked;
    sendSettings();
}

void LimeSDRInputGUI::on_iqImbalance_toggled(bool checked)
{
    m_settings.m_iqCorrection = checked;
    sendSettings();
}

void LimeSDRInputGUI::on_sampleRate_changed(quint64 value)
{
    m_settings.m_devSampleRate = value;
    setNCODisplay();
    sendSettings();}

void LimeSDRInputGUI::on_hwDecim_currentIndexChanged(int index)
{
    if ((index <0) || (index > 5))
        return;
    m_settings.m_log2HardDecim = index;
    setNCODisplay();
    sendSettings();
}

void LimeSDRInputGUI::on_swDecim_currentIndexChanged(int index)
{
    if ((index <0) || (index > 6))
        return;
    m_settings.m_log2SoftDecim = index;
    sendSettings();
}

void LimeSDRInputGUI::on_lpf_changed(quint64 value)
{
    m_settings.m_lpfBW = value * 1000;
    sendSettings();
}

void LimeSDRInputGUI::on_lpFIREnable_toggled(bool checked)
{
    m_settings.m_lpfFIREnable = checked;
    sendSettings();
}

void LimeSDRInputGUI::on_lpFIR_changed(quint64 value)
{
    m_settings.m_lpfFIRBW = value * 1000;

    if (m_settings.m_lpfFIREnable) { // do not send the update if the FIR is disabled
        sendSettings();
    }
}

void LimeSDRInputGUI::on_gain_valueChanged(int value)
{
    m_settings.m_gain = value;
    ui->gainText->setText(tr("%1dB").arg(m_settings.m_gain));
    sendSettings();
}

void LimeSDRInputGUI::on_antenna_currentIndexChanged(int index)
{
    m_settings.m_antennaPath = (LimeSDRInputSettings::PathRFE) index;
    sendSettings();
}
