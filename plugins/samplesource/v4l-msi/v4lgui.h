#ifndef INCLUDE_V4LGUI_H
#define INCLUDE_V4LGUI_H

#include <QTimer>
#include "plugin/plugingui.h"
#include "v4linput.h"

class PluginAPI;

namespace Ui {
	class V4LGui;
}

class V4LGui : public QWidget, public PluginGUI {
	Q_OBJECT

public:
	explicit V4LGui(PluginAPI* pluginAPI, QWidget* parent = NULL);
	~V4LGui();
	void destroy();

	void setName(const QString& name);

	void resetToDefaults();
	QByteArray serializeGeneral() const;
	bool deserializeGeneral(const QByteArray&data);
	quint64 getCenterFrequency() const;
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	bool handleMessage(Message* message);

private:
	Ui::V4LGui* ui;

	PluginAPI* m_pluginAPI;
	SampleSource::GeneralSettings m_generalSettings;
	V4LInput::Settings m_settings;
	QTimer m_updateTimer;
	std::vector<int> m_gains;
	SampleSource* m_sampleSource;

	void displaySettings();
	void sendSettings();

private slots:
	void on_centerFrequency_changed(quint64 value);
	void on_ifgain_valueChanged(int value);
	void on_checkBoxL_stateChanged(int state);
	void on_checkBoxM_stateChanged(int state);
	void updateHardware();
};

#endif // INCLUDE_V4LGUI_H
