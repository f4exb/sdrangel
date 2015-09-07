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
	void on_ppm_valueChanged(int value);
	// TOOD: defaults push button
	void on_lnaGain_currentIndexChanged(int index);
	void on_rfFilter_currentIndexChanged(int index);
	void on_lnaEnhance_currentIndexChanged(int index);
	void on_band_currentIndexChanged(int index);
	void on_mixGain_currentIndexChanged(int index);
	void on_mixFilter_currentIndexChanged(int index);
	void on_bias_currentIndexChanged(int index);
	void on_mode_currentIndexChanged(int index);
	void on_gain1_currentIndexChanged(int index);
	void on_rcFilter_currentIndexChanged(int index);
	void on_gain2_currentIndexChanged(int index);
	void on_gain3_currentIndexChanged(int index);
	void on_gain4_currentIndexChanged(int index);
	void on_ifFilter_currentIndexChanged(int index);
	void on_gain5_currentIndexChanged(int index);
	void on_gain6_currentIndexChanged(int index);
	void on_setDefaults_clicked(bool checked);
	void updateHardware();
};

#endif // INCLUDE_FCDPROGUI_H
