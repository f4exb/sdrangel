#ifndef INCLUDE_FCDPROGUI_H
#define INCLUDE_FCDPROGUI_H

#include <QTimer>

#include "fcdproinput.h"
#include "plugin/plugingui.h"

class PluginAPI;

namespace Ui {
	class FCDProGui;
}

class FCDProGui : public QWidget, public PluginGUI {
	Q_OBJECT

public:
	explicit FCDProGui(PluginAPI* pluginAPI, QWidget* parent = NULL);
	virtual ~FCDProGui();
	void destroy();

	void setName(const QString& name);
	QString getName() const;

	void resetToDefaults();
	qint64 getCenterFrequency() const;
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);

	virtual bool handleMessage(const Message& message);

private:
	Ui::FCDProGui* ui;

	PluginAPI* m_pluginAPI;
	FCDProInput::Settings m_settings;
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

#endif // INCLUDE_FCDPROGUI_H
