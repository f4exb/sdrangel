#ifndef INCLUDE_SSBDEMODGUI_H
#define INCLUDE_SSBDEMODGUI_H

#include <QIcon>

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"
#include "ssbdemodsettings.h"

class PluginAPI;
class DeviceUISet;

class AudioFifo;
class SSBDemod;
class SpectrumVis;
class BasebandSampleSink;

namespace Ui {
	class SSBDemodGUI;
}

class SSBDemodGUI : public ChannelGUI {
	Q_OBJECT

public:
	static SSBDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
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
	Ui::SSBDemodGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceUISet* m_deviceUISet;
	ChannelMarker m_channelMarker;
	RollupState m_rollupState;
	SSBDemodSettings m_settings;
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

	SSBDemod* m_ssbDemod;
	SpectrumVis* m_spectrumVis;
	MessageQueue m_inputMessageQueue;

	QIcon m_iconDSBUSB;
	QIcon m_iconDSBLSB;

	explicit SSBDemodGUI(PluginAPI* pluginAPI, DeviceUISet* deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
	virtual ~SSBDemodGUI();

    bool blockApplySettings(bool block);
	void applySettings(bool force = false);
	void applyBandwidths(unsigned int spanLog2, bool force = false);
    unsigned int spanLog2Max();
	void displaySettings();
	void displayAGCPowerThreshold(int value);
    void displayAGCThresholdGate(int value);
	bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

	void leaveEvent(QEvent*);
	void enterEvent(EnterEventType*);

private slots:
	void on_deltaFrequency_changed(qint64 value);
	void on_audioBinaural_toggled(bool binaural);
	void on_audioFlipChannels_toggled(bool flip);
	void on_dsb_toggled(bool dsb);
	void on_BW_valueChanged(int value);
	void on_lowCut_valueChanged(int value);
	void on_volume_valueChanged(int value);
	void on_agc_toggled(bool checked);
    void on_agcClamping_toggled(bool checked);
	void on_agcTimeLog2_valueChanged(int value);
    void on_agcPowerThreshold_valueChanged(int value);
    void on_agcThresholdGate_valueChanged(int value);
	void on_audioMute_toggled(bool checked);
	void on_spanLog2_valueChanged(int value);
	void on_flipSidebands_clicked(bool checked);
    void on_fftWindow_currentIndexChanged(int index);
    void on_filterIndex_valueChanged(int value);
	void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void audioSelect();
	void tick();
};

#endif // INCLUDE_SSBDEMODGUI_H
