///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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
#ifndef INCLUDE_SSBDEMODGUI_H
#define INCLUDE_SSBDEMODGUI_H

#include <QAbstractTableModel>

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/messagequeue.h"
#include "util/ft8message.h"
#include "settings/rollupstate.h"
#include "ft8demodsettings.h"
#include "ft8demodfilterproxy.h"

class QModelIndex;

class PluginAPI;
class DeviceUISet;
class AudioFifo;
class FT8Demod;
class SpectrumVis;
class BasebandSampleSink;

namespace Ui {
	class FT8DemodGUI;
}

struct FT8MesssageData
{
    QString m_utc;
    int m_pass;
    int m_okBits;
    float m_dt;
    int m_df;
    int m_snr;
    QString m_call1;
    QString m_call2;
    QString m_loc;
    QString m_info;
};

class FT8MessagesTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    FT8MessagesTableModel(QObject *parent = nullptr);

    virtual int rowCount(const QModelIndex &parent) const;
    virtual int columnCount(const QModelIndex &parent) const;
    virtual QVariant data(const QModelIndex &index, int role) const;
    virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    const QVector<FT8MesssageData> &getMessages() const;
    void messagesReceived(const QList<FT8Message>& messages);
    void setDefaultMessage();
    void clearMessages();
    int countAllMessages() const { return m_ft8Messages.size(); }

private:
    QVector<FT8MesssageData> m_ft8Messages;
    static const int m_columnCount = 10;
};

class FT8DemodGUI : public ChannelGUI {
	Q_OBJECT

public:
	static FT8DemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
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
	Ui::FT8DemodGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceUISet* m_deviceUISet;
	ChannelMarker m_channelMarker;
	RollupState m_rollupState;
	FT8DemodSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
	bool m_doApplySettings;
    int m_spectrumRate;
	bool m_audioBinaural;
	bool m_audioFlipChannels;
	bool m_audioMute;
	bool m_squelchOpen;
    int m_audioSampleRate;
	uint32_t m_tickCount;
    bool m_filterMessages;
    int m_selectedColumn;
    QString m_selectedValue;
    QVariant m_selectedData;

	FT8Demod* m_ft8Demod;
	SpectrumVis* m_spectrumVis;
	MessageQueue m_inputMessageQueue;

    FT8MessagesTableModel m_messagesModel;
    FT8DemodFilterProxy m_messagesFilterProxy;

	explicit FT8DemodGUI(PluginAPI* pluginAPI, DeviceUISet* deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
	virtual ~FT8DemodGUI();

    bool blockApplySettings(bool block);
	void applySettings(bool force = false);
	void applyBandwidths(unsigned int spanLog2, bool force = false);
    unsigned int spanLog2Max();
	void displaySettings();
	bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

	void leaveEvent(QEvent*);
	void enterEvent(EnterEventType*);

    void messagesReceived(const QList<FT8Message>& messages);
    void populateBandPresets();
    void filterMessages();
    void setupMessagesView();

private slots:
	void on_deltaFrequency_changed(qint64 value);
	void on_BW_valueChanged(int value);
	void on_lowCut_valueChanged(int value);
	void on_volume_valueChanged(int value);
	void on_agc_toggled(bool checked);
	void on_spanLog2_valueChanged(int value);
    void on_fftWindow_currentIndexChanged(int index);
    void on_filterIndex_valueChanged(int value);
    void on_moveToBottom_clicked();
    void on_filterMessages_toggled(bool checked);
    void on_applyBandPreset_clicked();
    void on_clearMessages_clicked();
    void on_recordWav_toggled(bool checked);
    void on_logMessages_toggled(bool checked);
    void on_settings_clicked();
	void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
	void tick();
    void messageViewClicked(const QModelIndex &index);
};

#endif // INCLUDE_SSBDEMODGUI_H
