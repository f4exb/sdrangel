///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_ILSDEMODGUI_H
#define INCLUDE_ILSDEMODGUI_H

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"
#include "ilsdemod.h"
#include "ilsdemodsettings.h"

#include <QGeoCoordinate>

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class ScopeVis;
class SpectrumVis;
class ILSDemod;
class ILSDemodGUI;

namespace Ui {
    class ILSDemodGUI;
}
class ILSDemodGUI;

class ILSDemodGUI : public ChannelGUI {
    Q_OBJECT

    struct ILS {
        QString m_airportICAO;
        QString m_ident;            // ILS identifier
        QString m_runway;
        int m_frequency;            // In Hz
        float m_trueBearing;        // In degrees
        float m_glidePath;          // In degrees
        double m_latitude;          // Position of threshold
        double m_longitude;
        int m_elevation;            // In feet as it is on most charts - FIXME: Meters
        float m_refHeight;              // ILS reference datum height above threshold
        int m_thresholdToLocalizer; // Distance from localizer antenna (GARP) to threshold (LTP)
        float m_slope;              // In %
    };

    // Send from G/S channel to LOC channel
    class MsgGSAngle : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        float getAngle() const { return m_angle; }

        static MsgGSAngle* create(float angle)
        {
            return new MsgGSAngle(angle);
        }

    private:
        float m_angle;

        MsgGSAngle(float angle) :
            m_angle(angle)
        {}
    };



public:
    static ILSDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual void setWorkspaceIndex(int index) { m_settings.m_workspaceIndex = index; };
    virtual int getWorkspaceIndex() const { return m_settings.m_workspaceIndex; };
    virtual void setGeometryBytes(const QByteArray& blob) { m_settings.m_geometryBytes = blob; };
    virtual QByteArray getGeometryBytes() const { return m_settings.m_geometryBytes; };
    virtual QString getTitle() const { return m_settings.m_title; };
    virtual QColor getTitleColor() const  { return m_settings.m_rgbColor; };
    virtual void zetHidden(bool hidden) { m_settings.m_hidden = hidden; }
    virtual bool getHidden() const { return m_settings.m_hidden; }
    virtual ChannelMarker& getChannelMarker() { return m_channelMarker; }
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }
    virtual void setStreamIndex(int streamIndex) { m_settings.m_streamIndex = streamIndex; }

public slots:
    void channelMarkerChangedByCursor();
    void channelMarkerHighlightedByCursor();

private:
    Ui::ILSDemodGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    ILSDemodSettings m_settings;
    qint64 m_deviceCenterFrequency;
    bool m_doApplySettings;
    ScopeVis* m_scopeVis;
	SpectrumVis* m_spectrumVis;

    ILSDemod* m_ilsDemod;
    bool m_squelchOpen;
    int m_basebandSampleRate;
    uint32_t m_tickCount;
    MessageQueue m_inputMessageQueue;
    int m_markerNo;
    QHash<QString, bool> m_mapMarkers;
    QHash<QString, bool> m_mapILS;
    bool m_disableDrawILS;
    bool m_hasDrawnILS;

    bool m_ilsValid;
    float m_locLatitude;
    float m_locLongitude;
    float m_tdLatitude;
    float m_tdLongitude;
    float m_altitude;       // Threshold, in metres
    float m_locDistance;    // Range of localizer in metres
    float m_gsDistance;
    float m_locToTouchdown;
    float m_locAltitude;

    float m_locAngle;
    float m_gsAngle;

    static const QStringList m_locFrequencies;
    static const QStringList m_gsFrequencies;
    static const QList<ILSDemodGUI::ILS> m_ils;

    explicit ILSDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
    virtual ~ILSDemodGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();
    qint64 getFrequency();
    QString formatFrequency(int frequency) const;
    QString formatDDM(float ddm) const;
    QString formatAngleDirection(float angle) const;
    void removeFromMap(const QString& name);
    void drawILSOnMap();
    void drawPath();
    void clearILSFromMap();
    void addLineToMap(const QString& name, const QString& label, float startLatitude, float startLongitude, float startAltitude, float endLatitude, float endLongitude, float endAltitude);
    void addPolygonToMap(const QString& name, const QString& label, const QList<QGeoCoordinate>& coordinates, QRgb color);
    void updateGPSAngle();
    float calcCourseWidth(int m_thresholdToLocalizer) const;

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

    bool sendToLOCChannel(float angle);
    void closePipes();
    void scanAvailableChannels();
    void handleChannelAdded(int deviceSetIndex, ChannelAPI *channel);
    void handleMessagePipeToBeDeleted(int reason, QObject* object);
    void handleChannelMessageQueue(MessageQueue* messageQueue);
    QSet<ChannelAPI*> m_availableChannels;

private slots:
    void on_deltaFrequency_changed(qint64 value);
    void on_rfBW_valueChanged(int index);
    void on_mode_currentIndexChanged(int index);
    void on_frequency_currentIndexChanged(int index);
    void on_average_clicked(bool checked);
    void on_thresh_valueChanged(int value);
    void on_volume_valueChanged(int value);
    void on_squelch_valueChanged(int value);
    void on_audioMute_toggled(bool checked);
    void on_ddmUnits_currentIndexChanged(int index);
    void on_ident_editingFinished();
    void on_ident_currentIndexChanged(int index);
    void on_runway_editingFinished();
    void on_trueBearing_valueChanged(double value);
    void on_latitude_editingFinished();
    void on_longitude_editingFinished();
    void on_elevation_valueChanged(int value);
    void on_glidePath_valueChanged(double value);
    void on_height_valueChanged(double value);
    void on_courseWidth_valueChanged(double value);
    void on_slope_valueChanged(double value);
    void on_findOnMap_clicked();
    void on_clearMarkers_clicked();
    void on_addMarker_clicked();
    void on_udpEnabled_clicked(bool checked);
    void on_udpAddress_editingFinished();
    void on_udpPort_editingFinished();
    void on_logEnable_clicked(bool checked=false);
    void on_logFilename_clicked();
    void on_channel1_currentIndexChanged(int index);
    void on_channel2_currentIndexChanged(int index);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void audioSelect(const QPoint& p);
    void tick();
    void preferenceChanged(int elementType);
};

#endif // INCLUDE_ILSDEMODGUI_H

