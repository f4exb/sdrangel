///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2025 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QMessageBox>

#include "spectranmisogui.h"
#include "ui_spectranmisogui.h"
#include "device/deviceuiset.h"
#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "gui/glspectrum.h"

SpectranMISOGui::SpectranMISOGui(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::SpectranMISOGui),
    m_settings(),
    m_settingsKeys(),
    m_doApplySettings(true),
    m_forceSettings(true),
    m_sampleMIMO(nullptr),
    m_lastEngineState(DeviceAPI::StNotStarted)
{
    qDebug("SpectranMISOGui::SpectranMISOGui");
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_sampleMIMO = (SpectranMISO*) m_deviceUISet->m_deviceAPI->getSampleMIMO();

    ui->setupUi(getContents());
    sizeToContents();
    getContents()->setStyleSheet("#SpectranMISOGui { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplemimo/spectranmiso/readme.md";
    ui->rxCenterFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->rxCenterFrequency->setValueRange(8, 9, 180000000); // 9 kHz to 18 GHz
    ui->txCenterFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->txCenterFrequency->setValueRange(8, 9, 180000000); // 9 kHz to 18 GHz
    ui->sampleRate->setColorMapper(ColorMapper(ColorMapper::GrayGreenYellow));
    ui->sampleRate->setValueRange(9, 12000, 100000000); // 12 kHz to 100 MHz

    // Disable unsupported clock rates for the moment
    ui->clockrateIndex->model()->setData(ui->clockrateIndex->model()->index(3, 0), 0, Qt::UserRole - 1); // disables 46 MHz
    ui->clockrateIndex->model()->setData(ui->clockrateIndex->model()->index(4, 0), 0, Qt::UserRole - 1); // disables 46 MHz
    ui->clockrateIndex->model()->setData(ui->clockrateIndex->model()->index(5, 0), 0, Qt::UserRole - 1); // disables 61 MHz
    ui->clockrateIndex->model()->setData(ui->clockrateIndex->model()->index(6, 0), 0, Qt::UserRole - 1); // disables 76 MHz

    if (m_sampleMIMO->getSpectranModel() == SpectranModel::SPECTRAN_V6Eco)
    {
        // V6 Eco only supports 61 MHz clock rate (61.44 actually)
        ui->clockrateIndex->setCurrentIndex(4);
        ui->clockrateIndex->setEnabled(false);
    }
    else
    {
        ui->clockrateIndex->setEnabled(true);
    }

    // Disable certain modes for the moment
    ui->mode->model()->setData(ui->mode->model()->index(1, 0), 0, Qt::UserRole - 1); // disables the item
    ui->mode->model()->setData(ui->mode->model()->index(2, 0), 0, Qt::UserRole - 1); // disables the item
    ui->mode->model()->setData(ui->mode->model()->index(3, 0), 0, Qt::UserRole - 1); // disables the item
    ui->mode->model()->setData(ui->mode->model()->index(5, 0), 0, Qt::UserRole - 1); // disables the item

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    displaySettings();

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleMIMO->setMessageQueueToGUI(&m_inputMessageQueue);

    makeUIConnections();
}

SpectranMISOGui::~SpectranMISOGui()
{
    qDebug("SpectranMISOGui::~SpectranMISOGui");
    delete ui;
}

void SpectranMISOGui::destroy()
{
    qDebug("SpectranMISOGui::destroy");
    delete this;
}

void SpectranMISOGui::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    m_forceSettings = true;
    sendSettings();
}

QByteArray SpectranMISOGui::serialize() const
{
    return m_settings.serialize();
}

bool SpectranMISOGui::deserialize(const QByteArray& data)
{
    qDebug("SpectranMISOGui::deserialize: data size=%d", data.size());
    if (m_settings.deserialize(data))
    {
        qDebug("SpectranMISOGui::deserialize: m_settings=%s", m_settings.getDebugString(QStringList(), true).toStdString().c_str());
        displaySettings();
        m_forceSettings = true;
        sendSettings();
        return true;
    }
    else
    {
        qDebug("SpectranMISOGui::deserialize: failed to deserialize data");
        resetToDefaults();
        return false;
    }
}

