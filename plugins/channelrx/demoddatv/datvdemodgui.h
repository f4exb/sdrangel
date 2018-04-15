///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
// using LeanSDR Framework (C) 2016 F4DAV                                        //
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

#ifndef INCLUDE_DATVDEMODGUI_H
#define INCLUDE_DATVDEMODGUI_H

#include "gui/rollupwidget.h"
#include "plugin/plugininstancegui.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"

#include "datvdemod.h"

#include <QTimer>


class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
//class DeviceSourceAPI;
//class ThreadedBasebandSampleSink;
class DownChannelizer;
//class DATVDemod;

namespace Ui
{
    class DATVDemodGUI;
}

class DATVDemodGUI : public RollupWidget, public PluginInstanceGUI
{
	Q_OBJECT

public:
    static DATVDemodGUI* create(PluginAPI* objPluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
    virtual void destroy();

    void setName(const QString& strName);
	QString getName() const;
	virtual qint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 intCenterFrequency);

	void resetToDefaults();
	QByteArray serialize() const;
    bool deserialize(const QByteArray& arrData);

    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

    virtual bool handleMessage(const Message& objMessage);

    static const QString m_strChannelID;

private slots:
    void channelMarkerChangedByCursor();
    void channelMarkerHighlightedByCursor();

    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDoubleClicked();
    void tick();

    void on_cmbStandard_currentIndexChanged(const QString &arg1);
    void on_cmbModulation_currentIndexChanged(const QString &arg1);
    void on_cmbFEC_currentIndexChanged(const QString &arg1);
    void on_chkViterbi_clicked();
    void on_chkHardMetric_clicked();
    void on_pushButton_2_clicked();
    void on_spiSymbolRate_valueChanged(int arg1);
    void on_spiNotchFilters_valueChanged(int arg1);
    void on_chkAllowDrift_clicked();
    void on_pushButton_3_clicked();
    void on_pushButton_4_clicked();
    void on_mouseEvent(QMouseEvent* obj);
    void on_StreamDataAvailable(int *intPackets, int *intBytes, int *intPercent, qint64 *intTotalReceived);
    void on_StreamMetaDataChanged(DataTSMetaData2 *objMetaData);
    void on_spiBandwidth_valueChanged(int arg1);
    void on_chkFastlock_clicked();
    void on_cmbFilter_currentIndexChanged(int index);
    void on_spiRollOff_valueChanged(int arg1);
    void on_spiExcursion_valueChanged(int arg1);
    void on_deltaFrequency_changed(qint64 value);
    void on_rfBandwidth_changed(qint64 value);

private:
    Ui::DATVDemodGUI* ui;
    PluginAPI* m_objPluginAPI;
    DeviceUISet* m_deviceUISet;

    ChannelMarker m_objChannelMarker;
    ThreadedBasebandSampleSink* m_objThreadedChannelizer;
    DownChannelizer* m_objChannelizer;
    DATVDemod* m_objDATVDemod;
    MessageQueue m_inputMessageQueue;
    int m_intCenterFrequency;

    QTimer m_objTimer;
    qint64 m_intPreviousDecodedData;
    qint64 m_intLastDecodedData;
    qint64 m_intLastSpeed;
    int m_intReadyDecodedData;

    bool m_blnBasicSettingsShown;
    bool m_blnDoApplySettings;
    bool m_blnButtonPlayClicked;

    MovingAverageUtil<double, double, 4> m_objMagSqAverage;

    explicit DATVDemodGUI(PluginAPI* objPluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* objParent = 0);
    virtual ~DATVDemodGUI();

    void blockApplySettings(bool blnBlock);
	void applySettings();
    QString formatBytes(qint64 intBytes);

    void displayRRCParameters(bool blnVisible);

	void leaveEvent(QEvent*);
	void enterEvent(QEvent*);
};

#endif // INCLUDE_DATVDEMODGUI_H
