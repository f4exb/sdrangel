///////////////////////////////////////////////////////////////////////////////////
// SDRangel Fobos SDR native source backend
// Auto backend selection: Agile API first, regular/classic API fallback.
// Runtime DLLs are loaded dynamically so SDRangel can still build without the Fobos SDK.
///////////////////////////////////////////////////////////////////////////////////

#include "fobosworker.h"

#include <QDebug>
#include <QDateTime>
#include <QStringList>
#include <QLibrary>
#include <QByteArray>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{
#if defined(FOBOS_DEBUG_FILE_LOG)
    static const char* kLogPath = "sdrangel_fobos_source.log";
#endif
#ifdef _WIN32
    static const char* kAgileDll = "fobos_sdr.dll";
    static const char* kRegularDll = "fobos.dll";
#else
    static const char* kAgileDll = "libfobos_sdr.so";
    static const char* kRegularDll = "libfobos.so";
#endif
    static const uint32_t kSyncComplexBufferLength = 65536; // Sync read block size, about 8.2 ms at 8 MS/s
    static const uint32_t kSyncFloatStorage = kSyncComplexBufferLength * 2u; // interleaved float I/Q
    static const uint32_t kFifoChunkComplexLength = kSyncComplexBufferLength; // One FIFO write per read block
    static const unsigned int kStartupProbeMaxAttempts = 8; // Verify read_sync before exposing a running stream
    static const unsigned int kStartupReaderErrorAbort = 32; // avoid endless dev==NULL read loop after a failed start
    static const double kDefaultFrequencyHz = 104000000.0;
    static const int kDefaultSampleRateHz = 25000000;
    static const unsigned int kDefaultLna = 2;
    static const unsigned int kDefaultVga = 8;
    static const double kDefaultIqGain = 32.0;
    static const int kDefaultInputMode = 0; // RF
    static const unsigned int kDefaultBandwidthPercent = 90;
    static const unsigned int kDefaultGpoMask = 0;
    static const bool kDefaultExternalClock = false;

#ifdef _WIN32
    using FobosLibraryHandle = HMODULE;
#else
    using DWORD = unsigned long;
    using HMODULE = void*;
    using FobosLibraryHandle = HMODULE;
    static QString g_lastLibraryError;

    static void SetLastError(DWORD) {}
    static DWORD GetLastError() { return 0; }

    static QString linuxRuntimeCandidate(const char* envName, const char* libName)
    {
        const char* root = std::getenv(envName);
        if (!root || !*root) {
            return QString();
        }
        return QString::fromLocal8Bit(root) + QStringLiteral("/lib/") + QString::fromLatin1(libName);
    }

    static HMODULE LoadLibraryA(const char* name)
    {
        QStringList candidates;
        const QString libName = QString::fromLatin1(name ? name : "");

        if (libName.contains(QStringLiteral("fobos_sdr"))) {
            const QString envPath = linuxRuntimeCandidate("FOBOS_SDR_DIR", "libfobos_sdr.so");
            if (!envPath.isEmpty()) { candidates << envPath; }
        }

        if (libName.contains(QStringLiteral("fobos")) && !libName.contains(QStringLiteral("fobos_sdr"))) {
            const QString envPath = linuxRuntimeCandidate("FOBOS_DIR", "libfobos.so");
            if (!envPath.isEmpty()) { candidates << envPath; }
        }

        candidates << libName;

        for (const QString& candidate : candidates) {
            QLibrary* lib = new QLibrary(candidate);
            if (lib->load()) {
                g_lastLibraryError.clear();
                return static_cast<void*>(lib);
            }
            g_lastLibraryError = QStringLiteral("%1: %2").arg(candidate, lib->errorString());
            delete lib;
        }

        return nullptr;
    }

    static QFunctionPointer GetProcAddress(HMODULE handle, const char* symbol)
    {
        QLibrary* lib = static_cast<QLibrary*>(handle);
        return lib ? lib->resolve(symbol) : nullptr;
    }

    static void Sleep(unsigned int msec)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(msec));
    }
#endif

    static QMutex g_sessionMutex;
    static FobosLibraryHandle g_agileLibrary = nullptr;   // runtime library is process-scoped; device is not.
    static FobosLibraryHandle g_regularLibrary = nullptr; // regular/classic runtime library, process-scoped as well.
    static bool g_deviceBusy = false;                     // prevent two SDRangel device sets from using one Fobos.

    static void formatLastError(DWORD err, char* out, size_t outSize)
    {
        if (!out || outSize == 0) {
            return;
        }
        out[0] = '\0';
#ifdef _WIN32
        DWORD n = FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            err,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            out,
            static_cast<DWORD>(outSize),
            nullptr);
        if (n == 0) {
            std::snprintf(out, outSize, "FormatMessage failed");
        } else {
            while (n > 0 && (out[n-1] == '\r' || out[n-1] == '\n' || out[n-1] == ' ' || out[n-1] == '\t')) {
                out[n-1] = '\0';
                --n;
            }
        }
#else
        (void) err;
        const QByteArray bytes = g_lastLibraryError.toLocal8Bit();
        std::snprintf(out, outSize, "%s", bytes.constData());
#endif
    }
}

FOBOSWorker::FOBOSWorker(SampleSinkFifo* sampleFifo, QObject* parent) :
    QObject(parent),
    m_sampleFifo(sampleFifo),
    m_convertBuffer(kSyncComplexBufferLength),
    m_centerFrequencyHz(static_cast<uint64_t>(kDefaultFrequencyHz)),
    m_samplerate(kDefaultSampleRateHz),
    m_log2Decim(0),
    m_fcPos(0),
    m_frequencyShift(0),
    m_lnaGain(kDefaultLna),
    m_vgaGain(kDefaultVga),
    m_inputMode(kDefaultInputMode),
    m_bandwidthPercent(kDefaultBandwidthPercent),
    m_gpoMask(kDefaultGpoMask),
    m_externalClock(kDefaultExternalClock),
    m_iqGain(kDefaultIqGain),
    m_runtimeInputMode(kDefaultInputMode),
    m_gainDiagActive(false),
    m_gainDiagCollecting(false),
    m_gainDiagKind(),
    m_gainDiagValue(0),
    m_gainDiagSkipBuffers(0),
    m_gainDiagTargetBuffers(0),
    m_gainDiagCollectedBuffers(0),
    m_gainDiagSamples(0),
    m_gainDiagPower(0.0),
    m_gainDiagPeak(0.0),
    m_running(false),
    m_stopRequested(false),
    m_syncStarted(false),
    m_readerActive(false),
    m_readerFinished(false),
    m_dev(nullptr),
    m_runtimeBackend(FobosRuntimeBackend::None),
    m_libraryHandle(nullptr),
    m_errorName(nullptr),
    m_closeDev(nullptr),
    m_readSync(nullptr),
    m_stopSync(nullptr),
    m_setFrequency(nullptr),
    m_setSamplerate(nullptr),
    m_getSamplerates(nullptr),
    m_setDirectSampling(nullptr),
    m_setAutoBandwidth(nullptr),
    m_setUserGpo(nullptr),
    m_setClkSource(nullptr),
    m_setLnaGain(nullptr),
    m_setVgaGain(nullptr),
    m_regularSetFrequency(nullptr),
    m_regularSetSamplerate(nullptr),
    m_regularGetSamplerates(nullptr),
    m_regularSetDirectSampling(nullptr),
    m_regularSetUserGpo(nullptr),
    m_regularSetClkSource(nullptr),
    m_regularSetLnaGain(nullptr),
    m_regularSetVgaGain(nullptr),
    m_totalReads(0),
    m_totalSamples(0),
    m_totalWritten(0),
    m_failedReads(0)
{
}

