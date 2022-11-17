///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef INCLUDE_M17DEMODGUI_H
#define INCLUDE_M17DEMODGUI_H

#include <QMenu>
#include <QtCharts>
#include <QDateTime>

#include "channel/channelgui.h"
#include "dsp/dsptypes.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "m17demodsettings.h"
#include "m17statustextdialog.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class ScopeVisXY;
class M17Demod;

namespace Ui {
	class M17DemodGUI;
}

class M17DemodGUI : public ChannelGUI {
	Q_OBJECT

public:
	static M17DemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
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
    struct BERPoint
    {
        QDateTime m_dateTime;
        uint32_t m_totalErrors;
        uint32_t m_totalBits;
        uint32_t m_currentErrors;
        uint32_t m_currentBits;
    };

	Ui::M17DemodGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceUISet* m_deviceUISet;
	ChannelMarker m_channelMarker;
	RollupState m_rollupState;
	M17DemodSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
	bool m_doApplySettings;

    ScopeVisXY* m_scopeVisXY;

	M17Demod* m_m17Demod;
	bool m_enableCosineFiltering;
	bool m_syncOrConstellation;
    bool m_audioMute;
	bool m_squelchOpen;
    int  m_audioSampleRate;
    uint32_t m_lsfCount;
	uint32_t m_tickCount;
    uint32_t m_lastBERErrors;
    uint32_t m_lastBERBits;
    bool m_showBERTotalOrCurrent;
    bool m_showBERNumbersOrRates;

    QChart m_berChart;
    QDateTimeAxis m_berChartXAxis;
    QList<BERPoint> m_berPoints;
    QList<uint32_t> m_currentErrors;
    uint32_t m_berHistory;

	float m_myLatitude;
	float m_myLongitude;

	MessageQueue m_inputMessageQueue;
    M17StatusTextDialog m_m17StatusTextDialog;

	explicit M17DemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = nullptr);
	virtual ~M17DemodGUI();

	void blockApplySettings(bool block);
	void applySettings(const QList<QString>& settingsKeys, bool force = false);
    void displaySettings();
    void updateAMBEFeaturesList();
	void updateMyPosition();
	bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();
    QString getStatus(int status, int sync_word_type, bool streamElsePacket, int packetProtocol);
    void packetReceived(QByteArray packet);
    QLineSeries *addBERSeries(bool total, uint32_t& min, uint32_t& max);
    QLineSeries *addBERSeriesRate(bool total, qreal& min, qreal& max);
    static void getLatLonFromGNSSMeta(const std::array<uint8_t, 14>& meta, float& lat, float& lon);
    static float bearing(float latFrom, float lonFrom, float latTo, float lonTo);
    static float distance(float lat1, float lon1, float lat2, float lon2);

	void leaveEvent(QEvent*);
	void enterEvent(EnterEventType*);

private slots:
    void on_deltaFrequency_changed(qint64 value);
    void on_rfBW_valueChanged(int index);
    void on_volume_valueChanged(int value);
    void on_baudRate_currentIndexChanged(int index);
    void on_syncOrConstellation_toggled(bool checked);
	void on_traceLength_valueChanged(int value);
    void on_traceStroke_valueChanged(int value);
    void on_traceDecay_valueChanged(int value);
    void on_fmDeviation_valueChanged(int value);
    void on_squelchGate_valueChanged(int value);
    void on_squelch_valueChanged(int value);
    void on_highPassFilter_toggled(bool checked);
    void on_audioMute_toggled(bool checked);
    void on_aprsClearTable_clicked();
    void on_totButton_toggled(bool checked);
    void on_curButton_toggled(bool checked);
    void on_berButton_toggled(bool checked);
    void on_berHistory_valueChanged(int value);
    void on_berReset_clicked();
    void on_viewStatusLog_clicked();
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void audioSelect();
    void tick();
};

#endif // INCLUDE_DSDDEMODGUI_H
