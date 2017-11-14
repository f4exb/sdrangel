///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_CHANNELTX_MODTV_ATVMODGUI_H_
#define PLUGINS_CHANNELTX_MODTV_ATVMODGUI_H_

#include <plugin/plugininstancegui.h>
#include "gui/rollupwidget.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/messagequeue.h"

#include "atvmod.h"
#include "atvmodsettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSource;
class QMessageBox;

namespace Ui {
    class ATVModGUI;
}

class ATVModGUI : public RollupWidget, public PluginInstanceGUI {
    Q_OBJECT

public:
    static ATVModGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx);
    virtual void destroy();

    void setName(const QString& name);
    QString getName() const;
    virtual qint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual bool handleMessage(const Message& message);

public slots:
    void channelMarkerChangedByCursor();

private:
    Ui::ATVModGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    ATVModSettings m_settings;
    bool m_doApplySettings;

    ATVMod* m_atvMod;
    MovingAverage<double> m_channelPowerDbAvg;

    QString m_imageFileName;
    QString m_videoFileName;
    quint32 m_videoLength;   //!< video file length in seconds
    float m_videoFrameRate;  //!< video file frame rate
    int m_frameCount;
    std::size_t m_tickCount;
    bool m_enableNavTime;
    QMessageBox *m_camBusyFPSMessageBox;
    int m_rfSliderDivisor;
    MessageQueue m_inputMessageQueue;

    explicit ATVModGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSource *channelTx, QWidget* parent = 0);
    virtual ~ATVModGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void updateWithStreamData();
    void updateWithStreamTime();
    void setRFFiltersSlidersRange(int sampleRate);
    void setChannelMarkerBandwidth();
    int getNbLines();
    int getFPS();
    int getNbLinesIndex(int nbLines);
    int getFPSIndex(int fps);

    void leaveEvent(QEvent*);
    void enterEvent(QEvent*);

private slots:
    void handleSourceMessages();

    void on_deltaFrequency_changed(qint64 value);
    void on_channelMute_toggled(bool checked);
    void on_forceDecimator_toggled(bool checked);
    void on_modulation_currentIndexChanged(int index);
    void on_rfScaling_valueChanged(int value);
    void on_fmExcursion_valueChanged(int value);
    void on_rfBW_valueChanged(int value);
    void on_rfOppBW_valueChanged(int value);
    void on_nbLines_currentIndexChanged(int index);
    void on_fps_currentIndexChanged(int index);
    void on_standard_currentIndexChanged(int index);
    void on_invertVideo_clicked(bool checked);
    void on_uniformLevel_valueChanged(int value);
    void on_inputSelect_currentIndexChanged(int index);
    void on_imageFileDialog_clicked(bool checked);
    void on_videoFileDialog_clicked(bool checked);

    void on_playVideo_toggled(bool checked);
    void on_playLoop_toggled(bool checked);
    void on_navTimeSlider_valueChanged(int value);

    void on_playCamera_toggled(bool checked);
    void on_camSelect_currentIndexChanged(int index);
    void on_cameraManualFPSEnable_toggled(bool checked);
    void on_cameraManualFPS_valueChanged(int value);

    void on_overlayTextShow_toggled(bool checked);
    void on_overlayText_textEdited(const QString& arg1);

    void onWidgetRolled(QWidget* widget, bool rollDown);

    void configureImageFileName();
    void configureVideoFileName();
    void tick();
};

#endif /* PLUGINS_CHANNELTX_MODAM_AMMODGUI_H_ */
