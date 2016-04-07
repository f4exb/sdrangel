#ifndef INCLUDE_DSDDEMODGUI_H
#define INCLUDE_DSDDEMODGUI_H

#include "gui/rollupwidget.h"
#include "plugin/plugingui.h"
#include "dsp/dsptypes.h"
#include "dsp/channelmarker.h"
#include "dsp/movingaverage.h"

class PluginAPI;

class ThreadedSampleSink;
class Channelizer;
class ScopeVis;
class DSDDemod;

namespace Ui {
	class DSDDemodGUI;
}

class DSDDemodGUI : public RollupWidget, public PluginGUI {
	Q_OBJECT

public:
	static DSDDemodGUI* create(PluginAPI* pluginAPI);
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
	void on_deltaFrequency_changed(quint64 value);
	void on_deltaMinus_toggled(bool minus);
	void on_rfBW_valueChanged(int index);
	void on_demodGain_valueChanged(int value);
    void on_volume_valueChanged(int value);
	void on_fmDeviation_valueChanged(int value);
	void on_squelchGate_valueChanged(int value);
	void on_squelch_valueChanged(int value);
    void on_audioMute_toggled(bool checked);
	void onWidgetRolled(QWidget* widget, bool rollDown);
	void onMenuDoubleClicked();
	void tick();

private:
	Ui::DSDDemodGUI* ui;
	PluginAPI* m_pluginAPI;
	ChannelMarker m_channelMarker;
	bool m_basicSettingsShown;
	bool m_doApplySettings;

	ThreadedSampleSink* m_threadedChannelizer;
	Channelizer* m_channelizer;
    ScopeVis* m_scopeVis;

	DSDDemod* m_dsdDemod;
    bool m_audioMute;
	bool m_squelchOpen;
	MovingAverage<Real> m_channelPowerDbAvg;

	explicit DSDDemodGUI(PluginAPI* pluginAPI, QWidget* parent = NULL);
	virtual ~DSDDemodGUI();

	void blockApplySettings(bool block);
	void applySettings();

	void leaveEvent(QEvent*);
	void enterEvent(QEvent*);
};

#endif // INCLUDE_DSDDEMODGUI_H
