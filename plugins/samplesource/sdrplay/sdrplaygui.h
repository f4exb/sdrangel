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

#include <QTimer>
#include "plugin/plugingui.h"

#include "sdrplayinput.h"
#include "sdrplaysettings.h"

class DeviceSampleSource;
class DeviceSourceAPI;
class FileRecord;

namespace Ui {
    class SDRPlayGui;
}

class SDRPlayGui : public QWidget, public PluginGUI {
    Q_OBJECT

public:
    explicit SDRPlayGui(DeviceSourceAPI *deviceAPI, QWidget* parent = NULL);
    virtual ~SDRPlayGui();
    void destroy();

    void setName(const QString& name);
    QString getName() const;

    virtual void resetToDefaults();
    virtual qint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);
    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);
    virtual bool handleMessage(const Message& message);

private:
    Ui::SDRPlayGui* ui;

    DeviceSourceAPI* m_deviceAPI;
    SDRPlaySettings m_settings;
    QTimer m_updateTimer;
    QTimer m_statusTimer;
    DeviceSampleSource* m_sampleSource;
    FileRecord *m_fileSink; //!< File sink to record device I/Q output
    int m_sampleRate;
    quint64 m_deviceCenterFrequency; //!< Center frequency in device
    int m_lastEngineState;

    static const unsigned int m_nbGRValues = 103;
    static const unsigned int m_nbBands = 8;
    static unsigned int m_bbGr[m_nbBands][m_nbGRValues];
    static unsigned int m_lnaGr[m_nbBands][m_nbGRValues];
    static unsigned int m_mixerGr[m_nbBands][m_nbGRValues];

    void displaySettings();
    void sendSettings();
    void updateSampleRateAndFrequency();

private slots:
    void updateHardware();
    void updateStatus();
    void handleSourceMessages();
    void handleDSPMessages();
    void on_centerFrequency_changed(quint64 value);
    void on_ppm_valueChanged(int value);
    void on_dcOffset_toggled(bool checked);
    void on_iqImbalance_toggled(bool checked);
    void on_fBand_currentIndexChanged(int index);
    void on_mirDCCorr_currentIndexChanged(int index);
    void on_mirDCCorrTrackTime_valueChanged(int value);
    void on_bandwidth_currentIndexChanged(int index);
    void on_samplerate_currentIndexChanged(int index);
    void on_decim_currentIndexChanged(int index);
    void on_fcPos_currentIndexChanged(int index);
    void on_gr_valueChanged(int value);
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

class SDRPlayDCCorr {
public:
    static QString getDCCorrName(unsigned int dcCorr_index);
    static unsigned int getNbDCCorrs();
private:
    static const unsigned int m_nb_dcCorrs = 6;
    static const char* m_dcCorrName[m_nb_dcCorrs];
};

#endif /* PLUGINS_SAMPLESOURCE_SDRPLAY_SDRPLAYGUI_H_ */
