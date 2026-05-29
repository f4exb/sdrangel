///////////////////////////////////////////////////////////////////////////////////
// SDRangel Fobos SDR Agile native source backend
// Agile-first synchronous streaming worker.
// Stop lifecycle: request reader exit, join reader, then stop_sync and close the device.
///////////////////////////////////////////////////////////////////////////////////

#ifndef _FOBOS_FOBOSWORKER_H_
#define _FOBOS_FOBOSWORKER_H_

#include <QObject>
#include <QMutex>
#include <QString>
#include <atomic>
#include <cstdint>
#include <thread>

#include "dsp/samplesinkfifo.h"
#include "fobossettings.h"

struct fobos_dev_t;

enum class FobosRuntimeBackend { None, Agile, Regular };

class FOBOSWorker : public QObject
{
    Q_OBJECT
public:
    explicit FOBOSWorker(SampleSinkFifo* sampleFifo, QObject* parent = nullptr);
    ~FOBOSWorker() override;

    void startWork();
    void stopWork();

    // Kept for compatibility with the TestSource-derived input/controller code.
    void setSamplerate(int samplerate);
    void setCenterFrequency(uint64_t centerFrequencyHz);
    void setLog2Decimation(unsigned int log2_decim);
    void setFcPos(int fcPos);
    void setBitSize(uint32_t bitSizeIndex);
    void setAmplitudeBits(int32_t amplitudeBits);
    void setLnaGain(unsigned int lnaGain);
    void setVgaGain(unsigned int vgaGain);
    void setInputMode(int inputMode);
    void setBandwidthPercent(unsigned int bandwidthPercent);
    void setGpoMask(unsigned int gpoMask);
    void setExternalClock(bool externalClock);
    void setDCFactor(float dcFactor);
    void setIFactor(float iFactor);
    void setQFactor(float qFactor);
    void setPhaseImbalance(float phaseImbalance);
    void setFrequencyShift(int shift);
    void setToneFrequency(int toneFrequency);
    void setModulation(int modulation);
    void setAMModulation(float amModulation);
    void setFMDeviation(float deviation);
    void setPattern0();
    void setPattern1();
    void setPattern2();

signals:
    void backendStatusChanged(const QString& backend, const QString& details);

private:
#ifdef _WIN32
    using fobos_sdr_get_api_info_t = int (__cdecl *)(char* lib_version, char* drv_version);
    using fobos_sdr_get_device_count_t = int (__cdecl *)();
    using fobos_sdr_list_devices_t = int (__cdecl *)(char* serials);
    using fobos_sdr_open_t = int (__cdecl *)(fobos_dev_t** out_dev, uint32_t index);
    using fobos_sdr_close_t = int (__cdecl *)(fobos_dev_t* dev);
    using fobos_sdr_get_board_info_t = int (__cdecl *)(fobos_dev_t* dev, char* hw_revision, char* fw_version, char* manufacturer, char* product, char* serial);
    using fobos_sdr_error_name_t = const char* (__cdecl *)(int error_code);
    using fobos_sdr_set_frequency_t = int (__cdecl *)(fobos_dev_t* dev, double value_hz);
    using fobos_sdr_set_samplerate_t = int (__cdecl *)(fobos_dev_t* dev, double value_hz);
    using fobos_sdr_get_samplerates_t = int (__cdecl *)(fobos_dev_t* dev, double* values, uint32_t* count);
    using fobos_sdr_set_direct_sampling_t = int (__cdecl *)(fobos_dev_t* dev, int value);
    using fobos_sdr_set_auto_bandwidth_t = int (__cdecl *)(fobos_dev_t* dev, double value);
    using fobos_sdr_set_user_gpo_t = int (__cdecl *)(fobos_dev_t* dev, uint32_t value);
    using fobos_sdr_set_clk_source_t = int (__cdecl *)(fobos_dev_t* dev, int value);
    using fobos_sdr_set_lna_gain_t = int (__cdecl *)(fobos_dev_t* dev, unsigned int value);
    using fobos_sdr_set_vga_gain_t = int (__cdecl *)(fobos_dev_t* dev, unsigned int value);
    using fobos_sdr_start_sync_t = int (__cdecl *)(fobos_dev_t* dev, uint32_t buf_length);
    using fobos_sdr_read_sync_t = int (__cdecl *)(fobos_dev_t* dev, float* buf, uint32_t* actual_buf_length);
    using fobos_sdr_stop_sync_t = int (__cdecl *)(fobos_dev_t* dev);

