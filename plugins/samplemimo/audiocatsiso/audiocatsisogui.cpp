///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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

#include <QDebug>

#include <QTime>
#include <QDateTime>
#include <QString>
#include <QMessageBox>
#include <QFileDialog>

#include "plugin/pluginapi.h"
#include "device/deviceapi.h"
#include "device/deviceuiset.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "gui/dialpopup.h"
#include "gui/dialogpositioner.h"
#include "gui/basicdevicesettingsdialog.h"
#include "gui/audioselectdialog.h"
#include "dsp/dspengine.h"
#include "dsp/dspdevicemimoengine.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplesource.h"
#include "util/db.h"

#include "mainwindow.h"

#include "ui_audiocatsisogui.h"
#include "audiocatsisogui.h"
#include "audiocatsisocatdialog.h"

AudioCATSISOGUI::AudioCATSISOGUI(DeviceUISet *deviceUISet, QWidget* parent) :
    DeviceGUI(parent),
    ui(new Ui::AudioCATSISOGUI),
    m_settings(),
    m_rxElseTx(true),
    m_doApplySettings(true),
    m_forceSettings(true),
    m_sampleMIMO(nullptr),
    m_tickCount(0),
    m_lastEngineState(DeviceAPI::StNotStarted),
    m_lastCATStatus(AudioCATSISOSettings::MsgCATReportStatus::StatusNone)
{
    qDebug("AudioCATSISOGUI::AudioCATSISOGUI");
    m_deviceUISet = deviceUISet;
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_sampleMIMO = (AudioCATSISO*) m_deviceUISet->m_deviceAPI->getSampleMIMO();

    m_rxSampleRate = m_sampleMIMO->getSourceSampleRate(0);
    m_settings.m_rxCenterFrequency = m_sampleMIMO->getSourceCenterFrequency(0);
    m_settings.m_rxDeviceName = m_sampleMIMO->getInputDeviceName();
    m_txSampleRate = m_sampleMIMO->getSinkSampleRate(0);
    m_settings.m_txCenterFrequency = m_sampleMIMO->getSinkCenterFrequency(0);
    m_settings.m_txDeviceName = m_sampleMIMO->getOutputDeviceName();

    ui->setupUi(getContents());
    sizeToContents();
    getContents()->setStyleSheet("#AudioCATSISOGUI { background-color: rgb(64, 64, 64); }");
    m_helpURL = "plugins/samplemimo/metismiso/readme.md";
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->centerFrequency->setValueRange(9, 0, m_absMaxFreq);
    ui->catStatusIndicator->setStyleSheet("QLabel { background-color:gray; border-radius: 7px; }");
    ui->streamLock->setChecked(true);

    for (const auto& comPortName : m_sampleMIMO->getComPorts()) {
        ui->catDevice->addItem(comPortName);
    }

    for (const auto& rigName : m_sampleMIMO->getRigNames().keys()) {
        ui->catType->addItem(rigName);
    }

    displaySettings();
    updateTxEnable();

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_sampleMIMO->setMessageQueueToGUI(&m_inputMessageQueue);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(openDeviceSettingsDialog(const QPoint &)));

    makeUIConnections();
    DialPopup::addPopupsToChildDials(this);
}

AudioCATSISOGUI::~AudioCATSISOGUI()
{
    delete ui;
}

void AudioCATSISOGUI::destroy()
{
    delete this;
}

void AudioCATSISOGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    m_forceSettings = true;
    sendSettings();
}

void AudioCATSISOGUI::setCenterFrequency(qint64 centerFrequency)
{
    if (m_rxElseTx)
    {
        m_settings.m_rxCenterFrequency = centerFrequency;
        m_settingsKeys.append("rxCenterFrequency");
    }
    else
    {
        m_settings.m_txCenterFrequency = centerFrequency;
        m_settingsKeys.append("txCenterFrequency");
    }

    displaySettings();
    sendSettings();
}

QByteArray AudioCATSISOGUI::serialize() const
{
    return m_settings.serialize();
}

