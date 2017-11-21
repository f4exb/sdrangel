#ifndef INCLUDE_SSBDEMODGUI_H
#define INCLUDE_SSBDEMODGUI_H

#include <QIcon>

#include "plugin/plugininstancegui.h"
#include "gui/rollupwidget.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/messagequeue.h"
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

class SSBDemodGUI : public RollupWidget, public PluginInstanceGUI {
	Q_OBJECT

public:
	static SSBDemodGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
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
    void channelMarkerHighlightedByCursor();

private:
	Ui::SSBDemodGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceUISet* m_deviceUISet;
	ChannelMarker m_channelMarker;
	SSBDemodSettings m_settings;
	bool m_doApplySettings;
    int m_spectrumRate;
	bool m_audioBinaural;
	bool m_audioFlipChannels;
	bool m_audioMute;
	bool m_squelchOpen;
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
	void applyBandwidths(bool force = false);
	void displaySettings();
    void displayUDPAddress();

	void displayAGCPowerThreshold(int value);

	void leaveEvent(QEvent*);
	void enterEvent(QEvent*);

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
	void on_copyAudioToUDP_toggled(bool copy);
	void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
	void tick();
};

#endif // INCLUDE_SSBDEMODGUI_H