    using fobos_rx_get_api_info_t = int (__cdecl *)(char* lib_version, char* drv_version);
    using fobos_rx_get_device_count_t = int (__cdecl *)();
    using fobos_rx_list_devices_t = int (__cdecl *)(char* serials);
    using fobos_rx_open_t = int (__cdecl *)(fobos_dev_t** out_dev, uint32_t index);
    using fobos_rx_close_t = int (__cdecl *)(fobos_dev_t* dev);
    using fobos_rx_get_board_info_t = int (__cdecl *)(fobos_dev_t* dev, char* hw_revision, char* fw_version, char* manufacturer, char* product, char* serial);
    using fobos_rx_error_name_t = const char* (__cdecl *)(int error_code);
    using fobos_rx_set_frequency_t = int (__cdecl *)(fobos_dev_t* dev, double value_hz, double* actual_hz);
    using fobos_rx_set_samplerate_t = int (__cdecl *)(fobos_dev_t* dev, double value_hz, double* actual_hz);
    using fobos_rx_get_samplerates_t = int (__cdecl *)(fobos_dev_t* dev, double* values, unsigned int* count);
    using fobos_rx_set_direct_sampling_t = int (__cdecl *)(fobos_dev_t* dev, unsigned int enabled);
    using fobos_rx_set_user_gpo_t = int (__cdecl *)(fobos_dev_t* dev, uint8_t value);
    using fobos_rx_set_clk_source_t = int (__cdecl *)(fobos_dev_t* dev, int value);
    using fobos_rx_set_lna_gain_t = int (__cdecl *)(fobos_dev_t* dev, unsigned int value);
    using fobos_rx_set_vga_gain_t = int (__cdecl *)(fobos_dev_t* dev, unsigned int value);
    using fobos_rx_start_sync_t = int (__cdecl *)(fobos_dev_t* dev, uint32_t buf_length);
    using fobos_rx_read_sync_t = int (__cdecl *)(fobos_dev_t* dev, float* buf, uint32_t* actual_buf_length);
    using fobos_rx_stop_sync_t = int (__cdecl *)(fobos_dev_t* dev);
#endif

    bool runAgileStart();
    bool runRegularStart();
    int callSetFrequency(fobos_dev_t* dev, double valueHz);
    int callSetSamplerate(fobos_dev_t* dev, double valueHz);
    int callSetDirectSampling(fobos_dev_t* dev, int inputMode);
    int callSetAutoBandwidth(fobos_dev_t* dev, unsigned int bandwidthPercent);
    int callSetGpo(fobos_dev_t* dev, unsigned int gpoMask);
    int callSetClockSource(fobos_dev_t* dev, bool externalClock);
    int callSetLnaGain(fobos_dev_t* dev, unsigned int lnaGain);
    int callSetVgaGain(fobos_dev_t* dev, unsigned int vgaGain);
    void readerLoop();
    void cleanupAfterReaderJoined(bool readerJoined);
    void logLine(const char* fmt, ...) const;
    void logTimestamp() const;
    const char* errorName(int code) const;
    FixReal floatToFix(float v) const;

    SampleSinkFifo* m_sampleFifo;
    SampleVector m_convertBuffer;

    QMutex m_settingsMutex;
    uint64_t m_centerFrequencyHz;
    int m_samplerate;
    unsigned int m_log2Decim;
    int m_fcPos;
    int m_frequencyShift;
    unsigned int m_lnaGain;
    unsigned int m_vgaGain;
    int m_inputMode;
    unsigned int m_bandwidthPercent;
    unsigned int m_gpoMask;
    bool m_externalClock;
    std::atomic<double> m_iqGain;
    std::atomic<int> m_runtimeInputMode;

    QMutex m_gainDiagMutex;
    bool m_gainDiagActive;
    bool m_gainDiagCollecting;
    QString m_gainDiagKind;
    unsigned int m_gainDiagValue;
    unsigned int m_gainDiagSkipBuffers;
    unsigned int m_gainDiagTargetBuffers;
    unsigned int m_gainDiagCollectedBuffers;
    uint64_t m_gainDiagSamples;
    double m_gainDiagPower;
    double m_gainDiagPeak;

    std::atomic_bool m_running;
    std::atomic_bool m_stopRequested;
    std::atomic_bool m_syncStarted;
    std::atomic_bool m_readerActive;
    std::atomic_bool m_readerFinished;
    std::atomic<fobos_dev_t*> m_dev;
    std::thread m_readerThread;
    FobosRuntimeBackend m_runtimeBackend;

#ifdef _WIN32
    void* m_libraryHandle;
    fobos_sdr_error_name_t m_errorName;
    fobos_sdr_close_t m_closeDev;
    fobos_sdr_read_sync_t m_readSync;
    fobos_sdr_stop_sync_t m_stopSync;
    fobos_sdr_set_frequency_t m_setFrequency;
    fobos_sdr_set_samplerate_t m_setSamplerate;
    fobos_sdr_get_samplerates_t m_getSamplerates;
    fobos_sdr_set_direct_sampling_t m_setDirectSampling;
    fobos_sdr_set_auto_bandwidth_t m_setAutoBandwidth;
    fobos_sdr_set_user_gpo_t m_setUserGpo;
    fobos_sdr_set_clk_source_t m_setClkSource;
    fobos_sdr_set_lna_gain_t m_setLnaGain;
    fobos_sdr_set_vga_gain_t m_setVgaGain;

    fobos_rx_set_frequency_t m_regularSetFrequency;
    fobos_rx_set_samplerate_t m_regularSetSamplerate;
    fobos_rx_get_samplerates_t m_regularGetSamplerates;
    fobos_rx_set_direct_sampling_t m_regularSetDirectSampling;
    fobos_rx_set_user_gpo_t m_regularSetUserGpo;
    fobos_rx_set_clk_source_t m_regularSetClkSource;
    fobos_rx_set_lna_gain_t m_regularSetLnaGain;
    fobos_rx_set_vga_gain_t m_regularSetVgaGain;
#endif

    uint64_t m_totalReads;
    uint64_t m_totalSamples;
    uint64_t m_totalWritten;
    uint64_t m_failedReads;
};

#endif // _FOBOS_FOBOSWORKER_H_
