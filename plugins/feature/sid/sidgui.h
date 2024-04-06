///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023-2024 Jon Beniston, M7RCE                                   //
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

#ifndef INCLUDE_FEATURE_SIDGUI_H_
#define INCLUDE_FEATURE_SIDGUI_H_

#include <QTimer>
#include <QMenu>
#include <QToolButton>
#include <QDateTime>
#include <QtCharts>
#include <QMediaPlayer>

#include "feature/featuregui.h"
#include "util/messagequeue.h"
#include "util/movingaverage.h"
#include "util/grb.h"
#include "util/goesxray.h"
#include "util/solardynamicsobservatory.h"
#include "util/stix.h"
#include "settings/rollupstate.h"
#include "availablechannelorfeaturehandler.h"

#include "sidsettings.h"

class PluginAPI;
class FeatureUISet;
class SIDMain;

namespace Ui {
    class SIDGUI;
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
using namespace QtCharts;
#endif

class SIDGUI : public FeatureGUI {
    Q_OBJECT

    struct Measurement {
        QDateTime m_dateTime;
        double m_measurement;
        Measurement(QDateTime dateTime, double measurement) :
            m_dateTime(dateTime),
            m_measurement(measurement)
        {
        }
    };

    struct ChannelMeasurement {
        QString m_id;
        QList<Measurement> m_measurements;
        QXYSeries *m_series;
        double m_minMeasurement;
        double m_maxMeasurement;
        MovingAverageUtilVar<double, double> m_movingAverage;

        ChannelMeasurement() :
            m_series(nullptr),
            m_minMeasurement(std::numeric_limits<double>::quiet_NaN()),
            m_maxMeasurement(std::numeric_limits<double>::quiet_NaN()),
            m_movingAverage(1)
        {
        }

        ChannelMeasurement(const QString& id, int averageSamples) :
            m_id(id),
            m_series(nullptr),
            m_minMeasurement(std::numeric_limits<double>::quiet_NaN()),
            m_maxMeasurement(std::numeric_limits<double>::quiet_NaN()),
            m_movingAverage(averageSamples)
        {
        }

        void append(QDateTime dateTime, double measurement, bool updateSeries=true)
        {
            m_measurements.append(Measurement(dateTime, measurement));
            if (std::isnan(m_minMeasurement)) {
                m_minMeasurement = measurement;
            } else {
                m_minMeasurement = std::min(m_minMeasurement, measurement);
            }
            if (std::isnan(m_maxMeasurement)) {
                m_maxMeasurement = measurement;
            } else {
                m_maxMeasurement = std::max(m_maxMeasurement, measurement);
            }
            if (m_series && updateSeries) {
                appendSeries(dateTime, measurement);
            }
        }

        void newSeries(QXYSeries *series, int samples)
        {
            m_series = series;
            m_movingAverage.resize(samples);
        }

        void appendSeries(QDateTime dateTime, double measurement)
        {
            m_movingAverage(measurement);
            m_series->append(dateTime.toMSecsSinceEpoch(), m_movingAverage.instantAverage());
        }

        void clear()
        {
            m_minMeasurement = std::numeric_limits<double>::quiet_NaN();
            m_maxMeasurement = std::numeric_limits<double>::quiet_NaN();
            m_measurements.clear();
            m_series = nullptr;
        }

    };

public:
    static SIDGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
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
    Ui::SIDGUI* ui;
    PluginAPI* m_pluginAPI;
    FeatureUISet* m_featureUISet;
    SIDSettings m_settings;
    QList<QString> m_settingsKeys;
    RollupState m_rollupState;
    bool m_doApplySettings;

    SIDMain* m_sid;
    MessageQueue m_inputMessageQueue;
    QTimer m_statusTimer;
    QTimer m_autosaveTimer;
    int m_lastFeatureState;

    QFileDialog m_fileDialog;

    QList<ChannelMeasurement> m_channelMeasurements;
    QDateTimeAxis *m_chartXAxis;
    QValueAxis *m_chartY1Axis;
    QCategoryAxis *m_chartY2Axis;
    QLogValueAxis *m_chartY3Axis;
    QLogValueAxis *m_chartProtonAxis;
    double m_minMeasurement;
    double m_maxMeasurement;
    QDateTime m_minDateTime;
    QDateTime m_maxDateTime;

    QDateTimeAxis *m_xRayChartXAxis;
    QCategoryAxis *m_xRayChartYAxis;

    GOESXRay *m_goesXRay;
    ChannelMeasurement m_xrayShortMeasurements[2]; // Primary and secondary
    ChannelMeasurement m_xrayLongMeasurements[2];
    ChannelMeasurement m_protonMeasurements[4]; // 4 energy bands
    static const QStringList m_protonEnergies;

    SolarDynamicsObservatory *m_solarDynamicsObservatory;
    QStringList m_sdoImageNames;
    QMediaPlayer *m_player;

    GRB *m_grb;
    QList<GRB::Data> m_grbData;
    QScatterSeries *m_grbSeries;
    float m_grbMin;
    float m_grbMax;

    STIX *m_stix;
    QList<STIX::FlareData> m_stixData;
    QScatterSeries *m_stixSeries;

    AvailableChannelOrFeatureHandler m_availableFeatureHandler;
    AvailableChannelOrFeatureHandler m_availableChannelHandler;

    QStringList m_mapItemNames;

    explicit SIDGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
    virtual ~SIDGUI();

