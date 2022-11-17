///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_RADIOASTRONOMYGUI_H
#define INCLUDE_RADIOASTRONOMYGUI_H

#include <QTableWidgetItem>
#include <QPushButton>
#include <QToolButton>
#include <QHBoxLayout>
#include <QMenu>
#include <QtCharts>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDebug>

#include "channel/channelgui.h"
#include "dsp/channelmarker.h"
#include "util/messagequeue.h"
#include "util/httpdownloadmanager.h"
#include "settings/rollupstate.h"

#include "radioastronomysettings.h"
#include "radioastronomy.h"

class PluginAPI;
class DeviceUISet;
class BasebandSampleSink;
class RadioAstronomy;
class RadioAstronomyGUI;

namespace Ui {
    class RadioAstronomyGUI;
}
class RadioAstronomyGUI;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
using namespace QtCharts;
#endif

class RadioAstronomyGUI : public ChannelGUI {
    Q_OBJECT

    struct FFTMeasurement {
        QDateTime m_dateTime;
        qint64 m_centerFrequency;
        int m_sampleRate;
        int m_integration;
        int m_rfBandwidth;

        int m_fftSize;
        Real* m_fftData;
        Real* m_db;             // dBFS
        Real* m_snr;            // SNR (noise is cold cal data)
        Real* m_temp;           // Temp in Kelvin base on hot/cold cal data
        Real m_totalPower;      // Total power based on sum of fftData (i.e unknown units)
        Real m_totalPowerdBFS;  // m_totalPower in dB
        Real m_totalPowerdBm;   // Total power in dBm
        Real m_totalPowerWatts; // Total power in Watts
        Real m_tSys;            // Total temp in K
        Real m_tSys0;           // Total unwanted noise (E.g. Trx+Tgal..) in K
        Real m_tSource;         // Estimated source temp (tSys-tSys0) in K
        Real m_flux;            // Average spectral flux density of source in W m^-2 Hz^-1
        Real m_sigmaT;          // Temperature variation
        Real m_sigmaS;          // Flux variation
        Real m_tempMin;         // Minimum value in m_temp array
        RadioAstronomySettings::SpectrumBaseline m_baseline;

        float m_omegaA;
        float m_omegaS;

        bool m_coordsValid;     //!< Whether follow variables are valid
        float m_ra;
        float m_dec;
        float m_azimuth;
        float m_elevation;
        float m_l;
        float m_b;
        float m_vBCRS;
        float m_vLSR;
        float m_solarFlux;
        float m_airTemp;
        float m_skyTemp;
        float m_sensor[RADIOASTRONOMY_SENSORS];

        int m_sweepIndex;

        FFTMeasurement() :
            m_fftSize(0),
            m_fftData(nullptr),
            m_db(nullptr),
            m_snr(nullptr),
            m_temp(nullptr),
            m_totalPower(0.0f),
            m_totalPowerdBFS(0.0f),
            m_totalPowerdBm(0.0f),
            m_totalPowerWatts(0.0f),
            m_tSys(0.0f),
            m_tSys0(0.0f),
            m_tSource(0.0f),
            m_flux(0.0f),
            m_sigmaT(0.0f),
            m_sigmaS(0.0f),
            m_tempMin(0.0f),
            m_baseline(RadioAstronomySettings::SBL_TSYS0),
            m_coordsValid(false),
            m_airTemp(0.0),
            m_skyTemp(0.0),
            m_sweepIndex(0)
        {
        }

        ~FFTMeasurement()
        {
            delete[] m_fftData;
            delete[] m_db;
            delete[] m_snr;
            delete[] m_temp;
        }
    };

    struct LABData {

        float m_l;
        float m_b;
        QList<Real> m_vlsr;
        QList<Real> m_temp;

        LABData() :
            m_l(0.0f),
            m_b(0.0f)
        {
        }

        void read(QFile* file, float l, float b);
        void toSeries(QLineSeries *series);
    };

    struct SensorMeasurement {

        QDateTime m_dateTime;
        double m_value;

        SensorMeasurement(QDateTime dateTime, double value) :
            m_dateTime(dateTime),
            m_value(value)
        {
        }
    };

    class SensorMeasurements {

        QLineSeries* m_series;
        QValueAxis* m_yAxis;
        double m_max;
        double m_min;
        QList<SensorMeasurement *> m_measurements;

    public:

        SensorMeasurements() :
            m_series(nullptr),
            m_yAxis(nullptr)
        {
        }

