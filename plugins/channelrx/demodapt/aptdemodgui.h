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

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/messagequeue.h"
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

class APTDemodGUI : public ChannelGUI {
    Q_OBJECT

public:
    static APTDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

public slots:
    void channelMarkerChangedByCursor();
    void channelMarkerHighlightedByCursor();

private:
    Ui::APTDemodGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    APTDemodSettings m_settings;
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

    explicit APTDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
    virtual ~APTDemodGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void displayStreamIndex();
    bool handleMessage(const Message& message);

    void leaveEvent(QEvent*);
    void enterEvent(QEvent*);

private slots:
    void on_deltaFrequency_changed(qint64 value);
    void on_rfBW_valueChanged(int index);
    void on_fmDev_valueChanged(int value);
    void on_channels_currentIndexChanged(int index);
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
    void on_image_zoomed();
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void tick();
};

#endif // INCLUDE_APTDEMODGUI_H