void SpectranMISOGui::displaySettings()
{
    qDebug("SpectranMISOGui::displaySettings");
    blockApplySettings(true);

    ui->rxCenterFrequency->setValue(m_settings.m_rxCenterFrequency / 1000); // Display in kHz
    ui->txCenterFrequency->setValue(m_settings.m_txCenterFrequency / 1000); // Display in kHz
    ui->sampleRate->setEnabled(true);
    ui->sampleRate->setValue(m_settings.m_sampleRate);
    ui->sampleRate->setEnabled(!SpectranMISO::isRawMode(m_settings.m_mode));
    ui->deviceRateText->setText(tr("%1k").arg(QString::number(SpectranMISO::getSampleRate(m_settings) / 1000.0f, 'g', 5)));
    ui->streamIndex->setCurrentIndex(m_settings.m_streamIndex);
    ui->spectrumSource->setCurrentIndex(m_settings.m_spectrumStreamIndex);
    ui->streamLock->setChecked(m_settings.m_streamLock);
    ui->mode->setCurrentIndex(m_settings.m_mode);
    ui->clockrateIndex->setCurrentIndex(m_settings.m_clockRate);
    ui->log2Decim->setCurrentIndex(m_settings.m_logDecimation);
    ui->rxChannel->setCurrentIndex(m_settings.m_rxChannel);

    blockApplySettings(false);
}

void SpectranMISOGui::sendSettings()
{
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void SpectranMISOGui::updateHardware()
{
    if (m_doApplySettings)
    {
        qDebug("SpectranMISOGui::updateHardware: send settings to SpectranMISO");
        MsgConfigureSpectranMISO* message = MsgConfigureSpectranMISO::create(m_settings, m_settingsKeys, m_forceSettings);
        m_sampleMIMO->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_settingsKeys.clear();
        m_updateTimer.stop();
    }
}

void SpectranMISOGui::updateStatus()
{
    int state = m_deviceUISet->m_deviceAPI->state();

    if (m_lastEngineState != state)
    {
        switch(state)
        {
            case DeviceAPI::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case DeviceAPI::StIdle:
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case DeviceAPI::StRunning:
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case DeviceAPI::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_deviceUISet->m_deviceAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastEngineState = state;
    }
}

bool SpectranMISOGui::handleMessage(const Message& message)
{
    if (MsgConfigureSpectranMISO::match(message))
    {
        const MsgConfigureSpectranMISO& notif = (const MsgConfigureSpectranMISO&) message;

        if (notif.getForce()) {
            m_settings = notif.getSettings();
        } else {
            m_settings.applySettings(notif.getSettingsKeys(), notif.getSettings());
        }

        displaySettings();

        return true;
    }
    else if (SpectranMISO::MsgStartStop::match(message))
    {
        const SpectranMISO::MsgStartStop& notif = (const SpectranMISO::MsgStartStop&) message;
        blockApplySettings(true);
        ui->startStop->setChecked(notif.getStartStop());
        blockApplySettings(false);
    }
    else if (DSPMIMOSignalNotification::match(message))
    {
        qDebug("SpectranMISOGui::handleMessage: DSPMIMOSignalNotification received");
        const DSPMIMOSignalNotification& notif = (const DSPMIMOSignalNotification&) message;
        bool sourceOrSink = notif.getSourceOrSink();
        int sampleRate = notif.getSampleRate();
        blockApplySettings(true);

        if (sourceOrSink)
        {
            m_settings.m_rxCenterFrequency = notif.getCenterFrequency();
            ui->rxCenterFrequency->setValue(notif.getCenterFrequency() / 1000); // Display in kHz

            if (isSpectrumRx()) {
                m_deviceUISet->getSpectrum()->setCenterFrequency(m_settings.m_rxCenterFrequency);
            }
        }
        else
        {
            m_settings.m_txCenterFrequency = notif.getCenterFrequency();
            ui->txCenterFrequency->setValue(notif.getCenterFrequency() / 1000); // Display in kHz

            if (!isSpectrumRx()) {
                m_deviceUISet->getSpectrum()->setCenterFrequency(m_settings.m_txCenterFrequency);
            }
        }

        m_settings.m_sampleRate = sampleRate;
        ui->sampleRate->setValue(sampleRate);
        ui->deviceRateText->setText(tr("%1k").arg(QString::number(sampleRate / 1000.0f, 'g', 5)));
        m_deviceUISet->getSpectrum()->setSampleRate(sampleRate);

        blockApplySettings(false);
        return true;
    }

    return false; // Message not handled in this GUI
}

void SpectranMISOGui::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        } else {
            qDebug("SpectranMISOGui::handleInputMessages: unhandled message: %s", message->getIdentifier());
        }
    }
}