        void init(const QString& name, bool visible);
        void setName(const QString& name);
        void clicked(bool checked);
        void append(SensorMeasurement *measurement);
        void addToSeries(SensorMeasurement *measurement);
        void addAllToSeries();
        void clear();
        void addToChart(QChart* chart, QDateTimeAxis* xAxis);
        void setPen(const QPen& pen);
        QValueAxis* yAxis() const;
        qreal lastValue();
    };

public:
    static RadioAstronomyGUI* create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel);
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual void setWorkspaceIndex(int index) { m_settings.m_workspaceIndex = index; };
    virtual int getWorkspaceIndex() const { return m_settings.m_workspaceIndex; };
    virtual void setGeometryBytes(const QByteArray& blob) { m_settings.m_geometryBytes = blob; };
    virtual QByteArray getGeometryBytes() const { return m_settings.m_geometryBytes; };
    virtual QString getTitle() const { return m_settings.m_title; };
    virtual QColor getTitleColor() const  { return m_settings.m_rgbColor; };
    virtual void zetHidden(bool hidden) { m_settings.m_hidden = hidden; }
    virtual bool getHidden() const { return m_settings.m_hidden; }
    virtual ChannelMarker& getChannelMarker() { return m_channelMarker; }
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }
    virtual void setStreamIndex(int streamIndex) { m_settings.m_streamIndex = streamIndex; }

public slots:
    void channelMarkerChangedByCursor();
    void channelMarkerHighlightedByCursor();

