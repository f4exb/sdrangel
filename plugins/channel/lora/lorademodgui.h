#ifndef INCLUDE_LoRaDEMODGUI_H
#define INCLUDE_LoRaDEMODGUI_H

#include "gui/rollupwidget.h"
#include "plugin/plugingui.h"

#define BANDWIDTHSTRING {7813,15625,20833,31250,62500}

class PluginAPI;
class ChannelMarker;
class Channelizer;
class LoRaDemod;
class SpectrumVis;

namespace Ui {
	class LoRaDemodGUI;
}

class LoRaDemodGUI : public RollupWidget, public PluginGUI {
	Q_OBJECT

public:
	static LoRaDemodGUI* create(PluginAPI* pluginAPI);
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
	void on_BW_valueChanged(int value);
	void on_Spread_valueChanged(int value);
	void onWidgetRolled(QWidget* widget, bool rollDown);
	void onMenuDoubleClicked();

private:
	Ui::LoRaDemodGUI* ui;
	PluginAPI* m_pluginAPI;
	ChannelMarker* m_channelMarker;
	bool m_basicSettingsShown;

	Channelizer* m_channelizer;
	LoRaDemod* m_LoRaDemod;
	SpectrumVis* m_spectrumVis;

	explicit LoRaDemodGUI(PluginAPI* pluginAPI, QWidget* parent = NULL);
	~LoRaDemodGUI();

	void applySettings();
};

#endif // INCLUDE_LoRaDEMODGUI_H
