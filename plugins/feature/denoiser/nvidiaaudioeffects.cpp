///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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
 
#include <QDir>
#include <QFileInfo>
#include <QByteArray>
#include <QFile>
#include <QLibrary>
#include <QThread>
#include <cstring>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "nvidiaaudioeffects.h"

#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
struct NvidiaAudioEffects::Impl
{
    using CreateEffect = int (*)(const char *, void **);
    using DestroyEffect = int (*)(void *);
    using SetString = int (*)(void *, const char *, const char *);
    using SetU32 = int (*)(void *, const char *, unsigned int);
    using SetFloat = int (*)(void *, const char *, float);
    using GetU32 = int (*)(void *, const char *, unsigned int *);
    using Load = int (*)(void *);
    using Run = int (*)(void *, const float **, float **, unsigned int, unsigned int);

    QLibrary library;
    void *effect = nullptr;
    QByteArray modelPath;
    Qt::HANDLE threadId = nullptr;
    DestroyEffect destroyEffect = nullptr;
    Run run = nullptr;
};
#else
struct NvidiaAudioEffects::Impl {};
#endif

NvidiaAudioEffects::NvidiaAudioEffects() : 
    m_impl(nullptr) 
{
}

NvidiaAudioEffects::~NvidiaAudioEffects()
{
    shutdown();
}

bool NvidiaAudioEffects::initialize(QString& error, float intensityRatio, bool enableVad)
{
    shutdown();
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    QString sdkDir = qEnvironmentVariable("NVAFX_SDK_DIR");

#ifdef Q_OS_WIN
    if (sdkDir.isEmpty()) {
        sdkDir = QDir(qEnvironmentVariable("ProgramFiles")).filePath("NVIDIA Corporation/NVIDIA Audio Effects SDK");
    }

    const QString libraryPath = QDir(sdkDir).filePath("NVAudioEffects.dll");
    const QString modelPath = QDir(sdkDir).filePath("models/denoiser_48k.trtpkg");
#else
    if (sdkDir.isEmpty()) {
        sdkDir = qEnvironmentVariable("AFX_SDK_ROOT");
    }
    if (sdkDir.isEmpty()) {
        error = QStringLiteral("Set NVAFX_SDK_DIR to the Linux Audio Effects SDK root");
        return false;
    }

    const QString libraryPath = QDir(sdkDir).filePath("nvafx/lib/libnv_audiofx.so");
    QString modelPath = qEnvironmentVariable("NVAFX_MODEL_PATH");
    if (modelPath.isEmpty())
    {
        // Packages contain GPU-specific models. Pick one only if unambiguous.
        QStringList models;
        for (const QString& feature : {QStringLiteral("denoiser"), QStringLiteral("nvafxdenoiser")})
        {
            const QDir modelDir(QDir(sdkDir).filePath(QStringLiteral("features/%1/models").arg(feature)));
            for (const QString& gpuDir : modelDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
            {
                const QString path = modelDir.filePath(gpuDir + QStringLiteral("/denoiser_48k.trtpkg"));
                if (QFileInfo(path).isFile()) {
                    models.append(path);
                }
            }
        }
        if (models.size() == 1) {
            modelPath = models.first();
        } else {
            error = QStringLiteral("Set NVAFX_MODEL_PATH to this GPU's denoiser_48k.trtpkg (found %1 models)").arg(models.size());
            return false;
        }
    }
#endif

    if (!QFileInfo(libraryPath).isFile() || !QFileInfo(modelPath).isFile())
    {
        error = QStringLiteral("NVIDIA Audio Effects library or 48 kHz model was not found: %1, %2").arg(libraryPath, modelPath);
        return false;
    }

    m_impl = new Impl;
    m_impl->modelPath = QFile::encodeName(QDir::toNativeSeparators(modelPath));
    m_impl->library.setFileName(QFileInfo(libraryPath).absoluteFilePath());
#ifdef Q_OS_WIN
    // QLibrary's ordinary load cannot find this SDK's adjacent DLL dependencies.
    // Load once with the SDK directory in the dependency search path, then let
    // QLibrary acquire and own its own reference to the already loaded DLL.
    HMODULE bootstrap = LoadLibraryExW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(libraryPath).utf16()),
        nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!bootstrap)
    {
        error = QStringLiteral("Could not load NVIDIA Audio Effects DLL (Windows error %1)").arg(GetLastError());
        shutdown();
        return false;
    }
#endif
    const bool loaded = m_impl->library.load();
#ifdef Q_OS_WIN
    FreeLibrary(bootstrap);
