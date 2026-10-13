// SPDX-License-Identifier: GPL-3.0-or-later
#include "SWGSELCALDemodReport.h"
#include "SWGHelpers.h"
#include <QJsonDocument>
namespace SWGSDRangel {
SWGSELCALDemodReport::SWGSELCALDemodReport() = default;
SWGSELCALDemodReport::SWGSELCALDemodReport(QString* json) { init(); if (json) { fromJson(*json); } }
SWGSELCALDemodReport::~SWGSELCALDemodReport() { cleanup(); }
void SWGSELCALDemodReport::cleanup()
{
    delete m_state; m_state = nullptr;
    delete m_lastCode; m_lastCode = nullptr;
    delete m_lastTimestamp; m_lastTimestamp = nullptr;
}
void SWGSELCALDemodReport::init()
{
    cleanup();
    m_squelchOpen = 0; m_squelchOpenIsSet = false;
    m_toneFrequencyOffset = 0; m_toneFrequencyOffsetIsSet = false;
    m_carrierFrequencyOffset = 0; m_carrierFrequencyOffsetIsSet = false;
    m_carrierLocked = 0; m_carrierLockedIsSet = false;
    m_channelPowerDb = 0; m_channelPowerDbIsSet = false;
    m_channelSampleRate = 0; m_channelSampleRateIsSet = false;
    m_audioSampleRate = 0; m_audioSampleRateIsSet = false;
    m_state = new QString(); m_stateIsSet = false;
    m_lastCode = new QString(); m_lastCodeIsSet = false;
    m_lastTimestamp = new QString(); m_lastTimestampIsSet = false;
    m_lastFrequency = 0; m_lastFrequencyIsSet = false;
    m_lastRssi = 0; m_lastRssiIsSet = false;
    m_lastSnr = 0; m_lastSnrIsSet = false;
    m_callCount = 0; m_callCountIsSet = false;
}
SWGSELCALDemodReport* SWGSELCALDemodReport::fromJson(QString& json)
{
    auto object = QJsonDocument::fromJson(json.toUtf8()).object(); fromJsonObject(object); return this;
}
void SWGSELCALDemodReport::fromJsonObject(QJsonObject& json)
{
    if (json.contains("squelchOpen")) { ::SWGSDRangel::setValue(&m_squelchOpen,json["squelchOpen"],"qint32",""); m_squelchOpenIsSet = true; }
    if (json.contains("toneFrequencyOffset")) { ::SWGSDRangel::setValue(&m_toneFrequencyOffset,json["toneFrequencyOffset"],"float",""); m_toneFrequencyOffsetIsSet = true; }
    if (json.contains("carrierFrequencyOffset")) { ::SWGSDRangel::setValue(&m_carrierFrequencyOffset,json["carrierFrequencyOffset"],"float",""); m_carrierFrequencyOffsetIsSet = true; }
    if (json.contains("carrierLocked")) { ::SWGSDRangel::setValue(&m_carrierLocked,json["carrierLocked"],"qint32",""); m_carrierLockedIsSet = true; }
    if (json.contains("channelPowerDb")) { ::SWGSDRangel::setValue(&m_channelPowerDb,json["channelPowerDb"],"double",""); m_channelPowerDbIsSet = true; }
    if (json.contains("channelSampleRate")) { ::SWGSDRangel::setValue(&m_channelSampleRate,json["channelSampleRate"],"qint32",""); m_channelSampleRateIsSet = true; }
    if (json.contains("audioSampleRate")) { ::SWGSDRangel::setValue(&m_audioSampleRate,json["audioSampleRate"],"qint32",""); m_audioSampleRateIsSet = true; }
    if (json.contains("state")) { ::SWGSDRangel::setValue(&m_state,json["state"],"QString","QString"); m_stateIsSet = true; }
    if (json.contains("lastCode")) { ::SWGSDRangel::setValue(&m_lastCode,json["lastCode"],"QString","QString"); m_lastCodeIsSet = true; }
    if (json.contains("lastTimestamp")) { ::SWGSDRangel::setValue(&m_lastTimestamp,json["lastTimestamp"],"QString","QString"); m_lastTimestampIsSet = true; }
    if (json.contains("lastFrequency")) { ::SWGSDRangel::setValue(&m_lastFrequency,json["lastFrequency"],"qint64",""); m_lastFrequencyIsSet = true; }
    if (json.contains("lastRssi")) { ::SWGSDRangel::setValue(&m_lastRssi,json["lastRssi"],"float",""); m_lastRssiIsSet = true; }
    if (json.contains("lastSnr")) { ::SWGSDRangel::setValue(&m_lastSnr,json["lastSnr"],"float",""); m_lastSnrIsSet = true; }
    if (json.contains("callCount")) { ::SWGSDRangel::setValue(&m_callCount,json["callCount"],"qint64",""); m_callCountIsSet = true; }
}
QString SWGSELCALDemodReport::asJson()
{
    auto* object = asJsonObject(); QString result = QJsonDocument(*object).toJson(); delete object; return result;
}
QJsonObject* SWGSELCALDemodReport::asJsonObject()
{
    auto* object = new QJsonObject();
    if (m_squelchOpenIsSet) { object->insert("squelchOpen",QJsonValue(m_squelchOpen)); }
    if (m_toneFrequencyOffsetIsSet) { object->insert("toneFrequencyOffset",QJsonValue(m_toneFrequencyOffset)); }
    if (m_carrierFrequencyOffsetIsSet) { object->insert("carrierFrequencyOffset",QJsonValue(m_carrierFrequencyOffset)); }
    if (m_carrierLockedIsSet) { object->insert("carrierLocked",QJsonValue(m_carrierLocked)); }
    if (m_channelPowerDbIsSet) { object->insert("channelPowerDb",QJsonValue(m_channelPowerDb)); }
    if (m_channelSampleRateIsSet) { object->insert("channelSampleRate",QJsonValue(m_channelSampleRate)); }
    if (m_audioSampleRateIsSet) { object->insert("audioSampleRate",QJsonValue(m_audioSampleRate)); }
    if (m_state && (m_stateIsSet || !m_state->isEmpty())) { toJsonValue(QString("state"),m_state,object,QString("QString")); }
    if (m_lastCode && (m_lastCodeIsSet || !m_lastCode->isEmpty())) { toJsonValue(QString("lastCode"),m_lastCode,object,QString("QString")); }
    if (m_lastTimestamp && (m_lastTimestampIsSet || !m_lastTimestamp->isEmpty())) { toJsonValue(QString("lastTimestamp"),m_lastTimestamp,object,QString("QString")); }
    if (m_lastFrequencyIsSet) { object->insert("lastFrequency",QJsonValue(m_lastFrequency)); }
    if (m_lastRssiIsSet) { object->insert("lastRssi",QJsonValue(m_lastRssi)); }
    if (m_lastSnrIsSet) { object->insert("lastSnr",QJsonValue(m_lastSnr)); }
    if (m_callCountIsSet) { object->insert("callCount",QJsonValue(m_callCount)); }
    return object;
}
bool SWGSELCALDemodReport::isSet()
{
    return m_squelchOpenIsSet || m_toneFrequencyOffsetIsSet || m_carrierFrequencyOffsetIsSet || m_carrierLockedIsSet || m_channelPowerDbIsSet || m_channelSampleRateIsSet || m_audioSampleRateIsSet || m_stateIsSet || m_lastCodeIsSet || m_lastTimestampIsSet || m_lastFrequencyIsSet || m_lastRssiIsSet || m_lastSnrIsSet || m_callCountIsSet;
}
double SWGSELCALDemodReport::getChannelPowerDb() { return m_channelPowerDb; }
void SWGSELCALDemodReport::setChannelPowerDb(double value) { m_channelPowerDb = value; m_channelPowerDbIsSet = true; }
qint32 SWGSELCALDemodReport::getChannelSampleRate() { return m_channelSampleRate; }
void SWGSELCALDemodReport::setChannelSampleRate(qint32 value) { m_channelSampleRate = value; m_channelSampleRateIsSet = true; }
qint32 SWGSELCALDemodReport::getAudioSampleRate() { return m_audioSampleRate; }
void SWGSELCALDemodReport::setAudioSampleRate(qint32 value) { m_audioSampleRate = value; m_audioSampleRateIsSet = true; }
QString* SWGSELCALDemodReport::getState() { return m_state; }
void SWGSELCALDemodReport::setState(QString* value) { if (m_state != value) { delete m_state; m_state = value; } m_stateIsSet = true; }
QString* SWGSELCALDemodReport::getLastCode() { return m_lastCode; }
void SWGSELCALDemodReport::setLastCode(QString* value) { if (m_lastCode != value) { delete m_lastCode; m_lastCode = value; } m_lastCodeIsSet = true; }
QString* SWGSELCALDemodReport::getLastTimestamp() { return m_lastTimestamp; }
void SWGSELCALDemodReport::setLastTimestamp(QString* value) { if (m_lastTimestamp != value) { delete m_lastTimestamp; m_lastTimestamp = value; } m_lastTimestampIsSet = true; }
qint64 SWGSELCALDemodReport::getLastFrequency() { return m_lastFrequency; }
void SWGSELCALDemodReport::setLastFrequency(qint64 value) { m_lastFrequency = value; m_lastFrequencyIsSet = true; }
float SWGSELCALDemodReport::getLastRssi() { return m_lastRssi; }
void SWGSELCALDemodReport::setLastRssi(float value) { m_lastRssi = value; m_lastRssiIsSet = true; }
float SWGSELCALDemodReport::getLastSnr() { return m_lastSnr; }
void SWGSELCALDemodReport::setLastSnr(float value) { m_lastSnr = value; m_lastSnrIsSet = true; }
qint64 SWGSELCALDemodReport::getCallCount() { return m_callCount; }
void SWGSELCALDemodReport::setCallCount(qint64 value) { m_callCount = value; m_callCountIsSet = true; }
qint32 SWGSELCALDemodReport::getCarrierLocked() { return m_carrierLocked; }
void SWGSELCALDemodReport::setCarrierLocked(qint32 value) { m_carrierLocked = value; m_carrierLockedIsSet = true; }
float SWGSELCALDemodReport::getCarrierFrequencyOffset() { return m_carrierFrequencyOffset; }
void SWGSELCALDemodReport::setCarrierFrequencyOffset(float value) { m_carrierFrequencyOffset = value; m_carrierFrequencyOffsetIsSet = true; }
float SWGSELCALDemodReport::getToneFrequencyOffset() { return m_toneFrequencyOffset; }
void SWGSELCALDemodReport::setToneFrequencyOffset(float value) { m_toneFrequencyOffset = value; m_toneFrequencyOffsetIsSet = true; }
qint32 SWGSELCALDemodReport::getSquelchOpen() { return m_squelchOpen; }
void SWGSELCALDemodReport::setSquelchOpen(qint32 value) { m_squelchOpen = value; m_squelchOpenIsSet = true; }
}
