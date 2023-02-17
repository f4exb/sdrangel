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

#ifndef INCLUDE_HEATMAPGUI_H
#define INCLUDE_HEATMAPGUI_H

#include <QTableWidgetItem>
#include <QPushButton>
#include <QToolButton>
#include <QHBoxLayout>
#include <QMenu>
#include <QPainter>
#include <QFileDialog>
#include <QtCharts>

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/messagequeue.h"
#include "util/colormap.h"
#include "settings/rollupstate.h"

#include "heatmapsettings.h"
#include "heatmap.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class ScopeVis;
class HeatMap;
class HeatMapGUI;

namespace Ui {
    class HeatMapGUI;
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
using namespace QtCharts;
#endif

class HeatMapGUI : public ChannelGUI {
    Q_OBJECT

public:
    static HeatMapGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
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
    void preferenceChanged(int elementType);

private:
    Ui::HeatMapGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    HeatMapSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
    bool m_doApplySettings;
    ScopeVis* m_scopeVis;

    HeatMap* m_heatMap;
    uint32_t m_tickCount;
    MessageQueue m_inputMessageQueue;

    QMenu *messagesMenu;                        // Column select context menu
    QMenu *copyMenu;

    int m_width;                                // In pixels
    int m_height;
    double m_resolution;
    double m_degreesLonPerPixel;
    double m_degreesLatPerPixel;
    float *m_powerAverage;
    float *m_powerPulseAverage;
    float *m_powerMaxPeak;
    float *m_powerMinPeak;
    float *m_powerPathLoss;
    QImage m_image;
    double m_east, m_west, m_north, m_south;
    double m_latitude;
    double m_longitude;
    double m_altitude;
    QPainter m_painter;
    QPen m_pen;
    const float *m_colorMap;
    int m_x;                                        // Current position
    int m_y;

    QChart *m_powerChart;
    QLineSeries *m_powerAverageSeries;
    QLineSeries *m_powerMaxPeakSeries;
    QLineSeries *m_powerMinPeakSeries;
    QLineSeries *m_powerPulseAverageSeries;
    QLineSeries *m_powerPathLossSeries;
    QDateTimeAxis *m_powerXAxis;
    QValueAxis *m_powerYAxis;

    const static QStringList m_averagePeriodTexts;
    const static QStringList m_sampleRateTexts;

    const static int m_blockSize = 512;            // osm uses powers of 2
    const static int m_zoomLevel = 15;             // 3m/pixel at lat=51    GPS accuracy ~5m at 1Hz

    QFileDialog m_csvFileDialog;
    QFileDialog m_imageFileDialog;

    explicit HeatMapGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
    virtual ~HeatMapGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void messageReceived(const QByteArray& message, const QDateTime& dateTime);
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

    float *getCurrentModePowerData();
    void coordsToPixel(double latitude, double longitude, int& x, int& y) const;
    void pixelToCoords(int x, int y, double& latitude, double& longitude) const;
    bool pixelValid(int x, int y) const;

    void createMap();
    void deleteMap();
    void resizeMap(int x, int y);
    void clearPower();
    void clearPower(float *power);
    void clearPower(float *power, int size);
    void clearImage();
    void updatePower(double latitude, double longitude, float power);
    void plotMap();
    void plotMap(float *power);
    void plotPixel(int x, int y, double power);
    qreal calcRange(double latitude, double longitude);
    void updateRange();
    void displayTXPosition(bool enabled);
    double calcFreeSpacePathLoss(double range, double frequency);

    void displayPowerChart();
    void plotPowerVsTimeChart();
    void addToPowerSeries(QDateTime dateTime, double average, double pulseAverage, double max, double min, double pathLoss);
    void trimPowerSeries(QDateTime dateTime);
    void trimPowerSeries(QLineSeries *series, QDateTime dateTime);
    void updateAxis();

    void deleteFromMap();
    void sendToMap();
    void sendTxToMap();
    void deleteTxFromMap();

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

private slots:
    void on_deltaFrequency_changed(qint64 value);
    void on_rfBW_valueChanged(int index);
    void on_displayChart_clicked(bool checked=false);
    void on_clearHeatMap_clicked();
    void on_writeCSV_clicked();
    void on_readCSV_clicked();
    void on_writeImage_clicked();
    void on_minPower_valueChanged(double value);
    void on_maxPower_valueChanged(double value);
    void on_colorMap_currentIndexChanged(int index);
    void on_pulseTH_valueChanged(int value);
    void on_averagePeriod_valueChanged(int value);
    void on_sampleRate_valueChanged(int value);
    void on_mode_currentIndexChanged(int index);
    void on_txPosition_clicked(bool checked=false);
    void on_txLatitude_editingFinished();
    void on_txLongitude_editingFinished();
    void on_txPower_valueChanged(double value);
    void on_txPositionSet_clicked(bool checked=false);
    void on_displayAverage_clicked(bool checked=false);
    void on_displayMax_clicked(bool checked=false);
    void on_displayMin_clicked(bool checked=false);
    void on_displayPulseAverage_clicked(bool checked=false);
    void on_displayPathLoss_clicked(bool checked=false);
    void on_displayMins_valueChanged(int value);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void tick();
};

#endif // INCLUDE_HEATMAPGUI_H

