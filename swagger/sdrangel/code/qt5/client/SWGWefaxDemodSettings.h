#ifndef SWGWefaxDemodSettings_H_
#define SWGWefaxDemodSettings_H_

#include <QJsonObject>
#include <QString>
#include "SWGObject.h"
#include "export.h"

namespace SWGSDRangel {

class SWG_API SWGWefaxDemodSettings : public SWGObject
{
public:
    SWGWefaxDemodSettings();
    explicit SWGWefaxDemodSettings(QString *json);
    ~SWGWefaxDemodSettings() override;
    void init();
    void cleanup();
    QString asJson() override;
    QJsonObject *asJsonObject() override;
    void fromJsonObject(QJsonObject& json) override;
    SWGWefaxDemodSettings *fromJson(QString& jsonString) override;

    qint64 getInputFrequencyOffset() const { return input_frequency_offset; }
    void setInputFrequencyOffset(qint64 value) { input_frequency_offset = value; m_input_frequency_offset_isSet = true; }
    float getRfBandwidth() const { return rf_bandwidth; }
    void setRfBandwidth(float value) { rf_bandwidth = value; m_rf_bandwidth_isSet = true; }
    float getFmDeviation() const { return fm_deviation; }
    void setFmDeviation(float value) { fm_deviation = value; m_fm_deviation_isSet = true; }
    qint32 getIoc() const { return ioc; }
    void setIoc(qint32 value) { ioc = value; m_ioc_isSet = true; }
    qint32 getLinesPerMinute() const { return lines_per_minute; }
    void setLinesPerMinute(qint32 value) { lines_per_minute = value; m_lines_per_minute_isSet = true; }
    qint32 getAutoMode() const { return auto_mode; }
    void setAutoMode(qint32 value) { auto_mode = value; m_auto_mode_isSet = true; }
    qint32 getInverted() const { return inverted; }
    void setInverted(qint32 value) { inverted = value; m_inverted_isSet = true; }
    qint32 getMinimumPhasingLines() const { return minimum_phasing_lines; }
    void setMinimumPhasingLines(qint32 value) { minimum_phasing_lines = value; m_minimum_phasing_lines_isSet = true; }
    float getStartConfirmSeconds() const { return start_confirm_seconds; }
    void setStartConfirmSeconds(float value) { start_confirm_seconds = value; m_start_confirm_seconds_isSet = true; }
    float getStopConfirmSeconds() const { return stop_confirm_seconds; }
    void setStopConfirmSeconds(float value) { stop_confirm_seconds = value; m_stop_confirm_seconds_isSet = true; }
    float getManualClockCorrectionPpm() const { return manual_clock_correction_ppm; }
    void setManualClockCorrectionPpm(float value) { manual_clock_correction_ppm = value; m_manual_clock_correction_ppm_isSet = true; }
    qint32 getMaxRows() const { return max_rows; }
    void setMaxRows(qint32 value) { max_rows = value; m_max_rows_isSet = true; }
    qint32 getAutoSave() const { return auto_save; }
    void setAutoSave(qint32 value) { auto_save = value; m_auto_save_isSet = true; }
    QString *getAutoSavePath() const { return auto_save_path; }
    void setAutoSavePath(QString *value);
    qint32 getDisplayInverted() const { return display_inverted; }
    void setDisplayInverted(qint32 value) { display_inverted = value; m_display_inverted_isSet = true; }
    qint32 getDisplayContrast() const { return display_contrast; }
    void setDisplayContrast(qint32 value) { display_contrast = value; m_display_contrast_isSet = true; }
    qint32 getDisplayThreshold() const { return display_threshold; }
    void setDisplayThreshold(qint32 value) { display_threshold = value; m_display_threshold_isSet = true; }
    qint32 getHorizontalAlignment() const { return horizontal_alignment; }
    void setHorizontalAlignment(qint32 value) { horizontal_alignment = value; m_horizontal_alignment_isSet = true; }
    float getDisplaySlantCorrectionPpm() const { return display_slant_correction_ppm; }
    void setDisplaySlantCorrectionPpm(float value) { display_slant_correction_ppm = value; m_display_slant_correction_ppm_isSet = true; }
    qint32 getDisplayRotation() const { return display_rotation; }
    void setDisplayRotation(qint32 value) { display_rotation = value; m_display_rotation_isSet = true; }
    qint32 getDisplayZoomPercent() const { return display_zoom_percent; }
    void setDisplayZoomPercent(qint32 value) { display_zoom_percent = value; m_display_zoom_percent_isSet = true; }
    qint32 getAutoScroll() const { return auto_scroll; }
    void setAutoScroll(qint32 value) { auto_scroll = value; m_auto_scroll_isSet = true; }
    qint32 getAutoSlant() const { return auto_slant; }
    void setAutoSlant(qint32 value) { auto_slant = value; m_auto_slant_isSet = true; }
    qint32 getRgbColor() const { return rgb_color; }
    void setRgbColor(qint32 value) { rgb_color = value; m_rgb_color_isSet = true; }
    QString *getTitle() const { return title; }
    void setTitle(QString *value);
    qint32 getStreamIndex() const { return stream_index; }
    void setStreamIndex(qint32 value) { stream_index = value; m_stream_index_isSet = true; }
    qint32 getUseReverseApi() const { return use_reverse_api; }
    void setUseReverseApi(qint32 value) { use_reverse_api = value; m_use_reverse_api_isSet = true; }
    QString *getReverseApiAddress() const { return reverse_api_address; }
    void setReverseApiAddress(QString *value);
    qint32 getReverseApiPort() const { return reverse_api_port; }
    void setReverseApiPort(qint32 value) { reverse_api_port = value; m_reverse_api_port_isSet = true; }
    qint32 getReverseApiDeviceIndex() const { return reverse_api_device_index; }
    void setReverseApiDeviceIndex(qint32 value) { reverse_api_device_index = value; m_reverse_api_device_index_isSet = true; }
    qint32 getReverseApiChannelIndex() const { return reverse_api_channel_index; }
    void setReverseApiChannelIndex(qint32 value) { reverse_api_channel_index = value; m_reverse_api_channel_index_isSet = true; }
    bool isSet() override;

private:
    qint64 input_frequency_offset;
    float rf_bandwidth;
    float fm_deviation;
    qint32 ioc;
    qint32 lines_per_minute;
    qint32 auto_mode;
    qint32 inverted;
    qint32 minimum_phasing_lines;
    float start_confirm_seconds;
    float stop_confirm_seconds;
    float manual_clock_correction_ppm;
    qint32 max_rows;
    qint32 auto_save;
    QString *auto_save_path;
    qint32 display_inverted;
    qint32 display_contrast;
    qint32 display_threshold;
    qint32 horizontal_alignment;
    float display_slant_correction_ppm;
    qint32 display_rotation;
    qint32 display_zoom_percent;
    qint32 auto_scroll;
    qint32 auto_slant;
    qint32 rgb_color;
    QString *title;
    qint32 stream_index;
    qint32 use_reverse_api;
    QString *reverse_api_address;
    qint32 reverse_api_port;
    qint32 reverse_api_device_index;
    qint32 reverse_api_channel_index;
    bool m_input_frequency_offset_isSet;
    bool m_rf_bandwidth_isSet;
    bool m_fm_deviation_isSet;
    bool m_ioc_isSet;
    bool m_lines_per_minute_isSet;
    bool m_auto_mode_isSet;
    bool m_inverted_isSet;
    bool m_minimum_phasing_lines_isSet;
    bool m_start_confirm_seconds_isSet;
    bool m_stop_confirm_seconds_isSet;
    bool m_manual_clock_correction_ppm_isSet;
    bool m_max_rows_isSet;
    bool m_auto_save_isSet;
    bool m_auto_save_path_isSet;
    bool m_display_inverted_isSet;
    bool m_display_contrast_isSet;
    bool m_display_threshold_isSet;
    bool m_horizontal_alignment_isSet;
    bool m_display_slant_correction_ppm_isSet;
    bool m_display_rotation_isSet;
    bool m_display_zoom_percent_isSet;
    bool m_auto_scroll_isSet;
    bool m_auto_slant_isSet;
    bool m_rgb_color_isSet;
    bool m_title_isSet;
    bool m_stream_index_isSet;
    bool m_use_reverse_api_isSet;
    bool m_reverse_api_address_isSet;
    bool m_reverse_api_port_isSet;
    bool m_reverse_api_device_index_isSet;
    bool m_reverse_api_channel_index_isSet;
};

}

#endif
