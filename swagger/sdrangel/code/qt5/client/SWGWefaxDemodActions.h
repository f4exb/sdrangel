#ifndef SWGWefaxDemodActions_H_
#define SWGWefaxDemodActions_H_

#include <QJsonObject>
#include "SWGObject.h"
#include "export.h"

namespace SWGSDRangel {

class SWG_API SWGWefaxDemodActions : public SWGObject
{
public:
    SWGWefaxDemodActions();
    explicit SWGWefaxDemodActions(QString *json);
    ~SWGWefaxDemodActions() override;
    void init();
    void cleanup();
    QString asJson() override;
    QJsonObject *asJsonObject() override;
    void fromJsonObject(QJsonObject& json) override;
    SWGWefaxDemodActions *fromJson(QString& jsonString) override;

    qint32 getStartPhasing() const { return start_phasing; }
    void setStartPhasing(qint32 value) { start_phasing = value; m_start_phasing_isSet = true; }
    qint32 getFinishPhasing() const { return finish_phasing; }
    void setFinishPhasing(qint32 value) { finish_phasing = value; m_finish_phasing_isSet = true; }
    qint32 getStartReceiving() const { return start_receiving; }
    void setStartReceiving(qint32 value) { start_receiving = value; m_start_receiving_isSet = true; }
    qint32 getStop() const { return stop; }
    void setStop(qint32 value) { stop = value; m_stop_isSet = true; }
    qint32 getClear() const { return clear; }
    void setClear(qint32 value) { clear = value; m_clear_isSet = true; }
    qint32 getSave() const { return save; }
    void setSave(qint32 value) { save = value; m_save_isSet = true; }
    bool isSet() override;

private:
    qint32 start_phasing;
    qint32 finish_phasing;
    qint32 start_receiving;
    qint32 stop;
    qint32 clear;
    qint32 save;
    bool m_start_phasing_isSet;
    bool m_finish_phasing_isSet;
    bool m_start_receiving_isSet;
    bool m_stop_isSet;
    bool m_clear_isSet;
    bool m_save_isSet;
};

}

#endif