bool SpectranMISOGui::isSpectrumRx() const
{
    return ui->spectrumSource->currentIndex() < 2; // 0 = Rx1, 1 = Rx2, 2 = Tx
}

void SpectranMISOGui::on_startStop_toggled(bool checked)
{
    if (!m_doApplySettings) {
        return;
    }

    SpectranMISO::MsgStartStop* message = SpectranMISO::MsgStartStop::create(checked);
    m_sampleMIMO->getInputMessageQueue()->push(message);
}

void SpectranMISOGui::on_mode_currentIndexChanged(int index)
{
    if (!m_doApplySettings || index < 0 || index >= SPECTRANMISO_MODE_END) {
        return;
    }

    if (SpectranMISO::isRawMode((SpectranMISOMode) index)) {
        ui->sampleRate->setEnabled(false);
    } else {
        ui->sampleRate->setEnabled(true);
    }

    if (SpectranMISO::isRxModeSingle((SpectranMISOMode) index)) {
        ui->rxChannel->setEnabled(true);
    } else {
        ui->rxChannel->setEnabled(false);
    }

    m_settings.m_mode = (SpectranMISOMode) index;
    SpectranMISO::MsgChangeMode* message = SpectranMISO::MsgChangeMode::create(m_settings.m_mode, m_settings.m_rxChannel);
    m_sampleMIMO->getInputMessageQueue()->push(message);
}

void SpectranMISOGui::on_rxChannel_currentIndexChanged(int index)
{
    if (index < 0 || index >= SPECTRAN_RX_CHANNEL_END) {
        return;
    }

    m_settings.m_rxChannel = (SpectranRxChannel) index;
    SpectranMISO::MsgChangeMode* message = SpectranMISO::MsgChangeMode::create(m_settings.m_mode, m_settings.m_rxChannel);
    m_sampleMIMO->getInputMessageQueue()->push(message);
}

void SpectranMISOGui::on_sampleRate_valueChanged(qint64 value)
{
    m_settings.m_sampleRate = value;
    m_settingsKeys.append("sampleRate");
    m_doApplySettings = true;
    sendSettings();
}

void SpectranMISOGui::on_clockRate_currentIndexChanged(int index)
{
    if (index < 0 || index >= SPECTRANMISO_CLOCK_END) {
        return;
    }

    m_settings.m_clockRate = (SpectranMISOClockRate) index;
    m_settingsKeys.append("clockRate");
    m_doApplySettings = true;
    sendSettings();
}

void SpectranMISOGui::on_log2Decimation_currentIndexChanged(int index)
{
    qDebug("SpectranMISOGui::on_log2Decimation_currentIndexChanged: index=%d", index);

    if (index < 0 || index > 9) {
        return;
    }

    m_settings.m_logDecimation = index;
    m_settingsKeys.append("logDecimation");
    m_doApplySettings = true;
    sendSettings();
}

void SpectranMISOGui::on_rxCenterFrequency_valueChanged(qint64 value)
{
    m_settings.m_rxCenterFrequency = value * 1000; // Convert kHz to Hz
    m_settingsKeys.append("rxCenterFrequency");
    m_doApplySettings = true;
    sendSettings();
}

void SpectranMISOGui::on_txCenterFrequency_valueChanged(qint64 value)
{
    m_settings.m_txCenterFrequency = value * 1000; // Convert kHz to Hz
    m_settingsKeys.append("txCenterFrequency");
    m_doApplySettings = true;
    sendSettings();
}

void SpectranMISOGui::makeUIConnections()
{
    connect(ui->startStop, &ButtonSwitch::toggled, this, &SpectranMISOGui::on_startStop_toggled);
    QObject::connect(ui->mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpectranMISOGui::on_mode_currentIndexChanged);
    QObject::connect(ui->rxChannel, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpectranMISOGui::on_rxChannel_currentIndexChanged);
    connect(ui->rxCenterFrequency, &ValueDial::changed, this, &SpectranMISOGui::on_rxCenterFrequency_valueChanged);
    connect(ui->txCenterFrequency, &ValueDial::changed, this, &SpectranMISOGui::on_txCenterFrequency_valueChanged);
    QObject::connect(ui->clockrateIndex, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpectranMISOGui::on_clockRate_currentIndexChanged);
    QObject::connect(ui->log2Decim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpectranMISOGui::on_log2Decimation_currentIndexChanged);
    QObject::connect(ui->sampleRate, &ValueDial::changed, this, &SpectranMISOGui::on_sampleRate_valueChanged);
}
