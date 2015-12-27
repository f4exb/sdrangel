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
	virtual qint64 getCenterFrequency() const;
	virtual void setCenterFrequency(qint64 centerFrequency);
	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);
	virtual bool handleMessage(const Message& message);

private:
	Ui::RTLSDRGui* ui;

	PluginAPI* m_pluginAPI;
	RTLSDRSettings m_settings;
	QTimer m_updateTimer;
	std::vector<int> m_gains;
	SampleSource* m_sampleSource;

	void displaySettings();
	void sendSettings();

private slots:
	void on_centerFrequency_changed(quint64 value);
	void on_dcOffset_toggled(bool checked);
	void on_iqImbalance_toggled(bool checked);
	void on_decim_valueChanged(int value);
	void on_fcPos_currentIndexChanged(int index);
	void on_ppm_valueChanged(int value);
	void on_gain_valueChanged(int value);
	void on_samplerate_valueChanged(int value);
	void on_checkBox_stateChanged(int state);
	void updateHardware();
	void handleSourceMessages();
};

class RTLSDRSampleRates {
public:
	static unsigned int getRate(unsigned int rate_index);
	static unsigned int getRateIndex(unsigned int rate);
private:
	static unsigned int m_rates[7];
	static unsigned int m_nb_rates;
};

#endif // INCLUDE_RTLSDRGUI_H
