///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_RADIOASTRONOMYSETTINGS_H
#define INCLUDE_RADIOASTRONOMYSETTINGS_H

#include <QByteArray>
#include <QString>
#include <QDateTime>

#include "dsp/dsptypes.h"

class Serializable;

// Number of columns in the tables
#define RADIOASTRONOMY_POWERTABLE_COLUMNS 28

// Number of sensors
#define RADIOASTRONOMY_SENSORS 2

struct RadioAstronomySettings
{
    struct AvailableFeature
    {
        int m_featureSetIndex;
        int m_featureIndex;
        QString m_type;

        AvailableFeature() = default;
        AvailableFeature(const AvailableFeature&) = default;
        AvailableFeature& operator=(const AvailableFeature&) = default;
        bool operator==(const AvailableFeature& a) const {
            return (m_featureSetIndex == a.m_featureSetIndex) && (m_featureIndex == a.m_featureIndex) && (m_type == a.m_type);
        }
    };

    int m_inputFrequencyOffset;
    int m_sampleRate;
    int m_rfBandwidth;
    int m_integration;          //!< Number of samples to integrate

    int m_fftSize;
    enum FFTWindow {
        REC,
        HAN
    } m_fftWindow;              //!< FFT windowing function
    QString m_filterFreqs;      //!< List of channels (bin indices) to filter in FFT to remove RFI

    QString m_starTracker;      //!< Name of Star Tracker plugin to link with
    QString m_rotator;          //!< Name of antenna rotator

    float m_tempRX;             //!< Receiver noise temperature in K (Front end NF)
    float m_tempCMB;            //!< Cosmic microwave background temperature in K
    float m_tempGal;            //!< Galactic background temperature in K
    float m_tempSP;             //!< Spillover temperature in K
    float m_tempAtm;            //!< Atmospheric temperature in K
    float m_tempAir;            //!< Surface air temperature in C
    float m_zenithOpacity;      //!< Opacity of atmosphere at zenith
    float m_elevation;          //!< Elevation in degrees, if not using value from Star Tracker
    bool m_tempGalLink;
    bool m_tempAtmLink;
    bool m_tempAirLink;
    bool m_elevationLink;

    float m_gainVariation;      //!< delta G/G
    enum SourceType {
        UNKNOWN,
        COMPACT,
        EXTENDED,
        SUN,
        CAS_A
    } m_sourceType;             //!< Whether the source it smaller than the beam
    float m_omegaS;             //!< Source angle
    enum AngleUnits {
        DEGREES,
        STERRADIANS
    } m_omegaSUnits;
    enum AngleUnits m_omegaAUnits;

    bool m_spectrumPeaks;
    bool m_spectrumMarkers;
    bool m_spectrumTemp;
    bool m_spectrumReverseXAxis;
    bool m_spectrumRefLine;
    bool m_spectrumLAB;
    bool m_spectrumDistance;
    bool m_spectrumLegend;
    float m_spectrumReference;  //!< In dB
    float m_spectrumRange;      //!< In dB
    float m_spectrumSpan;       //!< In Mhz
    float m_spectrumCenterFreqOffset; //!< Offset - rather than absolute - In Mhz
    bool m_spectrumAutoscale;
    enum SpectrumYScale {
        SY_DBFS,
        SY_SNR,
        SY_DBM,
        SY_TSYS,
        SY_TSOURCE
    } m_spectrumYScale;
    enum SpectrumBaseline {
        SBL_TSYS0,
        SBL_TMIN,
        SBL_CAL_COLD
    } m_spectrumBaseline;
    bool m_recalibrate;
    float m_tCalHot;            //!< Hot calibration antenna noise temperature in K (Sky + Spillover?)
    float m_tCalCold;           //!< Cold calibration antenna noise temperature in K
    enum Line {
        HI,
        OH,
        DI,
        CUSTOM_LINE
    } m_line;                   //!< Spectral line to plot and use as Doppler reference
    float m_lineCustomFrequency;    //!< Spectral line frequency when m_line==CUSTOM
    enum RefFrame {
        TOPOCENTRIC,
        BCRS,
        LSR
    } m_refFrame;               //!< Reference frame for velocities

