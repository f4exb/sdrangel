#ifndef INCLUDE_SSBDEMODGUI_H
#define INCLUDE_SSBDEMODGUI_H

#include "gui/rollupwidget.h"
#include "plugin/plugingui.h"
#include "dsp/channelmarker.h"

class PluginAPI;

class AudioFifo;
class ThreadedSampleSink;
class Channelizer;
class SSBDemod;
class SpectrumVis;

namespace Ui {
	class SSBDemodGUI;
}

class SSBDemodGUI : public RollupWidget, public PluginGUI {
	Q_OBJECT

public:
	static SSBDemodGUI* create(PluginAPI* pluginAPI);
	void destroy();

	void setName(const QString& name);
	QString getName() const;
	qint64 getCenterFrequency() const;

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);

	virtual bool handleMessage(const Message& message);

private slots:
	void viewChanged();
	void on_deltaFrequency_changed(quint64 value);
	void on_deltaMinus_clicked(bool minus);
	void on_BW_valueChanged(int value);
	void on_lowCut_valueChanged(int value);
	void on_volume_valueChanged(int value);
	void on_spanLog2_valueChanged(int value);
	void onWidgetRolled(QWidget* widget, bool rollDown);
	void onMenuDoubleClicked();

private:
	Ui::SSBDemodGUI* ui;
	PluginAPI* m_pluginAPI;
	ChannelMarker m_channelMarker;
	bool m_basicSettingsShown;
	bool m_doApplySettings;
	int m_rate;
	int m_spanLog2;

	ThreadedSampleSink* m_threadedChannelizer;
	Channelizer* m_channelizer;
	SSBDemod* m_ssbDemod;
	SpectrumVis* m_spectrumVis;

	explicit SSBDemodGUI(PluginAPI* pluginAPI, QWidget* parent = NULL);
	virtual ~SSBDemodGUI();

	int  getEffectiveLowCutoff(int lowCutoff);
	bool setNewRate(int spanLog2);
    
    void blockApplySettings(bool block);
	void applySettings();

	void leaveEvent(QEvent*);
	void enterEvent(QEvent*);
};

#endif // INCLUDE_SSBDEMODGUI_H