private:
    Ui::RadioAstronomyGUI* ui;
    PluginAPI* m_pluginAPI;
    DeviceUISet* m_deviceUISet;
    ChannelMarker m_channelMarker;
    RollupState m_rollupState;
    RadioAstronomySettings m_settings;
    qint64 m_deviceCenterFrequency;
    bool m_doApplySettings;
    QList<RadioAstronomySettings::AvailableFeature> m_availableFeatures;

    int m_basebandSampleRate;
    quint64 m_centerFrequency;

    RadioAstronomy* m_radioAstronomy;
    uint32_t m_tickCount;
    MessageQueue m_inputMessageQueue;

    QMenu *powerTableMenu;                        // Column select context menu
    QMenu *copyMenu;

    QChart *m_powerChart;
    QLineSeries *m_powerSeries;
    QDateTimeAxis *m_powerXAxis;
    bool m_powerXAxisSameDay;
    QValueAxis *m_powerYAxis;
    QScatterSeries *m_powerPeakSeries;
    QScatterSeries *m_powerMarkerSeries;
    QLineSeries *m_powerTsys0Series;
    QLineSeries *m_powerGaussianSeries;
    QLineSeries *m_powerFilteredSeries;
    double m_powerMin;                          // For axis autoscale
    double m_powerMax;
    bool m_powerPeakValid;
    qreal m_powerMinX;                          // For min peak
    qreal m_powerMinY;
    qreal m_powerMaxX;                          // For max peak
    qreal m_powerMaxY;

    QChart *m_2DChart;
    QValueAxis *m_2DXAxis;
    QValueAxis *m_2DYAxis;
    float *m_2DMapIntensity;
    float m_2DMapMax;
    float m_2DMapMin;
    QImage m_2DMap;
    int m_sweepIndex;

    SensorMeasurements m_airTemps;
    SensorMeasurements m_sensors[RADIOASTRONOMY_SENSORS];

    QChart *m_calChart;
    QValueAxis *m_calXAxis;
    QValueAxis *m_calYAxis;

    QLineSeries *m_calHotSeries;
    QLineSeries *m_calColdSeries;
    FFTMeasurement* m_calHot;
    FFTMeasurement* m_calCold;
    double *m_calG;

    QChart *m_fftChart;
    QLineSeries *m_fftSeries;
    QLineSeries *m_fftHlineSeries;
    QScatterSeries *m_fftPeakSeries;
    QScatterSeries *m_fftMarkerSeries;
    QLineSeries *m_fftGaussianSeries;
    QLineSeries *m_fftLABSeries;
    QValueAxis *m_fftXAxis;
    QValueAxis *m_fftYAxis;
    QValueAxis *m_fftDopplerAxis;
    QList<FFTMeasurement*> m_fftMeasurements;

    // Markers
    bool m_powerM1Valid;
    bool m_powerM2Valid;
    qreal m_powerM1X;
    qreal m_powerM1Y;
    qreal m_powerM2X;
    qreal m_powerM2Y;
    bool m_spectrumM1Valid;
    bool m_spectrumM2Valid;
    qreal m_spectrumM1X;
    qreal m_spectrumM1Y;
    qreal m_spectrumM2X;
    qreal m_spectrumM2Y;

    // Target received from Star Tracker
    bool m_coordsValid;
    float m_ra;
    float m_dec;
    float m_azimuth;
    float m_elevation;
    float m_l;
    float m_b;
    float m_vBCRS;
    float m_vLSR;
    float m_solarFlux;
    float m_skyTemp;
    float m_beamWidth;

    float m_lLAB;     // Galactic coords for current LAB data
    float m_bLAB;
    QString m_filenameLAB;
    bool m_downloadingLAB;
    QList<LABData *> m_dataLAB;

    // Circular buffer to filtering power series data
    qreal *m_window;
    qreal *m_windowSorted;
    int m_windowIdx;
    int m_windowCount;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;
    HttpDownloadManager m_dlm;

    explicit RadioAstronomyGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent = 0);
    virtual ~RadioAstronomyGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void displaySpectrumLineFrequency();
    void displayRunModeSettings();
    void updateAvailableFeatures();
    void updateRotatorList(const QList<RadioAstronomySettings::AvailableFeature>& rotators);
    bool handleMessage(const Message& message);
    void makeUIConnections();
    void updateAbsoluteCenterFrequency();

    double degreesToSteradian(double deg) const;
    double hpbwToSteradians(double hpbw) const;
    double calcOmegaA() const;
    double calcOmegaS() const;
    double beamFillingFactor() const;
    void updateOmegaA();
    void powerMeasurementReceived(FFTMeasurement *fft, bool skipCalcs);
    void calCompletetReceived(const RadioAstronomy::MsgCalComplete& measurement);
    void calcFFTPower(FFTMeasurement* fft);
    void calcFFTTotalPower(FFTMeasurement* fft);
    void calcFFTTemperatures(FFTMeasurement* fft);
    void calcFFTTotalTemperature(FFTMeasurement* fft);
    void calcFFTMinTemperature(FFTMeasurement* fft);
    void addFFT(FFTMeasurement *fft, bool skipCalcs=false);
    void fftMeasurementReceived(const RadioAstronomy::MsgFFTMeasurement& measurement);
    void addToPowerSeries(FFTMeasurement *fft, bool skipCalcs=false);
    void plotPowerGaussian();
    void plotSpectrum();
    void plotCalSpectrum();
    void plotCalMeasurements();
    void plotFFTMeasurement(int index);
    void plotFFTMeasurement();
    void plotTempGaussian(double startFreq, double freqStep, int steps);
    void plotRefLine(FFTMeasurement *fft);
    void plotLAB();
    void plotLAB(float l, float b, float beamWidth);
    LABData* parseLAB(QFile* file, float l, float b);
    int fftSizeToIndex(int size);
    double dopplerToVelocity(double centre, double f, FFTMeasurement *fft);
    QHash<QString,int> csvHeadersToHash(QStringList cols);
    QString csvData(QHash<QString,int> hash, QStringList cols, QString col);
    bool hasNeededFFTData(QHash<QString,int> hash);
    void saveFFT(QTextStream& out, const FFTMeasurement* fft);
    FFTMeasurement* loadFFT(QHash<QString,int> hash, QStringList cols);
    void clearData();
    void clearCalData();
    bool deleteRow(int row);
    void deleteRowsComplete(bool deletedCurrent, int next);
    void calcCalibrationScaleFactors();
    void calibrate();
    void recalibrate();
    void calcGalacticBackgroundTemp();
    void calcAtmosphericTemp();
    void calcCalAvgDiff();
    void calcCalTrx();
    void calcCalTsp();
    void calcAverages();
    void calcFWHM();
    void calcHPBWFromFWHM();
    void calcFHWMFromHPBW();
    void calcColumnDensity();
    qreal calcSeriesFloor(QXYSeries *series, int percent=10);
    void calcVrAndDistanceToPeak(double freq, FFTMeasurement *fft, int row);
    int calcDistanceToPeak(double vr, float l, float b, double& r, double &d1, double &d2);
    void calcDistances();
    void clearLoSMarker(const QString& name);
    void updateLoSMarker(const QString& name, float l, float b, float d);
    bool losMarkerEnabled(const QString& name);
    void showLoSMarker(const QString& name);
    void showLoSMarker(int row);
    void sensorMeasurementReceived(const RadioAstronomy::MsgSensorMeasurement& measurement);
    void updateSpectrumMarkerTableVisibility();
    void updatePowerMarkerTableVisibility();
    void updatePowerChartWidgetsVisibility();
    void updateSpectrumChartWidgetsVisibility();
    void updateSpectrumSelect();
    void updatePowerSelect();
    void spectrumAutoscale();
    void powerAutoscale();
    void powerAutoscaleY(bool adjustAxis);
    void calcSpectrumMarkerDelta();
    void calcPowerMarkerDelta();
    void calcPowerPeakDelta();
    QRgb intensityToColor(float intensity);
    void set2DAxisTitles();
    void update2DSettingsFromSweep();
    void create2DImage();
    void update2DImage(FFTMeasurement* fft, bool skipCalcs);
    void recolour2DImage();
    void power2DAutoscale();
    void powerColourAutoscale();
    void updatePowerColourScaleStep();
    void updateSpectrumMinMax(qreal x, qreal y);
    RadioAstronomyGUI::FFTMeasurement* currentFFT();
    void spectrumUpdateXRange(FFTMeasurement* fft = nullptr);
    void spectrumUpdateYRange(FFTMeasurement* fft = nullptr);
    void updateDistanceColumns();
    void updateBWLimits();
    void updateIntegrationTime();
    void updateTSys0();
    double calcTSys0() const;
    double calcTau() const;
    double calcTau(const FFTMeasurement* fft) const;
    double calcSigmaT(double tSys) const;
    double calcSigmaS(double tSys) const;
    double calcSigmaT(const FFTMeasurement* fft) const;
    double calcSigmaS(const FFTMeasurement* fft) const;
    double calcFlux(double Ta, const FFTMeasurement *fft) const;
    double calcTSource(FFTMeasurement *fft) const;
    void updatePowerColumns(int row, FFTMeasurement* fft);
    void calcPowerChartTickCount(int width);
    void calcSpectrumChartTickCount(QValueAxis *axis, int width);
    int powerYUnitsToIndex(RadioAstronomySettings::PowerYUnits units);

    void leaveEvent(QEvent*);
    void enterEvent(EnterEventType*);

    void resizePowerTable();
    void resizePowerMarkerTable();
    void resizeSpectrumMarkerTable();
    QAction *createCheckableItem(QString& text, int idx, bool checked, const char *slot);

    enum PowerTableCol {
        POWER_COL_DATE,
        POWER_COL_TIME,
        POWER_COL_POWER,
        POWER_COL_POWER_DB,
        POWER_COL_POWER_DBM,
        POWER_COL_TSYS,
        POWER_COL_TSYS0,
        POWER_COL_TSOURCE,
        POWER_COL_TB,
        POWER_COL_TSKY,
        POWER_COL_FLUX,
        POWER_COL_SIGMA_T,
        POWER_COL_SIGMA_S,
        POWER_COL_OMEGA_A,
        POWER_COL_OMEGA_S,
        POWER_COL_RA,
        POWER_COL_DEC,
        POWER_COL_GAL_LON,
        POWER_COL_GAL_LAT,
        POWER_COL_AZ,
        POWER_COL_EL,
        POWER_COL_VBCRS,
        POWER_COL_VLSR,
        POWER_COL_SOLAR_FLUX,
        POWER_COL_AIR_TEMP,
        POWER_COL_SENSOR_1,
        POWER_COL_SENSOR_2
    };

    enum PowerMarkerTable {
        POWER_MARKER_COL_NAME,
        POWER_MARKER_COL_DATE,
        POWER_MARKER_COL_TIME,
        POWER_MARKER_COL_VALUE,
        POWER_MARKER_COL_DELTA_X,
        POWER_MARKER_COL_DELTA_Y,
        POWER_MARKER_COL_DELTA_TO
    };

    enum PowerMarkerRow {
        POWER_MARKER_ROW_PEAK_MAX,
        POWER_MARKER_ROW_PEAK_MIN,
        POWER_MARKER_ROW_M1,
        POWER_MARKER_ROW_M2,
        POWER_MARKER_ROWS
    };

    enum SpectrumMarkerTable {
        SPECTRUM_MARKER_COL_NAME,
        SPECTRUM_MARKER_COL_FREQ,
        SPECTRUM_MARKER_COL_VALUE,
        SPECTRUM_MARKER_COL_DELTA_X,
        SPECTRUM_MARKER_COL_DELTA_Y,
        SPECTRUM_MARKER_COL_DELTA_TO,
        SPECTRUM_MARKER_COL_VR,
        SPECTRUM_MARKER_COL_R,
        SPECTRUM_MARKER_COL_D,
        SPECTRUM_MARKER_COL_PLOT_MAX,
        SPECTRUM_MARKER_COL_R_MIN,
        SPECTRUM_MARKER_COL_V
    };

    enum SpecrumMarkerRow {
        SPECTRUM_MARKER_ROW_PEAK,
        SPECTRUM_MARKER_ROW_M1,
        SPECTRUM_MARKER_ROW_M2,
        SPECTRUM_MARKER_ROWS
    };