FOBOSWorker::~FOBOSWorker()
{
    stopWork();
}

void FOBOSWorker::startWork()
{
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true)) {
        return;
    }

    qInfo() << "FOBOSWorker::startWork: Fobos SDR sync source starting";
    emit backendStatusChanged(QStringLiteral("Probing"), QStringLiteral("Backend mode: Auto. Trying Agile API first, then regular API."));
    if (runAgileStart()) {
        qInfo() << "FOBOSWorker::startWork: Agile sync source started";
        return;
    }

    if (!m_stopRequested.load()) {
        m_running.store(true);
        qInfo() << "FOBOSWorker::startWork: Agile not available, trying regular Fobos API";
        if (runRegularStart()) {
            qInfo() << "FOBOSWorker::startWork: Regular sync source started";
            return;
        }
    }

    qInfo() << "FOBOSWorker::startWork: no usable Fobos backend started";
    emit backendStatusChanged(QStringLiteral("No device"), QStringLiteral("No Fobos SDR device was found through Agile or regular API."));
}

void FOBOSWorker::stopWork()
{
    emit backendStatusChanged(QStringLiteral("Stopped"), QStringLiteral("Fobos source is stopped. Backend will be detected on next Start."));
    m_stopRequested.store(true);
    m_running.store(false);

    bool readerJoined = true;

    // Do not call stop_sync while read_sync is active.
    // The reader loop checks m_stopRequested after every successful read_sync and exits by itself.
    // stop_sync is called only after the reader thread has returned. This avoids driver/API
    // races between a blocking read_sync call and stop_sync.
    if (m_readerThread.joinable()) {
        logLine("reader_join=START strategy=stop_flag_then_join_before_stop_sync");
        m_readerThread.join();
        readerJoined = true;
        logLine("reader_join=RETURN joined=YES before_stop_sync");
    }

    fobos_dev_t* dev = m_dev.load();
    if (dev && m_stopSync && m_syncStarted.exchange(false)) {
        logLine("stopWork_stop_sync=START_AFTER_READER_JOIN");
        int r = m_stopSync(dev);
        logLine("stopWork_stop_sync=RETURN_AFTER_READER_JOIN result=%d error='%s'", r, errorName(r));
    } else {
        logLine("stopWork_stop_sync=SKIPPED no_dev_or_not_started");
    }

    cleanupAfterReaderJoined(readerJoined);
}

void FOBOSWorker::setSamplerate(int samplerate)
{
    QMutexLocker locker(&m_settingsMutex);
    m_samplerate = samplerate > 0 ? samplerate : kDefaultSampleRateHz;
    if (m_running.load()) {
        logLine("live_sample_rate_change=CONTROLLED_RESTART_REQUIRED_BUT_NOT_HANDLED_HERE requested_hz=%d", m_samplerate);
    }
}

void FOBOSWorker::setCenterFrequency(uint64_t centerFrequencyHz)
{
    QMutexLocker locker(&m_settingsMutex);
    m_centerFrequencyHz = centerFrequencyHz > 0 ? centerFrequencyHz : static_cast<uint64_t>(kDefaultFrequencyHz);
    fobos_dev_t* dev = m_dev.load();
    if (dev && m_runtimeInputMode.load() == 0) {
        int r = callSetFrequency(dev, static_cast<double>(m_centerFrequencyHz));
        logLine("live_frequency_call=RETURN result=%d error='%s' requested_hz=%llu", r, errorName(r), static_cast<unsigned long long>(m_centerFrequencyHz));
    } else if (m_running.load()) {
        logLine("live_frequency_call=SKIPPED mode=%d no_dev_or_direct_sampling", m_runtimeInputMode.load());
    }
}

void FOBOSWorker::setLog2Decimation(unsigned int log2_decim)
{
    QMutexLocker locker(&m_settingsMutex);
    m_log2Decim = log2_decim;
}

void FOBOSWorker::setFcPos(int fcPos)
{
    QMutexLocker locker(&m_settingsMutex);
    m_fcPos = fcPos;
}

void FOBOSWorker::setBitSize(uint32_t) {}
void FOBOSWorker::setAmplitudeBits(int32_t amplitudeBits)
{
    double gain = static_cast<double>(amplitudeBits) / 100.0;
    if (gain < 0.01) {
        gain = 0.01;
    } else if (gain > 512.0) {
        gain = 512.0;
    }
    m_iqGain.store(gain);
    if (m_running.load()) { logLine("live_iq_gain_set=%.3f", gain); }
}
void FOBOSWorker::setLnaGain(unsigned int lnaGain)
{
    QMutexLocker locker(&m_settingsMutex);
    m_lnaGain = lnaGain > 2u ? 2u : lnaGain;
    fobos_dev_t* dev = m_dev.load();
    if (dev) {
        int r = callSetLnaGain(dev, m_lnaGain);
        logLine("live_lna_gain_call=RETURN result=%d error='%s' value=%u", r, errorName(r), m_lnaGain);
    }
}
void FOBOSWorker::setVgaGain(unsigned int vgaGain)
{
    QMutexLocker locker(&m_settingsMutex);
    m_vgaGain = vgaGain > 31u ? 31u : vgaGain;
    fobos_dev_t* dev = m_dev.load();
    if (dev) {
        int r = callSetVgaGain(dev, m_vgaGain);
        logLine("live_vga_gain_call=RETURN result=%d error='%s' value=%u", r, errorName(r), m_vgaGain);
    }
}
void FOBOSWorker::setInputMode(int inputMode)
{
    QMutexLocker locker(&m_settingsMutex);
    if (inputMode < 0 || inputMode > 3) {
        inputMode = kDefaultInputMode;
    }
    m_inputMode = inputMode;
    if (m_running.load()) {
        logLine("live_input_mode_change=CONTROLLED_RESTART_REQUIRED_BUT_NOT_HANDLED_HERE input_mode=%d", inputMode);
    }
}

void FOBOSWorker::setBandwidthPercent(unsigned int bandwidthPercent)
{
    QMutexLocker locker(&m_settingsMutex);
    if (!(bandwidthPercent >= 20 && bandwidthPercent <= 100 && (bandwidthPercent % 10) == 0)) {
        bandwidthPercent = kDefaultBandwidthPercent;
    }
    m_bandwidthPercent = bandwidthPercent;
    fobos_dev_t* dev = m_dev.load();
    if (dev) {
        int r = callSetAutoBandwidth(dev, m_bandwidthPercent);
        logLine("live_auto_bandwidth_call=RETURN result=%d error='%s' percent=%u", r, errorName(r), m_bandwidthPercent);
    }
}

void FOBOSWorker::setGpoMask(unsigned int gpoMask)
{
    QMutexLocker locker(&m_settingsMutex);
    m_gpoMask = gpoMask & 0xffu;
    fobos_dev_t* dev = m_dev.load();
    if (dev) {
        int r = callSetGpo(dev, m_gpoMask);
        logLine("live_gpo_call=RETURN result=%d error='%s' mask=0x%02X", r, errorName(r), m_gpoMask);
    }
}

void FOBOSWorker::setExternalClock(bool externalClock)
{
    QMutexLocker locker(&m_settingsMutex);
    m_externalClock = externalClock;
    if (m_running.load()) {
        logLine("live_external_clock_change=CONTROLLED_RESTART_REQUIRED_BUT_NOT_HANDLED_HERE external_clock=%s", externalClock ? "true" : "false");
    }
}

