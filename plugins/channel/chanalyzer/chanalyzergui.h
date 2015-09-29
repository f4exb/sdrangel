#ifndef INCLUDE_CHANNELANALYZERGUI_H
#define INCLUDE_CHANNELANALYZERGUI_H

#include "gui/rollupwidget.h"
#include "plugin/plugingui.h"
#include "dsp/channelmarker.h"

class PluginAPI;

class ThreadedSampleSink;
class Channelizer;
class ChannelAnalyzer;
class SpectrumScopeComboVis;
class SpectrumVis;
class ScopeVis;

namespace Ui {
	class ChannelAnalyzerGUI;
}

class ChannelAnalyzerGUI : public RollupWidget, public PluginGUI {
	Q_OBJECT

public:
	static ChannelAnalyzerGUI* create(PluginAPI* pluginAPI);
	void destroy();

	void setName(const QString& name);
	QString getName() const;
	virtual qint64 getCenterFrequency() const;
	virtual void setCenterFrequency(qint64 centerFrequency);

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);

	virtual bool handleMessage(const Message& message);

private slots:
	void viewChanged();
	void channelSampleRateChanged();
	void on_deltaFrequency_changed(quint64 value);
	void on_deltaMinus_clicked(bool minus);
	void on_BW_valueChanged(int value);
	void on_lowCut_valueChanged(int value);
	void on_spanLog2_valueChanged(int value);
	void on_ssb_toggled(bool checked);
	void onWidgetRolled(QWidget* widget, bool rollDown);
	void onMenuDoubleClicked();

private:
	Ui::ChannelAnalyzerGUI* ui;
	PluginAPI* m_pluginAPI;
	ChannelMarker m_channelMarker;
	bool m_basicSettingsShown;
	bool m_doApplySettings;
	int m_rate;
	int m_spanLog2;

	ThreadedSampleSink* m_threadedChannelizer;
	Channelizer* m_channelizer;
	ChannelAnalyzer* m_channelAnalyzer;
	SpectrumScopeComboVis* m_spectrumScopeComboVis;
	SpectrumVis* m_spectrumVis;
	ScopeVis* m_scopeVis;

	explicit ChannelAnalyzerGUI(PluginAPI* pluginAPI, QWidget* parent = NULL);
	virtual ~ChannelAnalyzerGUI();

	int  getEffectiveLowCutoff(int lowCutoff);
	bool setNewRate(int spanLog2);

	void blockApplySettings(bool block);
	void applySettings();

	void leaveEvent(QEvent*);
	void enterEvent(QEvent*);
};

#endif // INCLUDE_CHANNELANALYZERGUI_H
