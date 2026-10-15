// SPDX-License-Identifier: GPL-3.0-or-later
#include "SWGSELCALDemodSettings.h"
#include "SWGHelpers.h"
#include <QJsonDocument>
namespace SWGSDRangel {
SWGSELCALDemodSettings::SWGSELCALDemodSettings() = default;
SWGSELCALDemodSettings::SWGSELCALDemodSettings(QString* json) { init(); if (json) { fromJson(*json); } }
SWGSELCALDemodSettings::~SWGSELCALDemodSettings() { cleanup(); }
void SWGSELCALDemodSettings::cleanup()
{
    delete m_audioDeviceName; m_audioDeviceName = nullptr;
    delete m_filter; m_filter = nullptr;
    delete m_udpAddress; m_udpAddress = nullptr;
    delete m_logFilename; m_logFilename = nullptr;
    delete m_title; m_title = nullptr;
    delete m_reverseAPIAddress; m_reverseAPIAddress = nullptr;
    delete m_channelMarker; m_channelMarker = nullptr;
    delete m_rollupState; m_rollupState = nullptr;
}
void SWGSELCALDemodSettings::init()
{
    cleanup();
    m_squelch = 0; m_squelchIsSet = false;
    m_autoToneFrequencyOffset = 0; m_autoToneFrequencyOffsetIsSet = false;
    m_inputFrequencyOffset = 0; m_inputFrequencyOffsetIsSet = false;
    m_rfBandwidth = 0; m_rfBandwidthIsSet = false;
    m_modulation = 0; m_modulationIsSet = false;
    m_toneFrequencyOffset = 0; m_toneFrequencyOffsetIsSet = false;
    m_threshold = 0; m_thresholdIsSet = false;
    m_volume = 0; m_volumeIsSet = false;
    m_audioMute = 0; m_audioMuteIsSet = false;
    m_audioDeviceName = new QString(); m_audioDeviceNameIsSet = false;
    m_filter = new QString(); m_filterIsSet = false;
    m_udpEnabled = 0; m_udpEnabledIsSet = false;
    m_udpAddress = new QString(); m_udpAddressIsSet = false;
    m_udpPort = 0; m_udpPortIsSet = false;
    m_logFilename = new QString(); m_logFilenameIsSet = false;
    m_logEnabled = 0; m_logEnabledIsSet = false;
    m_rgbColor = 0; m_rgbColorIsSet = false;
    m_title = new QString(); m_titleIsSet = false;
    m_streamIndex = 0; m_streamIndexIsSet = false;
    m_useReverseAPI = 0; m_useReverseAPIIsSet = false;
    m_reverseAPIAddress = new QString(); m_reverseAPIAddressIsSet = false;
    m_reverseAPIPort = 0; m_reverseAPIPortIsSet = false;
    m_reverseAPIDeviceIndex = 0; m_reverseAPIDeviceIndexIsSet = false;
    m_reverseAPIChannelIndex = 0; m_reverseAPIChannelIndexIsSet = false;
    m_channelMarker = new SWGChannelMarker(); m_channelMarkerIsSet = false;
    m_rollupState = new SWGRollupState(); m_rollupStateIsSet = false;
}
SWGSELCALDemodSettings* SWGSELCALDemodSettings::fromJson(QString& json)
{
    auto object = QJsonDocument::fromJson(json.toUtf8()).object(); fromJsonObject(object); return this;
}
void SWGSELCALDemodSettings::fromJsonObject(QJsonObject& json)
{
    if (json.contains("squelch")) { ::SWGSDRangel::setValue(&m_squelch,json["squelch"],"qint32",""); m_squelchIsSet = true; }
    if (json.contains("autoToneFrequencyOffset")) { ::SWGSDRangel::setValue(&m_autoToneFrequencyOffset,json["autoToneFrequencyOffset"],"qint32",""); m_autoToneFrequencyOffsetIsSet = true; }
    if (json.contains("inputFrequencyOffset")) { ::SWGSDRangel::setValue(&m_inputFrequencyOffset,json["inputFrequencyOffset"],"qint64",""); m_inputFrequencyOffsetIsSet = true; }
    if (json.contains("rfBandwidth")) { ::SWGSDRangel::setValue(&m_rfBandwidth,json["rfBandwidth"],"float",""); m_rfBandwidthIsSet = true; }
    if (json.contains("modulation")) { ::SWGSDRangel::setValue(&m_modulation,json["modulation"],"qint32",""); m_modulationIsSet = true; }
    if (json.contains("toneFrequencyOffset")) { ::SWGSDRangel::setValue(&m_toneFrequencyOffset,json["toneFrequencyOffset"],"float",""); m_toneFrequencyOffsetIsSet = true; }
    if (json.contains("threshold")) { ::SWGSDRangel::setValue(&m_threshold,json["threshold"],"float",""); m_thresholdIsSet = true; }
    if (json.contains("volume")) { ::SWGSDRangel::setValue(&m_volume,json["volume"],"float",""); m_volumeIsSet = true; }
    if (json.contains("audioMute")) { ::SWGSDRangel::setValue(&m_audioMute,json["audioMute"],"qint32",""); m_audioMuteIsSet = true; }
    if (json.contains("audioDeviceName")) { ::SWGSDRangel::setValue(&m_audioDeviceName,json["audioDeviceName"],"QString","QString"); m_audioDeviceNameIsSet = true; }
    if (json.contains("filter")) { ::SWGSDRangel::setValue(&m_filter,json["filter"],"QString","QString"); m_filterIsSet = true; }
    if (json.contains("udpEnabled")) { ::SWGSDRangel::setValue(&m_udpEnabled,json["udpEnabled"],"qint32",""); m_udpEnabledIsSet = true; }
    if (json.contains("udpAddress")) { ::SWGSDRangel::setValue(&m_udpAddress,json["udpAddress"],"QString","QString"); m_udpAddressIsSet = true; }
    if (json.contains("udpPort")) { ::SWGSDRangel::setValue(&m_udpPort,json["udpPort"],"qint32",""); m_udpPortIsSet = true; }
    if (json.contains("logFilename")) { ::SWGSDRangel::setValue(&m_logFilename,json["logFilename"],"QString","QString"); m_logFilenameIsSet = true; }
    if (json.contains("logEnabled")) { ::SWGSDRangel::setValue(&m_logEnabled,json["logEnabled"],"qint32",""); m_logEnabledIsSet = true; }
    if (json.contains("rgbColor")) { ::SWGSDRangel::setValue(&m_rgbColor,json["rgbColor"],"qint32",""); m_rgbColorIsSet = true; }
    if (json.contains("title")) { ::SWGSDRangel::setValue(&m_title,json["title"],"QString","QString"); m_titleIsSet = true; }
    if (json.contains("streamIndex")) { ::SWGSDRangel::setValue(&m_streamIndex,json["streamIndex"],"qint32",""); m_streamIndexIsSet = true; }
    if (json.contains("useReverseAPI")) { ::SWGSDRangel::setValue(&m_useReverseAPI,json["useReverseAPI"],"qint32",""); m_useReverseAPIIsSet = true; }
    if (json.contains("reverseAPIAddress")) { ::SWGSDRangel::setValue(&m_reverseAPIAddress,json["reverseAPIAddress"],"QString","QString"); m_reverseAPIAddressIsSet = true; }
    if (json.contains("reverseAPIPort")) { ::SWGSDRangel::setValue(&m_reverseAPIPort,json["reverseAPIPort"],"qint32",""); m_reverseAPIPortIsSet = true; }
    if (json.contains("reverseAPIDeviceIndex")) { ::SWGSDRangel::setValue(&m_reverseAPIDeviceIndex,json["reverseAPIDeviceIndex"],"qint32",""); m_reverseAPIDeviceIndexIsSet = true; }
    if (json.contains("reverseAPIChannelIndex")) { ::SWGSDRangel::setValue(&m_reverseAPIChannelIndex,json["reverseAPIChannelIndex"],"qint32",""); m_reverseAPIChannelIndexIsSet = true; }
    if (json.contains("channelMarker")) { ::SWGSDRangel::setValue(&m_channelMarker,json["channelMarker"],"SWGChannelMarker","SWGChannelMarker"); m_channelMarkerIsSet = true; }
    if (json.contains("rollupState")) { ::SWGSDRangel::setValue(&m_rollupState,json["rollupState"],"SWGRollupState","SWGRollupState"); m_rollupStateIsSet = true; }
}
QString SWGSELCALDemodSettings::asJson()
{
    auto* object = asJsonObject(); QString result = QJsonDocument(*object).toJson(); delete object; return result;
}
QJsonObject* SWGSELCALDemodSettings::asJsonObject()
{
    auto* object = new QJsonObject();
    if (m_squelchIsSet) { object->insert("squelch",QJsonValue(m_squelch)); }
    if (m_autoToneFrequencyOffsetIsSet) { object->insert("autoToneFrequencyOffset",QJsonValue(m_autoToneFrequencyOffset)); }
    if (m_inputFrequencyOffsetIsSet) { object->insert("inputFrequencyOffset",QJsonValue(m_inputFrequencyOffset)); }
    if (m_rfBandwidthIsSet) { object->insert("rfBandwidth",QJsonValue(m_rfBandwidth)); }
    if (m_modulationIsSet) { object->insert("modulation",QJsonValue(m_modulation)); }
    if (m_toneFrequencyOffsetIsSet) { object->insert("toneFrequencyOffset",QJsonValue(m_toneFrequencyOffset)); }
    if (m_thresholdIsSet) { object->insert("threshold",QJsonValue(m_threshold)); }
    if (m_volumeIsSet) { object->insert("volume",QJsonValue(m_volume)); }
    if (m_audioMuteIsSet) { object->insert("audioMute",QJsonValue(m_audioMute)); }
    if (m_audioDeviceName && (m_audioDeviceNameIsSet || !m_audioDeviceName->isEmpty())) { toJsonValue(QString("audioDeviceName"),m_audioDeviceName,object,QString("QString")); }
    if (m_filter && (m_filterIsSet || !m_filter->isEmpty())) { toJsonValue(QString("filter"),m_filter,object,QString("QString")); }
    if (m_udpEnabledIsSet) { object->insert("udpEnabled",QJsonValue(m_udpEnabled)); }
    if (m_udpAddress && (m_udpAddressIsSet || !m_udpAddress->isEmpty())) { toJsonValue(QString("udpAddress"),m_udpAddress,object,QString("QString")); }
    if (m_udpPortIsSet) { object->insert("udpPort",QJsonValue(m_udpPort)); }
    if (m_logFilename && (m_logFilenameIsSet || !m_logFilename->isEmpty())) { toJsonValue(QString("logFilename"),m_logFilename,object,QString("QString")); }
    if (m_logEnabledIsSet) { object->insert("logEnabled",QJsonValue(m_logEnabled)); }
    if (m_rgbColorIsSet) { object->insert("rgbColor",QJsonValue(m_rgbColor)); }
    if (m_title && (m_titleIsSet || !m_title->isEmpty())) { toJsonValue(QString("title"),m_title,object,QString("QString")); }
    if (m_streamIndexIsSet) { object->insert("streamIndex",QJsonValue(m_streamIndex)); }
    if (m_useReverseAPIIsSet) { object->insert("useReverseAPI",QJsonValue(m_useReverseAPI)); }
    if (m_reverseAPIAddress && (m_reverseAPIAddressIsSet || !m_reverseAPIAddress->isEmpty())) { toJsonValue(QString("reverseAPIAddress"),m_reverseAPIAddress,object,QString("QString")); }
    if (m_reverseAPIPortIsSet) { object->insert("reverseAPIPort",QJsonValue(m_reverseAPIPort)); }
    if (m_reverseAPIDeviceIndexIsSet) { object->insert("reverseAPIDeviceIndex",QJsonValue(m_reverseAPIDeviceIndex)); }
    if (m_reverseAPIChannelIndexIsSet) { object->insert("reverseAPIChannelIndex",QJsonValue(m_reverseAPIChannelIndex)); }
    if (m_channelMarker && (m_channelMarkerIsSet || m_channelMarker->isSet())) { toJsonValue(QString("channelMarker"),m_channelMarker,object,QString("SWGChannelMarker")); }
    if (m_rollupState && (m_rollupStateIsSet || m_rollupState->isSet())) { toJsonValue(QString("rollupState"),m_rollupState,object,QString("SWGRollupState")); }
    return object;
}
bool SWGSELCALDemodSettings::isSet()
{
    return m_squelchIsSet || m_autoToneFrequencyOffsetIsSet || m_inputFrequencyOffsetIsSet || m_rfBandwidthIsSet || m_modulationIsSet || m_toneFrequencyOffsetIsSet || m_thresholdIsSet || m_volumeIsSet || m_audioMuteIsSet || m_audioDeviceNameIsSet || m_filterIsSet || m_udpEnabledIsSet || m_udpAddressIsSet || m_udpPortIsSet || m_logFilenameIsSet || m_logEnabledIsSet || m_rgbColorIsSet || m_titleIsSet || m_streamIndexIsSet || m_useReverseAPIIsSet || m_reverseAPIAddressIsSet || m_reverseAPIPortIsSet || m_reverseAPIDeviceIndexIsSet || m_reverseAPIChannelIndexIsSet || m_channelMarkerIsSet || m_rollupStateIsSet;
}
qint64 SWGSELCALDemodSettings::getInputFrequencyOffset() { return m_inputFrequencyOffset; }
void SWGSELCALDemodSettings::setInputFrequencyOffset(qint64 value) { m_inputFrequencyOffset = value; m_inputFrequencyOffsetIsSet = true; }
float SWGSELCALDemodSettings::getRfBandwidth() { return m_rfBandwidth; }
void SWGSELCALDemodSettings::setRfBandwidth(float value) { m_rfBandwidth = value; m_rfBandwidthIsSet = true; }
qint32 SWGSELCALDemodSettings::getModulation() { return m_modulation; }
void SWGSELCALDemodSettings::setModulation(qint32 value) { m_modulation = value; m_modulationIsSet = true; }
float SWGSELCALDemodSettings::getToneFrequencyOffset() { return m_toneFrequencyOffset; }
void SWGSELCALDemodSettings::setToneFrequencyOffset(float value) { m_toneFrequencyOffset = value; m_toneFrequencyOffsetIsSet = true; }
float SWGSELCALDemodSettings::getThreshold() { return m_threshold; }
void SWGSELCALDemodSettings::setThreshold(float value) { m_threshold = value; m_thresholdIsSet = true; }
float SWGSELCALDemodSettings::getVolume() { return m_volume; }
void SWGSELCALDemodSettings::setVolume(float value) { m_volume = value; m_volumeIsSet = true; }
qint32 SWGSELCALDemodSettings::getAudioMute() { return m_audioMute; }
void SWGSELCALDemodSettings::setAudioMute(qint32 value) { m_audioMute = value; m_audioMuteIsSet = true; }
QString* SWGSELCALDemodSettings::getAudioDeviceName() { return m_audioDeviceName; }
void SWGSELCALDemodSettings::setAudioDeviceName(QString* value) { if (m_audioDeviceName != value) { delete m_audioDeviceName; m_audioDeviceName = value; } m_audioDeviceNameIsSet = true; }
QString* SWGSELCALDemodSettings::getFilter() { return m_filter; }
void SWGSELCALDemodSettings::setFilter(QString* value) { if (m_filter != value) { delete m_filter; m_filter = value; } m_filterIsSet = true; }
qint32 SWGSELCALDemodSettings::getUdpEnabled() { return m_udpEnabled; }
void SWGSELCALDemodSettings::setUdpEnabled(qint32 value) { m_udpEnabled = value; m_udpEnabledIsSet = true; }
QString* SWGSELCALDemodSettings::getUdpAddress() { return m_udpAddress; }
void SWGSELCALDemodSettings::setUdpAddress(QString* value) { if (m_udpAddress != value) { delete m_udpAddress; m_udpAddress = value; } m_udpAddressIsSet = true; }
qint32 SWGSELCALDemodSettings::getUdpPort() { return m_udpPort; }
void SWGSELCALDemodSettings::setUdpPort(qint32 value) { m_udpPort = value; m_udpPortIsSet = true; }
QString* SWGSELCALDemodSettings::getLogFilename() { return m_logFilename; }
void SWGSELCALDemodSettings::setLogFilename(QString* value) { if (m_logFilename != value) { delete m_logFilename; m_logFilename = value; } m_logFilenameIsSet = true; }
qint32 SWGSELCALDemodSettings::getLogEnabled() { return m_logEnabled; }
void SWGSELCALDemodSettings::setLogEnabled(qint32 value) { m_logEnabled = value; m_logEnabledIsSet = true; }
qint32 SWGSELCALDemodSettings::getRgbColor() { return m_rgbColor; }
void SWGSELCALDemodSettings::setRgbColor(qint32 value) { m_rgbColor = value; m_rgbColorIsSet = true; }
QString* SWGSELCALDemodSettings::getTitle() { return m_title; }
void SWGSELCALDemodSettings::setTitle(QString* value) { if (m_title != value) { delete m_title; m_title = value; } m_titleIsSet = true; }
qint32 SWGSELCALDemodSettings::getStreamIndex() { return m_streamIndex; }
void SWGSELCALDemodSettings::setStreamIndex(qint32 value) { m_streamIndex = value; m_streamIndexIsSet = true; }
qint32 SWGSELCALDemodSettings::getUseReverseApi() { return m_useReverseAPI; }
void SWGSELCALDemodSettings::setUseReverseApi(qint32 value) { m_useReverseAPI = value; m_useReverseAPIIsSet = true; }
QString* SWGSELCALDemodSettings::getReverseApiAddress() { return m_reverseAPIAddress; }
void SWGSELCALDemodSettings::setReverseApiAddress(QString* value) { if (m_reverseAPIAddress != value) { delete m_reverseAPIAddress; m_reverseAPIAddress = value; } m_reverseAPIAddressIsSet = true; }
qint32 SWGSELCALDemodSettings::getReverseApiPort() { return m_reverseAPIPort; }
void SWGSELCALDemodSettings::setReverseApiPort(qint32 value) { m_reverseAPIPort = value; m_reverseAPIPortIsSet = true; }
qint32 SWGSELCALDemodSettings::getReverseApiDeviceIndex() { return m_reverseAPIDeviceIndex; }
void SWGSELCALDemodSettings::setReverseApiDeviceIndex(qint32 value) { m_reverseAPIDeviceIndex = value; m_reverseAPIDeviceIndexIsSet = true; }
qint32 SWGSELCALDemodSettings::getReverseApiChannelIndex() { return m_reverseAPIChannelIndex; }
void SWGSELCALDemodSettings::setReverseApiChannelIndex(qint32 value) { m_reverseAPIChannelIndex = value; m_reverseAPIChannelIndexIsSet = true; }
SWGChannelMarker* SWGSELCALDemodSettings::getChannelMarker() { return m_channelMarker; }
void SWGSELCALDemodSettings::setChannelMarker(SWGChannelMarker* value) { if (m_channelMarker != value) { delete m_channelMarker; m_channelMarker = value; } m_channelMarkerIsSet = true; }
SWGRollupState* SWGSELCALDemodSettings::getRollupState() { return m_rollupState; }
void SWGSELCALDemodSettings::setRollupState(SWGRollupState* value) { if (m_rollupState != value) { delete m_rollupState; m_rollupState = value; } m_rollupStateIsSet = true; }
qint32 SWGSELCALDemodSettings::getAutoToneFrequencyOffset() { return m_autoToneFrequencyOffset; }
void SWGSELCALDemodSettings::setAutoToneFrequencyOffset(qint32 value) { m_autoToneFrequencyOffset = value; m_autoToneFrequencyOffsetIsSet = true; }
qint32 SWGSELCALDemodSettings::getSquelch() { return m_squelch; }
void SWGSELCALDemodSettings::setSquelch(qint32 value) { m_squelch = value; m_squelchIsSet = true; }
}
