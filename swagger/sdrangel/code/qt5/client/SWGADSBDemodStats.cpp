#include "SWGADSBDemodStats.h"
#include "SWGHelpers.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QByteArray>
namespace SWGSDRangel {
SWGADSBDemodStats::SWGADSBDemodStats(QString* json) { init(); this->fromJson(*json); }
SWGADSBDemodStats::SWGADSBDemodStats() {
    correlator_matches = 0; m_correlator_matches_isSet = false;
    adsb_frames = 0; m_adsb_frames_isSet = false;
    modes_frames = 0; m_modes_frames_isSet = false;
    preamble_fails = 0; m_preamble_fails_isSet = false;
    crc_fails = 0; m_crc_fails_isSet = false;
    type_fails = 0; m_type_fails_isSet = false;
    invalid_fails = 0; m_invalid_fails_isSet = false;
    icao_fails = 0; m_icao_fails_isSet = false;
}
SWGADSBDemodStats::~SWGADSBDemodStats() { this->cleanup(); }
void SWGADSBDemodStats::init() {
    correlator_matches = 0; m_correlator_matches_isSet = false;
    adsb_frames = 0; m_adsb_frames_isSet = false;
    modes_frames = 0; m_modes_frames_isSet = false;
    preamble_fails = 0; m_preamble_fails_isSet = false;
    crc_fails = 0; m_crc_fails_isSet = false;
    type_fails = 0; m_type_fails_isSet = false;
    invalid_fails = 0; m_invalid_fails_isSet = false;
    icao_fails = 0; m_icao_fails_isSet = false;
}
void SWGADSBDemodStats::cleanup() {}
SWGADSBDemodStats* SWGADSBDemodStats::fromJson(QString &json) {
    QByteArray array(json.toStdString().c_str());
    QJsonDocument doc = QJsonDocument::fromJson(array);
    QJsonObject obj = doc.object(); this->fromJsonObject(obj); return this;
}
void SWGADSBDemodStats::fromJsonObject(QJsonObject &pJson) {
    ::SWGSDRangel::setValue(&correlator_matches, pJson["correlatorMatches"], "qint64", "");
    ::SWGSDRangel::setValue(&adsb_frames, pJson["adsbFrames"], "qint64", "");
    ::SWGSDRangel::setValue(&modes_frames, pJson["modesFrames"], "qint64", "");
    ::SWGSDRangel::setValue(&preamble_fails, pJson["preambleFails"], "qint64", "");
    ::SWGSDRangel::setValue(&crc_fails, pJson["crcFails"], "qint64", "");
    ::SWGSDRangel::setValue(&type_fails, pJson["typeFails"], "qint64", "");
    ::SWGSDRangel::setValue(&invalid_fails, pJson["invalidFails"], "qint64", "");
    ::SWGSDRangel::setValue(&icao_fails, pJson["icaoFails"], "qint64", "");
}
QString SWGADSBDemodStats::asJson() {
    QJsonObject* obj = this->asJsonObject(); QJsonDocument doc(*obj);
    QByteArray bytes = doc.toJson(); delete obj; return QString(bytes);
}
QJsonObject* SWGADSBDemodStats::asJsonObject() {
    QJsonObject* obj = new QJsonObject();
    if(m_correlator_matches_isSet){ obj->insert("correlatorMatches", QJsonValue(correlator_matches)); }
    if(m_adsb_frames_isSet){ obj->insert("adsbFrames", QJsonValue(adsb_frames)); }
    if(m_modes_frames_isSet){ obj->insert("modesFrames", QJsonValue(modes_frames)); }
    if(m_preamble_fails_isSet){ obj->insert("preambleFails", QJsonValue(preamble_fails)); }
    if(m_crc_fails_isSet){ obj->insert("crcFails", QJsonValue(crc_fails)); }
    if(m_type_fails_isSet){ obj->insert("typeFails", QJsonValue(type_fails)); }
    if(m_invalid_fails_isSet){ obj->insert("invalidFails", QJsonValue(invalid_fails)); }
    if(m_icao_fails_isSet){ obj->insert("icaoFails", QJsonValue(icao_fails)); }
    return obj;
}
qint64 SWGADSBDemodStats::getCorrelatorMatches() { return correlator_matches; }
void SWGADSBDemodStats::setCorrelatorMatches(qint64 correlator_matches) { this->correlator_matches = correlator_matches; this->m_correlator_matches_isSet = true; }
qint64 SWGADSBDemodStats::getAdsbFrames() { return adsb_frames; }
void SWGADSBDemodStats::setAdsbFrames(qint64 adsb_frames) { this->adsb_frames = adsb_frames; this->m_adsb_frames_isSet = true; }
qint64 SWGADSBDemodStats::getModesFrames() { return modes_frames; }
void SWGADSBDemodStats::setModesFrames(qint64 modes_frames) { this->modes_frames = modes_frames; this->m_modes_frames_isSet = true; }
qint64 SWGADSBDemodStats::getPreambleFails() { return preamble_fails; }
void SWGADSBDemodStats::setPreambleFails(qint64 preamble_fails) { this->preamble_fails = preamble_fails; this->m_preamble_fails_isSet = true; }
qint64 SWGADSBDemodStats::getCrcFails() { return crc_fails; }
void SWGADSBDemodStats::setCrcFails(qint64 crc_fails) { this->crc_fails = crc_fails; this->m_crc_fails_isSet = true; }
qint64 SWGADSBDemodStats::getTypeFails() { return type_fails; }
void SWGADSBDemodStats::setTypeFails(qint64 type_fails) { this->type_fails = type_fails; this->m_type_fails_isSet = true; }
qint64 SWGADSBDemodStats::getInvalidFails() { return invalid_fails; }
void SWGADSBDemodStats::setInvalidFails(qint64 invalid_fails) { this->invalid_fails = invalid_fails; this->m_invalid_fails_isSet = true; }
qint64 SWGADSBDemodStats::getIcaoFails() { return icao_fails; }
void SWGADSBDemodStats::setIcaoFails(qint64 icao_fails) { this->icao_fails = icao_fails; this->m_icao_fails_isSet = true; }
bool SWGADSBDemodStats::isSet(){
    bool u=false; do{
        if(m_correlator_matches_isSet){ u=true; break; }
        if(m_adsb_frames_isSet){ u=true; break; }
        if(m_modes_frames_isSet){ u=true; break; }
        if(m_preamble_fails_isSet){ u=true; break; }
        if(m_crc_fails_isSet){ u=true; break; }
        if(m_type_fails_isSet){ u=true; break; }
        if(m_invalid_fails_isSet){ u=true; break; }
        if(m_icao_fails_isSet){ u=true; break; }
    }while(false); return u;
}
}
