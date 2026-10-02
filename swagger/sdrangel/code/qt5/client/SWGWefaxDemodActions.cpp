#include "SWGWefaxDemodActions.h"

#include <QJsonDocument>
#include "SWGHelpers.h"

namespace SWGSDRangel {

SWGWefaxDemodActions::SWGWefaxDemodActions() { init(); }
SWGWefaxDemodActions::SWGWefaxDemodActions(QString *json) { init(); fromJson(*json); }
SWGWefaxDemodActions::~SWGWefaxDemodActions() { cleanup(); }

void SWGWefaxDemodActions::init()
{
    start_phasing = finish_phasing = start_receiving = stop = clear = save = 0;
    m_start_phasing_isSet = m_finish_phasing_isSet = m_start_receiving_isSet = false;
    m_stop_isSet = m_clear_isSet = m_save_isSet = false;
}

void SWGWefaxDemodActions::cleanup() {}

SWGWefaxDemodActions *SWGWefaxDemodActions::fromJson(QString& json)
{
    QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    QJsonObject object = document.object();
    fromJsonObject(object);
    return this;
}

void SWGWefaxDemodActions::fromJsonObject(QJsonObject& json)
{
#define READ_ACTION(name, member, flag) \
    if (json.contains(name)) { ::SWGSDRangel::setValue(&member, json[name], "qint32", ""); flag = true; }
    READ_ACTION("startPhasing", start_phasing, m_start_phasing_isSet)
    READ_ACTION("finishPhasing", finish_phasing, m_finish_phasing_isSet)
    READ_ACTION("startReceiving", start_receiving, m_start_receiving_isSet)
    READ_ACTION("stop", stop, m_stop_isSet)
    READ_ACTION("clear", clear, m_clear_isSet)
    READ_ACTION("save", save, m_save_isSet)
#undef READ_ACTION
}

QString SWGWefaxDemodActions::asJson()
{
    QJsonObject *object = asJsonObject();
    const QString json = QString::fromUtf8(QJsonDocument(*object).toJson());
    delete object;
    return json;
}

QJsonObject *SWGWefaxDemodActions::asJsonObject()
{
    auto *object = new QJsonObject();
#define WRITE_ACTION(name, member, flag) if (flag) object->insert(name, member);
    WRITE_ACTION("startPhasing", start_phasing, m_start_phasing_isSet)
    WRITE_ACTION("finishPhasing", finish_phasing, m_finish_phasing_isSet)
    WRITE_ACTION("startReceiving", start_receiving, m_start_receiving_isSet)
    WRITE_ACTION("stop", stop, m_stop_isSet)
    WRITE_ACTION("clear", clear, m_clear_isSet)
    WRITE_ACTION("save", save, m_save_isSet)
#undef WRITE_ACTION
    return object;
}

bool SWGWefaxDemodActions::isSet()
{
    return m_start_phasing_isSet || m_finish_phasing_isSet || m_start_receiving_isSet
        || m_stop_isSet || m_clear_isSet || m_save_isSet;
}

}
