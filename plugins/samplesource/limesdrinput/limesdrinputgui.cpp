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
    m_lastEngineState((DSPDeviceSourceEngine::State)-1)
{
    m_limeSDRInput = new LimeSDRInput(m_deviceAPI);
    m_sampleSource = (DeviceSampleSource *) m_limeSDRInput;
    m_deviceAPI->setSource(m_sampleSource);

    ui->setupUi(this);

    float minF, maxF, stepF;

    m_limeSDRInput->getLORange(minF, maxF, stepF);
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));
    ui->centerFrequency->setValueRange(7, ((uint32_t) minF)/1000, ((uint32_t) maxF)/1000); // frequency dial is in kHz

    m_limeSDRInput->getSRRange(minF, maxF, stepF);
    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::ReverseGreenYellow));
    ui->sampleRate->setValueRange(8, (uint32_t) minF, (uint32_t) maxF);

    m_limeSDRInput->getLPRange(minF, maxF, stepF);
    int minLP = (int) (minF / stepF);
    int maxLP = (int) (maxF / stepF);
    int nbSteps = (int) ((maxF - minF) / stepF);
    ui->lpf->setMinimum(minLP);
    ui->lpf->setMaximum(maxLP);

    ui->channelNumberText->setText(tr("#%1").arg(m_limeSDRInput->getChannelIndex()));

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    displaySettings();

    char recFileNameCStr[30];
    sprintf(recFileNameCStr, "test_%d.sdriq", m_deviceAPI->getDeviceUID());
    m_fileSink = new FileRecord(std::string(recFileNameCStr));
    m_deviceAPI->addSink(m_fileSink);

    connect(m_deviceAPI->getDeviceOutputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleMessagesToUI()), Qt::QueuedConnection);
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
    if(m_settings.deserialize(data)) {
        displaySettings();
        sendSettings();
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

void LimeSDRInputGUI::handleMessagesToUI()
{
    Message* message;

    while ((message = m_deviceAPI->getDeviceOutputMessageQueue()->pop()) != 0)
    {
        qDebug("LimeSDRInputGUI::handleDSPMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("BladerfGui::handleDSPMessages: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();
            m_fileSink->handleMessage(*notif); // forward to file sink

            delete message;
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
    ui->fcPos->setCurrentIndex((int) m_settings.m_fcPos);

    ui->lpf->setValue(m_limeSDRInput->getLPIndex(m_settings.m_lpfFIRBW));
    ui->lpfText->setText(tr("%1k").arg(QString::number(m_settings.m_lpfFIRBW / 1000.0f, 'f', 0)));

    ui->lpFIREnable->setChecked(m_settings.m_lpfFIREnable);
    ui->sampleRate->setValue(m_settings.m_lpfFIRBW);

    ui->gain->setValue(m_settings.m_gain);
    ui->gainText->setText(tr("%1dB").arg(m_settings.m_gain));
}

void LimeSDRInputGUI::sendSettings()
{
    if(!m_updateTimer.isActive())
        m_updateTimer.start(100);
}

void LimeSDRInputGUI::updateHardware()
{
    qDebug() << "BladerfGui::updateHardware";
    LimeSDRInput::MsgConfigureLimeSDR* message = LimeSDRInput::MsgConfigureLimeSDR::create( m_settings);
    m_sampleSource->getInputMessageQueue()->push(message);
    m_updateTimer.stop();
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
}

