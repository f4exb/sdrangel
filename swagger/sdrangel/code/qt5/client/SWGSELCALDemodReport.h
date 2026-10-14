// SPDX-License-Identifier: GPL-3.0-or-later
// Qt model for SELCALDemod.yaml, following the Swagger client model interface.
#ifndef SWGSELCALDemodReport_H_
#define SWGSELCALDemodReport_H_
#include <QJsonObject>
#include <QString>
#include "SWGObject.h"
#include "export.h"

namespace SWGSDRangel {
class SWG_API SWGSELCALDemodReport : public SWGObject {
public:
    SWGSELCALDemodReport();
    explicit SWGSELCALDemodReport(QString* json);
    ~SWGSELCALDemodReport() override;
    void init();
    void cleanup();
    QString asJson() override;
    QJsonObject* asJsonObject() override;
    void fromJsonObject(QJsonObject& json) override;
    SWGSELCALDemodReport* fromJson(QString& json) override;
    bool isSet() override;
    double getChannelPowerDb();
    void setChannelPowerDb(double value);
    qint32 getChannelSampleRate();
    void setChannelSampleRate(qint32 value);
    qint32 getAudioSampleRate();
    void setAudioSampleRate(qint32 value);
    QString* getState();
    void setState(QString* value);
    QString* getLastCode();
    void setLastCode(QString* value);
    QString* getLastTimestamp();
    void setLastTimestamp(QString* value);
    qint64 getLastFrequency();
    void setLastFrequency(qint64 value);
    float getLastRssi();
    void setLastRssi(float value);
    float getLastSnr();
    void setLastSnr(float value);
    qint64 getCallCount();
    void setCallCount(qint64 value);
    qint32 getCarrierLocked();
    void setCarrierLocked(qint32 value);
    float getCarrierFrequencyOffset();
    void setCarrierFrequencyOffset(float value);
    float getToneFrequencyOffset();
    void setToneFrequencyOffset(float value);
    qint32 getSquelchOpen();
    void setSquelchOpen(qint32 value);
private:
    qint32 m_squelchOpen = 0;
    bool m_squelchOpenIsSet = false;
    float m_toneFrequencyOffset = 0;
    bool m_toneFrequencyOffsetIsSet = false;
    float m_carrierFrequencyOffset = 0;
    bool m_carrierFrequencyOffsetIsSet = false;
    qint32 m_carrierLocked = 0;
    bool m_carrierLockedIsSet = false;
    double m_channelPowerDb = 0;
    bool m_channelPowerDbIsSet = false;
    qint32 m_channelSampleRate = 0;
    bool m_channelSampleRateIsSet = false;
    qint32 m_audioSampleRate = 0;
    bool m_audioSampleRateIsSet = false;
    QString* m_state = nullptr;
    bool m_stateIsSet = false;
    QString* m_lastCode = nullptr;
    bool m_lastCodeIsSet = false;
    QString* m_lastTimestamp = nullptr;
    bool m_lastTimestampIsSet = false;
    qint64 m_lastFrequency = 0;
    bool m_lastFrequencyIsSet = false;
    float m_lastRssi = 0;
    bool m_lastRssiIsSet = false;
    float m_lastSnr = 0;
    bool m_lastSnrIsSet = false;
    qint64 m_callCount = 0;
    bool m_callCountIsSet = false;
};
}
#endif
