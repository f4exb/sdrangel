///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_SOAPYSDRINPUT_H_
#define PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_SOAPYSDRINPUT_H_

#include <stdint.h>

#include <QString>
#include <QByteArray>
#include <QNetworkRequest>

#include "soapysdr/devicesoapysdrshared.h"
#include "dsp/devicesamplesource.h"

#include "soapysdrinputsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;
class SoapySDRInputThread;

namespace SoapySDR
{
    class Device;
    class ArgInfo;
}

namespace SWGSDRangel
{
    class SWGArgValue;
    class SWGArgInfo;
}

class SoapySDRInput : public DeviceSampleSource
{
    Q_OBJECT
public:
    class MsgConfigureSoapySDRInput : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const SoapySDRInputSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureSoapySDRInput* create(const SoapySDRInputSettings& settings, bool force)
        {
            return new MsgConfigureSoapySDRInput(settings, force);
        }

    private:
        SoapySDRInputSettings m_settings;
        bool m_force;

        MsgConfigureSoapySDRInput(const SoapySDRInputSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgStartStop : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getStartStop() const { return m_startStop; }

        static MsgStartStop* create(bool startStop) {
            return new MsgStartStop(startStop);
        }

    protected:
        bool m_startStop;

        MsgStartStop(bool startStop) :
            Message(),
            m_startStop(startStop)
        { }
    };

    class MsgReportGainChange : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const SoapySDRInputSettings& getSettings() const { return m_settings; }
        bool getGlobalGain() const { return m_globalGain; }
        bool getIndividualGains() const { return m_individualGains; }

        static MsgReportGainChange* create(const SoapySDRInputSettings& settings, bool globalGain, bool individualGains)
        {
            return new MsgReportGainChange(settings, globalGain, individualGains);
        }

    private:
        SoapySDRInputSettings m_settings;
        bool m_globalGain;
        bool m_individualGains;

        MsgReportGainChange(const SoapySDRInputSettings& settings, bool globalGain, bool individualGains) :
            Message(),
            m_settings(settings),
            m_globalGain(globalGain),
            m_individualGains(individualGains)
        { }
    };

    SoapySDRInput(DeviceAPI *deviceAPI);
    virtual ~SoapySDRInput();
    virtual void destroy();

    virtual void init();
    virtual bool start();
    virtual void stop();
    SoapySDRInputThread *getThread() { return m_thread; }
    void setThread(SoapySDRInputThread *thread) { m_thread = thread; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
    virtual const QString& getDeviceDescription() const;
    virtual int getSampleRate() const;
    virtual void setSampleRate(int sampleRate) { (void) sampleRate; }
    virtual quint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);

    virtual bool handleMessage(const Message& message);

    void getFrequencyRange(uint64_t& min, uint64_t& max);
    void getGlobalGainRange(int& min, int& max);
    bool isAGCSupported();
    const std::vector<std::string>& getAntennas();
    const SoapySDR::RangeList& getRateRanges();
    const SoapySDR::RangeList& getBandwidthRanges();
    int getAntennaIndex(const std::string& antenna);
    const std::vector<DeviceSoapySDRParams::FrequencySetting>& getTunableElements();
    const std::vector<DeviceSoapySDRParams::GainSetting>& getIndividualGainsRanges();
    const SoapySDR::ArgInfoList& getStreamArgInfoList();
    const SoapySDR::ArgInfoList& getDeviceArgInfoList();
    void initGainSettings(SoapySDRInputSettings& settings);
    void initTunableElementsSettings(SoapySDRInputSettings& settings);
    void initStreamArgSettings(SoapySDRInputSettings& settings);
    void initDeviceArgSettings(SoapySDRInputSettings& settings);
    bool hasDCAutoCorrection();
    bool hasDCCorrectionValue();
    bool hasIQAutoCorrection() { return false; } // not in SoapySDR interface
    bool hasIQCorrectionValue();

    virtual int webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage);

    virtual int webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage);

    virtual int webapiReportGet(
            SWGSDRangel::SWGDeviceReport& response,
            QString& errorMessage);

    virtual int webapiRunGet(
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    virtual int webapiRun(
            bool run,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    static void webapiFormatDeviceSettings(
            SWGSDRangel::SWGDeviceSettings& response,
            const SoapySDRInputSettings& settings);

    static void webapiUpdateDeviceSettings(
            SoapySDRInputSettings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

private:
    DeviceAPI *m_deviceAPI;
    QMutex m_mutex;
    SoapySDRInputSettings m_settings;
    QString m_deviceDescription;
    bool m_running;
    SoapySDRInputThread *m_thread;
    DeviceSoapySDRShared m_deviceShared;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    bool openDevice();
    void closeDevice();
    SoapySDRInputThread *findThread();
    void moveThreadToBuddy();
    bool applySettings(const SoapySDRInputSettings& settings, bool force = false);
    bool setDeviceCenterFrequency(SoapySDR::Device *dev, int requestedChannel, quint64 freq_hz, int loPpmTenths);
    void updateGains(SoapySDR::Device *dev, int requestedChannel, SoapySDRInputSettings& settings);
    void updateTunableElements(SoapySDR::Device *dev, int requestedChannel, SoapySDRInputSettings& settings);
    void webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response);
    static QVariant webapiVariantFromArgValue(SWGSDRangel::SWGArgValue *argValue);
    static void webapiFormatArgValue(const QVariant& v, SWGSDRangel::SWGArgValue *argValue);
    void webapiFormatArgInfo(const SoapySDR::ArgInfo& arg, SWGSDRangel::SWGArgInfo *argInfo);
    void webapiReverseSendSettings(QList<QString>& deviceSettingsKeys, const SoapySDRInputSettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};



#endif /* PLUGINS_SAMPLESOURCE_SOAPYSDRINPUT_SOAPYSDRINPUT_H_ */