void FOBOSWorker::setDCFactor(float) {}
void FOBOSWorker::setIFactor(float) {}
void FOBOSWorker::setQFactor(float) {}
void FOBOSWorker::setPhaseImbalance(float) {}
void FOBOSWorker::setFrequencyShift(int shift) { QMutexLocker locker(&m_settingsMutex); m_frequencyShift = shift; }
void FOBOSWorker::setToneFrequency(int) {}
void FOBOSWorker::setModulation(int modulation)
{
    // Legacy modulation combo is now the uSDR-style VGA gain control.
    // Direct range is 0..31.
    if (modulation < 0) {
        modulation = 0;
    } else if (modulation > 31) {
        modulation = 31;
    }
    setVgaGain(static_cast<unsigned int>(modulation));
}
void FOBOSWorker::setAMModulation(float) {}
void FOBOSWorker::setFMDeviation(float) {}
void FOBOSWorker::setPattern0() {}
void FOBOSWorker::setPattern1() {}
void FOBOSWorker::setPattern2() {}



bool FOBOSWorker::runAgileStart()
{
    logLine("============================================================");
    logLine("SDRangel Fobos SDR Agile native source");
    logTimestamp();
    logLine("mode=agile_sync_to_sdrangel_fifo");
    logLine("lifecycle_policy=stopflag_join_then_stopsync_close");
    logLine("stream_policy=stable_fast_float_to_fix_single_write_no_decimation");
    logLine("agile_dll=%s", kAgileDll);

    uint64_t centerFrequency = 0;
    int sampleRate = 0;
    unsigned int lnaGain = kDefaultLna;
    unsigned int vgaGain = kDefaultVga;
    int inputMode = kDefaultInputMode;
    unsigned int bandwidthPercent = kDefaultBandwidthPercent;
    unsigned int gpoMask = kDefaultGpoMask;
    bool externalClock = kDefaultExternalClock;
    {
        QMutexLocker locker(&m_settingsMutex);
        centerFrequency = m_centerFrequencyHz;
        sampleRate = m_samplerate > 0 ? m_samplerate : kDefaultSampleRateHz;
        lnaGain = m_lnaGain;
        vgaGain = m_vgaGain;
        inputMode = m_inputMode;
        bandwidthPercent = m_bandwidthPercent;
        gpoMask = m_gpoMask;
        externalClock = m_externalClock;
    }

    logLine("requested_center_frequency_hz=%llu", static_cast<unsigned long long>(centerFrequency));
    logLine("requested_sample_rate_hz=%d", sampleRate);
    logLine("requested_lna=%u", lnaGain);
    logLine("requested_vga=%u", vgaGain);
    logLine("requested_iq_gain=%.3f", m_iqGain.load());
    logLine("requested_input_mode=%d", inputMode);
    logLine("requested_bandwidth_percent=%u", bandwidthPercent);
    logLine("requested_gpo_mask=0x%02X", gpoMask);
    logLine("requested_external_clock=%s", externalClock ? "true" : "false");
    HMODULE lib = nullptr;

    {
        QMutexLocker sessionLock(&g_sessionMutex);

        if (g_deviceBusy) {
            logLine("device_busy_guard=ACTIVE another_FOBOS_instance_is_already_streaming");
            logLine("source_result=REFUSED_DEVICE_ALREADY_BUSY");
            m_running.store(false);
            return false;
        }

        if (!g_agileLibrary) {
            // Resolve fobos_sdr.dll using the normal Windows DLL search order.
            // No developer-machine paths are used here; distribution packages should place
            // fobos_sdr.dll and its runtime dependencies next to the SDRangel executable,
            // or the user may expose them through PATH.
            SetLastError(0);
            g_agileLibrary = LoadLibraryA(kAgileDll);
            DWORD gle = GetLastError();
            logLine("LoadLibrary=%s handle=0x%p gle=%lu", g_agileLibrary ? "OK" : "FAILED", g_agileLibrary, gle);
            if (!g_agileLibrary) {
                char errText[512] = {};
                formatLastError(gle, errText, sizeof(errText));
                logLine("LoadLibrary_error='%s'", errText);
                logLine("operator_action=install_fobos_sdr_dll_next_to_sdrangel_or_add_it_to_PATH_then_restart_SDRangel");
                m_running.store(false);
                return false;
            }
        } else {
            logLine("LoadLibrary=REUSE_PROCESS_SCOPED handle=0x%p", g_agileLibrary);
        }

        lib = g_agileLibrary;
        g_deviceBusy = true;
    }

    m_libraryHandle = lib;
    m_runtimeBackend = FobosRuntimeBackend::Agile;

    auto getApiInfo = reinterpret_cast<fobos_sdr_get_api_info_t>(GetProcAddress(lib, "fobos_sdr_get_api_info"));
    auto getDeviceCount = reinterpret_cast<fobos_sdr_get_device_count_t>(GetProcAddress(lib, "fobos_sdr_get_device_count"));
    auto listDevices = reinterpret_cast<fobos_sdr_list_devices_t>(GetProcAddress(lib, "fobos_sdr_list_devices"));
    auto openDev = reinterpret_cast<fobos_sdr_open_t>(GetProcAddress(lib, "fobos_sdr_open"));
    m_closeDev = reinterpret_cast<fobos_sdr_close_t>(GetProcAddress(lib, "fobos_sdr_close"));
    auto getBoardInfo = reinterpret_cast<fobos_sdr_get_board_info_t>(GetProcAddress(lib, "fobos_sdr_get_board_info"));
    m_errorName = reinterpret_cast<fobos_sdr_error_name_t>(GetProcAddress(lib, "fobos_sdr_error_name"));
    m_setFrequency = reinterpret_cast<fobos_sdr_set_frequency_t>(GetProcAddress(lib, "fobos_sdr_set_frequency"));
    auto setFrequency = m_setFrequency;
    m_setSamplerate = reinterpret_cast<fobos_sdr_set_samplerate_t>(GetProcAddress(lib, "fobos_sdr_set_samplerate"));
    m_getSamplerates = reinterpret_cast<fobos_sdr_get_samplerates_t>(GetProcAddress(lib, "fobos_sdr_get_samplerates"));
    m_setDirectSampling = reinterpret_cast<fobos_sdr_set_direct_sampling_t>(GetProcAddress(lib, "fobos_sdr_set_direct_sampling"));
    m_setAutoBandwidth = reinterpret_cast<fobos_sdr_set_auto_bandwidth_t>(GetProcAddress(lib, "fobos_sdr_set_auto_bandwidth"));
    m_setUserGpo = reinterpret_cast<fobos_sdr_set_user_gpo_t>(GetProcAddress(lib, "fobos_sdr_set_user_gpo"));
    m_setClkSource = reinterpret_cast<fobos_sdr_set_clk_source_t>(GetProcAddress(lib, "fobos_sdr_set_clk_source"));
    auto setSamplerate = m_setSamplerate;
    m_setLnaGain = reinterpret_cast<fobos_sdr_set_lna_gain_t>(GetProcAddress(lib, "fobos_sdr_set_lna_gain"));
    m_setVgaGain = reinterpret_cast<fobos_sdr_set_vga_gain_t>(GetProcAddress(lib, "fobos_sdr_set_vga_gain"));
    auto setLnaGain = m_setLnaGain;
    auto setVgaGain = m_setVgaGain;
    auto startSync = reinterpret_cast<fobos_sdr_start_sync_t>(GetProcAddress(lib, "fobos_sdr_start_sync"));
    m_readSync = reinterpret_cast<fobos_sdr_read_sync_t>(GetProcAddress(lib, "fobos_sdr_read_sync"));
    m_stopSync = reinterpret_cast<fobos_sdr_stop_sync_t>(GetProcAddress(lib, "fobos_sdr_stop_sync"));

    bool ok = getApiInfo && getDeviceCount && listDevices && openDev && m_closeDev && getBoardInfo && m_errorName && setFrequency && setSamplerate && setLnaGain && setVgaGain && startSync && m_readSync && m_stopSync;
    logLine("exports_ok=%s", ok ? "YES" : "NO");
    if (!ok) {
        logLine("source_result=FAILED_MISSING_AGILE_EXPORTS");
        QMutexLocker sessionLock(&g_sessionMutex);
        g_deviceBusy = false;
        m_running.store(false);
        return false;
    }

    char libVersion[128] = {};
    char drvVersion[128] = {};
    int rInfo = getApiInfo(libVersion, drvVersion);
    logLine("api_info_call=result=%d lib='%s' drv='%s'", rInfo, libVersion, drvVersion);

    int count = getDeviceCount();
    logLine("device_count_call=OK count=%d", count);
    if (count <= 0) {
        logLine("device_detect_hint=NO_FOBOS_DEVICE_VISIBLE_TO_AGILE_API");
        logLine("device_busy_hint=check_USB_connection_and_close_microSDR_uSDR_other_SDRangel_instances");
        logLine("source_result=FAILED_NO_DEVICE_VISIBLE");
        QMutexLocker sessionLock(&g_sessionMutex);
        g_deviceBusy = false;
        m_running.store(false);
        return false;
    }

    char serials[4096] = {};
    int rList = listDevices(serials);
    logLine("list_devices_call=RETURN result=%d serials='%s'", rList, serials);

    fobos_dev_t* dev = nullptr;
    int r = openDev(&dev, 0);
    logLine("open_call=RETURN result=%d error='%s' dev=0x%p", r, errorName(r), dev);
    if ((r != 0) || !dev) {
        logLine("device_open_hint=FAILED_TO_OPEN_FOBOS_INDEX_0");
        logLine("device_busy_hint=Fobos_may_be_owned_by_microSDR_uSDR_or_another_SDRangel_instance");
        logLine("operator_action=close_other_Fobos_software_then_Stop_Start_or_restart_SDRangel");
        logLine("source_result=FAILED_OPEN_DEVICE_BUSY_OR_UNAVAILABLE");
        QMutexLocker sessionLock(&g_sessionMutex);
        g_deviceBusy = false;
        m_running.store(false);
        return false;
    }
    m_dev.store(dev);

    char hw[128] = {}, fw[128] = {}, manufacturer[128] = {}, product[128] = {}, serial[128] = {};
    int rBoard = getBoardInfo(dev, hw, fw, manufacturer, product, serial);
    logLine("board_info_call=RETURN result=%d error='%s'", rBoard, errorName(rBoard));
    logLine("board_hw_revision='%s'", hw);
    logLine("board_fw_version='%s'", fw);
    logLine("board_manufacturer='%s'", manufacturer);
    logLine("board_product='%s'", product);
    logLine("board_serial='%s'", serial);
    const QString agileBackendDetails = QStringLiteral("Fobos Agile API %1; driver %2; HW %3; FW %4; serial %5")
        .arg(QString::fromLocal8Bit(libVersion))
        .arg(QString::fromLocal8Bit(drvVersion))
        .arg(QString::fromLocal8Bit(hw))
        .arg(QString::fromLocal8Bit(fw))
        .arg(QString::fromLocal8Bit(serial));

    if (m_getSamplerates) {
        double rates[32] = {};
        uint32_t rateCount = 32;
        int rRates = m_getSamplerates(dev, rates, &rateCount);
        logLine("get_samplerates_call=RETURN result=%d count=%u", rRates, rateCount);
        for (uint32_t i = 0; (rRates == 0) && (i < rateCount) && (i < 32); ++i) {
            logLine("supported_samplerate[%u]=%.0f", i, rates[i]);
        }
    } else {
        logLine("get_samplerates_call=SKIPPED missing_export fallback_list=8000000,10000000,12500000,16000000,20000000,25000000,32000000,40000000,50000000");
    }

    const int direct = (inputMode == 0) ? 0 : 1;
    if (m_setDirectSampling) {
        r = m_setDirectSampling(dev, direct);
        logLine("set_direct_sampling_call=RETURN result=%d error='%s' input_mode=%d direct=%d", r, errorName(r), inputMode, direct);
    } else {
        logLine("set_direct_sampling_call=SKIPPED missing_export input_mode=%d direct=%d", inputMode, direct);
    }
    m_runtimeInputMode.store(inputMode);

    if (m_setClkSource) {
        r = m_setClkSource(dev, externalClock ? 1 : 0);
        logLine("set_clk_source_call=RETURN result=%d error='%s' external_clock=%s", r, errorName(r), externalClock ? "true" : "false");
    } else {
        logLine("set_clk_source_call=SKIPPED missing_export external_clock=%s", externalClock ? "true" : "false");
    }

    if (m_setUserGpo) {
        r = m_setUserGpo(dev, gpoMask);
        logLine("set_user_gpo_call=RETURN result=%d error='%s' mask=0x%02X", r, errorName(r), gpoMask);
    } else {
        logLine("set_user_gpo_call=SKIPPED missing_export mask=0x%02X", gpoMask);
    }

    if (m_setAutoBandwidth) {
        const double bwRatio = static_cast<double>(bandwidthPercent) * 0.01;
        r = m_setAutoBandwidth(dev, bwRatio);
        logLine("set_auto_bandwidth_call=RETURN result=%d error='%s' percent=%u ratio=%.3f", r, errorName(r), bandwidthPercent, bwRatio);
    } else {
        logLine("set_auto_bandwidth_call=SKIPPED missing_export percent=%u", bandwidthPercent);
    }

    r = callSetFrequency(dev, static_cast<double>(centerFrequency));
    logLine("set_frequency_call=RETURN result=%d error='%s'", r, errorName(r));
    r = callSetSamplerate(dev, static_cast<double>(sampleRate));
    logLine("set_samplerate_call=RETURN result=%d error='%s'", r, errorName(r));
    r = callSetLnaGain(dev, lnaGain);
    logLine("set_lna_gain_call=RETURN result=%d error='%s'", r, errorName(r));
    r = callSetVgaGain(dev, vgaGain);
    logLine("set_vga_gain_call=RETURN result=%d error='%s'", r, errorName(r));

    r = startSync(dev, kSyncComplexBufferLength);
    logLine("start_sync_call=RETURN result=%d error='%s' complex_buf_length=%u float_storage=%u fifo_write_complex=%u", r, errorName(r), kSyncComplexBufferLength, kSyncFloatStorage, kFifoChunkComplexLength);
    if (r != 0) {
        m_dev.store(nullptr);
        m_running.store(false);
        if (m_closeDev) {
            int rc = m_closeDev(dev);
            logLine("close_after_start_sync_failure=RETURN result=%d error='%s'", rc, errorName(rc));
        }
        QMutexLocker sessionLock(&g_sessionMutex);
        g_deviceBusy = false;
        return false;
    }

    // Start/read robustness:
    // Some driver/device states can report a successful start while the first read_sync calls
    // still fail, for example with a null device handle reported by the runtime. Do not expose
    // such a half-started source as Running.
    // The first read_sync loop may return repeated errors and
    // no samples are delivered. Probe read_sync synchronously, close/release cleanly if the device handle is unusable,
    // and let the operator Start again without flooding the log or leaving a stuck channel.
    {
        std::vector<float> startupProbe(kSyncFloatStorage);
        bool startupProbeOk = false;
        uint32_t startupProbeActual = 0;
        int startupProbeResult = -9999;

        for (unsigned int attempt = 1; attempt <= kStartupProbeMaxAttempts && !m_stopRequested.load(); ++attempt) {
            startupProbeActual = 0;
            startupProbeResult = m_readSync(dev, startupProbe.data(), &startupProbeActual);
            if ((startupProbeResult == 0) && (startupProbeActual > 0)) {
                startupProbeOk = true;
                logLine("startup_read_probe=OK attempt=%u actual=%u", attempt, startupProbeActual);
                break;
            }
            logLine("startup_read_probe=RETRY attempt=%u result=%d error='%s' actual=%u",
                    attempt, startupProbeResult, errorName(startupProbeResult), startupProbeActual);
            Sleep(20);
        }

        if (!startupProbeOk) {
            logLine("startup_read_probe=FAILED max_attempts=%u last_result=%d last_error='%s' action=stop_sync_close_release_busy",
                    kStartupProbeMaxAttempts, startupProbeResult, errorName(startupProbeResult));
            if (m_stopSync) {
                int rs = m_stopSync(dev);
                logLine("stop_sync_after_startup_probe_failure=RETURN result=%d error='%s'", rs, errorName(rs));
            }
            if (m_closeDev) {
                int rc = m_closeDev(dev);
                logLine("close_after_startup_probe_failure=RETURN result=%d error='%s'", rc, errorName(rc));
            }
            m_dev.store(nullptr);
            m_syncStarted.store(false);
            m_running.store(false);
            m_stopRequested.store(false);
            {
                QMutexLocker sessionLock(&g_sessionMutex);
                g_deviceBusy = false;
            }
            logLine("source_result=FAILED_STARTUP_READ_PROBE_NO_SAMPLES_DEVICE_RELEASED");
            return false;
        }
    }

    m_syncStarted.store(true);
    m_readerFinished.store(false);
    m_readerActive.store(true);
    m_stopRequested.store(false);
    m_totalReads = 0;
    m_totalSamples = 0;
    m_totalWritten = 0;
    m_failedReads = 0;

    logLine("reader_thread=STARTING");
    m_readerThread = std::thread(&FOBOSWorker::readerLoop, this);
    logLine("reader_thread=STARTED");
    emit backendStatusChanged(QStringLiteral("Agile"), agileBackendDetails);
    return true;
}


