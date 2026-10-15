#include "SWGWefaxDemodReport.h"

#include <QJsonDocument>
#include "SWGHelpers.h"

namespace SWGSDRangel {

SWGWefaxDemodReport::SWGWefaxDemodReport() { init(); }
SWGWefaxDemodReport::SWGWefaxDemodReport(QString *json) { init(); fromJson(*json); }
SWGWefaxDemodReport::~SWGWefaxDemodReport() { cleanup(); }

void SWGWefaxDemodReport::init()
{
    channel_power_db = samples_per_line = clock_correction_ppm = applied_clock_correction_ppm = confidence = 0.0f;
    tuning_error_hz = effective_lines_per_minute = required_bandwidth_hz = slant_correction_ppm = 0.0f;
    state = new QString();
    completion_reason = new QString();
    last_save_error = new QString();
    timing_source = new QString();
    timing_status = new QString();
    capture_start_time = new QString();
    image_id = 0;
    capture_frequency_hz = 0;
    timing_accepted = bandwidth_sufficient = complete = 0;
    phasing_line_count = ioc = lines_per_minute = image_width = image_height = image_queue_overflows = 0;
    m_channel_power_db_isSet = m_state_isSet = m_phasing_line_count_isSet = false;
    m_samples_per_line_isSet = m_clock_correction_ppm_isSet = m_confidence_isSet = false;
    m_applied_clock_correction_ppm_isSet = false;
    m_ioc_isSet = m_lines_per_minute_isSet = m_image_width_isSet = m_image_height_isSet = false;
    m_image_queue_overflows_isSet = false;
    m_image_id_isSet = m_completion_reason_isSet = m_last_save_error_isSet = false;
    m_tuning_error_hz_isSet = m_effective_lines_per_minute_isSet = m_timing_source_isSet = false;
    m_slant_correction_ppm_isSet = false;
    m_timing_status_isSet = m_timing_accepted_isSet = m_required_bandwidth_hz_isSet = false;
    m_bandwidth_sufficient_isSet = m_complete_isSet = m_capture_start_time_isSet = false;
    m_capture_frequency_hz_isSet = false;
}

void SWGWefaxDemodReport::cleanup()
{
    delete state; delete completion_reason; delete last_save_error; delete timing_source;
    delete timing_status; delete capture_start_time;
    state = completion_reason = last_save_error = timing_source = timing_status = capture_start_time = nullptr;
}
void SWGWefaxDemodReport::setState(QString *value) { if (state != value) { delete state; state = value; } m_state_isSet = true; }
void SWGWefaxDemodReport::setCompletionReason(QString *value) { if (completion_reason != value) { delete completion_reason; completion_reason = value; } m_completion_reason_isSet = true; }
void SWGWefaxDemodReport::setLastSaveError(QString *value) { if (last_save_error != value) { delete last_save_error; last_save_error = value; } m_last_save_error_isSet = true; }
void SWGWefaxDemodReport::setTimingSource(QString *value) { if (timing_source != value) { delete timing_source; timing_source = value; } m_timing_source_isSet = true; }
void SWGWefaxDemodReport::setTimingStatus(QString *value) { if (timing_status != value) { delete timing_status; timing_status = value; } m_timing_status_isSet = true; }
void SWGWefaxDemodReport::setCaptureStartTime(QString *value) { if (capture_start_time != value) { delete capture_start_time; capture_start_time = value; } m_capture_start_time_isSet = true; }

SWGWefaxDemodReport *SWGWefaxDemodReport::fromJson(QString& json)
{
    QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    QJsonObject object = document.object();
    fromJsonObject(object);
    return this;
}

void SWGWefaxDemodReport::fromJsonObject(QJsonObject& json)
{
    if (json.contains("channelPowerDB")) { setChannelPowerDb(json["channelPowerDB"].toDouble()); }
    if (json.contains("state")) { *state = json["state"].toString(); m_state_isSet = true; }
    if (json.contains("phasingLineCount")) { setPhasingLineCount(json["phasingLineCount"].toInt()); }
    if (json.contains("samplesPerLine")) { setSamplesPerLine(json["samplesPerLine"].toDouble()); }
    if (json.contains("clockCorrectionPpm")) { setClockCorrectionPpm(json["clockCorrectionPpm"].toDouble()); }
    if (json.contains("appliedClockCorrectionPpm")) { setAppliedClockCorrectionPpm(json["appliedClockCorrectionPpm"].toDouble()); }
    if (json.contains("confidence")) { setConfidence(json["confidence"].toDouble()); }
    if (json.contains("ioc")) { setIoc(json["ioc"].toInt()); }
    if (json.contains("linesPerMinute")) { setLinesPerMinute(json["linesPerMinute"].toInt()); }
    if (json.contains("imageWidth")) { setImageWidth(json["imageWidth"].toInt()); }
    if (json.contains("imageHeight")) { setImageHeight(json["imageHeight"].toInt()); }
    if (json.contains("imageQueueOverflows")) { setImageQueueOverflows(json["imageQueueOverflows"].toInt()); }
    if (json.contains("imageId")) { setImageId(json["imageId"].toVariant().toLongLong()); }
    if (json.contains("completionReason")) { *completion_reason = json["completionReason"].toString(); m_completion_reason_isSet = true; }
    if (json.contains("lastSaveError")) { *last_save_error = json["lastSaveError"].toString(); m_last_save_error_isSet = true; }
    if (json.contains("tuningErrorHz")) { setTuningErrorHz(json["tuningErrorHz"].toDouble()); }
    if (json.contains("slantCorrectionPpm")) { setSlantCorrectionPpm(json["slantCorrectionPpm"].toDouble()); }
    if (json.contains("effectiveLinesPerMinute")) { setEffectiveLinesPerMinute(json["effectiveLinesPerMinute"].toDouble()); }
    if (json.contains("timingSource")) { *timing_source = json["timingSource"].toString(); m_timing_source_isSet = true; }
    if (json.contains("timingStatus")) { *timing_status = json["timingStatus"].toString(); m_timing_status_isSet = true; }
    if (json.contains("timingAccepted")) { setTimingAccepted(json["timingAccepted"].toInt()); }
    if (json.contains("requiredBandwidthHz")) { setRequiredBandwidthHz(json["requiredBandwidthHz"].toDouble()); }
    if (json.contains("bandwidthSufficient")) { setBandwidthSufficient(json["bandwidthSufficient"].toInt()); }
    if (json.contains("complete")) { setComplete(json["complete"].toInt()); }
    if (json.contains("captureStartTime")) { *capture_start_time = json["captureStartTime"].toString(); m_capture_start_time_isSet = true; }
    if (json.contains("captureFrequencyHz")) { setCaptureFrequencyHz(json["captureFrequencyHz"].toVariant().toLongLong()); }
}

QString SWGWefaxDemodReport::asJson()
{
    QJsonObject *object = asJsonObject();
    const QString json = QString::fromUtf8(QJsonDocument(*object).toJson());
    delete object;
    return json;
}

QJsonObject *SWGWefaxDemodReport::asJsonObject()
{
    auto *object = new QJsonObject();
    if (m_channel_power_db_isSet) object->insert("channelPowerDB", channel_power_db);
    if (m_state_isSet) object->insert("state", *state);
    if (m_phasing_line_count_isSet) object->insert("phasingLineCount", phasing_line_count);
    if (m_samples_per_line_isSet) object->insert("samplesPerLine", samples_per_line);
    if (m_clock_correction_ppm_isSet) object->insert("clockCorrectionPpm", clock_correction_ppm);
    if (m_applied_clock_correction_ppm_isSet) object->insert("appliedClockCorrectionPpm", applied_clock_correction_ppm);
    if (m_confidence_isSet) object->insert("confidence", confidence);
    if (m_ioc_isSet) object->insert("ioc", ioc);
    if (m_lines_per_minute_isSet) object->insert("linesPerMinute", lines_per_minute);
    if (m_image_width_isSet) object->insert("imageWidth", image_width);
    if (m_image_height_isSet) object->insert("imageHeight", image_height);
    if (m_image_queue_overflows_isSet) object->insert("imageQueueOverflows", image_queue_overflows);
    if (m_image_id_isSet) object->insert("imageId", static_cast<double>(image_id));
    if (m_completion_reason_isSet) object->insert("completionReason", *completion_reason);
    if (m_last_save_error_isSet) object->insert("lastSaveError", *last_save_error);
    if (m_tuning_error_hz_isSet) object->insert("tuningErrorHz", tuning_error_hz);
    if (m_slant_correction_ppm_isSet) object->insert("slantCorrectionPpm", slant_correction_ppm);
    if (m_effective_lines_per_minute_isSet) object->insert("effectiveLinesPerMinute", effective_lines_per_minute);
    if (m_timing_source_isSet) object->insert("timingSource", *timing_source);
    if (m_timing_status_isSet) object->insert("timingStatus", *timing_status);
    if (m_timing_accepted_isSet) object->insert("timingAccepted", timing_accepted);
    if (m_required_bandwidth_hz_isSet) object->insert("requiredBandwidthHz", required_bandwidth_hz);
    if (m_bandwidth_sufficient_isSet) object->insert("bandwidthSufficient", bandwidth_sufficient);
    if (m_complete_isSet) object->insert("complete", complete);
    if (m_capture_start_time_isSet) object->insert("captureStartTime", *capture_start_time);
    if (m_capture_frequency_hz_isSet) object->insert("captureFrequencyHz", static_cast<double>(capture_frequency_hz));
    return object;
}

bool SWGWefaxDemodReport::isSet()
{
    return m_channel_power_db_isSet || m_state_isSet || m_phasing_line_count_isSet
        || m_samples_per_line_isSet || m_clock_correction_ppm_isSet || m_confidence_isSet
        || m_applied_clock_correction_ppm_isSet
        || m_ioc_isSet || m_lines_per_minute_isSet || m_image_width_isSet || m_image_height_isSet
        || m_image_queue_overflows_isSet || m_image_id_isSet || m_completion_reason_isSet
        || m_last_save_error_isSet || m_tuning_error_hz_isSet || m_slant_correction_ppm_isSet
        || m_effective_lines_per_minute_isSet || m_timing_source_isSet
        || m_timing_status_isSet || m_timing_accepted_isSet || m_required_bandwidth_hz_isSet
        || m_bandwidth_sufficient_isSet || m_complete_isSet || m_capture_start_time_isSet
        || m_capture_frequency_hz_isSet;
}

}
