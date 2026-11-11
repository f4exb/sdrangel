// Preserve JSON types and field presence for strict DSEMod action validation.
#ifndef SWGDSEModActions_H_
#define SWGDSEModActions_H_
#include <QJsonObject>
#include <QString>
#include "SWGObject.h"
#include "export.h"
namespace SWGSDRangel {
class SWG_API SWGDSEModActions : public SWGObject {
public:
    SWGDSEModActions() = default;
    explicit SWGDSEModActions(QString* json) { fromJson(*json); }
    ~SWGDSEModActions() override = default;
    void init() { m_json = {}; }
    void cleanup() { m_json = {}; }
    QString asJson() override;
    QJsonObject* asJsonObject() override { return new QJsonObject(m_json); }
    void fromJsonObject(QJsonObject& json) override { m_json = json; }
    SWGDSEModActions* fromJson(QString& json) override;
    bool isSet() override { return !m_json.isEmpty(); }
    QJsonObject getTransmit() const { return m_json["transmit"].toObject(); }
    void setTransmit(const QJsonObject& value) { m_json["transmit"] = value; }
    QJsonObject getMessage() const { return m_json["message"].toObject(); }
    void setMessage(const QJsonObject& value) { m_json["message"] = value; }
    QString* getDtmf() { m_dtmf = m_json["dtmf"].toString(); return &m_dtmf; }
    void setDtmf(QString* value) { if (value) { m_json["dtmf"] = *value; delete value; } }
private:
    QJsonObject m_json;
    QString m_dtmf;
};
}
#endif
