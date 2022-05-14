///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2020 Edouard Griffiths, F4EXB                              //
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

#ifndef INCLUDE_CHIRPCHATDEMODGUI_H
#define INCLUDE_CHIRPCHATDEMODGUI_H

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "chirpchatdemodsettings.h"

class PluginAPI;
class DeviceUISet;
class ChirpChatDemod;
class SpectrumVis;
class BasebandSampleSink;

namespace Ui {
	class ChirpChatDemodGUI;
}

class ChirpChatDemodGUI : public ChannelGUI {
	Q_OBJECT

public:
	static ChirpChatDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceAPI, BasebandSampleSink *rxChannel);
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

private slots:
	void channelMarkerChangedByCursor();
    void on_deltaFrequency_changed(qint64 value);
	void on_BW_valueChanged(int value);
	void on_Spread_valueChanged(int value);
    void on_deBits_valueChanged(int value);
    void on_fftWindow_currentIndexChanged(int index);
    void on_preambleChirps_valueChanged(int value);
	void on_scheme_currentIndexChanged(int index);
	void on_mute_toggled(bool checked);
	void on_clear_clicked(bool checked);
    void on_eomSquelch_valueChanged(int value);
    void on_messageLength_valueChanged(int value);
	void on_messageLengthAuto_stateChanged(int state);
	void on_header_stateChanged(int state);
	void on_fecParity_valueChanged(int value);
	void on_crc_stateChanged(int state);
	void on_packetLength_valueChanged(int value);
	void on_udpSend_stateChanged(int state);
	void on_udpAddress_editingFinished();
	void on_udpPort_editingFinished();
	void onWidgetRolled(QWidget* widget, bool rollDown);
	void onMenuDialogCalled(const QPoint& p);
    void channelMarkerHighlightedByCursor();
	void handleInputMessages();
	void tick();

private:
	enum ParityStatus // matches decoder status
	{
		ParityUndefined,
		ParityError,
		ParityCorrected,
		ParityOK
	};

	Ui::ChirpChatDemodGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceUISet* m_deviceUISet;
	ChannelMarker m_channelMarker;
	RollupState m_rollupState;
	ChirpChatDemodSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
	bool m_doApplySettings;

	ChirpChatDemod* m_chirpChatDemod;
	SpectrumVis* m_spectrumVis;
	MessageQueue m_inputMessageQueue;
	unsigned int m_tickCount;

	explicit ChirpChatDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
	virtual ~ChirpChatDemodGUI();

    void blockApplySettings(bool block);
	void applySettings(bool force = false);
	void displaySettings();
    void displaySquelch();
    void setBandwidths();
    void showLoRaMessage(const Message& message);
    void showTextMessage(const Message& message); //!< For TTY and ASCII
	void displayText(const QString& text);
	void displayBytes(const QByteArray& bytes);
	void displayStatus(const QString& status);
    void displayLoRaStatus(int headerParityStatus, bool headerCRCStatus, int payloadParityStatus, bool payloadCRCStatus);
	QString getParityStr(int parityStatus);
    void resetLoRaStatus();
	bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();
};

#endif // INCLUDE_CHIRPCHATDEMODGUI_H
