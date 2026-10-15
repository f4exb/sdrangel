// SPDX-License-Identifier: GPL-3.0-or-later
// Qt model for SELCALDemod.yaml, following the Swagger client model interface.
#ifndef SWGSELCALDemodSettings_H_
#define SWGSELCALDemodSettings_H_
#include <QJsonObject>
#include <QString>
#include "SWGObject.h"
#include "export.h"
#include "SWGChannelMarker.h"
#include "SWGRollupState.h"

namespace SWGSDRangel {
class SWG_API SWGSELCALDemodSettings : public SWGObject {
public:
    SWGSELCALDemodSettings();
    explicit SWGSELCALDemodSettings(QString* json);
    ~SWGSELCALDemodSettings() override;
    void init();
    void cleanup();
    QString asJson() override;
    QJsonObject* asJsonObject() override;
    void fromJsonObject(QJsonObject& json) override;
    SWGSELCALDemodSettings* fromJson(QString& json) override;
    bool isSet() override;
    qint64 getInputFrequencyOffset();
    void setInputFrequencyOffset(qint64 value);
    float getRfBandwidth();
    void setRfBandwidth(float value);
    qint32 getModulation();
    void setModulation(qint32 value);
    float getToneFrequencyOffset();
    void setToneFrequencyOffset(float value);
    float getThreshold();
    void setThreshold(float value);
    float getVolume();
    void setVolume(float value);
    qint32 getAudioMute();
    void setAudioMute(qint32 value);
    QString* getAudioDeviceName();
    void setAudioDeviceName(QString* value);
    QString* getFilter();
    void setFilter(QString* value);
    qint32 getUdpEnabled();
    void setUdpEnabled(qint32 value);
    QString* getUdpAddress();
    void setUdpAddress(QString* value);
    qint32 getUdpPort();
    void setUdpPort(qint32 value);
    QString* getLogFilename();
    void setLogFilename(QString* value);
    qint32 getLogEnabled();
    void setLogEnabled(qint32 value);
    qint32 getRgbColor();
    void setRgbColor(qint32 value);
    QString* getTitle();
    void setTitle(QString* value);
    qint32 getStreamIndex();
    void setStreamIndex(qint32 value);
    qint32 getUseReverseApi();
    void setUseReverseApi(qint32 value);
    QString* getReverseApiAddress();
    void setReverseApiAddress(QString* value);
    qint32 getReverseApiPort();
    void setReverseApiPort(qint32 value);
    qint32 getReverseApiDeviceIndex();
    void setReverseApiDeviceIndex(qint32 value);
    qint32 getReverseApiChannelIndex();
    void setReverseApiChannelIndex(qint32 value);
    SWGChannelMarker* getChannelMarker();
    void setChannelMarker(SWGChannelMarker* value);
    SWGRollupState* getRollupState();
    void setRollupState(SWGRollupState* value);
    qint32 getAutoToneFrequencyOffset();
    void setAutoToneFrequencyOffset(qint32 value);
    qint32 getSquelch();
    void setSquelch(qint32 value);
private:
    qint32 m_squelch = 0;
    bool m_squelchIsSet = false;
    qint32 m_autoToneFrequencyOffset = 0;
    bool m_autoToneFrequencyOffsetIsSet = false;
    qint64 m_inputFrequencyOffset = 0;
    bool m_inputFrequencyOffsetIsSet = false;
    float m_rfBandwidth = 0;
    bool m_rfBandwidthIsSet = false;
    qint32 m_modulation = 0;
    bool m_modulationIsSet = false;
    float m_toneFrequencyOffset = 0;
    bool m_toneFrequencyOffsetIsSet = false;
    float m_threshold = 0;
    bool m_thresholdIsSet = false;
    float m_volume = 0;
    bool m_volumeIsSet = false;
    qint32 m_audioMute = 0;
    bool m_audioMuteIsSet = false;
    QString* m_audioDeviceName = nullptr;
    bool m_audioDeviceNameIsSet = false;
    QString* m_filter = nullptr;
    bool m_filterIsSet = false;
    qint32 m_udpEnabled = 0;
    bool m_udpEnabledIsSet = false;
    QString* m_udpAddress = nullptr;
    bool m_udpAddressIsSet = false;
    qint32 m_udpPort = 0;
    bool m_udpPortIsSet = false;
    QString* m_logFilename = nullptr;
    bool m_logFilenameIsSet = false;
    qint32 m_logEnabled = 0;
    bool m_logEnabledIsSet = false;
    qint32 m_rgbColor = 0;
    bool m_rgbColorIsSet = false;
    QString* m_title = nullptr;
    bool m_titleIsSet = false;
    qint32 m_streamIndex = 0;
    bool m_streamIndexIsSet = false;
    qint32 m_useReverseAPI = 0;
    bool m_useReverseAPIIsSet = false;
    QString* m_reverseAPIAddress = nullptr;
    bool m_reverseAPIAddressIsSet = false;
    qint32 m_reverseAPIPort = 0;
    bool m_reverseAPIPortIsSet = false;
    qint32 m_reverseAPIDeviceIndex = 0;
    bool m_reverseAPIDeviceIndexIsSet = false;
    qint32 m_reverseAPIChannelIndex = 0;
    bool m_reverseAPIChannelIndexIsSet = false;
    SWGChannelMarker* m_channelMarker = nullptr;
    bool m_channelMarkerIsSet = false;
    SWGRollupState* m_rollupState = nullptr;
    bool m_rollupStateIsSet = false;
};
}
#endif