bool AudioCATSISOGUI::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data)) {
        displaySettings();
        m_forceSettings = true;
        sendSettings();
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

void AudioCATSISOGUI::on_startStop_toggled(bool checked)
{
    ui->txEnable->setEnabled(!checked);

    if (m_doApplySettings)
    {
        AudioCATSISO::MsgStartStop *message = AudioCATSISO::MsgStartStop::create(checked);
        m_sampleMIMO->getInputMessageQueue()->push(message);
    }
}

void AudioCATSISOGUI::on_ptt_toggled(bool checked)
{
    if (m_settings.m_pttSpectrumLink) {
        ui->streamSide->setCurrentIndex(checked ? 1 : 0);
    }

    AudioCATSISOSettings::MsgPTT *msg = AudioCATSISOSettings::MsgPTT::create(checked);
    m_sampleMIMO->getInputMessageQueue()->push(msg);
}

void AudioCATSISOGUI::on_pttSpectrumLinkToggled(bool checked)
{
    m_settings.m_pttSpectrumLink = checked;
    m_settingsKeys.append("pttSpectrumLink");
    sendSettings();
}

void AudioCATSISOGUI::on_catConnect_toggled(bool checked)
{
    AudioCATSISOSettings::MsgCATConnect *msg = AudioCATSISOSettings::MsgCATConnect::create(checked);
    m_sampleMIMO->getInputMessageQueue()->push(msg);
}

void AudioCATSISOGUI::on_streamSide_currentIndexChanged(int index)
{
    qDebug("AudioCATSISOGUI::on_streamSide_currentIndexChanged: %d", index);
    m_rxElseTx = index == 0;

    if (ui->streamLock->isChecked())
    {
        if (index == 0)
        {
            m_deviceUISet->m_spectrum->setDisplayedStream(true, index);
            m_deviceUISet->m_deviceAPI->setSpectrumSinkInput(true, 0);
            m_deviceUISet->setSpectrumScalingFactor(SDR_RX_SCALEF);
        }
        else
        {
            m_deviceUISet->m_spectrum->setDisplayedStream(false, 0);
            m_deviceUISet->m_deviceAPI->setSpectrumSinkInput(false, 0);
            m_deviceUISet->setSpectrumScalingFactor(SDR_TX_SCALEF);
        }

        updateSpectrum(m_rxElseTx);

        ui->spectrumSide->blockSignals(true);
        ui->spectrumSide->setCurrentIndex(index);
        ui->spectrumSide->blockSignals(false);
    }

    displayDecim();
    displayFrequency();
    displaySampleRate();
}

void AudioCATSISOGUI::on_spectrumSide_currentIndexChanged(int index)
{
    if (index == 0)
    {
        m_deviceUISet->m_spectrum->setDisplayedStream(true, index);
        m_deviceUISet->m_deviceAPI->setSpectrumSinkInput(true, 0);
        m_deviceUISet->setSpectrumScalingFactor(SDR_RX_SCALEF);
    }
    else
    {
        m_deviceUISet->m_spectrum->setDisplayedStream(false, 0);
        m_deviceUISet->m_deviceAPI->setSpectrumSinkInput(false, 0);
        m_deviceUISet->setSpectrumScalingFactor(SDR_TX_SCALEF);
    }

    updateSpectrum(index == 0);

    if (ui->streamLock->isChecked())
    {
        ui->streamSide->blockSignals(true);
        ui->streamSide->setCurrentIndex(index);
        ui->streamSide->blockSignals(false);
        m_rxElseTx = index == 0;
        displayFrequency();
        displaySampleRate();
    }
}

void AudioCATSISOGUI::on_streamLock_toggled(bool checked)
{
    if (checked && (ui->streamSide->currentIndex() != ui->spectrumSide->currentIndex())) {
        ui->spectrumSide->setCurrentIndex(ui->streamSide->currentIndex());
    }
}

void AudioCATSISOGUI::on_centerFrequency_changed(quint64 value)
{
    if (m_rxElseTx)
    {
        m_settings.m_rxCenterFrequency = value * 1000;
        m_settingsKeys.append("rxCenterFrequency");
    }
    else
    {
        m_settings.m_txCenterFrequency = value * 1000;
        m_settingsKeys.append("txCenterFrequency");
    }

    sendSettings();
}

void AudioCATSISOGUI::on_log2Decim_currentIndexChanged(int index)
{
    if (!m_rxElseTx) { // No interpolation
        return;
    }

    m_settings.m_log2Decim = index < 0 ? 0 : index > 3 ? 3 : index;
    m_settingsKeys.append("log2Decim");
    // displaySampleRate();
    displayFcRxTooltip();
    sendSettings();
}

void AudioCATSISOGUI::on_dcBlock_toggled(bool checked)
{
    m_settings.m_dcBlock = checked;
    m_settingsKeys.append("dcBlock");
    sendSettings();
}

void AudioCATSISOGUI::on_iqCorrection_toggled(bool checked)
{
    m_settings.m_iqCorrection = checked;
    m_settingsKeys.append("iqCorrection");
    sendSettings();
}

void AudioCATSISOGUI::on_txEnable_toggled(bool checked)
{
    m_settings.m_txEnable = checked;
    updateTxEnable();
    m_settingsKeys.append("txEnable");
    sendSettings();
}

void AudioCATSISOGUI::on_transverter_clicked()
{
    m_settings.m_transverterMode = ui->transverter->getDeltaFrequencyAcive();
    m_settings.m_transverterDeltaFrequency = ui->transverter->getDeltaFrequency();
    m_settings.m_iqOrder = ui->transverter->getIQOrder();
    m_settingsKeys.append("transverterMode");
    m_settingsKeys.append("transverterDeltaFrequency");
    m_settingsKeys.append("iqOrder");
    qDebug("AudioCATSISOGUI::on_transverter_clicked: Rx: %lld Hz %s",
        m_settings.m_transverterDeltaFrequency, m_settings.m_transverterMode ? "on" : "off");
    displayFrequency();
    setCenterFrequency(ui->centerFrequency->getValueNew() * 1000);
    sendSettings();
}

void AudioCATSISOGUI::on_rxDeviceSelect_clicked()
{
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_txDeviceName, true, this);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_rxDeviceName = audioSelect.m_audioDeviceName;
        m_settingsKeys.append("rxDeviceName");
        ui->rxDeviceLabel->setText(m_settings.m_rxDeviceName);
        sendSettings();
    }
}