bool FOBOSWorker::runRegularStart()
{
    logLine("============================================================");
    logLine("SDRangel Fobos SDR regular native source");
    logTimestamp();
    logLine("mode=regular_sync_to_sdrangel_fifo");
    logLine("regular_dll=%s", kRegularDll);

    uint64_t centerFrequency = 0;
    int sampleRate = 0;
    unsigned int lnaGain = kDefaultLna;
    unsigned int vgaGain = kDefaultVga;
    int inputMode = kDefaultInputMode;
    unsigned int bandwidthPercent = kDefaultBandwidthPercent;
    unsigned int gpoMask = kDefaultGpoMask;
    bool externalClock = kDefaultExternalClock;
    {
        QMutexLocker locker(&m_settingsMutex);
        centerFrequency = m_centerFrequencyHz;
        sampleRate = m_samplerate > 0 ? m_samplerate : kDefaultSampleRateHz;
        lnaGain = m_lnaGain;
        vgaGain = m_vgaGain;
        inputMode = m_inputMode;
        bandwidthPercent = m_bandwidthPercent;
        gpoMask = m_gpoMask;
        externalClock = m_externalClock;
    }

    HMODULE lib = nullptr;
    {
        QMutexLocker sessionLock(&g_sessionMutex);
        if (g_deviceBusy) {
            logLine("regular_device_busy_guard=ACTIVE another_FOBOS_instance_is_already_streaming");
            m_running.store(false);
            return false;
        }
        if (!g_regularLibrary) {
            SetLastError(0);
            g_regularLibrary = LoadLibraryA(kRegularDll);
            DWORD gle = GetLastError();
            logLine("regular_LoadLibrary=%s handle=0x%p gle=%lu", g_regularLibrary ? "OK" : "FAILED", g_regularLibrary, gle);
            if (!g_regularLibrary) {
                char errText[512] = {};
                formatLastError(gle, errText, sizeof(errText));
                logLine("regular_LoadLibrary_error='%s'", errText);
                m_running.store(false);
                return false;
            }
        } else {
            logLine("regular_LoadLibrary=REUSE_PROCESS_SCOPED handle=0x%p", g_regularLibrary);
        }
        lib = g_regularLibrary;
        g_deviceBusy = true;
    }

    m_libraryHandle = lib;
    m_runtimeBackend = FobosRuntimeBackend::Regular;

    auto getApiInfo = reinterpret_cast<fobos_rx_get_api_info_t>(GetProcAddress(lib, "fobos_rx_get_api_info"));
    auto getDeviceCount = reinterpret_cast<fobos_rx_get_device_count_t>(GetProcAddress(lib, "fobos_rx_get_device_count"));
    auto listDevices = reinterpret_cast<fobos_rx_list_devices_t>(GetProcAddress(lib, "fobos_rx_list_devices"));
    auto openDev = reinterpret_cast<fobos_rx_open_t>(GetProcAddress(lib, "fobos_rx_open"));
    m_closeDev = reinterpret_cast<fobos_sdr_close_t>(GetProcAddress(lib, "fobos_rx_close"));
    auto getBoardInfo = reinterpret_cast<fobos_sdr_get_board_info_t>(GetProcAddress(lib, "fobos_rx_get_board_info"));
    m_errorName = reinterpret_cast<fobos_sdr_error_name_t>(GetProcAddress(lib, "fobos_rx_error_name"));
    m_regularSetFrequency = reinterpret_cast<fobos_rx_set_frequency_t>(GetProcAddress(lib, "fobos_rx_set_frequency"));
    m_regularSetSamplerate = reinterpret_cast<fobos_rx_set_samplerate_t>(GetProcAddress(lib, "fobos_rx_set_samplerate"));
    m_regularGetSamplerates = reinterpret_cast<fobos_rx_get_samplerates_t>(GetProcAddress(lib, "fobos_rx_get_samplerates"));
    m_regularSetDirectSampling = reinterpret_cast<fobos_rx_set_direct_sampling_t>(GetProcAddress(lib, "fobos_rx_set_direct_sampling"));
    m_regularSetUserGpo = reinterpret_cast<fobos_rx_set_user_gpo_t>(GetProcAddress(lib, "fobos_rx_set_user_gpo"));
    m_regularSetClkSource = reinterpret_cast<fobos_rx_set_clk_source_t>(GetProcAddress(lib, "fobos_rx_set_clk_source"));
    m_regularSetLnaGain = reinterpret_cast<fobos_rx_set_lna_gain_t>(GetProcAddress(lib, "fobos_rx_set_lna_gain"));
    m_regularSetVgaGain = reinterpret_cast<fobos_rx_set_vga_gain_t>(GetProcAddress(lib, "fobos_rx_set_vga_gain"));
    auto startSync = reinterpret_cast<fobos_rx_start_sync_t>(GetProcAddress(lib, "fobos_rx_start_sync"));
    m_readSync = reinterpret_cast<fobos_sdr_read_sync_t>(GetProcAddress(lib, "fobos_rx_read_sync"));
    m_stopSync = reinterpret_cast<fobos_sdr_stop_sync_t>(GetProcAddress(lib, "fobos_rx_stop_sync"));

    const bool ok = getApiInfo && getDeviceCount && listDevices && openDev && m_closeDev && getBoardInfo && m_errorName &&
                    m_regularSetFrequency && m_regularSetSamplerate && m_regularSetLnaGain && m_regularSetVgaGain &&
                    startSync && m_readSync && m_stopSync;
    logLine("regular_exports_ok=%s", ok ? "YES" : "NO");
    if (!ok) {
        QMutexLocker sessionLock(&g_sessionMutex);
        g_deviceBusy = false;
        m_running.store(false);
        return false;
    }

    char libVersion[128] = {};
    char drvVersion[128] = {};
    int rInfo = getApiInfo(libVersion, drvVersion);
    logLine("regular_api_info_call=result=%d lib='%s' drv='%s'", rInfo, libVersion, drvVersion);

    int count = getDeviceCount();
    logLine("regular_device_count_call=count=%d", count);
    if (count <= 0) {
        QMutexLocker sessionLock(&g_sessionMutex);
        g_deviceBusy = false;
        m_running.store(false);
        return false;
    }

    char serials[4096] = {};
    int rList = listDevices(serials);
    logLine("regular_list_devices_call=RETURN result=%d serials='%s'", rList, serials);

    fobos_dev_t* dev = nullptr;
    int r = openDev(&dev, 0);
    logLine("regular_open_call=RETURN result=%d error='%s' dev=0x%p", r, errorName(r), dev);
    if ((r != 0) || !dev) {
        QMutexLocker sessionLock(&g_sessionMutex);
        g_deviceBusy = false;
        m_running.store(false);
        return false;
    }
    m_dev.store(dev);

    char hw[128] = {}, fw[128] = {}, manufacturer[128] = {}, product[128] = {}, serial[128] = {};
    int rBoard = getBoardInfo(dev, hw, fw, manufacturer, product, serial);
    logLine("regular_board_info_call=RETURN result=%d error='%s'", rBoard, errorName(rBoard));
    logLine("regular_board_hw_revision='%s'", hw);
    logLine("regular_board_fw_version='%s'", fw);
    logLine("regular_board_manufacturer='%s'", manufacturer);
    logLine("regular_board_product='%s'", product);
    logLine("regular_board_serial='%s'", serial);
    const QString regularBackendDetails = QStringLiteral("Fobos regular API %1; driver %2; HW %3; FW %4; serial %5")
        .arg(QString::fromLocal8Bit(libVersion))
        .arg(QString::fromLocal8Bit(drvVersion))
        .arg(QString::fromLocal8Bit(hw))
        .arg(QString::fromLocal8Bit(fw))
        .arg(QString::fromLocal8Bit(serial));

    if (m_regularGetSamplerates) {
        double rates[32] = {};
        unsigned int rateCount = 32;
        int rRates = m_regularGetSamplerates(dev, rates, &rateCount);
        logLine("regular_get_samplerates_call=RETURN result=%d count=%u", rRates, rateCount);
        for (unsigned int i = 0; (rRates == 0) && (i < rateCount) && (i < 32); ++i) {
            logLine("regular_supported_samplerate[%u]=%.0f", i, rates[i]);
        }
    }

    r = callSetDirectSampling(dev, inputMode);
    logLine("regular_set_direct_sampling_call=RETURN result=%d error='%s' input_mode=%d", r, errorName(r), inputMode);
    m_runtimeInputMode.store(inputMode);
    r = callSetClockSource(dev, externalClock);
    logLine("regular_set_clk_source_call=RETURN result=%d error='%s' external_clock=%s", r, errorName(r), externalClock ? "true" : "false");
    r = callSetGpo(dev, gpoMask);
    logLine("regular_set_user_gpo_call=RETURN result=%d error='%s' mask=0x%02X", r, errorName(r), gpoMask);
    r = callSetAutoBandwidth(dev, bandwidthPercent);
    logLine("regular_set_auto_bandwidth_call=SKIPPED result=%d percent=%u", r, bandwidthPercent);
    r = callSetFrequency(dev, static_cast<double>(centerFrequency));
    logLine("regular_set_frequency_call=RETURN result=%d error='%s'", r, errorName(r));
    r = callSetSamplerate(dev, static_cast<double>(sampleRate));
    logLine("regular_set_samplerate_call=RETURN result=%d error='%s'", r, errorName(r));
    r = callSetLnaGain(dev, lnaGain);
    logLine("regular_set_lna_gain_call=RETURN result=%d error='%s'", r, errorName(r));
    r = callSetVgaGain(dev, vgaGain);
    logLine("regular_set_vga_gain_call=RETURN result=%d error='%s'", r, errorName(r));

    r = startSync(dev, kSyncComplexBufferLength);
    logLine("regular_start_sync_call=RETURN result=%d error='%s' complex_buf_length=%u", r, errorName(r), kSyncComplexBufferLength);
    if (r != 0) {
        m_dev.store(nullptr);
        m_running.store(false);
        if (m_closeDev) {
            int rc = m_closeDev(dev);
            logLine("regular_close_after_start_sync_failure=RETURN result=%d error='%s'", rc, errorName(rc));
        }
        QMutexLocker sessionLock(&g_sessionMutex);
        g_deviceBusy = false;
        return false;
    }

    {
        std::vector<float> startupProbe(kSyncFloatStorage);
        bool startupProbeOk = false;
        uint32_t startupProbeActual = 0;
        int startupProbeResult = -9999;
        for (unsigned int attempt = 1; attempt <= kStartupProbeMaxAttempts && !m_stopRequested.load(); ++attempt) {
            startupProbeActual = 0;
            startupProbeResult = m_readSync(dev, startupProbe.data(), &startupProbeActual);
            if ((startupProbeResult == 0) && (startupProbeActual > 0)) {
                startupProbeOk = true;
                logLine("regular_startup_read_probe=OK attempt=%u actual=%u", attempt, startupProbeActual);
                break;
            }
            logLine("regular_startup_read_probe=RETRY attempt=%u result=%d error='%s' actual=%u",
                    attempt, startupProbeResult, errorName(startupProbeResult), startupProbeActual);
            Sleep(20);
        }
        if (!startupProbeOk) {
            if (m_stopSync) {
                int rs = m_stopSync(dev);
                logLine("regular_stop_sync_after_startup_probe_failure=RETURN result=%d error='%s'", rs, errorName(rs));
            }
            if (m_closeDev) {
                int rc = m_closeDev(dev);
                logLine("regular_close_after_startup_probe_failure=RETURN result=%d error='%s'", rc, errorName(rc));
            }
            m_dev.store(nullptr);
            m_syncStarted.store(false);
            m_running.store(false);
            m_stopRequested.store(false);
            QMutexLocker sessionLock(&g_sessionMutex);
            g_deviceBusy = false;
            return false;
        }
    }

    m_syncStarted.store(true);
    m_readerFinished.store(false);
    m_readerActive.store(true);
    m_stopRequested.store(false);
    m_totalReads = 0;
    m_totalSamples = 0;
    m_totalWritten = 0;
    m_failedReads = 0;

    logLine("regular_reader_thread=STARTING");
    m_readerThread = std::thread(&FOBOSWorker::readerLoop, this);
    logLine("regular_reader_thread=STARTED");
    emit backendStatusChanged(QStringLiteral("Regular"), regularBackendDetails);
    return true;
}

