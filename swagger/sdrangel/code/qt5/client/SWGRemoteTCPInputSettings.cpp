/**
 * SDRangel
 * This is the web REST/JSON API of SDRangel SDR software. SDRangel is an Open Source Qt5/OpenGL 3.0+ (4.3+ in Windows) GUI and server Software Defined Radio and signal analyzer in software. It supports Airspy, BladeRF, HackRF, LimeSDR, PlutoSDR, RTL-SDR, SDRplay RSP1 and FunCube    ---   Limitations and specifcities:    * In SDRangel GUI the first Rx device set cannot be deleted. Conversely the server starts with no device sets and its number of device sets can be reduced to zero by as many calls as necessary to /sdrangel/deviceset with DELETE method.   * Preset import and export from/to file is a server only feature.   * Device set focus is a GUI only feature.   * The following channels are not implemented (status 501 is returned): ATV and DATV demodulators, Channel Analyzer NG, LoRa demodulator   * The device settings and report structures contains only the sub-structure corresponding to the device type. The DeviceSettings and DeviceReport structures documented here shows all of them but only one will be or should be present at a time   * The channel settings and report structures contains only the sub-structure corresponding to the channel type. The ChannelSettings and ChannelReport structures documented here shows all of them but only one will be or should be present at a time    --- 
 *
 * OpenAPI spec version: 7.0.0
 * Contact: f4exb06@gmail.com
 *
 * NOTE: This class is auto generated by the swagger code generator program.
 * https://github.com/swagger-api/swagger-codegen.git
 * Do not edit the class manually.
 */


#include "SWGRemoteTCPInputSettings.h"

#include "SWGHelpers.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QObject>
#include <QDebug>