void AudioCATSISOGUI::on_txDeviceSelect_clicked()
{
    AudioSelectDialog audioSelect(DSPEngine::instance()->getAudioDeviceManager(), m_settings.m_txDeviceName, false, this);
    audioSelect.exec();

    if (audioSelect.m_selected)
    {
        m_settings.m_txDeviceName = audioSelect.m_audioDeviceName;
        m_settingsKeys.append("txDeviceName");
        ui->txDeviceLabel->setText(m_settings.m_txDeviceName);
        sendSettings();
    }
}

void AudioCATSISOGUI::on_rxChannels_currentIndexChanged(int index)
{
    m_settings.m_rxIQMapping = (AudioCATSISOSettings::IQMapping)index;
    // updateSampleRateAndFrequency();
    m_settingsKeys.append("rxIQMapping");
    sendSettings();
}

void AudioCATSISOGUI::on_rxVolume_valueChanged(int value)
{
    m_settings.m_rxVolume = value/10.0f;
    ui->rxVolumeText->setText(QString("%1").arg(m_settings.m_rxVolume, 3, 'f', 1));
    m_settingsKeys.append("rxVolume");
    sendSettings();
}

void AudioCATSISOGUI::on_txChannels_currentIndexChanged(int index)
{
    m_settings.m_txIQMapping = (AudioCATSISOSettings::IQMapping) index;
    m_settingsKeys.append("txIQMapping");
    // updateSampleRateAndFrequency();
    sendSettings();
}