int FOBOSWorker::callSetFrequency(fobos_dev_t* dev, double valueHz)
{
    if (m_runtimeBackend == FobosRuntimeBackend::Regular && m_regularSetFrequency) {
        double actual = 0.0;
        int r = m_regularSetFrequency(dev, valueHz, &actual);
        logLine("regular_frequency_actual_hz=%.0f", actual);
        return r;
    }
    if (m_setFrequency) {
        return m_setFrequency(dev, valueHz);
    }
    return -9999;
}

int FOBOSWorker::callSetSamplerate(fobos_dev_t* dev, double valueHz)
{
    if (m_runtimeBackend == FobosRuntimeBackend::Regular && m_regularSetSamplerate)
    {
        double actual = 0.0;
        return m_regularSetSamplerate(dev, valueHz, &actual);
    }

    if (m_setSamplerate)
    {
        return m_setSamplerate(dev, valueHz);
    }
    return -9999;
}

int FOBOSWorker::callSetDirectSampling(fobos_dev_t* dev, int inputMode)
{
    const int direct = (inputMode == 0) ? 0 : 1;
    if (m_runtimeBackend == FobosRuntimeBackend::Regular && m_regularSetDirectSampling) {
        return m_regularSetDirectSampling(dev, static_cast<unsigned int>(direct));
    }
    if (m_setDirectSampling) {
        return m_setDirectSampling(dev, direct);
    }
    return 0;
}

