#ifndef INCLUDE_FCDGUI_H
#define INCLUDE_FCDGUI_H

#include <QTimer>

#include "fcdproplusinput.h"
#include "plugin/plugingui.h"

class PluginAPI;

namespace Ui {
	class FCDProPlusGui;
}

class FCDProPlusGui : public QWidget, public PluginGUI {
	Q_OBJECT

public:
	explicit FCDProPlusGui(PluginAPI* pluginAPI, QWidget* parent = NULL);
	virtual ~FCDProPlusGui();
	void destroy();

	void setName(const QString& name);
	QString getName() const;

	void resetToDefaults();
	qint64 getCenterFrequency() const;
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);

	virtual bool handleMessage(const Message& message);

private:
	Ui::FCDProPlusGui* ui;

	PluginAPI* m_pluginAPI;
	FCDProPlusInput::Settings m_settings;
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
	void on_mixGain_stateChanged(int state);
	void on_ifGain_valueChanged(int value);
	void on_filterRF_currentIndexChanged(int index);
	void on_filterIF_currentIndexChanged(int index);
	void updateHardware();
};

#endif // INCLUDE_FCDGUI_H