void AudioCATSISOGUI::on_txVolume_valueChanged(int value)
{
    m_settings.m_txVolume = value;
    ui->txVolumeText->setText(tr("%1").arg(m_settings.m_txVolume));
    m_settingsKeys.append("txVolume");
    sendSettings();
}

void AudioCATSISOGUI::on_fcPosRx_currentIndexChanged(int index)
{
    m_settings.m_fcPosRx = (AudioCATSISOSettings::fcPos_t) (index < 0 ? 0 : index > 2 ? 2 : index);
    displayFcRxTooltip();
    m_settingsKeys.append("fcPosRx");
    sendSettings();

}

void AudioCATSISOGUI::on_catDevice_currentIndexChanged(int index)
{
    m_settings.m_catDevicePath = ui->catDevice->itemText(index);
    m_settingsKeys.append("catDevicePath");
    sendSettings();
}

void AudioCATSISOGUI::on_catType_currentIndexChanged(int index)
{
    m_settings.m_hamlibModel = m_sampleMIMO->getRigNames()[ui->catType->itemText(index)];
    m_settingsKeys.append("hamlibModel");
    sendSettings();
}

void AudioCATSISOGUI::on_catSettings_clicked()
{
    AudioCATSISOCATDialog dialog(m_settings, m_settingsKeys);

    if (dialog.exec() == QDialog::Accepted) {
        sendSettings();
    }
}

void AudioCATSISOGUI::displaySettings()
{
    blockApplySettings(true);

    ui->rxDeviceLabel->setText(m_settings.m_rxDeviceName);
    ui->txDeviceLabel->setText(m_settings.m_txDeviceName);
    ui->dcBlock->setChecked(m_settings.m_dcBlock);
    ui->iqCorrection->setChecked(m_settings.m_iqCorrection);
    ui->txEnable->setChecked(m_settings.m_txEnable);
    ui->pttSpectrumLink->setChecked(m_settings.m_pttSpectrumLink);
    ui->rxVolume->setValue((int)(m_settings.m_rxVolume*10.0f));
    ui->rxVolumeText->setText(QString("%1").arg(m_settings.m_rxVolume, 3, 'f', 1));
    ui->rxChannels->setCurrentIndex((int)m_settings.m_rxIQMapping);
    ui->txVolume->setValue((int)(m_settings.m_txVolume));
    ui->txVolumeText->setText(tr("%1").arg(m_settings.m_txVolume));
    ui->txChannels->setCurrentIndex((int)m_settings.m_txIQMapping);
    ui->fcPosRx->setCurrentIndex(m_settings.m_fcPosRx);

    blockApplySettings(false);

    displayFrequency();
    displaySampleRate();
    displayDecim();
    updateSpectrum(ui->spectrumSide->currentIndex() == 0);
    displayFcRxTooltip();
    displayCatDevice();
    displayCatType();
}

void AudioCATSISOGUI::displayFcRxTooltip()
{
    int32_t fShift = DeviceSampleSource::calculateFrequencyShift(
        m_settings.m_log2Decim,
        (DeviceSampleSource::fcPos_t) m_settings.m_fcPosRx,
        m_rxSampleRate,
        DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD
    );
    ui->fcPosRx->setToolTip(tr("Relative position of device center frequency: %1 kHz").arg(QString::number(fShift / 1000.0f, 'g', 5)));
}

void AudioCATSISOGUI::displayCatDevice()
{
    blockApplySettings(true);
    QMap<QString, int> catDevices;

    for (int index = 0; index < ui->catDevice->count(); index++) {
        catDevices[ui->catDevice->itemText(index)] = index;
    }

    if (catDevices.contains(m_settings.m_catDevicePath)) {
        ui->catDevice->setCurrentIndex(catDevices[m_settings.m_catDevicePath]);
    } else if (ui->catDevice->count() > 0) {
        m_settings.m_catDevicePath = ui->catDevice->itemText(0);
    }

    blockApplySettings(false);
}