int FOBOSWorker::callSetAutoBandwidth(fobos_dev_t* dev, unsigned int bandwidthPercent)
{
    if (m_runtimeBackend == FobosRuntimeBackend::Regular) {
        (void) dev;
        (void) bandwidthPercent;
        return 0; // regular libfobos has no auto-bandwidth setter
    }
    if (m_setAutoBandwidth) {
        const double ratio = static_cast<double>(bandwidthPercent) * 0.01;
        return m_setAutoBandwidth(dev, ratio);
    }
    return 0;
}

int FOBOSWorker::callSetGpo(fobos_dev_t* dev, unsigned int gpoMask)
{
    if (m_runtimeBackend == FobosRuntimeBackend::Regular && m_regularSetUserGpo) {
        return m_regularSetUserGpo(dev, static_cast<uint8_t>(gpoMask & 0xffu));
    }
    if (m_setUserGpo) {
        return m_setUserGpo(dev, gpoMask & 0xffu);
    }
    return 0;
}

int FOBOSWorker::callSetClockSource(fobos_dev_t* dev, bool externalClock)
{
    if (m_runtimeBackend == FobosRuntimeBackend::Regular && m_regularSetClkSource) {
        return m_regularSetClkSource(dev, externalClock ? 1 : 0);
    }
    if (m_setClkSource) {
        return m_setClkSource(dev, externalClock ? 1 : 0);
    }
    return 0;
}

