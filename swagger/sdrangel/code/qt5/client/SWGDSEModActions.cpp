#include "SWGDSEModActions.h"
#include <QJsonDocument>
namespace SWGSDRangel {
QString SWGDSEModActions::asJson() { return QString::fromUtf8(QJsonDocument(m_json).toJson()); }
SWGDSEModActions* SWGDSEModActions::fromJson(QString& json) {
    m_json = QJsonDocument::fromJson(json.toUtf8()).object(); return this;
}
}
