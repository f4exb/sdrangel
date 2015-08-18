#ifndef INCLUDE_NFMDEMODGUI_H
#define INCLUDE_NFMDEMODGUI_H

#include "gui/rollupwidget.h"
#include "plugin/plugingui.h"
#include "dsp/dsptypes.h"

class PluginAPI;
class ChannelMarker;

class AudioFifo;
class Channelizer;
class NFMDemod;

namespace Ui {
	class NFMDemodGUI;
}

class NFMDemodGUI : public RollupWidget, public PluginGUI {
	Q_OBJECT

public:
	static NFMDemodGUI* create(PluginAPI* pluginAPI);
	void destroy();

	void setName(const QString& name);
	QString getName() const;
	qint64 getCenterFrequency() const;

	void resetToDefaults();
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);

	virtual bool handleMessage(const Message& message);
	void setCtcssFreq(Real ctcssFreq);

private slots:
	void viewChanged();
	void on_deltaFrequency_changed(quint64 value);
	void on_deltaMinus_clicked(bool minus);
	void on_rfBW_valueChanged(int value);
	void on_afBW_valueChanged(int value);
	void on_volume_valueChanged(int value);
	void on_squelch_valueChanged(int value);
	void on_ctcss_currentIndexChanged(int index);
	void onWidgetRolled(QWidget* widget, bool rollDown);
	void onMenuDoubleClicked();

private:
	Ui::NFMDemodGUI* ui;
	PluginAPI* m_pluginAPI;
	ChannelMarker* m_channelMarker;
	bool m_basicSettingsShown;
	bool m_doApplySettings;

	AudioFifo* m_audioFifo;
	Channelizer* m_channelizer;
	NFMDemod* m_nfmDemod;

	static const int m_rfBW[];

	explicit NFMDemodGUI(PluginAPI* pluginAPI, QWidget* parent = NULL);
	virtual ~NFMDemodGUI();

	void blockApplySettings(bool block);
	void applySettings();

	void leaveEvent(QEvent*);
	void enterEvent(QEvent*);
};

#endif // INCLUDE_NFMDEMODGUI_H