int FOBOSWorker::callSetLnaGain(fobos_dev_t* dev, unsigned int lnaGain)
{
    if (m_runtimeBackend == FobosRuntimeBackend::Regular && m_regularSetLnaGain) {
        return m_regularSetLnaGain(dev, lnaGain);
    }
    if (m_setLnaGain) {
        return m_setLnaGain(dev, lnaGain);
    }
    return -9999;
}

int FOBOSWorker::callSetVgaGain(fobos_dev_t* dev, unsigned int vgaGain)
{
    if (m_runtimeBackend == FobosRuntimeBackend::Regular && m_regularSetVgaGain) {
        return m_regularSetVgaGain(dev, vgaGain);
    }
    if (m_setVgaGain) {
        return m_setVgaGain(dev, vgaGain);
    }
    return -9999;
}

void FOBOSWorker::readerLoop()
{
#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    logLine("reader_thread_priority=THREAD_PRIORITY_HIGHEST");
#else
    logLine("reader_thread_priority=DEFAULT_PORTABLE");
#endif
    fobos_dev_t* dev = m_dev.load();
    if (!dev || !m_readSync) {
        logLine("reader_loop=ABORT no_device_or_readSync");
        m_readerActive.store(false);
        m_readerFinished.store(true);
        return;
    }

    std::vector<float> iq(kSyncFloatStorage);
    if (m_convertBuffer.size() < kFifoChunkComplexLength) {
        m_convertBuffer.resize(kFifoChunkComplexLength);
    }

    bool firstLogged = false;
    unsigned int startupReadErrors = 0;
    uint64_t totalChunks = 0;
    uint64_t shortWrites = 0;
    uint64_t timingWindowReads = 0;
    uint64_t timingWindowChunks = 0;
    uint64_t timingWindowSamples = 0;
    uint64_t timingWindowWritten = 0;
    uint64_t timingWindowShortWrites = 0;
    double timingWindowReadMs = 0.0;
    double timingWindowConvertPushMs = 0.0;
    const auto streamStart = std::chrono::steady_clock::now();

    logLine("reader_loop=ENTER fast_convert_single_fifo_write=%u", kFifoChunkComplexLength);

    while (!m_stopRequested.load()) {
        uint32_t actual = 0;
        const auto readStart = std::chrono::steady_clock::now();
        int rr = m_readSync(dev, iq.data(), &actual);
        const auto readEnd = std::chrono::steady_clock::now();
        const double readMs = std::chrono::duration<double, std::milli>(readEnd - readStart).count();

        if (rr != 0) {
            m_failedReads++;
            logLine("read_sync_error result=%d error='%s' after_reads=%llu", rr, errorName(rr), static_cast<unsigned long long>(m_totalReads));
            if (m_stopRequested.load()) {
                break;
            }
            if (!firstLogged) {
                ++startupReadErrors;
                if (startupReadErrors >= kStartupReaderErrorAbort) {
                    logLine("startup_read_guard=ABORT no_successful_read_after_errors=%u action=reader_exit_waiting_for_stop", startupReadErrors);
                    m_stopRequested.store(true);
                    m_running.store(false);
                    break;
                }
                Sleep(20);
            } else {
                Sleep(2);
            }
            continue;
        }

        if (actual == 0) {
            continue;
        }
        startupReadErrors = 0;

        double mean = 0.0;
        double power = 0.0;
        double peak = 0.0;
        const int inputMode = m_runtimeInputMode.load();
        const double gainScale = m_iqGain.load() * SDR_RX_SCALEF;
        const double fixLimit = SDR_RX_SCALEF - 1.0;
        const auto convertStart = std::chrono::steady_clock::now();

        // Fast conversion path:
        // Load the software gain once per read block, convert inline, and write the whole
        // block to the FIFO once. This keeps the source realtime-safe at high sample rates.
        for (uint32_t i = 0; i < actual; ++i) {
            float re = iq[2u*i];
            float im = iq[2u*i + 1u];

            // Direct-sampling mode handling:
            // mode 0 RF: normal complex IQ; mode 1 IQ direct: normal complex IQ;
            // mode 2 HF1: use I channel as real-only with alternating sign;
            // mode 3 HF2: use Q channel as real-only with alternating sign.
            if (inputMode == 2) {
                if ((i & 1u) != 0u) { re = -re; }
                im = 0.0f;
            } else if (inputMode == 3) {
                re = im;
                if ((i & 1u) != 0u) { re = -re; }
                im = 0.0f;
            }

            if (!firstLogged) {
                const double reD = static_cast<double>(re);
                const double imD = static_cast<double>(im);
                mean += re + im;
                power += reD*reD + imD*imD;
                peak = std::max(peak, std::max(std::fabs(reD), std::fabs(imD)));
            }

            double sr = static_cast<double>(re) * gainScale;
            double si = static_cast<double>(im) * gainScale;
            if (sr > fixLimit) { sr = fixLimit; } else if (sr < -fixLimit) { sr = -fixLimit; }
            if (si > fixLimit) { si = fixLimit; } else if (si < -fixLimit) { si = -fixLimit; }

            // Fast round-to-nearest for normal finite Fobos floats. Fobos Agile API returns finite
            // normalized IQ samples; pathological NaN/Inf protection is intentionally not in this
            // hot path because it was part of the realtime bottleneck.
            m_convertBuffer[i].setReal(static_cast<FixReal>(sr >= 0.0 ? sr + 0.5 : sr - 0.5));
            m_convertBuffer[i].setImag(static_cast<FixReal>(si >= 0.0 ? si + 0.5 : si - 0.5));
        }

        const auto convertEnd = std::chrono::steady_clock::now();
        const unsigned int writtenThisRead = m_sampleFifo->write(m_convertBuffer.cbegin(), m_convertBuffer.cbegin() + actual);
        const auto pushEnd = std::chrono::steady_clock::now();
        const unsigned int chunksThisRead = 1;
        totalChunks++;
        if (writtenThisRead != actual) {
            shortWrites++;
            timingWindowShortWrites++;
            logLine("fifo_write_short requested=%u written=%u fifo_fill=%u", actual, writtenThisRead, m_sampleFifo->fill());
        }

        const double convertMs = std::chrono::duration<double, std::milli>(convertEnd - convertStart).count();
        const double pushMs = std::chrono::duration<double, std::milli>(pushEnd - convertEnd).count();
        const double convertPushMs = convertMs + pushMs;

        m_totalReads++;
        m_totalSamples += actual;
        m_totalWritten += writtenThisRead;

        timingWindowReads++;
        timingWindowChunks += chunksThisRead;
        timingWindowSamples += actual;
        timingWindowWritten += writtenThisRead;
        timingWindowReadMs += readMs;
        timingWindowConvertPushMs += convertPushMs;

        if (!firstLogged) {
            const double denom = static_cast<double>(actual) * 2.0;
            logLine("first_read_ok actual=%u written=%u chunks=%u mean=%.9f rms=%.9f peak=%.9f iq_gain=%.3f input_mode=%d read_ms=%.3f convert_ms=%.3f fifo_write_ms=%.3f convert_push_ms=%.3f",
                    actual,
                    writtenThisRead,
                    chunksThisRead,
                    mean / denom,
                    std::sqrt(power / denom),
                    peak,
                    m_iqGain.load(),
                    m_runtimeInputMode.load(),
                    readMs,
                    convertMs,
                    pushMs,
                    convertPushMs);
            firstLogged = true;
        }

        if ((m_totalReads % 50u) == 0u) {
            const auto now = std::chrono::steady_clock::now();
            const double elapsedS = std::chrono::duration<double>(now - streamStart).count();
            const double effectiveMsps = elapsedS > 0.0 ? (static_cast<double>(m_totalSamples) / elapsedS / 1.0e6) : 0.0;
            const double avgReadMs = timingWindowReads > 0 ? timingWindowReadMs / static_cast<double>(timingWindowReads) : 0.0;
            const double avgConvertPushMs = timingWindowReads > 0 ? timingWindowConvertPushMs / static_cast<double>(timingWindowReads) : 0.0;
            logLine("stream_timing reads=%llu samples=%llu written=%llu fifo_writes=%llu fifo_fill=%u avg_read_ms=%.3f avg_convert_write_ms=%.3f effective_msps=%.3f window_short_writes=%llu total_short_writes=%llu",
                    static_cast<unsigned long long>(m_totalReads),
                    static_cast<unsigned long long>(m_totalSamples),
                    static_cast<unsigned long long>(m_totalWritten),
                    static_cast<unsigned long long>(totalChunks),
                    m_sampleFifo->fill(),
                    avgReadMs,
                    avgConvertPushMs,
                    effectiveMsps,
                    static_cast<unsigned long long>(timingWindowShortWrites),
                    static_cast<unsigned long long>(shortWrites));
            timingWindowReads = 0;
            timingWindowChunks = 0;
            timingWindowSamples = 0;
            timingWindowWritten = 0;
            timingWindowShortWrites = 0;
            timingWindowReadMs = 0.0;
            timingWindowConvertPushMs = 0.0;
        }
    }

    logLine("stream_loop_exit reads=%llu samples=%llu written=%llu failed_reads=%llu fifo_fill=%u",
            static_cast<unsigned long long>(m_totalReads),
            static_cast<unsigned long long>(m_totalSamples),
            static_cast<unsigned long long>(m_totalWritten),
            static_cast<unsigned long long>(m_failedReads),
            m_sampleFifo->fill());

    m_readerActive.store(false);
    m_readerFinished.store(true);
}