    float m_sunDistanceToGC;            //!< Sun distance to Galactic Center In kpc
    float m_sunOrbitalVelocity;         //!< In km/s around GC

    bool m_powerPeaks;
    bool m_powerMarkers;
    bool m_powerAvg;
    bool m_powerLegend;
    bool m_powerShowTsys0;     //!< Plot total noise temperature
    bool m_powerShowAirTemp;
    bool m_powerShowGaussian;
    bool m_powerShowFiltered;
    bool m_powerShowMeasurement;
    float m_powerReference;  //!< In dB
    float m_powerRange;      //!< In dB
    bool m_powerAutoscale;
    enum PowerYData {
        PY_POWER,
        PY_TSYS,
        PY_TSOURCE,
        PY_FLUX,
        PY_2D_MAP
    } m_powerYData;
    enum PowerYUnits {
        PY_DBFS,
        PY_DBM,
        PY_WATTS,
        PY_KELVIN,
        PY_SFU,
        PY_JANSKY
    } m_powerYUnits;
    enum PowerFilter {
        FILT_MEDIAN,
        FILT_MOVING_AVERAGE,
        // TODO: FILT_LPF
    } m_powerFilter;
    int m_powerFilterN;

    enum SweepType {
        SWP_AZEL,
        SWP_LB,
        SWP_OFFSET
    };

    bool m_power2DLinkSweep;    //<! Update setting below to match sweep parameters
    SweepType m_power2DSweepType;
    int m_power2DWidth;
    int m_power2DHeight;
    float m_power2DXMin;
    float m_power2DXMax;
    float m_power2DYMin;
    float m_power2DYMax;
    bool m_powerColourAutoscale;
    float m_powerColourScaleMin;
    float m_powerColourScaleMax;
    QString m_powerColourPalette;

    enum RunMode {
        SINGLE,
        CONTINUOUS,
        SWEEP
    } m_runMode;
    bool m_sweepStartAtTime;
    QDateTime m_sweepStartDateTime;
    SweepType m_sweepType;
    float m_sweep1Start;
    float m_sweep1Stop;
    float m_sweep1Step;
    float m_sweep1Delay;                //!< Delay after rotation before a measurement
    float m_sweep2Start;
    float m_sweep2Stop;
    float m_sweep2Step;
    float m_sweep2Delay;                //!< Delay after a measurement before starting the next

    QString m_sensorName[RADIOASTRONOMY_SENSORS];
    QString m_sensorDevice[RADIOASTRONOMY_SENSORS];
    QString m_sensorInit[RADIOASTRONOMY_SENSORS];
    QString m_sensorMeasure[RADIOASTRONOMY_SENSORS];
    bool m_sensorEnabled[RADIOASTRONOMY_SENSORS];
    bool m_sensorVisible[RADIOASTRONOMY_SENSORS];
    float m_sensorMeasurePeriod;

    bool m_gpioEnabled;
    int m_gpioPin;
    int m_gpioSense;
    QString m_startCalCommand;
    QString m_stopCalCommand;
    float m_calCommandDelay;            //!< Delay in seconds after command before starting cal

    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    Serializable *m_rollupState;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    int m_powerTableColumnIndexes[RADIOASTRONOMY_POWERTABLE_COLUMNS];//!< How the columns are ordered in the table
    int m_powerTableColumnSizes[RADIOASTRONOMY_POWERTABLE_COLUMNS];  //!< Size of the columns in the table

    RadioAstronomySettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    static const QStringList m_pipeTypes;
    static const QStringList m_pipeURIs;
};

#endif /* INCLUDE_RADIOASTRONOMYSETTINGS_H */
