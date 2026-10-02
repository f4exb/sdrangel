///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include <algorithm>
#include <functional>

#include <QDebug>

#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/types/tune_request.hpp>

#include "usrp/deviceusrp.h"
#include "usrp/deviceusrpparam.h"
#include "usrp/deviceusrpshared.h"

#include "usrpmimo.h"
#include "usrpmimoworker.h"

USRPMIMOWorker::USRPMIMOWorker(DeviceUSRPParams *deviceParams, int nbRx, int nbTx, MessageQueue *reportQueue) :
    m_deviceParams(deviceParams),
    m_nbRx(nbRx),
    m_nbTx(nbTx),
    m_reportQueue(reportQueue)
{
    // Executes in the thread this object has been moved to
    connect(&m_inputMessageQueue, &MessageQueue::messageEnqueued, this, &USRPMIMOWorker::handleInputMessages);
}

void USRPMIMOWorker::handleInputMessages()
{
    Message* message;

    // Merge consecutive configuration messages, so we only apply the latest settings.
    // This avoids a backlog of slow device calls (E.g. when a slider is dragged)
    bool pendingConfig = false;
    USRPMIMOSettings settings;
    QList<QString> settingsKeys;
    bool force = false;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (USRPMIMO::MsgConfigureUSRPMIMO::match(*message))
        {
            const USRPMIMO::MsgConfigureUSRPMIMO& conf = (const USRPMIMO::MsgConfigureUSRPMIMO&) *message;

            if (conf.getForce() || !pendingConfig)
            {
                settings = conf.getSettings();
                settingsKeys = conf.getSettingsKeys();
                force = force || conf.getForce();
            }
            else
            {
                settings.applySettings(conf.getSettingsKeys(), conf.getSettings());
                for (const auto& key : conf.getSettingsKeys())
                {
                    if (!settingsKeys.contains(key)) {
                        settingsKeys.append(key);
                    }
                }
            }
            pendingConfig = true;
        }
        else
        {
            qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
        }

        delete message;
    }

    if (pendingConfig) {
        applyDeviceSettings(m_deviceParams, m_nbRx, m_nbTx, settings, settingsKeys, force, true, true, true, true, m_reportQueue);
    }
}