void FOBOSWorker::cleanupAfterReaderJoined(bool readerJoined)
{
    fobos_dev_t* dev = m_dev.load();

    if (!dev) {
        QMutexLocker sessionLock(&g_sessionMutex);
        g_deviceBusy = false;
        logLine("cleanup=no_device");
        return;
    }

    if (readerJoined && m_closeDev) {
        Sleep(100);
        logLine("close_call=START_AFTER_READER_JOIN");
        int r = m_closeDev(dev);
        logLine("close_call=RETURN result=%d error='%s'", r, errorName(r));
    } else {
        logLine("close_call=SKIPPED_READER_NOT_JOINED");
    }

    m_dev.store(nullptr);
    m_running.store(false);
    m_stopRequested.store(false);

    {
        QMutexLocker sessionLock(&g_sessionMutex);
        g_deviceBusy = false;
    }

    logLine("freelibrary_call=SKIPPED_PROCESS_SCOPED_DLL");
    logLine("source_result=STOPPED_CLEANLY_READER_JOINED_CLOSE_DONE");
}

void FOBOSWorker::logLine(const char* fmt, ...) const
{
#if defined(FOBOS_DEBUG_FILE_LOG)
    FILE* f = nullptr;
#ifdef _WIN32
    fopen_s(&f, kLogPath, "a");
#else
    f = std::fopen(kLogPath, "a");
#endif
    if (!f) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
#else
    (void) fmt;
#endif
}

void FOBOSWorker::logTimestamp() const
{
#if defined(FOBOS_DEBUG_FILE_LOG)
    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    logLine("timestamp=%s", ts.toUtf8().constData());
#endif
}

const char* FOBOSWorker::errorName(int code) const
{
    if (m_errorName) {
        const char* s = m_errorName(code);
        return s ? s : "null_error_name";
    }
    return "error_name_unavailable";
}

FixReal FOBOSWorker::floatToFix(float v) const
{
    // Legacy helper kept for ABI/source compatibility. The realtime reader loop uses an
    // inline fast conversion path and does not call this function per sample.
    if (!std::isfinite(v)) {
        return 0;
    }

    const double limit = SDR_RX_SCALEF - 1.0;
    double scaled = static_cast<double>(v) * m_iqGain.load() * SDR_RX_SCALEF;

    if (scaled > limit) {
        scaled = limit;
    } else if (scaled < -limit) {
        scaled = -limit;
    }

    return static_cast<FixReal>(scaled >= 0.0 ? scaled + 0.5 : scaled - 0.5);
}
