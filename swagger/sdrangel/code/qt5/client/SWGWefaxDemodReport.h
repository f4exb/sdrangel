#ifndef SWGWefaxDemodReport_H_
#define SWGWefaxDemodReport_H_

#include <QJsonObject>
#include <QString>
#include "SWGObject.h"
#include "export.h"

namespace SWGSDRangel {

class SWG_API SWGWefaxDemodReport : public SWGObject
{
public:
    SWGWefaxDemodReport();
    explicit SWGWefaxDemodReport(QString *json);
    ~SWGWefaxDemodReport() override;
    void init();
    void cleanup();
    QString asJson() override;
    QJsonObject *asJsonObject() override;
    void fromJsonObject(QJsonObject& json) override;
    SWGWefaxDemodReport *fromJson(QString& jsonString) override;

    float getChannelPowerDb() const { return channel_power_db; }
    void setChannelPowerDb(float value) { channel_power_db = value; m_channel_power_db_isSet = true; }
    QString *getState() const { return state; }
    void setState(QString *value);
    qint32 getPhasingLineCount() const { return phasing_line_count; }
    void setPhasingLineCount(qint32 value) { phasing_line_count = value; m_phasing_line_count_isSet = true; }
    float getSamplesPerLine() const { return samples_per_line; }
    void setSamplesPerLine(float value) { samples_per_line = value; m_samples_per_line_isSet = true; }
    float getClockCorrectionPpm() const { return clock_correction_ppm; }
    void setClockCorrectionPpm(float value) { clock_correction_ppm = value; m_clock_correction_ppm_isSet = true; }
    float getAppliedClockCorrectionPpm() const { return applied_clock_correction_ppm; }
    void setAppliedClockCorrectionPpm(float value) { applied_clock_correction_ppm = value; m_applied_clock_correction_ppm_isSet = true; }
    float getConfidence() const { return confidence; }
    void setConfidence(float value) { confidence = value; m_confidence_isSet = true; }
    qint32 getIoc() const { return ioc; }
    void setIoc(qint32 value) { ioc = value; m_ioc_isSet = true; }
    qint32 getLinesPerMinute() const { return lines_per_minute; }
    void setLinesPerMinute(qint32 value) { lines_per_minute = value; m_lines_per_minute_isSet = true; }
    qint32 getImageWidth() const { return image_width; }
    void setImageWidth(qint32 value) { image_width = value; m_image_width_isSet = true; }
    qint32 getImageHeight() const { return image_height; }
    void setImageHeight(qint32 value) { image_height = value; m_image_height_isSet = true; }
    qint32 getImageQueueOverflows() const { return image_queue_overflows; }
    void setImageQueueOverflows(qint32 value) { image_queue_overflows = value; m_image_queue_overflows_isSet = true; }
    qint64 getImageId() const { return image_id; }
    void setImageId(qint64 value) { image_id = value; m_image_id_isSet = true; }
    QString *getCompletionReason() const { return completion_reason; }
    void setCompletionReason(QString *value);
    QString *getLastSaveError() const { return last_save_error; }
    void setLastSaveError(QString *value);
    float getTuningErrorHz() const { return tuning_error_hz; }
    void setTuningErrorHz(float value) { tuning_error_hz = value; m_tuning_error_hz_isSet = true; }
    float getSlantCorrectionPpm() const { return slant_correction_ppm; }
    void setSlantCorrectionPpm(float value) { slant_correction_ppm = value; m_slant_correction_ppm_isSet = true; }
    float getEffectiveLinesPerMinute() const { return effective_lines_per_minute; }
    void setEffectiveLinesPerMinute(float value) { effective_lines_per_minute = value; m_effective_lines_per_minute_isSet = true; }
    QString *getTimingSource() const { return timing_source; }
    void setTimingSource(QString *value);
    QString *getTimingStatus() const { return timing_status; }
    void setTimingStatus(QString *value);
    qint32 getTimingAccepted() const { return timing_accepted; }
    void setTimingAccepted(qint32 value) { timing_accepted = value; m_timing_accepted_isSet = true; }
    float getRequiredBandwidthHz() const { return required_bandwidth_hz; }
    void setRequiredBandwidthHz(float value) { required_bandwidth_hz = value; m_required_bandwidth_hz_isSet = true; }
    qint32 getBandwidthSufficient() const { return bandwidth_sufficient; }
    void setBandwidthSufficient(qint32 value) { bandwidth_sufficient = value; m_bandwidth_sufficient_isSet = true; }
    qint32 getComplete() const { return complete; }
    void setComplete(qint32 value) { complete = value; m_complete_isSet = true; }
    QString *getCaptureStartTime() const { return capture_start_time; }
    void setCaptureStartTime(QString *value);
    qint64 getCaptureFrequencyHz() const { return capture_frequency_hz; }
    void setCaptureFrequencyHz(qint64 value) { capture_frequency_hz = value; m_capture_frequency_hz_isSet = true; }
    bool isSet() override;

private:
    float channel_power_db;
    QString *state;
    qint32 phasing_line_count;
    float samples_per_line;
    float clock_correction_ppm;
    float applied_clock_correction_ppm;
    float confidence;
    qint32 ioc;
    qint32 lines_per_minute;
    qint32 image_width;
    qint32 image_height;
    qint32 image_queue_overflows;
    qint64 image_id;
    QString *completion_reason;
    QString *last_save_error;
    float tuning_error_hz;
    float slant_correction_ppm;
    float effective_lines_per_minute;
    QString *timing_source;
    QString *timing_status;
    qint32 timing_accepted;
    float required_bandwidth_hz;
    qint32 bandwidth_sufficient;
    qint32 complete;
    QString *capture_start_time;
    qint64 capture_frequency_hz;
    bool m_channel_power_db_isSet;
    bool m_state_isSet;
    bool m_phasing_line_count_isSet;
    bool m_samples_per_line_isSet;
    bool m_clock_correction_ppm_isSet;
    bool m_applied_clock_correction_ppm_isSet;
    bool m_confidence_isSet;
    bool m_ioc_isSet;
    bool m_lines_per_minute_isSet;
    bool m_image_width_isSet;
    bool m_image_height_isSet;
    bool m_image_queue_overflows_isSet;
    bool m_image_id_isSet;
    bool m_completion_reason_isSet;
    bool m_last_save_error_isSet;
    bool m_tuning_error_hz_isSet;
    bool m_slant_correction_ppm_isSet;
    bool m_effective_lines_per_minute_isSet;
    bool m_timing_source_isSet;
    bool m_timing_status_isSet;
    bool m_timing_accepted_isSet;
    bool m_required_bandwidth_hz_isSet;
    bool m_bandwidth_sufficient_isSet;
    bool m_complete_isSet;
    bool m_capture_start_time_isSet;
    bool m_capture_frequency_hz_isSet;
};

}

#endif
