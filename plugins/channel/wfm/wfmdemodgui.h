#ifndef INCLUDE_WFMDEMODGUI_H
#define INCLUDE_WFMDEMODGUI_H

#include "gui/rollupwidget.h"
#include "plugin/plugingui.h"

class PluginAPI;
class ChannelMarker;

class AudioFifo;
class Channelizer;
class WFMDemod;

namespace Ui {
	class WFMDemodGUI;
}

class WFMDemodGUI : public RollupWidget, public PluginGUI {
	Q_OBJECT

public:
	static WFMDemodGUI* create(PluginAPI* pluginAPI);
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
	void on_rfBW_valueChanged(int value);
	void on_afBW_valueChanged(int value);
	void on_volume_valueChanged(int value);
	void on_squelch_valueChanged(int value);
	void onWidgetRolled(QWidget* widget, bool rollDown);
	void onMenuDoubleClicked();

private:
	Ui::WFMDemodGUI* ui;
	PluginAPI* m_pluginAPI;
	ChannelMarker* m_channelMarker;
	bool m_basicSettingsShown;

	AudioFifo* m_audioFifo;
	Channelizer* m_channelizer;
	WFMDemod* m_wfmDemod;

	static const int m_rfBW[];

	explicit WFMDemodGUI(PluginAPI* pluginAPI, QWidget* parent = NULL);
	virtual ~WFMDemodGUI();

	void applySettings();

	void leaveEvent(QEvent*);
	void enterEvent(QEvent*);
};

#endif // INCLUDE_WFMDEMODGUI_H
