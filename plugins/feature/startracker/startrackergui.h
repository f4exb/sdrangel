///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FEATURE_STARTRACKERGUI_H_
#define INCLUDE_FEATURE_STARTRACKERGUI_H_

#include <QTimer>
#include <QtCharts>
#include <QNetworkRequest>
#include <QImage>
#include <QProgressDialog>

#include "feature/featuregui.h"
#include "util/messagequeue.h"
#include "util/fits.h"
#include "gui/httpdownloadmanagergui.h"
#include "settings/rollupstate.h"

#include "startrackersettings.h"

class PluginAPI;
class FeatureUISet;
class StarTracker;
class QNetworkAccessManager;
class QNetworkReply;
class GraphicsViewZoom;
class QGraphicsPixmapItem;

namespace Ui {
    class StarTrackerGUI;
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
using namespace QtCharts;
#endif

class StarTrackerGUI : public FeatureGUI {
    Q_OBJECT

    struct LoSMarker {
        QString m_name;
        float m_l;
        float m_b;
        float m_d;
        QGraphicsTextItem* m_text;
    };

public:
    static StarTrackerGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual void setWorkspaceIndex(int index);
    virtual int getWorkspaceIndex() const { return m_settings.m_workspaceIndex; }
    virtual void setGeometryBytes(const QByteArray& blob) { m_settings.m_geometryBytes = blob; }
    virtual QByteArray getGeometryBytes() const { return m_settings.m_geometryBytes; }

private:
    Ui::StarTrackerGUI* ui;
    PluginAPI* m_pluginAPI;
    FeatureUISet* m_featureUISet;
    StarTrackerSettings m_settings;
    QList<QString> m_settingsKeys;
    RollupState m_rollupState;
    bool m_doApplySettings;
    bool m_doPlotChart;

    StarTracker* m_starTracker;
    MessageQueue m_inputMessageQueue;
    QTimer m_statusTimer;
    QTimer m_solarFluxTimer;
    int m_lastFeatureState;

    QChart *m_azElLineChart;
    QPolarChart *m_azElPolarChart;
    QTimer m_redrawTimer;

    QChart m_chart;
    QDateTimeAxis m_chartXAxis;
    QValueAxis m_chartYAxis;

    QCategoryAxis m_skyTempGalacticLXAxis;
    QCategoryAxis m_skyTempRAXAxis;
    QValueAxis m_skyTempYAxis;

    QChart *m_solarFluxChart;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;
    HttpDownloadManagerGUI m_dlm;

    // Solar flux plot
    double m_solarFlux; // 10.7cm/2800MHz
    bool m_solarFluxesValid;
    int m_solarFluxes[8]; // Frequency (MHz), flux density (sfu)
    const int m_solarFluxFrequencies[8] = {245, 410, 610, 1415, 2695, 4995, 8800, 15400};

    // Sky temperature
    QList<QImage> m_images;

    // Galactic line of sight
    QList<QPixmap> m_milkyWayImages;
    GraphicsViewZoom* m_zoom;
    QList<QGraphicsPixmapItem *> m_milkyWayItems;
    QGraphicsLineItem* m_lineOfSight;
    QList<LoSMarker *> m_lineOfSightMarkers;

    // Images that are part of the animation
    QList<QImage> m_animationImages;

    // Sun and Moon position for drawing on Sky Temperature chart
    double m_sunRA;
    double m_sunDec;
    double m_moonRA;
    double m_moonDec;

    explicit StarTrackerGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
    virtual ~StarTrackerGUI();

    void blockApplySettings(bool block);
    void blockPlotChart();
    void unblockPlotChartAndPlot();
    void applySettings(bool force = false);
    void displaySettings();
    void updateForTarget();
    bool handleMessage(const Message& message);
    void updateLST();
    void mapRaDec(double ra, double dec, bool galactic, double& x, double& y);
    QList<QLineSeries*> createDriftScan(bool galactic);
    QColor getSeriesColor(int series);
    void plotElevationLineChart();
    void plotElevationPolarChart();
    void plotSkyTemperatureChart();
    void plotSolarFluxChart();
    void plotGalacticLineOfSight();
    void createGalacticLineOfSightScene();
    void plotGalacticMarker(LoSMarker* marker);
    void plotChart();
    void removeAllAxes();
    double convertSolarFluxUnits(double sfu);
    QString solarFluxUnit();
    double calcSolarFlux(double freqMhz);
    void displaySolarFlux();
    QString getSolarFluxFilename();
    bool readSolarFlux();
    void raDecChanged();
    void updateChartSubSelect();
    void updateSolarFlux(bool all);
    void makeUIConnections();
    void limitAzElRange(double& azimuth, double& elevation) const;
    void updateSatelliteTrackerList(const QList<StarTrackerSettings::AvailableFeature>& satelliteTrackers);

private slots:
    void onMenuDialogCalled(const QPoint &p);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void handleInputMessages();
    void on_startStop_toggled(bool checked);
    void on_link_clicked(bool checked=false);
    void on_useMyPosition_clicked(bool checked=false);
    void on_latitude_valueChanged(double value);
    void on_longitude_valueChanged(double value);
    void on_rightAscension_editingFinished();
    void on_declination_editingFinished();
    void on_azimuth_valueChanged(double value);
    void on_elevation_valueChanged(double value);
    void on_azimuthOffset_valueChanged(double value);
    void on_elevationOffset_valueChanged(double value);
    void on_galacticLatitude_valueChanged(double value);
    void on_galacticLongitude_valueChanged(double value);
    void on_frequency_valueChanged(int value);
    void on_beamwidth_valueChanged(double value);
    void on_target_currentTextChanged(const QString &text);
    void on_displaySettings_clicked();
    void on_dateTimeSelect_currentTextChanged(const QString &text);
    void on_dateTime_dateTimeChanged(const QDateTime &datetime);
    void updateStatus();
    void on_viewOnMap_clicked();
    void on_chartSelect_currentIndexChanged(int index);
    void on_chartSubSelect_currentIndexChanged(int index);
    void plotAreaChanged(const QRectF &plotArea);
    void autoUpdateSolarFlux();
    void on_downloadSolarFlux_clicked();
    void on_darkTheme_clicked(bool checked);
    void on_zoomIn_clicked();
    void on_zoomOut_clicked();
    void on_addAnimationFrame_clicked();
    void on_clearAnimation_clicked();
    void on_saveAnimation_clicked();
    void on_drawSun_clicked(bool checked);
    void on_drawMoon_clicked(bool checked);
    void networkManagerFinished(QNetworkReply *reply);
    void downloadFinished(const QString& filename, bool success);
};


#endif // INCLUDE_FEATURE_STARTRACKERGUI_H_
