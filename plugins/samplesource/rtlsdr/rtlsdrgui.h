#ifndef INCLUDE_RTLSDRGUI_H
#define INCLUDE_RTLSDRGUI_H

#include <QTimer>
#include "plugin/plugingui.h"
#include "rtlsdrinput.h"

class PluginAPI;

namespace Ui {
	class RTLSDRGui;
}

class RTLSDRGui : public QWidget, public PluginGUI {
	Q_OBJECT

public:
	explicit RTLSDRGui(PluginAPI* pluginAPI, QWidget* parent = NULL);
	~RTLSDRGui();
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
	Ui::RTLSDRGui* ui;

	PluginAPI* m_pluginAPI;
	SampleSource::GeneralSettings m_generalSettings;
	RTLSDRInput::Settings m_settings;
	QTimer m_updateTimer;
	std::vector<int> m_gains;
	SampleSource* m_sampleSource;

	void displaySettings();
	void sendSettings();

private slots:
	void on_centerFrequency_changed(quint64 value);
	void on_gain_valueChanged(int value);
	void on_decimation_valueChanged(int value);

	void updateHardware();
};

#endif // INCLUDE_RTLSDRGUI_H
