///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
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

#ifndef INCLUDE_ATVDEMODGUI_H
#define INCLUDE_ATVDEMODGUI_H

#include "gui/rollupwidget.h"
#include "plugin/plugingui.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"

class PluginAPI;
class DeviceSourceAPI;

class ThreadedBasebandSampleSink;
class DownChannelizer;
class ATVDemod;

namespace Ui
{
	class ATVDemodGUI;
}

class ATVDemodGUI : public RollupWidget, public PluginGUI
{
	Q_OBJECT

public:
    static ATVDemodGUI* create(PluginAPI* objPluginAPI, DeviceSourceAPI *objDeviceAPI);
	void destroy();

    void setName(const QString& strName);
	QString getName() const;
	virtual qint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 intCenterFrequency);

	void resetToDefaults();
	QByteArray serialize() const;
    bool deserialize(const QByteArray& arrData);

    virtual bool handleMessage(const Message& objMessage);

    static const QString m_strChannelID;

private slots:
	void viewChanged();
    void channelSampleRateChanged();
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDoubleClicked();
    void tick();
    void on_synchLevel_valueChanged(int value);
    void on_blackLevel_valueChanged(int value);
    void on_lineTime_valueChanged(int value);
    void on_topTime_valueChanged(int value);
    void on_hSync_clicked();
    void on_vSync_clicked();
    void on_halfImage_clicked();
    void on_modulation_currentIndexChanged(int index);
    void on_fps_currentIndexChanged(int index);
    void on_reset_clicked(bool checked);
    void on_rfBW_valueChanged(int value);
    void on_rfOppBW_valueChanged(int value);
    void on_rfFiltering_toggled(bool checked);

private:
	Ui::ATVDemodGUI* ui;
    PluginAPI* m_objPluginAPI;
    DeviceSourceAPI* m_objDeviceAPI;
    ChannelMarker m_objChannelMarker;
    ThreadedBasebandSampleSink* m_objThreadedChannelizer;
    DownChannelizer* m_objChannelizer;
    ATVDemod* m_objATVDemod;

    bool m_blnBasicSettingsShown;
    bool m_blnDoApplySettings;

    int m_intTickCount;

    explicit ATVDemodGUI(PluginAPI* objPluginAPI, DeviceSourceAPI *objDeviceAPI, QWidget* objParent = NULL);
	virtual ~ATVDemodGUI();

    void blockApplySettings(bool blnBlock);
	void applySettings();
    void applyRFSettings();

	void leaveEvent(QEvent*);
	void enterEvent(QEvent*);
};

#endif // INCLUDE_ATVDEMODGUI_H
