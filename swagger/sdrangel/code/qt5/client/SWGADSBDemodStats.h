#ifndef SWGADSBDemodStats_H_
#define SWGADSBDemodStats_H_
#include <QJsonObject>
#include "SWGObject.h"
#include "export.h"
namespace SWGSDRangel {
class SWG_API SWGADSBDemodStats: public SWGObject {
public:
    SWGADSBDemodStats();
    SWGADSBDemodStats(QString* json);
    virtual ~SWGADSBDemodStats();
    void init();
    void cleanup();
    virtual QString asJson() override;
    virtual QJsonObject* asJsonObject() override;
    virtual void fromJsonObject(QJsonObject &json) override;
    virtual SWGADSBDemodStats* fromJson(QString &jsonString) override;
    qint64 getCorrelatorMatches();
    void setCorrelatorMatches(qint64 correlator_matches);
    qint64 getAdsbFrames();
    void setAdsbFrames(qint64 adsb_frames);
    qint64 getModesFrames();
    void setModesFrames(qint64 modes_frames);
    qint64 getPreambleFails();
    void setPreambleFails(qint64 preamble_fails);
    qint64 getCrcFails();
    void setCrcFails(qint64 crc_fails);
    qint64 getTypeFails();
    void setTypeFails(qint64 type_fails);
    qint64 getInvalidFails();
    void setInvalidFails(qint64 invalid_fails);
    qint64 getIcaoFails();
    void setIcaoFails(qint64 icao_fails);
    virtual bool isSet() override;
private:
    qint64 correlator_matches; bool m_correlator_matches_isSet;
    qint64 adsb_frames; bool m_adsb_frames_isSet;
    qint64 modes_frames; bool m_modes_frames_isSet;
    qint64 preamble_fails; bool m_preamble_fails_isSet;
    qint64 crc_fails; bool m_crc_fails_isSet;
    qint64 type_fails; bool m_type_fails_isSet;
    qint64 invalid_fails; bool m_invalid_fails_isSet;
    qint64 icao_fails; bool m_icao_fails_isSet;
};
}
#endif
