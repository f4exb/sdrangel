///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_APTDEMODGUI_H
#define INCLUDE_APTDEMODGUI_H

#include <QIcon>
#include <QAbstractListModel>
#include <QModelIndex>
#include <QProgressDialog>
#include <QTableWidgetItem>
#include <QPushButton>
#include <QToolButton>
#include <QHBoxLayout>
#include <QMenu>
#include <QImage>
#include <QPixmap>
#include <QGraphicsRectItem>

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"
#include "aptdemodsettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class APTDemod;
class APTDemodGUI;
class QGraphicsScene;
class QGraphicsPixmapItem;
class GraphicsViewZoom;

namespace Ui {
    class APTDemodGUI;
}
class APTDemodGUI;

// Temperature scale
class TempScale : public QObject, public QGraphicsRectItem {
    Q_OBJECT
public:
    TempScale(QGraphicsItem *parent = nullptr);
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
private:
    QLinearGradient m_gradient;
};

class APTDemodGUI : public ChannelGUI {
    Q_OBJECT

public:
    static APTDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual bool eventFilter(QObject *watched, QEvent *event) override;
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
    Ui::APTDemodGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    APTDemodSettings m_settings;
    qint64 m_deviceCenterFrequency;
    bool m_doApplySettings;

    APTDemod* m_aptDemod;
    int m_basebandSampleRate;
    uint32_t m_tickCount;
    MessageQueue m_inputMessageQueue;

    QImage m_image;
    QPixmap m_pixmap;
    QGraphicsScene* m_scene;
    QGraphicsPixmapItem* m_pixmapItem;
    GraphicsViewZoom* m_zoom;
    TempScale *m_tempScale;
    QGraphicsRectItem *m_tempScaleBG;
    QGraphicsSimpleTextItem *m_tempText;

    QList<QString> m_mapImages;

    explicit APTDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
    virtual ~APTDemodGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void displayPalettes();
    void displayLabels();
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

    void deleteImageFromMap(const QString &name);
    void resetDecoder();

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

private slots:
    void on_deltaFrequency_changed(qint64 value);
    void on_rfBW_valueChanged(int index);
    void on_fmDev_valueChanged(int value);
    void on_channels_currentIndexChanged(int index);
    void on_transparencyThreshold_valueChanged(int value);
    void on_transparencyThreshold_sliderReleased();
    void on_opacityThreshold_valueChanged(int value);
    void on_opacityThreshold_sliderReleased();
    void on_deleteImageFromMap_clicked();
    void on_cropNoise_clicked(bool checked=false);
    void on_denoise_clicked(bool checked=false);
    void on_linear_clicked(bool checked=false);
    void on_histogram_clicked(bool checked=false);
    void on_precipitation_clicked(bool checked=false);
    void on_flip_clicked(bool checked=false);
    void on_startStop_clicked(bool checked=false);
    void on_showSettings_clicked();
    void on_resetDecoder_clicked();
    void on_saveImage_clicked();
    void on_zoomIn_clicked();
    void on_zoomOut_clicked();
    void on_zoomAll_clicked(bool checked=false);
    void onImageZoomed();
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void tick();
};

#endif // INCLUDE_APTDEMODGUI_H