protected:
    void resizeEvent(QResizeEvent* size);

private slots:
    void on_deltaFrequency_changed(qint64 value);
    void on_sampleRate_changed(qint64 index);
    void on_rfBW_changed(qint64 index);

    void on_integration_changed(qint64 value);
    void on_fftSize_currentIndexChanged(int index);
    void on_fftWindow_currentIndexChanged(int index);
    void on_filterFreqs_editingFinished();

    void on_starTracker_currentTextChanged(const QString& text);
    void on_rotator_currentTextChanged(const QString& text);
    void on_showSensors_clicked();

    void on_tempRXSelect_currentIndexChanged(int value);
    void on_tempRX_valueChanged(double value);
    void on_tempCMB_valueChanged(double value);
    void on_tempGal_valueChanged(double value);
    void on_tempSP_valueChanged(double value);
    void on_tempAtm_valueChanged(double value);
    void on_tempAir_valueChanged(double value);
    void on_zenithOpacity_valueChanged(double value);
    void on_elevation_valueChanged(double value);
    void on_tempAtmLink_toggled(bool checked);
    void on_tempAirLink_toggled(bool checked);
    void on_tempGalLink_toggled(bool checked);
    void on_elevationLink_toggled(bool checked);

    void on_gainVariation_valueChanged(double value);
    void on_omegaAUnits_currentIndexChanged(int index);
    void on_sourceType_currentIndexChanged(int index);
    void on_omegaS_valueChanged(double value);
    void on_omegaSUnits_currentIndexChanged(int index);

    void on_spectrumChartSelect_currentIndexChanged(int index);
    void on_showCalSettings_clicked();
    void on_startCalHot_clicked();
    void on_startCalCold_clicked();
    void on_recalibrate_toggled(bool checked=false);
    void on_spectrumShowLegend_toggled(bool checked);
    void on_spectrumShowRefLine_toggled(bool checked);
    void on_spectrumTemp_toggled(bool checked);
    void on_spectrumMarker_toggled(bool checked);
    void on_spectrumPeak_toggled(bool checked);
    void on_spectrumReverseXAxis_toggled(bool checked);
    void on_savePowerData_clicked();
    void on_savePowerChartImage_clicked();
    void on_saveSpectrumData_clicked();
    void on_loadSpectrumData_clicked();
    void on_saveSpectrumChartImage_clicked();
    void on_saveSpectrumChartImages_clicked();
    void on_clearData_clicked();
    void on_clearCal_clicked();

    void spectrumSeries_clicked(const QPointF &point);
    void on_spectrumAutoscale_toggled(bool checked=false);
    void on_spectrumAutoscaleX_clicked();
    void on_spectrumAutoscaleY_clicked();
    void on_spectrumReference_valueChanged(double value);
    void on_spectrumRange_valueChanged(double value);
    void on_spectrumCenterFreq_valueChanged(double value);
    void on_spectrumSpan_valueChanged(double value);
    void on_spectrumYUnits_currentIndexChanged(int index);
    void on_spectrumBaseline_currentIndexChanged(int index);
    void on_spectrumIndex_valueChanged(int value);

    void on_spectrumLine_currentIndexChanged(int value);
    void on_spectrumLineFrequency_valueChanged(double value);
    void on_refFrame_currentIndexChanged(int value);
    void on_sunDistanceToGC_valueChanged(double value);
    void on_sunOrbitalVelocity_valueChanged(double value);
    void spectrumMarkerTableItemChanged(QTableWidgetItem *item);

    void on_spectrumGaussianFreq_valueChanged(double value);
    void on_spectrumGaussianAmp_valueChanged(double value);
    void on_spectrumGaussianFloor_valueChanged(double value);
    void on_spectrumGaussianFWHM_valueChanged(double value);
    void on_spectrumGaussianTurb_valueChanged(double value);
    void on_spectrumTemperature_valueChanged(double t);
    void on_spectrumShowLAB_toggled(bool checked=false);
    void on_spectrumShowDistance_toggled(bool checked=false);

    void on_tCalHotSelect_currentIndexChanged(int index);
    void on_tCalHot_valueChanged(double value);
    void on_tCalColdSelect_currentIndexChanged(int index);
    void on_tCalCold_valueChanged(double value);

    void on_powerChartSelect_currentIndexChanged(int index);
    void on_powerYUnits_currentIndexChanged(int index);
    void on_powerShowMarker_toggled(bool checked);
    void on_powerShowTsys0_toggled(bool checked);
    void on_powerShowAirTemp_toggled(bool checked);
    void on_powerShowSensor1_toggled(bool checked);
    void on_powerShowSensor2_toggled(bool checked);
    void on_powerShowPeak_toggled(bool checked);
    void on_powerShowAvg_toggled(bool checked);
    void on_powerShowLegend_toggled(bool checked);
    void powerSeries_clicked(const QPointF &point);

    void on_powerAutoscale_toggled(bool checked);
    void on_powerAutoscaleY_clicked();
    void on_powerAutoscaleX_clicked();
    void on_powerReference_valueChanged(double value);
    void on_powerRange_valueChanged(double value);
    void on_powerStartTime_dateTimeChanged(QDateTime value);
    void on_powerEndTime_dateTimeChanged(QDateTime value);

    void on_powerShowGaussian_clicked(bool checked=false);
    void on_powerGaussianCenter_dateTimeChanged(QDateTime dateTime);
    void on_powerGaussianAmp_valueChanged(double value);
    void on_powerGaussianFloor_valueChanged(double value);
    void on_powerGaussianFWHM_valueChanged(double value);
    void on_powerGaussianHPBW_valueChanged(double value);

    void plotPowerFiltered();
    void addToPowerFilter(qreal y, qreal x);
    void on_powerShowFiltered_clicked(bool checked=false);
    void on_powerFilter_currentIndexChanged(int index);
    void on_powerFilterN_valueChanged(int value);
    void on_powerShowMeasurement_clicked(bool checked=false);

    void on_runMode_currentIndexChanged(int index);
    void on_sweepType_currentIndexChanged(int index);
    void on_startStop_clicked(bool checked=false);
    void on_sweepStartAtTime_currentIndexChanged(int index);
    void on_sweepStartDateTime_dateTimeChanged(const QDateTime& dateTime);
    void on_sweep1Start_valueChanged(double value);
    void on_sweep1Stop_valueChanged(double value);
    void on_sweep1Step_valueChanged(double value);
    void on_sweep1Delay_valueChanged(double value);
    void on_sweep2Start_valueChanged(double value);
    void on_sweep2Stop_valueChanged(double value);
    void on_sweep2Step_valueChanged(double value);
    void on_sweep2Delay_valueChanged(double value);

    void on_power2DLinkSweep_toggled(bool checked=false);
    void on_power2DAutoscale_clicked();
    void on_power2DSweepType_currentIndexChanged(int index);
    void on_power2DWidth_valueChanged(int value);
    void on_power2DHeight_valueChanged(int value);
    void on_power2DXMin_valueChanged(double value);
    void on_power2DXMax_valueChanged(double value);
    void on_power2DYMin_valueChanged(double value);
    void on_power2DYMax_valueChanged(double value);

    void on_powerColourAutoscale_toggled(bool checked=false);
    void on_powerColourScaleMin_valueChanged(double value);
    void on_powerColourScaleMax_valueChanged(double value);
    void on_powerColourPalette_currentIndexChanged(int index);

    void powerTable_sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void powerTable_sectionResized(int logicalIndex, int oldSize, int newSize);
    void powerTableColumnSelectMenu(QPoint pos);
    void powerTableColumnSelectMenuChecked(bool checked = false);
    void on_powerTable_cellDoubleClicked(int row, int column);

    void plotPowerChart();
    void plotPowerVsTimeChart();
    void plot2DChart();
    void plotAreaChanged(const QRectF& plotArea);
    void customContextMenuRequested(QPoint point);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void onMenuDialogCalled(const QPoint& p);
    void handleInputMessages();
    void tick();
    void networkManagerFinished(QNetworkReply *reply);
    void downloadFinished(const QString& filename, bool success);
};

#endif // INCLUDE_RADIOASTRONOMYGUI_H
