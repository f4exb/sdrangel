#include "SWGWefaxDemodSettings.h"

#include <QJsonDocument>

namespace SWGSDRangel {

SWGWefaxDemodSettings::SWGWefaxDemodSettings() { init(); }
SWGWefaxDemodSettings::SWGWefaxDemodSettings(QString *json) { init(); fromJson(*json); }
SWGWefaxDemodSettings::~SWGWefaxDemodSettings() { cleanup(); }

void SWGWefaxDemodSettings::init()
{
    input_frequency_offset = 0;
    rf_bandwidth = fm_deviation = manual_clock_correction_ppm = 0.0f;
    start_confirm_seconds = stop_confirm_seconds = display_slant_correction_ppm = 0.0f;
    ioc = lines_per_minute = auto_mode = inverted = minimum_phasing_lines = 0;
    max_rows = auto_save = rgb_color = stream_index = use_reverse_api = 0;
    display_inverted = display_contrast = display_threshold = horizontal_alignment = 0;
    display_rotation = display_zoom_percent = auto_scroll = auto_slant = 0;
    reverse_api_port = reverse_api_device_index = reverse_api_channel_index = 0;
    auto_save_path = new QString();
    title = new QString();
    reverse_api_address = new QString();
#define RESET_FLAG(name) m_##name##_isSet = false
    RESET_FLAG(input_frequency_offset); RESET_FLAG(rf_bandwidth); RESET_FLAG(fm_deviation);
    RESET_FLAG(ioc); RESET_FLAG(lines_per_minute); RESET_FLAG(auto_mode); RESET_FLAG(inverted);
    RESET_FLAG(minimum_phasing_lines); RESET_FLAG(manual_clock_correction_ppm); RESET_FLAG(max_rows);
    RESET_FLAG(start_confirm_seconds); RESET_FLAG(stop_confirm_seconds);
    RESET_FLAG(auto_save); RESET_FLAG(auto_save_path); RESET_FLAG(rgb_color); RESET_FLAG(title);
    RESET_FLAG(display_inverted); RESET_FLAG(display_contrast); RESET_FLAG(display_rotation);
    RESET_FLAG(display_threshold); RESET_FLAG(horizontal_alignment); RESET_FLAG(display_slant_correction_ppm);
    RESET_FLAG(display_zoom_percent); RESET_FLAG(auto_scroll); RESET_FLAG(auto_slant);
    RESET_FLAG(stream_index); RESET_FLAG(use_reverse_api); RESET_FLAG(reverse_api_address);
    RESET_FLAG(reverse_api_port); RESET_FLAG(reverse_api_device_index); RESET_FLAG(reverse_api_channel_index);
#undef RESET_FLAG
}

void SWGWefaxDemodSettings::cleanup()
{
    delete auto_save_path;
    delete title;
    delete reverse_api_address;
    auto_save_path = title = reverse_api_address = nullptr;
}

void SWGWefaxDemodSettings::setAutoSavePath(QString *value)
{
    if (auto_save_path != value) { delete auto_save_path; auto_save_path = value; }
    m_auto_save_path_isSet = true;
}

void SWGWefaxDemodSettings::setTitle(QString *value)
{
    if (title != value) { delete title; title = value; }
    m_title_isSet = true;
}

void SWGWefaxDemodSettings::setReverseApiAddress(QString *value)
{
    if (reverse_api_address != value) { delete reverse_api_address; reverse_api_address = value; }
    m_reverse_api_address_isSet = true;
}

SWGWefaxDemodSettings *SWGWefaxDemodSettings::fromJson(QString& json)
{
    QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    QJsonObject object = document.object();
    fromJsonObject(object);
    return this;
}

void SWGWefaxDemodSettings::fromJsonObject(QJsonObject& json)
{
    if (json.contains("inputFrequencyOffset")) setInputFrequencyOffset(json["inputFrequencyOffset"].toVariant().toLongLong());
    if (json.contains("rfBandwidth")) setRfBandwidth(json["rfBandwidth"].toDouble());
    if (json.contains("fmDeviation")) setFmDeviation(json["fmDeviation"].toDouble());
    if (json.contains("ioc")) setIoc(json["ioc"].toInt());
    if (json.contains("linesPerMinute")) setLinesPerMinute(json["linesPerMinute"].toInt());
    if (json.contains("autoMode")) setAutoMode(json["autoMode"].toInt());
    if (json.contains("inverted")) setInverted(json["inverted"].toInt());
    if (json.contains("minimumPhasingLines")) setMinimumPhasingLines(json["minimumPhasingLines"].toInt());
    if (json.contains("startConfirmSeconds")) setStartConfirmSeconds(json["startConfirmSeconds"].toDouble());
    if (json.contains("stopConfirmSeconds")) setStopConfirmSeconds(json["stopConfirmSeconds"].toDouble());
    if (json.contains("manualClockCorrectionPpm")) setManualClockCorrectionPpm(json["manualClockCorrectionPpm"].toDouble());
    if (json.contains("maxRows")) setMaxRows(json["maxRows"].toInt());
    if (json.contains("autoSave")) setAutoSave(json["autoSave"].toInt());
    if (json.contains("autoSavePath")) { *auto_save_path = json["autoSavePath"].toString(); m_auto_save_path_isSet = true; }
    if (json.contains("displayInverted")) setDisplayInverted(json["displayInverted"].toInt());
    if (json.contains("displayContrast")) setDisplayContrast(json["displayContrast"].toInt());
    if (json.contains("displayThreshold")) setDisplayThreshold(json["displayThreshold"].toInt());
    if (json.contains("horizontalAlignment")) setHorizontalAlignment(json["horizontalAlignment"].toInt());
    if (json.contains("displaySlantCorrectionPpm")) setDisplaySlantCorrectionPpm(json["displaySlantCorrectionPpm"].toDouble());
    if (json.contains("displayRotation")) setDisplayRotation(json["displayRotation"].toInt());
    if (json.contains("displayZoomPercent")) setDisplayZoomPercent(json["displayZoomPercent"].toInt());
    if (json.contains("autoScroll")) setAutoScroll(json["autoScroll"].toInt());
    if (json.contains("autoSlant")) setAutoSlant(json["autoSlant"].toInt());
    if (json.contains("rgbColor")) setRgbColor(json["rgbColor"].toInt());
    if (json.contains("title")) { *title = json["title"].toString(); m_title_isSet = true; }
    if (json.contains("streamIndex")) setStreamIndex(json["streamIndex"].toInt());
    if (json.contains("useReverseAPI")) setUseReverseApi(json["useReverseAPI"].toInt());
    if (json.contains("reverseAPIAddress")) { *reverse_api_address = json["reverseAPIAddress"].toString(); m_reverse_api_address_isSet = true; }
    if (json.contains("reverseAPIPort")) setReverseApiPort(json["reverseAPIPort"].toInt());
    if (json.contains("reverseAPIDeviceIndex")) setReverseApiDeviceIndex(json["reverseAPIDeviceIndex"].toInt());
    if (json.contains("reverseAPIChannelIndex")) setReverseApiChannelIndex(json["reverseAPIChannelIndex"].toInt());
}

QString SWGWefaxDemodSettings::asJson()
{
    QJsonObject *object = asJsonObject();
    const QString json = QString::fromUtf8(QJsonDocument(*object).toJson());
    delete object;
    return json;
}

QJsonObject *SWGWefaxDemodSettings::asJsonObject()
{
    auto *object = new QJsonObject();
#define WRITE_VALUE(flag, key, value) if (flag) object->insert(key, QJsonValue(value))
    WRITE_VALUE(m_input_frequency_offset_isSet, "inputFrequencyOffset", static_cast<double>(input_frequency_offset));
    WRITE_VALUE(m_rf_bandwidth_isSet, "rfBandwidth", rf_bandwidth);
    WRITE_VALUE(m_fm_deviation_isSet, "fmDeviation", fm_deviation);
    WRITE_VALUE(m_ioc_isSet, "ioc", ioc);
    WRITE_VALUE(m_lines_per_minute_isSet, "linesPerMinute", lines_per_minute);
    WRITE_VALUE(m_auto_mode_isSet, "autoMode", auto_mode);
    WRITE_VALUE(m_inverted_isSet, "inverted", inverted);
    WRITE_VALUE(m_minimum_phasing_lines_isSet, "minimumPhasingLines", minimum_phasing_lines);
    WRITE_VALUE(m_start_confirm_seconds_isSet, "startConfirmSeconds", start_confirm_seconds);
    WRITE_VALUE(m_stop_confirm_seconds_isSet, "stopConfirmSeconds", stop_confirm_seconds);
    WRITE_VALUE(m_manual_clock_correction_ppm_isSet, "manualClockCorrectionPpm", manual_clock_correction_ppm);
    WRITE_VALUE(m_max_rows_isSet, "maxRows", max_rows);
    WRITE_VALUE(m_auto_save_isSet, "autoSave", auto_save);
    WRITE_VALUE(m_auto_save_path_isSet, "autoSavePath", *auto_save_path);
    WRITE_VALUE(m_display_inverted_isSet, "displayInverted", display_inverted);
    WRITE_VALUE(m_display_contrast_isSet, "displayContrast", display_contrast);
    WRITE_VALUE(m_display_threshold_isSet, "displayThreshold", display_threshold);
    WRITE_VALUE(m_horizontal_alignment_isSet, "horizontalAlignment", horizontal_alignment);
    WRITE_VALUE(m_display_slant_correction_ppm_isSet, "displaySlantCorrectionPpm", display_slant_correction_ppm);
    WRITE_VALUE(m_display_rotation_isSet, "displayRotation", display_rotation);
    WRITE_VALUE(m_display_zoom_percent_isSet, "displayZoomPercent", display_zoom_percent);
    WRITE_VALUE(m_auto_scroll_isSet, "autoScroll", auto_scroll);
    WRITE_VALUE(m_auto_slant_isSet, "autoSlant", auto_slant);
    WRITE_VALUE(m_rgb_color_isSet, "rgbColor", rgb_color);
    WRITE_VALUE(m_title_isSet, "title", *title);
    WRITE_VALUE(m_stream_index_isSet, "streamIndex", stream_index);
    WRITE_VALUE(m_use_reverse_api_isSet, "useReverseAPI", use_reverse_api);
    WRITE_VALUE(m_reverse_api_address_isSet, "reverseAPIAddress", *reverse_api_address);
    WRITE_VALUE(m_reverse_api_port_isSet, "reverseAPIPort", reverse_api_port);
    WRITE_VALUE(m_reverse_api_device_index_isSet, "reverseAPIDeviceIndex", reverse_api_device_index);
    WRITE_VALUE(m_reverse_api_channel_index_isSet, "reverseAPIChannelIndex", reverse_api_channel_index);
#undef WRITE_VALUE
    return object;
}

bool SWGWefaxDemodSettings::isSet()
{
    return m_input_frequency_offset_isSet || m_rf_bandwidth_isSet || m_fm_deviation_isSet
        || m_ioc_isSet || m_lines_per_minute_isSet || m_auto_mode_isSet || m_inverted_isSet
        || m_minimum_phasing_lines_isSet || m_manual_clock_correction_ppm_isSet || m_max_rows_isSet
        || m_start_confirm_seconds_isSet || m_stop_confirm_seconds_isSet
        || m_auto_save_isSet || m_auto_save_path_isSet || m_display_inverted_isSet
        || m_display_contrast_isSet || m_display_rotation_isSet || m_display_zoom_percent_isSet
        || m_display_threshold_isSet || m_horizontal_alignment_isSet || m_display_slant_correction_ppm_isSet
        || m_auto_scroll_isSet || m_auto_slant_isSet || m_rgb_color_isSet || m_title_isSet
        || m_stream_index_isSet || m_use_reverse_api_isSet || m_reverse_api_address_isSet
        || m_reverse_api_port_isSet || m_reverse_api_device_index_isSet || m_reverse_api_channel_index_isSet;
}

}
