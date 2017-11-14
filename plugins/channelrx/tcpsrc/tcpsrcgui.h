#ifndef INCLUDE_TCPSRCGUI_H
#define INCLUDE_TCPSRCGUI_H

#include <plugin/plugininstancegui.h>
#include <QHostAddress>

#include "gui/rollupwidget.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"
#include "util/messagequeue.h"

#include "tcpsrc.h"
#include "tcpsrcsettings.h"

class PluginAPI;
class DeviceUISet;
class TCPSrc;
class SpectrumVis;

namespace Ui {
	class TCPSrcGUI;
}

class TCPSrcGUI : public RollupWidget, public PluginInstanceGUI {
	Q_OBJECT

public:
	static TCPSrcGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
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
	Ui::TCPSrcGUI* ui;
	PluginAPI* m_pluginAPI;
	DeviceUISet* m_deviceUISet;
	TCPSrc* m_tcpSrc;
	ChannelMarker m_channelMarker;
	MovingAverage<double> m_channelPowerDbAvg;

	// settings
	TCPSrcSettings m_settings;
	TCPSrcSettings::SampleFormat m_sampleFormat;
	Real m_outputSampleRate;
	Real m_rfBandwidth;
	int m_boost;
	int m_tcpPort;
	bool m_rfBandwidthChanged;
	bool m_doApplySettings;

	// RF path
	SpectrumVis* m_spectrumVis;
	MessageQueue m_inputMessageQueue;

	explicit TCPSrcGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
	virtual ~TCPSrcGUI();

    void blockApplySettings(bool block);
	void applySettings();
	void displaySettings();
    void setSampleFormat(int index);
    void setSampleFormatIndex(const TCPSrcSettings::SampleFormat& sampleFormat);

	void addConnection(quint32 id, const QHostAddress& peerAddress, int peerPort);
	void delConnection(quint32 id);

private slots:
	void on_deltaFrequency_changed(qint64 value);
	void on_sampleFormat_currentIndexChanged(int index);
	void on_sampleRate_textEdited(const QString& arg1);
	void on_rfBandwidth_textEdited(const QString& arg1);
	void on_tcpPort_textEdited(const QString& arg1);
	void on_applyBtn_clicked();
	void onWidgetRolled(QWidget* widget, bool rollDown);
	void on_volume_valueChanged(int value);
	void tick();
};

#endif // INCLUDE_TCPSRCGUI_H
