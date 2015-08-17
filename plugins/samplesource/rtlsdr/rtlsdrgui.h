#ifndef INCLUDE_RTLSDRGUI_H
#define INCLUDE_RTLSDRGUI_H

#include <QTimer>
#include "plugin/plugingui.h"
#include "rtlsdrinput.h"

class PluginAPI;

namespace Ui {
	class RTLSDRGui;
	class RTLSDRSampleRates;
}

class RTLSDRGui : public QWidget, public PluginGUI {
	Q_OBJECT

public:
	explicit RTLSDRGui(PluginAPI* pluginAPI, QWidget* parent = NULL);
	virtual ~RTLSDRGui();
	void destroy();

	void setName(const QString& name);
	QString getName() const;

	void resetToDefaults();
	qint64 getCenterFrequency() const;
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual bool handleMessage(const Message& message);

private:
	Ui::RTLSDRGui* ui;

	PluginAPI* m_pluginAPI;
	RTLSDRInput::Settings m_settings;
	QTimer m_updateTimer;
	std::vector<int> m_gains;
	SampleSource* m_sampleSource;

	void displaySettings();
	void sendSettings();

private slots:
	void on_centerFrequency_changed(quint64 value);
	void on_decim_valueChanged(int value);
	void on_ppm_valueChanged(int value);
	void on_gain_valueChanged(int value);
	void on_samplerate_valueChanged(int value);
	void on_checkBox_stateChanged(int state);
	void updateHardware();
};

class RTLSDRSampleRates {
public:
	static unsigned int getRate(unsigned int rate_index);
	static unsigned int getRateIndex(unsigned int rate);
private:
	static unsigned int m_rates[6];
	static unsigned int m_nb_rates;
};

#endif // INCLUDE_RTLSDRGUI_H