namespace SWGSDRangel {

SWGRemoteTCPInputSettings::SWGRemoteTCPInputSettings(QString* json) {
    init();
    this->fromJson(*json);
}

SWGRemoteTCPInputSettings::SWGRemoteTCPInputSettings() {
    center_frequency = 0L;
    m_center_frequency_isSet = false;
    lo_ppm_correction = 0;
    m_lo_ppm_correction_isSet = false;
    dc_block = 0;
    m_dc_block_isSet = false;
    iq_correction = 0;
    m_iq_correction_isSet = false;
    bias_tee = 0;
    m_bias_tee_isSet = false;
    direct_sampling = 0;
    m_direct_sampling_isSet = false;
    dev_sample_rate = 0;
    m_dev_sample_rate_isSet = false;
    log2_decim = 0;
    m_log2_decim_isSet = false;
    gain = 0;
    m_gain_isSet = false;
    agc = 0;
    m_agc_isSet = false;
    rf_bw = 0;
    m_rf_bw_isSet = false;
    input_frequency_offset = 0L;
    m_input_frequency_offset_isSet = false;
    channel_gain = 0;
    m_channel_gain_isSet = false;
    channel_sample_rate = 0;
    m_channel_sample_rate_isSet = false;
    channel_decimation = 0;
    m_channel_decimation_isSet = false;
    sample_bits = 0;
    m_sample_bits_isSet = false;
    data_address = nullptr;
    m_data_address_isSet = false;
    data_port = 0;
    m_data_port_isSet = false;
    override_remote_settings = 0;
    m_override_remote_settings_isSet = false;
    pre_fill = 0;
    m_pre_fill_isSet = false;
    protocol = nullptr;
    m_protocol_isSet = false;
    use_reverse_api = 0;
    m_use_reverse_api_isSet = false;
    reverse_api_address = nullptr;
    m_reverse_api_address_isSet = false;
    reverse_api_port = 0;
    m_reverse_api_port_isSet = false;
    reverse_api_device_index = 0;
    m_reverse_api_device_index_isSet = false;
}

SWGRemoteTCPInputSettings::~SWGRemoteTCPInputSettings() {
    this->cleanup();
}

void
SWGRemoteTCPInputSettings::init() {
    center_frequency = 0L;
    m_center_frequency_isSet = false;
    lo_ppm_correction = 0;
    m_lo_ppm_correction_isSet = false;
    dc_block = 0;
    m_dc_block_isSet = false;
    iq_correction = 0;
    m_iq_correction_isSet = false;
    bias_tee = 0;
    m_bias_tee_isSet = false;
    direct_sampling = 0;
    m_direct_sampling_isSet = false;
    dev_sample_rate = 0;
    m_dev_sample_rate_isSet = false;
    log2_decim = 0;
    m_log2_decim_isSet = false;
    gain = 0;
    m_gain_isSet = false;
    agc = 0;
    m_agc_isSet = false;
    rf_bw = 0;
    m_rf_bw_isSet = false;
    input_frequency_offset = 0L;
    m_input_frequency_offset_isSet = false;
    channel_gain = 0;
    m_channel_gain_isSet = false;
    channel_sample_rate = 0;
    m_channel_sample_rate_isSet = false;
    channel_decimation = 0;
    m_channel_decimation_isSet = false;
    sample_bits = 0;
    m_sample_bits_isSet = false;
    data_address = new QString("");
    m_data_address_isSet = false;
    data_port = 0;
    m_data_port_isSet = false;
    override_remote_settings = 0;
    m_override_remote_settings_isSet = false;
    pre_fill = 0;
    m_pre_fill_isSet = false;
    protocol = new QString("");
    m_protocol_isSet = false;
    use_reverse_api = 0;
    m_use_reverse_api_isSet = false;
    reverse_api_address = new QString("");
    m_reverse_api_address_isSet = false;
    reverse_api_port = 0;
    m_reverse_api_port_isSet = false;
    reverse_api_device_index = 0;
    m_reverse_api_device_index_isSet = false;
}

void
SWGRemoteTCPInputSettings::cleanup() {
















    if(data_address != nullptr) { 
        delete data_address;
    }



    if(protocol != nullptr) { 
        delete protocol;
    }

    if(reverse_api_address != nullptr) { 
        delete reverse_api_address;
    }


}

SWGRemoteTCPInputSettings*
SWGRemoteTCPInputSettings::fromJson(QString &json) {
    QByteArray array (json.toStdString().c_str());
    QJsonDocument doc = QJsonDocument::fromJson(array);
    QJsonObject jsonObject = doc.object();
    this->fromJsonObject(jsonObject);
    return this;
}

void
SWGRemoteTCPInputSettings::fromJsonObject(QJsonObject &pJson) {
    ::SWGSDRangel::setValue(&center_frequency, pJson["centerFrequency"], "qint64", "");
    
    ::SWGSDRangel::setValue(&lo_ppm_correction, pJson["loPpmCorrection"], "qint32", "");
    
    ::SWGSDRangel::setValue(&dc_block, pJson["dcBlock"], "qint32", "");
    
    ::SWGSDRangel::setValue(&iq_correction, pJson["iqCorrection"], "qint32", "");
    
    ::SWGSDRangel::setValue(&bias_tee, pJson["biasTee"], "qint32", "");
    
    ::SWGSDRangel::setValue(&direct_sampling, pJson["directSampling"], "qint32", "");
    
    ::SWGSDRangel::setValue(&dev_sample_rate, pJson["devSampleRate"], "qint32", "");
    
    ::SWGSDRangel::setValue(&log2_decim, pJson["log2Decim"], "qint32", "");
    
    ::SWGSDRangel::setValue(&gain, pJson["gain"], "qint32", "");
    
    ::SWGSDRangel::setValue(&agc, pJson["agc"], "qint32", "");
    
    ::SWGSDRangel::setValue(&rf_bw, pJson["rfBW"], "qint32", "");
    
    ::SWGSDRangel::setValue(&input_frequency_offset, pJson["inputFrequencyOffset"], "qint64", "");
    
    ::SWGSDRangel::setValue(&channel_gain, pJson["channelGain"], "qint32", "");
    
    ::SWGSDRangel::setValue(&channel_sample_rate, pJson["channelSampleRate"], "qint32", "");
    
    ::SWGSDRangel::setValue(&channel_decimation, pJson["channelDecimation"], "qint32", "");
    
    ::SWGSDRangel::setValue(&sample_bits, pJson["sampleBits"], "qint32", "");
    
    ::SWGSDRangel::setValue(&data_address, pJson["dataAddress"], "QString", "QString");
    
    ::SWGSDRangel::setValue(&data_port, pJson["dataPort"], "qint32", "");
    
    ::SWGSDRangel::setValue(&override_remote_settings, pJson["overrideRemoteSettings"], "qint32", "");
    
    ::SWGSDRangel::setValue(&pre_fill, pJson["preFill"], "qint32", "");
    
    ::SWGSDRangel::setValue(&protocol, pJson["protocol"], "QString", "QString");
    
    ::SWGSDRangel::setValue(&use_reverse_api, pJson["useReverseAPI"], "qint32", "");
    
    ::SWGSDRangel::setValue(&reverse_api_address, pJson["reverseAPIAddress"], "QString", "QString");
    
    ::SWGSDRangel::setValue(&reverse_api_port, pJson["reverseAPIPort"], "qint32", "");
    
    ::SWGSDRangel::setValue(&reverse_api_device_index, pJson["reverseAPIDeviceIndex"], "qint32", "");
    
}

QString
SWGRemoteTCPInputSettings::asJson ()
{
    QJsonObject* obj = this->asJsonObject();

    QJsonDocument doc(*obj);
    QByteArray bytes = doc.toJson();
    delete obj;
    return QString(bytes);
}

QJsonObject*
SWGRemoteTCPInputSettings::asJsonObject() {
    QJsonObject* obj = new QJsonObject();
    if(m_center_frequency_isSet){
        obj->insert("centerFrequency", QJsonValue(center_frequency));
    }
    if(m_lo_ppm_correction_isSet){
        obj->insert("loPpmCorrection", QJsonValue(lo_ppm_correction));
    }
    if(m_dc_block_isSet){
        obj->insert("dcBlock", QJsonValue(dc_block));
    }
    if(m_iq_correction_isSet){
        obj->insert("iqCorrection", QJsonValue(iq_correction));
    }
    if(m_bias_tee_isSet){
        obj->insert("biasTee", QJsonValue(bias_tee));
    }
    if(m_direct_sampling_isSet){
        obj->insert("directSampling", QJsonValue(direct_sampling));
    }
    if(m_dev_sample_rate_isSet){
        obj->insert("devSampleRate", QJsonValue(dev_sample_rate));
    }
    if(m_log2_decim_isSet){
        obj->insert("log2Decim", QJsonValue(log2_decim));
    }
    if(m_gain_isSet){
        obj->insert("gain", QJsonValue(gain));
    }
    if(m_agc_isSet){
        obj->insert("agc", QJsonValue(agc));
    }
    if(m_rf_bw_isSet){
        obj->insert("rfBW", QJsonValue(rf_bw));
    }
    if(m_input_frequency_offset_isSet){
        obj->insert("inputFrequencyOffset", QJsonValue(input_frequency_offset));
    }
    if(m_channel_gain_isSet){
        obj->insert("channelGain", QJsonValue(channel_gain));
    }
    if(m_channel_sample_rate_isSet){
        obj->insert("channelSampleRate", QJsonValue(channel_sample_rate));
    }
    if(m_channel_decimation_isSet){
        obj->insert("channelDecimation", QJsonValue(channel_decimation));
    }
    if(m_sample_bits_isSet){
        obj->insert("sampleBits", QJsonValue(sample_bits));
    }
    if(data_address != nullptr && *data_address != QString("")){
        toJsonValue(QString("dataAddress"), data_address, obj, QString("QString"));
    }
    if(m_data_port_isSet){
        obj->insert("dataPort", QJsonValue(data_port));
    }
    if(m_override_remote_settings_isSet){
        obj->insert("overrideRemoteSettings", QJsonValue(override_remote_settings));
    }
    if(m_pre_fill_isSet){
        obj->insert("preFill", QJsonValue(pre_fill));
    }
    if(protocol != nullptr && *protocol != QString("")){
        toJsonValue(QString("protocol"), protocol, obj, QString("QString"));
    }
    if(m_use_reverse_api_isSet){
        obj->insert("useReverseAPI", QJsonValue(use_reverse_api));
    }
    if(reverse_api_address != nullptr && *reverse_api_address != QString("")){
        toJsonValue(QString("reverseAPIAddress"), reverse_api_address, obj, QString("QString"));
    }
    if(m_reverse_api_port_isSet){
        obj->insert("reverseAPIPort", QJsonValue(reverse_api_port));
    }
    if(m_reverse_api_device_index_isSet){
        obj->insert("reverseAPIDeviceIndex", QJsonValue(reverse_api_device_index));
    }

    return obj;
}

qint64
SWGRemoteTCPInputSettings::getCenterFrequency() {
    return center_frequency;
}
void
SWGRemoteTCPInputSettings::setCenterFrequency(qint64 center_frequency) {
    this->center_frequency = center_frequency;
    this->m_center_frequency_isSet = true;
}

qint32
SWGRemoteTCPInputSettings::getLoPpmCorrection() {
    return lo_ppm_correction;
}
void
SWGRemoteTCPInputSettings::setLoPpmCorrection(qint32 lo_ppm_correction) {
    this->lo_ppm_correction = lo_ppm_correction;
    this->m_lo_ppm_correction_isSet = true;
}

qint32
SWGRemoteTCPInputSettings::getDcBlock() {
    return dc_block;
}
void
SWGRemoteTCPInputSettings::setDcBlock(qint32 dc_block) {
    this->dc_block = dc_block;
    this->m_dc_block_isSet = true;
}

qint32
SWGRemoteTCPInputSettings::getIqCorrection() {
    return iq_correction;
}
void
SWGRemoteTCPInputSettings::setIqCorrection(qint32 iq_correction) {
    this->iq_correction = iq_correction;
    this->m_iq_correction_isSet = true;
}

qint32
SWGRemoteTCPInputSettings::getBiasTee() {
    return bias_tee;
}
void
SWGRemoteTCPInputSettings::setBiasTee(qint32 bias_tee) {
    this->bias_tee = bias_tee;
    this->m_bias_tee_isSet = true;
}

qint32
SWGRemoteTCPInputSettings::getDirectSampling() {
    return direct_sampling;
}
void
SWGRemoteTCPInputSettings::setDirectSampling(qint32 direct_sampling) {
    this->direct_sampling = direct_sampling;
    this->m_direct_sampling_isSet = true;
}

qint32
SWGRemoteTCPInputSettings::getDevSampleRate() {
    return dev_sample_rate;
}
void
SWGRemoteTCPInputSettings::setDevSampleRate(qint32 dev_sample_rate) {
    this->dev_sample_rate = dev_sample_rate;
    this->m_dev_sample_rate_isSet = true;
}

qint32
SWGRemoteTCPInputSettings::getLog2Decim() {
    return log2_decim;
}
void
SWGRemoteTCPInputSettings::setLog2Decim(qint32 log2_decim) {
    this->log2_decim = log2_decim;
    this->m_log2_decim_isSet = true;
}

qint32
SWGRemoteTCPInputSettings::getGain() {
    return gain;
}
void
SWGRemoteTCPInputSettings::setGain(qint32 gain) {
    this->gain = gain;
    this->m_gain_isSet = true;
}

qint32
SWGRemoteTCPInputSettings::getAgc() {
    return agc;
}
void
SWGRemoteTCPInputSettings::setAgc(qint32 agc) {
    this->agc = agc;
    this->m_agc_isSet = true;
}

qint32
SWGRemoteTCPInputSettings::getRfBw() {
    return rf_bw;
}
void
SWGRemoteTCPInputSettings::setRfBw(qint32 rf_bw) {
    this->rf_bw = rf_bw;
    this->m_rf_bw_isSet = true;
}

qint64
SWGRemoteTCPInputSettings::getInputFrequencyOffset() {
    return input_frequency_offset;
}
void
SWGRemoteTCPInputSettings::setInputFrequencyOffset(qint64 input_frequency_offset) {
    this->input_frequency_offset = input_frequency_offset;
    this->m_input_frequency_offset_isSet = true;
}

qint32
SWGRemoteTCPInputSettings::getChannelGain() {
    return channel_gain;
}
void
SWGRemoteTCPInputSettings::setChannelGain(qint32 channel_gain) {
    this->channel_gain = channel_gain;
    this->m_channel_gain_isSet = true;
}

qint32
SWGRemoteTCPInputSettings::getChannelSampleRate() {
    return channel_sample_rate;
}
void
SWGRemoteTCPInputSettings::setChannelSampleRate(qint32 channel_sample_rate) {
    this->channel_sample_rate = channel_sample_rate;
    this->m_channel_sample_rate_isSet = true;
}

qint32
SWGRemoteTCPInputSettings::getChannelDecimation() {
    return channel_decimation;
}
void
SWGRemoteTCPInputSettings::setChannelDecimation(qint32 channel_decimation) {
    this->channel_decimation = channel_decimation;
    this->m_channel_decimation_isSet = true;
}

qint32
SWGRemoteTCPInputSettings::getSampleBits() {
    return sample_bits;
}
void
SWGRemoteTCPInputSettings::setSampleBits(qint32 sample_bits) {
    this->sample_bits = sample_bits;
    this->m_sample_bits_isSet = true;
}

QString*
SWGRemoteTCPInputSettings::getDataAddress() {
    return data_address;
}
void
SWGRemoteTCPInputSettings::setDataAddress(QString* data_address) {
    this->data_address = data_address;
    this->m_data_address_isSet = true;
}

qint32
SWGRemoteTCPInputSettings::getDataPort() {
    return data_port;
}
void
SWGRemoteTCPInputSettings::setDataPort(qint32 data_port) {
    this->data_port = data_port;
    this->m_data_port_isSet = true;
}

qint32
SWGRemoteTCPInputSettings::getOverrideRemoteSettings() {
    return override_remote_settings;
}
void
SWGRemoteTCPInputSettings::setOverrideRemoteSettings(qint32 override_remote_settings) {
    this->override_remote_settings = override_remote_settings;
    this->m_override_remote_settings_isSet = true;
}

qint32
SWGRemoteTCPInputSettings::getPreFill() {
    return pre_fill;
}
void
SWGRemoteTCPInputSettings::setPreFill(qint32 pre_fill) {
    this->pre_fill = pre_fill;
    this->m_pre_fill_isSet = true;
}

QString*
SWGRemoteTCPInputSettings::getProtocol() {
    return protocol;
}
void
SWGRemoteTCPInputSettings::setProtocol(QString* protocol) {
    this->protocol = protocol;
    this->m_protocol_isSet = true;
}

qint32
SWGRemoteTCPInputSettings::getUseReverseApi() {
    return use_reverse_api;
}
void
SWGRemoteTCPInputSettings::setUseReverseApi(qint32 use_reverse_api) {
    this->use_reverse_api = use_reverse_api;
    this->m_use_reverse_api_isSet = true;
}

QString*
SWGRemoteTCPInputSettings::getReverseApiAddress() {
    return reverse_api_address;
}
void
SWGRemoteTCPInputSettings::setReverseApiAddress(QString* reverse_api_address) {
    this->reverse_api_address = reverse_api_address;
    this->m_reverse_api_address_isSet = true;
}

qint32
SWGRemoteTCPInputSettings::getReverseApiPort() {
    return reverse_api_port;
}
void
SWGRemoteTCPInputSettings::setReverseApiPort(qint32 reverse_api_port) {
    this->reverse_api_port = reverse_api_port;
    this->m_reverse_api_port_isSet = true;
}

qint32
SWGRemoteTCPInputSettings::getReverseApiDeviceIndex() {
    return reverse_api_device_index;
}
void
SWGRemoteTCPInputSettings::setReverseApiDeviceIndex(qint32 reverse_api_device_index) {
    this->reverse_api_device_index = reverse_api_device_index;
    this->m_reverse_api_device_index_isSet = true;
}


bool
SWGRemoteTCPInputSettings::isSet(){
    bool isObjectUpdated = false;
    do{
        if(m_center_frequency_isSet){
            isObjectUpdated = true; break;
        }
        if(m_lo_ppm_correction_isSet){
            isObjectUpdated = true; break;
        }
        if(m_dc_block_isSet){
            isObjectUpdated = true; break;
        }
        if(m_iq_correction_isSet){
            isObjectUpdated = true; break;
        }
        if(m_bias_tee_isSet){
            isObjectUpdated = true; break;
        }
        if(m_direct_sampling_isSet){
            isObjectUpdated = true; break;
        }
        if(m_dev_sample_rate_isSet){
            isObjectUpdated = true; break;
        }
        if(m_log2_decim_isSet){
            isObjectUpdated = true; break;
        }
        if(m_gain_isSet){
            isObjectUpdated = true; break;
        }
        if(m_agc_isSet){
            isObjectUpdated = true; break;
        }
        if(m_rf_bw_isSet){
            isObjectUpdated = true; break;
        }
        if(m_input_frequency_offset_isSet){
            isObjectUpdated = true; break;
        }
        if(m_channel_gain_isSet){
            isObjectUpdated = true; break;
        }
        if(m_channel_sample_rate_isSet){
            isObjectUpdated = true; break;
        }
        if(m_channel_decimation_isSet){
            isObjectUpdated = true; break;
        }
        if(m_sample_bits_isSet){
            isObjectUpdated = true; break;
        }
        if(data_address && *data_address != QString("")){
            isObjectUpdated = true; break;
        }
        if(m_data_port_isSet){
            isObjectUpdated = true; break;
        }
        if(m_override_remote_settings_isSet){
            isObjectUpdated = true; break;
        }
        if(m_pre_fill_isSet){
            isObjectUpdated = true; break;
        }
        if(protocol && *protocol != QString("")){
            isObjectUpdated = true; break;
        }
        if(m_use_reverse_api_isSet){
            isObjectUpdated = true; break;
        }
        if(reverse_api_address && *reverse_api_address != QString("")){
            isObjectUpdated = true; break;
        }
        if(m_reverse_api_port_isSet){
            isObjectUpdated = true; break;
        }
        if(m_reverse_api_device_index_isSet){
            isObjectUpdated = true; break;
        }
    }while(false);
    return isObjectUpdated;
}
}