// Apply settings to the device and send what was actually set to reportQueue.
// applyCommon - apply settings common to Rx and Tx (clock source and GPIO)
// applyRx/applyTx - apply Rx/Tx settings
// applyBandwidth - apply LPF bandwidths. These shouldn't be set to a narrow bandwidth before the stream is created.
void USRPMIMOWorker::applyDeviceSettings(
    DeviceUSRPParams *deviceParams,
    int nbRx,
    int nbTx,
    const USRPMIMOSettings& settings,
    const QList<QString>& settingsKeys,
    bool force,
    bool applyCommon,
    bool applyRx,
    bool applyTx,
    bool applyBandwidth,
    MessageQueue *reportQueue)
{
    uhd::usrp::multi_usrp::sptr device = deviceParams ? deviceParams->getDevice() : nullptr;

    if (!device) {
        return;
    }

    QMutexLocker mutexLocker(&deviceParams->m_mutex);

    bool reapplyRx = false;
    bool reapplyTx = false;
    bool checkRates = false;
    bool checkClockSource = false;

    const quint32 rxGain[2] = {settings.m_rx0Gain, settings.m_rx1Gain};
    const USRPMIMOSettings::GainMode rxGainMode[2] = {settings.m_rx0GainMode, settings.m_rx1GainMode};
    const QString rxAntenna[2] = {settings.m_rx0AntennaPath, settings.m_rx1AntennaPath};
    const quint32 txGain[2] = {settings.m_tx0Gain, settings.m_tx1Gain};
    const QString txAntenna[2] = {settings.m_tx0AntennaPath, settings.m_tx1AntennaPath};

    // Apply each setting separately, so a failure (E.g. an antenna name not supported by the
    // daughterboard) doesn't prevent other settings from being applied
    auto apply = [](const char *what, const std::function<void()>& func) {
        try {
            func();
        } catch (std::exception &e) {
            qWarning() << "USRPMIMOWorker::applyDeviceSettings: failed to set" << what << ":" << e.what();
        }
    };

    auto setRxGain = [&](int channel) {
        if (rxGainMode[channel] == USRPMIMOSettings::GAIN_AUTO)
        {
            try {
                device->set_rx_agc(true, channel);
            } catch (uhd::not_implemented_error &e) {
                qDebug() << "USRPMIMOWorker::applyDeviceSettings: AGC not implemented on this radio. Please set to manual.";
            }
        }
        else
        {
            try {
                device->set_rx_agc(false, channel);
            } catch (uhd::not_implemented_error &e) {
                // Ignore
            }
            device->set_rx_gain(rxGain[channel], channel);
        }
    };

    // Common settings

    if (applyCommon && (settingsKeys.contains("clockSource") || force))
    {
        try
        {
            device->set_clock_source(settings.m_clockSource.toStdString(), 0);
            qDebug() << "USRPMIMOWorker::applyDeviceSettings: clock set to " << settings.m_clockSource;
        }
        catch (std::exception &e)
        {
            // An exception will be thrown if the clock is not detected
            qCritical() << "USRPMIMOWorker::applyDeviceSettings: could not set clock " << settings.m_clockSource;
            // So, default back to internal
            apply("clock source to internal", [&]() { device->set_clock_source("internal", 0); });
        }
        // Report actual clock source, in case requested clock couldn't be set
        checkClockSource = true;
        reapplyRx = true;
        reapplyTx = true;
    }

    const std::string gpioBank = "FP0"; // Front Panel GPIO

    if (applyCommon && (settingsKeys.contains("gpioDir") || settingsKeys.contains("gpioPins") || force))
    {
        apply("GPIO", [&]() {
            std::vector<std::string> banks = device->get_gpio_banks(0);

            if (std::find(banks.begin(), banks.end(), gpioBank) != banks.end())
            {
                device->set_gpio_attr(gpioBank, "CTRL", ~settings.m_gpioDir, 0xff); // 0 for GPIO, 1 for ATR
                device->set_gpio_attr(gpioBank, "DDR", settings.m_gpioDir, 0xff); // 0 for input, 1 for output
                device->set_gpio_attr(gpioBank, "OUT", settings.m_gpioPins, 0xff);
                qDebug() << "USRPMIMOWorker::applyDeviceSettings: set GPIO dir to" << settings.m_gpioDir << "pins to" << settings.m_gpioPins;
            }
        });
    }

    // All channels use the same sample rate, so set a master clock rate suitable for it first,
    // otherwise UHD may not be able to change it automatically (See DeviceUSRP::setMasterClockRateForSampleRate)
    bool reenableAutoMasterClockRate = false;

    if ((settingsKeys.contains("devSampleRate") || force) && ((applyRx && (nbRx > 0)) || (applyTx && (nbTx > 0))))
    {
        apply("master clock rate", [&]() {
            reenableAutoMasterClockRate = DeviceUSRP::setMasterClockRateForSampleRate(device, settings.m_devSampleRate, std::max(nbRx, nbTx));
        });
    }

    // Rx settings

    if (applyRx && (nbRx > 0))
    {
        if (settingsKeys.contains("devSampleRate") || force)
        {
            apply("Rx sample rate", [&]() {
                for (int channel = 0; channel < nbRx; channel++) {
                    device->set_rx_rate(settings.m_devSampleRate, channel);
                }
                qDebug("USRPMIMOWorker::applyDeviceSettings: Rx sample rate set to %d", settings.m_devSampleRate);
            });
            checkRates = true;
            reapplyRx = true;
        }

        if (settingsKeys.contains("rxCenterFrequency")
            || settingsKeys.contains("rxLOOffset")
            || settingsKeys.contains("rxTransverterMode")
            || settingsKeys.contains("rxTransverterDeltaFrequency")
            || force)
        {
            apply("Rx frequency", [&]() {
                qint64 deviceCenterFrequency = settings.m_rxCenterFrequency;
                deviceCenterFrequency -= settings.m_rxTransverterMode ? settings.m_rxTransverterDeltaFrequency : 0;
                deviceCenterFrequency = deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency;
                uhd::tune_request_t tuneRequest = settings.m_rxLOOffset != 0
                    ? uhd::tune_request_t(deviceCenterFrequency, settings.m_rxLOOffset)
                    : uhd::tune_request_t(deviceCenterFrequency);

                for (int channel = 0; channel < nbRx; channel++) {
                    device->set_rx_freq(tuneRequest, channel);
                }
                qDebug("USRPMIMOWorker::applyDeviceSettings: Rx frequency set to %lld with LO offset %d", deviceCenterFrequency, settings.m_rxLOOffset);
            });
        }

        if (settingsKeys.contains("dcBlock") || force)
        {
            apply("Rx DC offset correction", [&]() {
                for (int channel = 0; channel < nbRx; channel++) {
                    device->set_rx_dc_offset(settings.m_dcBlock, channel);
                }
            });
        }

        if (settingsKeys.contains("iqCorrection") || force)
        {
            apply("Rx IQ correction", [&]() {
                for (int channel = 0; channel < nbRx; channel++) {
                    device->set_rx_iq_balance(settings.m_iqCorrection, channel);
                }
            });
        }

        for (int channel = 0; channel < nbRx; channel++)
        {
            QString prefix = QString("rx%1").arg(channel);

            if (settingsKeys.contains(prefix + "GainMode") || settingsKeys.contains(prefix + "Gain") || force) {
                apply("Rx gain", [&]() { setRxGain(channel); });
            }

            if (settingsKeys.contains(prefix + "AntennaPath") || force) {
                apply("Rx antenna", [&]() { device->set_rx_antenna(rxAntenna[channel].toStdString(), channel); });
            }
        }

        if (applyBandwidth && (settingsKeys.contains("rxLpfBW") || force))
        {
            apply("Rx LPF bandwidth", [&]() {
                for (int channel = 0; channel < nbRx; channel++) {
                    device->set_rx_bandwidth(settings.m_rxLpfBW, channel);
                }
                qDebug("USRPMIMOWorker::applyDeviceSettings: Rx LPF BW set to %f", settings.m_rxLpfBW);
            });
        }
    }

    // Tx settings

    if (applyTx && (nbTx > 0))
    {
        if (settingsKeys.contains("devSampleRate") || force)
        {
            apply("Tx sample rate", [&]() {
                for (int channel = 0; channel < nbTx; channel++) {
                    device->set_tx_rate(settings.m_devSampleRate, channel);
                }
                qDebug("USRPMIMOWorker::applyDeviceSettings: Tx sample rate set to %d", settings.m_devSampleRate);
            });
            checkRates = true;
            reapplyTx = true;
        }

        if (settingsKeys.contains("txCenterFrequency")
            || settingsKeys.contains("txLOOffset")
            || settingsKeys.contains("txTransverterMode")
            || settingsKeys.contains("txTransverterDeltaFrequency")
            || force)
        {
            apply("Tx frequency", [&]() {
                qint64 deviceCenterFrequency = settings.m_txCenterFrequency;
                deviceCenterFrequency -= settings.m_txTransverterMode ? settings.m_txTransverterDeltaFrequency : 0;
                deviceCenterFrequency = deviceCenterFrequency < 0 ? 0 : deviceCenterFrequency;
                uhd::tune_request_t tuneRequest = settings.m_txLOOffset != 0
                    ? uhd::tune_request_t(deviceCenterFrequency, settings.m_txLOOffset)
                    : uhd::tune_request_t(deviceCenterFrequency);

                for (int channel = 0; channel < nbTx; channel++) {
                    device->set_tx_freq(tuneRequest, channel);
                }
                qDebug("USRPMIMOWorker::applyDeviceSettings: Tx frequency set to %lld with LO offset %d", deviceCenterFrequency, settings.m_txLOOffset);
            });
        }

        for (int channel = 0; channel < nbTx; channel++)
        {
            QString prefix = QString("tx%1").arg(channel);

            if (settingsKeys.contains(prefix + "Gain") || force) {
                apply("Tx gain", [&]() { device->set_tx_gain(txGain[channel], channel); });
            }

            if (settingsKeys.contains(prefix + "AntennaPath") || force) {
                apply("Tx antenna", [&]() { device->set_tx_antenna(txAntenna[channel].toStdString(), channel); });
            }
        }

        if (applyBandwidth && (settingsKeys.contains("txLpfBW") || force))
        {
            apply("Tx LPF bandwidth", [&]() {
                for (int channel = 0; channel < nbTx; channel++) {
                    device->set_tx_bandwidth(settings.m_txLpfBW, channel);
                }
                qDebug("USRPMIMOWorker::applyDeviceSettings: Tx LPF BW set to %f", settings.m_txLpfBW);
            });
        }
    }

    if (reenableAutoMasterClockRate) {
        apply("automatic master clock rate", [&]() { DeviceUSRP::enableAutoMasterClockRate(device); });
    }

    // Need to re-set bandwidth and AGC after changing sample rate or clock source

    if (applyBandwidth && applyRx && reapplyRx)
    {
        for (int channel = 0; channel < nbRx; channel++)
        {
            apply("Rx LPF bandwidth", [&]() { device->set_rx_bandwidth(settings.m_rxLpfBW, channel); });
            apply("Rx gain", [&]() { setRxGain(channel); });
        }
    }

    if (applyBandwidth && applyTx && reapplyTx)
    {
        for (int channel = 0; channel < nbTx; channel++) {
            apply("Tx LPF bandwidth", [&]() { device->set_tx_bandwidth(settings.m_txLpfBW, channel); });
        }
    }

    // Report what was actually set, so USRPMIMO can update its settings and the GUI
    double sampleRate = 0.0;
    double masterClockRate = 0.0;
    QString clockSource;

    if (checkRates)
    {
        try
        {
            sampleRate = DeviceUSRP::getSampleRate(device, nbRx > 0, 0);
            masterClockRate = device->get_master_clock_rate();
            qDebug("USRPMIMOWorker::applyDeviceSettings: actual sample rate %f master_clock_rate %f", sampleRate, masterClockRate);
        }
        catch (std::exception &e)
        {
            qDebug() << "USRPMIMOWorker::applyDeviceSettings: could not get sample rate: " << e.what();
            checkRates = false;
        }
    }

    if (checkClockSource)
    {
        try
        {
            clockSource = QString::fromStdString(device->get_clock_source(0));
            qDebug() << "USRPMIMOWorker::applyDeviceSettings: clock source is " << clockSource;
        }
        catch (std::exception &e)
        {
            qDebug() << "USRPMIMOWorker::applyDeviceSettings: could not get clock source: " << e.what();
            checkClockSource = false;
        }
    }

    if ((checkRates || checkClockSource) && reportQueue) {
        reportQueue->push(DeviceUSRPShared::MsgReportDeviceSettings::create(checkRates, sampleRate, masterClockRate, checkClockSource, clockSource, false));
    }
}