    void blockApplySettings(bool block);
    void applySetting(const QString& settingsKey);
    void applySettings(const QStringList& settingsKeys, bool force = false);
    void applyAllSettings();
    void displaySettings();
    bool handleMessage(const Message& message);
    void makeUIConnections();

    void writeCSV(const QString& filename);
    void readCSV(const QString& filename, bool autoload);
    void setAutosaveTimer();

    ChannelMeasurement& addMeasurements(const QString& id);
    ChannelMeasurement& getMeasurements(const QString& id);
    void addMeasurement(const QString& id, QDateTime dateTime, double measurement);

    void plotChart();
    void plotXRayChart();
    void createGRBSeries(QChart *chart, QDateTimeAxis *xAxis, QLogValueAxis *yAxis);
    void createXRaySeries(QChart *chart, QDateTimeAxis *xAxis, QCategoryAxis *yAxis);
    void createProtonSeries(QChart *chart, QDateTimeAxis *xAxis, QLogValueAxis *yAxis);
    void createSTIXSeries(QChart *chart, QDateTimeAxis *xAxis, QCategoryAxis *yAxis);
    void createFlareAxis(QCategoryAxis *yAxis);
    void setXAxisRange();
    void setY1AxisRange();
    void setAutoscaleX();
    void setAutoscaleY();
    void autoscaleX();
    void autoscaleY();
    void setButtonBackground(QToolButton *button, bool checked);
    void updateMeasurementRange(double measurement);
    void updateTimeRange(QDateTime dateTime);
    void applySDO();
    void applyDateTime();
    bool eventFilter(QObject *obj, QEvent *event) override;
    void sendToSkyMap(const AvailableChannelOrFeature& skymap, float ra, float dec);
    void showGRBContextMenu(QContextMenuEvent *contextEvent, QChartView *chartView, int closestPoint);
    void showStixContextMenu(QContextMenuEvent *contextEvent, QChartView *chartView, int closestPoint);
    void showContextMenu(QContextMenuEvent *contextEvent);
    bool findClosestPoint(QContextMenuEvent *contextEvent, QChart *chart, QScatterSeries *series, int& closestPoint);
    void clearMinMax();
    bool plotAnyXRay() const;
    void clearAllData();
    void connectDataUpdates();
    void disconnectDataUpdates();
    void getData();
    void clearFromMap();

    static qreal pixelDistance(QChart *chart, QAbstractSeries *series, QPointF a, QPointF b);

private slots:
    void onMenuDialogCalled(const QPoint &p);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void handleInputMessages();
    void on_startStop_toggled(bool checked);
    void on_samples_valueChanged(int value);
    void on_separateCharts_toggled(bool checked);
    void on_displayLegend_toggled(bool checked);
    void on_plotXRayLongPrimary_toggled(bool checked);
    void on_plotXRayLongSecondary_toggled(bool checked);
    void on_plotXRayShortPrimary_toggled(bool checked);
    void on_plotXRayShortSecondary_toggled(bool checked);
    void on_plotGRB_toggled(bool checked);
    void on_plotSTIX_toggled(bool checked);
    void on_plotProton_toggled(bool checked);
    void on_deleteAll_clicked();
    void on_autoscaleX_clicked();
    void on_autoscaleY_clicked();
    void on_today_clicked();
    void on_prevDay_clicked();
    void on_nextDay_clicked();
    void on_startDateTime_dateTimeChanged(QDateTime value);
    void on_endDateTime_dateTimeChanged(QDateTime value);
    void on_y1Min_valueChanged(double value);
    void on_y1Max_valueChanged(double value);
    void on_saveData_clicked();
    void on_loadData_clicked();
    void on_saveChartImage_clicked();
    void autoscaleXRightClicked();
    void autoscaleYRightClicked();
    void todayRightClicked();
    void updateStatus();
    void autosave();
    void on_settings_clicked();
    void on_addChannels_clicked();
    void xRayDataUpdated(const QList<GOESXRay::XRayData>& data, bool primary);
    void protonDataUpdated(const QList<GOESXRay::ProtonData>& data, bool primary);
    void grbDataUpdated(const QList<GRB::Data>& data);
    void stixDataUpdated(const QList<STIX::FlareData>& data);
    void legendMarkerClicked();
    void seriesClicked(const QPointF &point);
    void chartSplitterMoved(int pos, int index);
    void sdoSplitterMoved(int pos, int index);
    void on_sdoEnabled_toggled(bool checked);
    void on_sdoVideoEnabled_toggled(bool checked);
    void on_sdoData_currentIndexChanged(int index);
    void on_sdoNow_toggled(bool checked);
    void on_sdoDateTime_dateTimeChanged(QDateTime value);
    void sdoImageUpdated(const QImage& image);
    void sdoVideoError(QMediaPlayer::Error error);
    void sdoVideoStatusChanged(QMediaPlayer::MediaStatus status);
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    void sdoBufferStatusChanged(int percentFilled);
#else
    void sdoBufferProgressChanged(float filled);
#endif
    void on_showSats_clicked();
    void onSatTrackerAdded(int featureSetIndex, Feature *feature);
    void on_map_currentTextChanged(const QString& text);
    void on_showPaths_clicked();
    void featuresChanged(const QStringList& renameFrom, const QStringList& renameTo);
    void channelsChanged(const QStringList& renameFrom, const QStringList& renameTo, const QStringList& removed, const QStringList& added);
    void removeChannels(const QStringList& ids);
};

#endif // INCLUDE_FEATURE_SIDGUI_H_
