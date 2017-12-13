///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_SAMPLESOURCE_SDRPLAY_SDRPLAYGUI_H_
#define PLUGINS_SAMPLESOURCE_SDRPLAY_SDRPLAYGUI_H_

#include <plugin/plugininstancegui.h>
#include <QTimer>
#include <QWidget>
#include <vector>

#include "util/messagequeue.h"

#include "sdrplayinput.h"
#include "sdrplaysettings.h"

class DeviceSourceAPI;
class DeviceUISet;

namespace Ui {
    class SDRPlayGui;
}

class SDRPlayGui : public QWidget, public PluginInstanceGUI {
    Q_OBJECT

public:
    explicit SDRPlayGui(DeviceUISet *deviceUISet, QWidget* parent = 0);
    virtual ~SDRPlayGui();
    virtual void destroy();

    void setName(const QString& name);
    QString getName() const;

    virtual void resetToDefaults();
    virtual qint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);
    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual bool handleMessage(const Message& message);

private:
    Ui::SDRPlayGui* ui;

    DeviceUISet* m_deviceUISet;
    bool m_doApplySettings;
    bool m_forceSettings;
    SDRPlaySettings m_settings;
    QTimer m_updateTimer;
    QTimer m_statusTimer;
    DeviceSampleSource* m_sampleSource;
    int m_sampleRate;
    quint64 m_deviceCenterFrequency; //!< Center frequency in device
    int m_lastEngineState;
    MessageQueue m_inputMessageQueue;

    void blockApplySettings(bool block) { m_doApplySettings = !block; }
    void displaySettings();
    void sendSettings();
    void updateSampleRateAndFrequency();

private slots:
    void updateHardware();
    void updateStatus();
    void handleInputMessages();
    void on_centerFrequency_changed(quint64 value);
    void on_ppm_valueChanged(int value);
    void on_dcOffset_toggled(bool checked);
    void on_iqImbalance_toggled(bool checked);
    void on_fBand_currentIndexChanged(int index);
    void on_bandwidth_currentIndexChanged(int index);
    void on_samplerate_currentIndexChanged(int index);
    void on_ifFrequency_currentIndexChanged(int index);
    void on_decim_currentIndexChanged(int index);
    void on_fcPos_currentIndexChanged(int index);
    void on_gainTunerOn_toggled(bool checked);
    void on_gainTuner_valueChanged(int value);
    void on_gainManualOn_toggled(bool checked);
    void on_gainLNA_toggled(bool checked);
    void on_gainMixer_toggled(bool checked);
    void on_gainBaseband_valueChanged(int value);
    void on_startStop_toggled(bool checked);
    void on_record_toggled(bool checked);
};


// ====================================================================

class SDRPlaySampleRates {
public:
    static unsigned int getRate(unsigned int rate_index);
    static unsigned int getRateIndex(unsigned int rate);
    static unsigned int getNbRates();
private:
    static const unsigned int m_nb_rates = 18;
    static unsigned int m_rates[m_nb_rates];
};

class SDRPlayBandwidths {
public:
    static unsigned int getBandwidth(unsigned int bandwidth_index);
    static unsigned int getBandwidthIndex(unsigned int bandwidth);
    static unsigned int getNbBandwidths();
private:
    static const unsigned int m_nb_bw = 8;
    static unsigned int m_bw[m_nb_bw];
};

class SDRPlayIF {
public:
    static unsigned int getIF(unsigned int if_index);
    static unsigned int getIFIndex(unsigned int iff);
    static unsigned int getNbIFs();
private:
    static const unsigned int m_nb_if = 4;
    static unsigned int m_if[m_nb_if];
};

class SDRPlayBands {
public:
    static QString getBandName(unsigned int band_index);
    static unsigned int getBandLow(unsigned int band_index);
    static unsigned int getBandHigh(unsigned int band_index);
    static unsigned int getNbBands();
private:
    static const unsigned int m_nb_bands = 8;
    static unsigned int m_bandLow[m_nb_bands];
    static unsigned int m_bandHigh[m_nb_bands];
    static const char* m_bandName[m_nb_bands];
};

#endif /* PLUGINS_SAMPLESOURCE_SDRPLAY_SDRPLAYGUI_H_ */
