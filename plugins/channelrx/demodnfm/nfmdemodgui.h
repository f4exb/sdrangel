#ifndef INCLUDE_NFMDEMODGUI_H
#define INCLUDE_NFMDEMODGUI_H

#include "channel/channelgui.h"
#include "dsp/dsptypes.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/messagequeue.h"
#include "settings/rollupstate.h"

#include "nfmdemodsettings.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class NFMDemod;

namespace Ui {
	class NFMDemodGUI;
}

class NFMDemodGUI : public ChannelGUI {
	Q_OBJECT

public:
	static NFMDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
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
	Ui::NFMDemodGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceUISet* m_deviceUISet;
	ChannelMarker m_channelMarker;
	RollupState m_rollupState;
	NFMDemodSettings m_settings;
    qint64 m_deviceCenterFrequency;
    int m_basebandSampleRate;
	bool m_basicSettingsShown;
	bool m_doApplySettings;

	NFMDemod* m_nfmDemod;
	bool m_squelchOpen;
    int m_audioSampleRate;
	bool m_reportedDcsCode;
	bool m_dcsShowPositive;
	uint32_t m_tickCount;
	MessageQueue m_inputMessageQueue;

	explicit NFMDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
	virtual ~NFMDemodGUI();

	void blockApplySettings(bool block);
	void applySettings(bool force = false);
	void displaySettings();
	void setCtcssFreq(Real ctcssFreq);
	void setDcsCode(unsigned int dcsCode);
	bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

	void leaveEvent(QEvent*);
	void enterEvent(EnterEventType*);

private slots:
	void on_deltaFrequency_changed(qint64 value);
	void on_channelSpacingApply_clicked();
	void on_rfBW_valueChanged(int value);
	void on_afBW_valueChanged(int value);
	void on_fmDev_valueChanged(int value);
	void on_volume_valueChanged(int value);
	void on_squelchGate_valueChanged(int value);
	void on_deltaSquelch_toggled(bool checked);
	void on_squelch_valueChanged(int value);
	void on_ctcss_currentIndexChanged(int index);
	void on_ctcssOn_toggled(bool checked);
	void on_dcsOn_toggled(bool checked);
	void on_dcsCode_currentIndexChanged(int index);
    void on_highPassFilter_toggled(bool checked);
	void on_audioMute_toggled(bool checked);
	void onWidgetRolled(QWidget* widget, bool rollDown);
	void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void audioSelect();
	void tick();
};

#endif // INCLUDE_NFMDEMODGUI_H