void AudioCATSISOGUI::displayCatType()
{
    blockApplySettings(true);
    QMap<QString, int> catTypes;

    for (int index = 0; index < ui->catType->count(); index++) {
        catTypes[ui->catType->itemText(index)] = index;
    }

    auto const it = m_sampleMIMO->getRigModels().find(m_settings.m_hamlibModel);

    if (it != m_sampleMIMO->getRigModels().end())
    {
        if (catTypes.contains(it.value())) {
            ui->catType->setCurrentIndex(catTypes[it.value()]);
        }
    }

    blockApplySettings(false);
}

void AudioCATSISOGUI::updateTxEnable()
{
    if (!m_settings.m_txEnable) // Rx only
    {
        ui->streamLock->setChecked(true);
        ui->streamSide->setCurrentIndex(0);
        ui->spectrumSide->setCurrentIndex(0);
    }

    ui->ptt->setEnabled(m_settings.m_txEnable);
    ui->streamLock->setEnabled(m_settings.m_txEnable);
    ui->streamSide->setEnabled(m_settings.m_txEnable);
    ui->spectrumSide->setEnabled(m_settings.m_txEnable);
}

void AudioCATSISOGUI::sendSettings()
{
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start(100);
    }
}

void AudioCATSISOGUI::updateHardware()
{
    if (m_doApplySettings)
    {
        AudioCATSISO::MsgConfigureAudioCATSISO* message = AudioCATSISO::MsgConfigureAudioCATSISO::create(m_settings, m_settingsKeys, m_forceSettings);
        m_sampleMIMO->getInputMessageQueue()->push(message);
        m_forceSettings = false;
        m_settingsKeys.clear();
        m_updateTimer.stop();
    }
}

void AudioCATSISOGUI::updateStatus()
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

bool AudioCATSISOGUI::handleMessage(const Message& message)
{
    if (DSPMIMOSignalNotification::match(message))
    {
        DSPMIMOSignalNotification& notif = (DSPMIMOSignalNotification&) message;
        int istream = notif.getIndex();
        bool sourceOrSink = notif.getSourceOrSink();
        qint64 frequency = notif.getCenterFrequency();

        if (sourceOrSink)
        {
            m_rxSampleRate = notif.getSampleRate() * (1<<m_settings.m_log2Decim);
            m_settings.m_rxCenterFrequency = frequency;
        }
        else
        {
            m_txSampleRate = notif. getSampleRate();
            m_settings.m_txCenterFrequency = frequency;
        }

        qDebug() << "AudioCATSISOGUI::handleInputMessages: DSPMIMOSignalNotification: "
            << "sourceOrSink:" << sourceOrSink
            << "istream:" << istream
            << "m_rxSampleRate:" << m_rxSampleRate
            << "m_log2Decim:" << m_settings.m_log2Decim
            << "m_txSampleRate:" << m_txSampleRate
            << "m_rxCenterFrequency:" << m_settings.m_rxCenterFrequency
            << "m_txCenterFrequency:" << m_settings.m_txCenterFrequency;

        displayFrequency();
        displaySampleRate();
        updateSpectrum(ui->spectrumSide->currentIndex() == 0);

        return true;
    }
    else if (AudioCATSISO::MsgConfigureAudioCATSISO::match(message))
    {
        qDebug("AudioCATSISOGUI::handleMessage: MsgConfigureAudioCATSISO");
        const AudioCATSISO::MsgConfigureAudioCATSISO& cfg = (AudioCATSISO::MsgConfigureAudioCATSISO&) message;

        if (cfg.getForce()) {
            m_settings = cfg.getSettings();
        } else {
            m_settings.applySettings(cfg.getSettingsKeys(), cfg.getSettings());
        }

        displaySettings();
        return true;
    }
    else if (AudioCATSISO::MsgStartStop::match(message))
    {
        qDebug("AudioCATSISOGUI::handleMessage: MsgStartStop");
        AudioCATSISO::MsgStartStop& notif = (AudioCATSISO::MsgStartStop&) message;
        blockApplySettings(true);
        ui->startStop->setChecked(notif.getStartStop());
        blockApplySettings(false);

        return true;
    }
    else if (AudioCATSISOSettings::MsgCATReportStatus::match(message))
    {
        AudioCATSISOSettings::MsgCATReportStatus& notif = (AudioCATSISOSettings::MsgCATReportStatus&) message;
        updateCATStatus(notif.getStatus());
        return true;
    }
    else
    {
        return false;
    }
}

