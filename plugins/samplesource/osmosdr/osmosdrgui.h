#ifndef INCLUDE_OSMOSDRGUI_H
#define INCLUDE_OSMOSDRGUI_H

#include <QTimer>
#include "plugin/plugingui.h"
#include "osmosdrinput.h"

class PluginAPI;

namespace Ui {
	class OsmoSDRGui;
}

class OsmoSDRGui : public QWidget, public PluginGUI {
	Q_OBJECT

public:
	explicit OsmoSDRGui(PluginAPI* pluginAPI, QWidget* parent = NULL);
	~OsmoSDRGui();
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
	Ui::OsmoSDRGui* ui;

	PluginAPI* m_pluginAPI;
	SampleSource::GeneralSettings m_generalSettings;
	OsmoSDRInput::Settings m_settings;
	QTimer m_updateTimer;
	std::vector<int> m_gains;
	SampleSource* m_sampleSource;

	void displaySettings();
	void sendSettings();
	int e4kLNAGainToIdx(int gain) const;
	int e4kIdxToLNAGain(int idx) const;

private slots:
	void on_iqSwap_toggled(bool checked);
	void on_e4000MixerGain_currentIndexChanged(int index);
	void on_e4000MixerEnh_currentIndexChanged(int index);
	void on_e4000if1_currentIndexChanged(int index);
	void on_e4000if2_currentIndexChanged(int index);
	void on_e4000if3_currentIndexChanged(int index);
	void on_e4000if4_currentIndexChanged(int index);
	void on_e4000if5_currentIndexChanged(int index);
	void on_e4000if6_currentIndexChanged(int index);
	void on_centerFrequency_changed(quint64 value);
	void on_filterI1_valueChanged(int value);
	void on_filterI2_valueChanged(int value);
	void on_filterQ1_valueChanged(int value);
	void on_filterQ2_valueChanged(int value);
	void on_decimation_valueChanged(int value);
	void on_e4000LNAGain_valueChanged(int value);
	void on_e4kI_valueChanged(int value);
	void on_e4kQ_valueChanged(int value);

	void updateHardware();
};

#endif // INCLUDE_OSMOSDRGUI_H
