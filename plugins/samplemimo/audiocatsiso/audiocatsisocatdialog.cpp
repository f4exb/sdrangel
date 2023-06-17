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

#include "audiocatsisosettings.h"
#include "audiocatsisocatdialog.h"

AudioCATSISOCATDialog::AudioCATSISOCATDialog(AudioCATSISOSettings& settings, QList<QString>& settingsKeys, QWidget* parent) :
    QDialog(parent),
    ui(new Ui::AudioCATSISOCATDialog),
    m_settings(settings),
    m_settingsKeys(settingsKeys)
{
    ui->setupUi(this);

    ui->baudRate->blockSignals(true);
    ui->handshake->blockSignals(true);
    ui->dataBits->blockSignals(true);
    ui->stopBits->blockSignals(true);
    ui->pttMethod->blockSignals(true);
    ui->dtrHigh->blockSignals(true);
    ui->rtsHigh->blockSignals(true);
    ui->pollingTime->blockSignals(true);

    ui->baudRate->setCurrentIndex(m_settings.m_catSpeedIndex);
    ui->handshake->setCurrentIndex(m_settings.m_catHandshakeIndex);
    ui->dataBits->setCurrentIndex(m_settings.m_catDataBitsIndex);
    ui->stopBits->setCurrentIndex(m_settings.m_catStopBitsIndex);
    ui->pttMethod->setCurrentIndex(m_settings.m_catPTTMethodIndex);
    ui->dtrHigh->setCurrentIndex(m_settings.m_catDTRHigh ? 1 : 0);
    ui->rtsHigh->setCurrentIndex(m_settings.m_catRTSHigh ? 1 : 0);
    ui->pollingTime->setValue(m_settings.m_catPollingMs);

    ui->baudRate->blockSignals(false);
    ui->handshake->blockSignals(false);
    ui->dataBits->blockSignals(false);
    ui->stopBits->blockSignals(false);
    ui->pttMethod->blockSignals(false);
    ui->dtrHigh->blockSignals(false);
    ui->rtsHigh->blockSignals(false);
    ui->pollingTime->blockSignals(false);
}

AudioCATSISOCATDialog::~AudioCATSISOCATDialog()
{
    delete ui;
}

void AudioCATSISOCATDialog::accept()
{
    QDialog::accept();
}

void AudioCATSISOCATDialog::on_baudRate_currentIndexChanged(int index)
{
    m_settings.m_catSpeedIndex = index;

    if (!m_settingsKeys.contains("catSpeedIndex")) {
        m_settingsKeys.append("catSpeedIndex");
    }
}

void AudioCATSISOCATDialog::on_handshake_currentIndexChanged(int index)
{
    m_settings.m_catHandshakeIndex = index;

    if (!m_settingsKeys.contains("catHandshakeIndex")) {
        m_settingsKeys.append("catHandshakeIndex");
    }
}

void AudioCATSISOCATDialog::on_dataBits_currentIndexChanged(int index)
{
    m_settings.m_catDataBitsIndex = index;

    if (!m_settingsKeys.contains("catDataBitsIndex")) {
        m_settingsKeys.append("catDataBitsIndex");
    }
}

void AudioCATSISOCATDialog::on_stopBits_currentIndexChanged(int index)
{
    m_settings.m_catStopBitsIndex = index;

    if (!m_settingsKeys.contains("catStopBitsIndex")) {
        m_settingsKeys.append("catStopBitsIndex");
    }
}

void AudioCATSISOCATDialog::on_pttMethod_currentIndexChanged(int index)
{
    m_settings.m_catPTTMethodIndex = index;

    if (!m_settingsKeys.contains("catPTTMethodIndex")) {
        m_settingsKeys.append("catPTTMethodIndex");
    }
}

void AudioCATSISOCATDialog::on_dtrHigh_currentIndexChanged(int index)
{
    m_settings.m_catDTRHigh = index == 1;

    if (!m_settingsKeys.contains("catDTRHigh")) {
        m_settingsKeys.append("catDTRHigh");
    }
}

void AudioCATSISOCATDialog::on_rtsHigh_currentIndexChanged(int index)
{
    m_settings.m_catRTSHigh = index == 1;

    if (!m_settingsKeys.contains("catRTSHigh")) {
        m_settingsKeys.append("catRTSHigh");
    }
}

void AudioCATSISOCATDialog::on_pollingTime_valueChanged(int value)
{
    m_settings.m_catPollingMs = value;

    if (!m_settingsKeys.contains("catPollingMs")) {
        m_settingsKeys.append("catPollingMs");
    }
}