void AudioCATSISOGUI::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void AudioCATSISOGUI::displayFrequency()
{
    qint64 centerFrequency;

    if (m_rxElseTx) {
        centerFrequency = m_settings.m_rxCenterFrequency;
    } else {
        centerFrequency = m_settings.m_txCenterFrequency;
    }

    blockApplySettings(true);
    ui->centerFrequency->setValueRange(9, 0, 9999999999);
    ui->centerFrequency->setValue(centerFrequency / 1000);
    blockApplySettings(false);
}

void AudioCATSISOGUI::displaySampleRate()
{
    if (m_rxElseTx) {
        ui->deviceRateText->setText(tr("%1k").arg((float) (m_rxSampleRate/(1<<m_settings.m_log2Decim)) / 1000));
    } else {
        ui->deviceRateText->setText(tr("%1k").arg((float) m_txSampleRate / 1000));
    }
}

void AudioCATSISOGUI::displayDecim()
{
    blockApplySettings(true);

    if (m_rxElseTx)
    {
        ui->log2Decim->setCurrentIndex(m_settings.m_log2Decim);
        ui->fcPosRx->setCurrentIndex(m_settings.m_fcPosRx);
    }
    else
    {
        ui->log2Decim->setCurrentIndex(0); // no interpolation
        ui->fcPosRx->setCurrentIndex(2);   // center
    }

    blockApplySettings(false);
}

void AudioCATSISOGUI::updateCATStatus(AudioCATSISOSettings::MsgCATReportStatus::Status status)
{
    if (m_lastCATStatus != status)
    {
        switch (status)
        {
            case AudioCATSISOSettings::MsgCATReportStatus::StatusNone:
                ui->catStatusIndicator->setStyleSheet("QLabel { background-color:gray; border-radius: 7px; }");
                ui->catStatusIndicator->setToolTip("Idle");
                break;
            case AudioCATSISOSettings::MsgCATReportStatus::StatusConnected:
                ui->catStatusIndicator->setStyleSheet("QLabel { background-color:rgb(35, 138, 35); border-radius: 7px; }");
                ui->catStatusIndicator->setToolTip("Connected");
                break;
            case AudioCATSISOSettings::MsgCATReportStatus::StatusError:
                ui->catStatusIndicator->setStyleSheet("QLabel { background-color:rgb(232, 85, 85); border-radius: 7px; }");
                ui->catStatusIndicator->setToolTip("Error");
                break;
            default:
                break;
        }
    }

    m_lastCATStatus = status;
}

void AudioCATSISOGUI::updateSpectrum(bool rxElseTx)
{
    qint64 centerFrequency;
    int sampleRate;
    bool realElseComplex;

    if (rxElseTx)
    {
        realElseComplex = (m_settings.m_rxIQMapping == AudioCATSISOSettings::L)
            || (m_settings.m_rxIQMapping == AudioCATSISOSettings::R);
        sampleRate = m_rxSampleRate/(1<<m_settings.m_log2Decim);
        centerFrequency = m_settings.m_rxCenterFrequency;
    }
    else
    {
        realElseComplex = (m_settings.m_txIQMapping == AudioCATSISOSettings::L)
            || (m_settings.m_txIQMapping == AudioCATSISOSettings::R);
        centerFrequency = m_settings.m_txCenterFrequency;
        sampleRate = m_txSampleRate;
    }

    m_deviceUISet->getSpectrum()->setCenterFrequency(centerFrequency);
    m_deviceUISet->getSpectrum()->setSampleRate(sampleRate);
    m_deviceUISet->getSpectrum()->setSsbSpectrum(realElseComplex);
}