#endif
    if (!loaded)
    {
        error = QStringLiteral("Could not load NVIDIA Audio Effects library: %1").arg(m_impl->library.errorString());
        shutdown();
        return false;
    }

    m_impl->threadId = QThread::currentThreadId();
    const auto resolve = [this](const char *name) { return m_impl->library.resolve(name); };
    const auto createEffect = reinterpret_cast<Impl::CreateEffect>(resolve("NvAFX_CreateEffect"));
    m_impl->destroyEffect = reinterpret_cast<Impl::DestroyEffect>(resolve("NvAFX_DestroyEffect"));
    const auto setString = reinterpret_cast<Impl::SetString>(resolve("NvAFX_SetString"));
    const auto setU32 = reinterpret_cast<Impl::SetU32>(resolve("NvAFX_SetU32"));
    const auto setFloat = reinterpret_cast<Impl::SetFloat>(resolve("NvAFX_SetFloat"));
    const auto getU32 = reinterpret_cast<Impl::GetU32>(resolve("NvAFX_GetU32"));
    const auto load = reinterpret_cast<Impl::Load>(resolve("NvAFX_Load"));
    m_impl->run = reinterpret_cast<Impl::Run>(resolve("NvAFX_Run"));

    if (!createEffect || !m_impl->destroyEffect || !setString || !setU32 || !setFloat || !getU32 || !load || !m_impl->run)
    {
        error = QStringLiteral("NVIDIA Audio Effects library is missing required functions");
        shutdown();
        return false;
    }

    int status = createEffect("denoiser", &m_impl->effect);

    if (status == 0) {
        status = setU32(m_impl->effect, "input_sample_rate", 48000);
    }
    if (status == 0) {
        status = setFloat(m_impl->effect, "intensity_ratio", qBound(0.0f, intensityRatio, 1.0f));
    }
    if (status == 0) {
        status = setU32(m_impl->effect, "enable_vad", enableVad ? 1U : 0U);
    }

#ifdef Q_OS_LINUX
    if (status == 0) {
        status = setU32(m_impl->effect, "num_streams", 1);
    }
    if (status == 0) {
        status = setU32(m_impl->effect, "num_samples_per_input_frame", 480);
    }
#endif

    if (status == 0) {
        status = setString(m_impl->effect, "model_path", m_impl->modelPath.constData());
    }

    if (status == 0) {
        status = load(m_impl->effect);
    }

    unsigned int frameSize = 0;
    unsigned int outputFrameSize = 0;
    unsigned int inputChannels = 0;
    unsigned int outputChannels = 0;
    unsigned int outputSampleRate = 0;

#ifdef Q_OS_LINUX
    const char *inputFrameSelector = "num_samples_per_input_frame";
    const char *outputFrameSelector = "num_samples_per_output_frame";
#else
    const char *inputFrameSelector = "num_input_samples_per_frame";
    const char *outputFrameSelector = "num_output_samples_per_frame";
#endif

    if (status == 0) {
        status = getU32(m_impl->effect, inputFrameSelector, &frameSize);
    }

    if (status == 0) {
        status = getU32(m_impl->effect, outputFrameSelector, &outputFrameSize);
    }

    if (status == 0) {
        status = getU32(m_impl->effect, "num_input_channels", &inputChannels);
    }

    if (status == 0) {
        status = getU32(m_impl->effect, "num_output_channels", &outputChannels);
    }

    if (status == 0) {
        status = getU32(m_impl->effect, "output_sample_rate", &outputSampleRate);
    }

    if (status != 0 || frameSize != 480 || outputFrameSize != 480 || inputChannels != 1 || outputChannels != 1 || outputSampleRate != 48000)
    {
        error = QStringLiteral("NVIDIA denoiser initialization failed (status %1, frames %2/%3, channels %4/%5, output rate %6)")
            .arg(status).arg(frameSize).arg(outputFrameSize).arg(inputChannels).arg(outputChannels).arg(outputSampleRate);
        shutdown();
        return false;
    }

    return true;
#else
    error = QStringLiteral("NVIDIA Audio Effects is available only on Windows and Linux");
    return false;
#endif
}

void NvidiaAudioEffects::shutdown()
{
    if (!m_impl) {
        return;
    }
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    if (m_impl->effect && m_impl->destroyEffect) {
        m_impl->destroyEffect(m_impl->effect);
    }
    if (m_impl->library.isLoaded()) {
        m_impl->library.unload();
    }
#endif
    delete m_impl;
    m_impl = nullptr;
}

bool NvidiaAudioEffects::process(const float *input, float *output, QString& error)
{
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    if (!m_impl || !m_impl->effect) {
        error = QStringLiteral("NVIDIA denoiser is not initialized");
        return false;
    }

    if (QThread::currentThreadId() != m_impl->threadId) {
        error = QStringLiteral("NVIDIA denoiser was called from a different thread than it was initialized on");
        return false;
    }

    // Keep the caller's input intact for failure passthrough.
    std::memcpy(output, input, 480 * sizeof(float));
#ifdef Q_OS_WIN
    // The installed Windows runtime processes the buffer in place, as in OBS.
    const float *inputs[] = {output};
#else
    const float *inputs[] = {input};
#endif
    float *outputs[] = {output};
    const int status = m_impl->run(m_impl->effect, inputs, outputs, 480, 1);

    if (status != 0)
    {
        error = QStringLiteral("NVIDIA denoiser processing failed (status %1)").arg(status);
        shutdown();
        return false;
    }

    return true;
#else
    Q_UNUSED(input)
    Q_UNUSED(output)
    error = QStringLiteral("NVIDIA Audio Effects is available only on Windows and Linux");
    return false;
#endif
}
