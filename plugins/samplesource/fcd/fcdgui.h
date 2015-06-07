#ifndef INCLUDE_FCDGUI_H
#define INCLUDE_FCDGUI_H

#include <QTimer>
#include "plugin/plugingui.h"
#include "fcdinput.h"

class PluginAPI;

namespace Ui {
	class FCDGui;
}

class FCDGui : public QWidget, public PluginGUI {
	Q_OBJECT

public:
	explicit FCDGui(PluginAPI* pluginAPI, QWidget* parent = NULL);
	~FCDGui();
	void destroy();

	void setName(const QString& name);
	QString getName() const;

	void resetToDefaults();
	QByteArray serializeGeneral() const;
	bool deserializeGeneral(const QByteArray&data);
	quint64 getCenterFrequency() const;
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	bool handleMessage(Message* message);

private:
	Ui::FCDGui* ui;

	PluginAPI* m_pluginAPI;
	SampleSource::GeneralSettings m_generalSettings;
	FCDInput::Settings m_settings;
	QTimer m_updateTimer;
	std::vector<int> m_gains;
	SampleSource* m_sampleSource;

	void displaySettings();
	void sendSettings();

private slots:
	void on_centerFrequency_changed(quint64 value);
	void on_checkBoxR_stateChanged(int state);
	void on_checkBoxG_stateChanged(int state);
	void on_checkBoxB_stateChanged(int state);
	void updateHardware();
};

#endif // INCLUDE_FCDGUI_H
