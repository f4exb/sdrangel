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

/*
 * SWGSkyMapSettings.h
 *
 * Sky Map
 */

#ifndef SWGSkyMapSettings_H_
#define SWGSkyMapSettings_H_

#include <QJsonObject>


#include "SWGRollupState.h"
#include <QString>

#include "SWGObject.h"
#include "export.h"

namespace SWGSDRangel {

class SWG_API SWGSkyMapSettings: public SWGObject {
public:
    SWGSkyMapSettings();
    SWGSkyMapSettings(QString* json);
    virtual ~SWGSkyMapSettings();
    void init();
    void cleanup();

    virtual QString asJson () override;
    virtual QJsonObject* asJsonObject() override;
    virtual void fromJsonObject(QJsonObject &json) override;
    virtual SWGSkyMapSettings* fromJson(QString &jsonString) override;

    qint32 getDisplayNames();
    void setDisplayNames(qint32 display_names);

    qint32 getDisplayConstellations();
    void setDisplayConstellations(qint32 display_constellations);

    qint32 getDisplayReticle();
    void setDisplayReticle(qint32 display_reticle);

    qint32 getDisplayGrid();
    void setDisplayGrid(qint32 display_grid);

    qint32 getDisplayAntennaFoV();
    void setDisplayAntennaFoV(qint32 display_antenna_fo_v);

    QString* getMap();
    void setMap(QString* map);

    QString* getBackground();
    void setBackground(QString* background);

    QString* getProjection();
    void setProjection(QString* projection);

    QString* getSource();
    void setSource(QString* source);

    qint32 getTrack();
    void setTrack(qint32 track);

    float getLatitude();
    void setLatitude(float latitude);

    float getLongitude();
    void setLongitude(float longitude);

    float getAltitude();
    void setAltitude(float altitude);

    float getHpbw();
    void setHpbw(float hpbw);

    float getUseMyPosition();
    void setUseMyPosition(float use_my_position);

    QString* getTitle();
    void setTitle(QString* title);

    qint32 getRgbColor();
    void setRgbColor(qint32 rgb_color);

    qint32 getUseReverseApi();
    void setUseReverseApi(qint32 use_reverse_api);

    QString* getReverseApiAddress();
    void setReverseApiAddress(QString* reverse_api_address);

    qint32 getReverseApiPort();
    void setReverseApiPort(qint32 reverse_api_port);

    qint32 getReverseApiFeatureSetIndex();
    void setReverseApiFeatureSetIndex(qint32 reverse_api_feature_set_index);

    qint32 getReverseApiFeatureIndex();
    void setReverseApiFeatureIndex(qint32 reverse_api_feature_index);

    SWGRollupState* getRollupState();
    void setRollupState(SWGRollupState* rollup_state);


    virtual bool isSet() override;

private:
    qint32 display_names;
    bool m_display_names_isSet;

    qint32 display_constellations;
    bool m_display_constellations_isSet;

    qint32 display_reticle;
    bool m_display_reticle_isSet;

    qint32 display_grid;
    bool m_display_grid_isSet;

    qint32 display_antenna_fo_v;
    bool m_display_antenna_fo_v_isSet;

    QString* map;
    bool m_map_isSet;

    QString* background;
    bool m_background_isSet;

    QString* projection;
    bool m_projection_isSet;

    QString* source;
    bool m_source_isSet;

    qint32 track;
    bool m_track_isSet;

    float latitude;
    bool m_latitude_isSet;

    float longitude;
    bool m_longitude_isSet;

    float altitude;
    bool m_altitude_isSet;

    float hpbw;
    bool m_hpbw_isSet;

    float use_my_position;
    bool m_use_my_position_isSet;

    QString* title;
    bool m_title_isSet;

    qint32 rgb_color;
    bool m_rgb_color_isSet;

    qint32 use_reverse_api;
    bool m_use_reverse_api_isSet;

    QString* reverse_api_address;
    bool m_reverse_api_address_isSet;

    qint32 reverse_api_port;
    bool m_reverse_api_port_isSet;

    qint32 reverse_api_feature_set_index;
    bool m_reverse_api_feature_set_index_isSet;

    qint32 reverse_api_feature_index;
    bool m_reverse_api_feature_index_isSet;

    SWGRollupState* rollup_state;
    bool m_rollup_state_isSet;

};

}

#endif /* SWGSkyMapSettings_H_ */