void AudioCATSISOGUI::openDeviceSettingsDialog(const QPoint& p)
{
    if (m_contextMenuType == ContextMenuDeviceSettings)
    {
        BasicDeviceSettingsDialog dialog(this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);

        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settingsKeys.append("useReverseAPI");
        m_settingsKeys.append("reverseAPIAddress");
        m_settingsKeys.append("reverseAPIPort");
        m_settingsKeys.append("reverseAPIDeviceIndex");

        sendSettings();
    }

    resetContextMenuType();
}

void AudioCATSISOGUI::makeUIConnections()
{
	QObject::connect(ui->streamSide, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioCATSISOGUI::on_streamSide_currentIndexChanged);
    QObject::connect(ui->spectrumSide, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioCATSISOGUI::on_spectrumSide_currentIndexChanged);
    QObject::connect(ui->streamLock, &QToolButton::toggled, this, &AudioCATSISOGUI::on_streamLock_toggled);
	QObject::connect(ui->startStop, &ButtonSwitch::toggled, this, &AudioCATSISOGUI::on_startStop_toggled);
	QObject::connect(ui->ptt, &ButtonSwitch::toggled, this, &AudioCATSISOGUI::on_ptt_toggled);
	QObject::connect(ui->catConnect, &ButtonSwitch::toggled, this, &AudioCATSISOGUI::on_catConnect_toggled);
    QObject::connect(ui->centerFrequency, &ValueDial::changed, this, &AudioCATSISOGUI::on_centerFrequency_changed);
    QObject::connect(ui->log2Decim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioCATSISOGUI::on_log2Decim_currentIndexChanged);
    QObject::connect(ui->dcBlock, &ButtonSwitch::toggled, this, &AudioCATSISOGUI::on_dcBlock_toggled);
    QObject::connect(ui->iqCorrection, &ButtonSwitch::toggled, this, &AudioCATSISOGUI::on_iqCorrection_toggled);
    QObject::connect(ui->txEnable, &ButtonSwitch::toggled, this, &AudioCATSISOGUI::on_txEnable_toggled);
    QObject::connect(ui->pttSpectrumLink, &ButtonSwitch::toggled, this, &AudioCATSISOGUI::on_pttSpectrumLinkToggled);
    QObject::connect(ui->transverter, &TransverterButton::clicked, this, &AudioCATSISOGUI::on_transverter_clicked);
    QObject::connect(ui->rxDeviceSelect, &QPushButton::clicked, this, &AudioCATSISOGUI::on_rxDeviceSelect_clicked);
    QObject::connect(ui->txDeviceSelect, &QPushButton::clicked, this, &AudioCATSISOGUI::on_txDeviceSelect_clicked);
    QObject::connect(ui->rxChannels, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioCATSISOGUI::on_rxChannels_currentIndexChanged);
    QObject::connect(ui->rxVolume, &QDial::valueChanged, this, &AudioCATSISOGUI::on_rxVolume_valueChanged);
    QObject::connect(ui->txChannels, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioCATSISOGUI::on_txChannels_currentIndexChanged);
    QObject::connect(ui->txVolume, &QDial::valueChanged, this, &AudioCATSISOGUI::on_txVolume_valueChanged);
    QObject::connect(ui->fcPosRx, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioCATSISOGUI::on_fcPosRx_currentIndexChanged);
    QObject::connect(ui->catDevice, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioCATSISOGUI::on_catDevice_currentIndexChanged);
    QObject::connect(ui->catType, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AudioCATSISOGUI::on_catType_currentIndexChanged);
    QObject::connect(ui->catSettings, &QPushButton::clicked, this, &AudioCATSISOGUI::on_catSettings_clicked);
}
