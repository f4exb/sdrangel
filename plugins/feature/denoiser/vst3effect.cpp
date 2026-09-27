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
#include <QCoreApplication>
#include <QDataStream>
#include <QFileInfo>
#include <QLibrary>
#include <QMutex>
#include <QMutexLocker>
#include <QSysInfo>
#ifdef QT_WIDGETS_LIB
#include <QGuiApplication>
#include <QWidget>
#endif
#include <QtGlobal>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <functional>
#ifdef Q_OS_LINUX
#include <dlfcn.h>
#endif
#ifdef Q_OS_MAC
#include <CoreFoundation/CoreFoundation.h>
#endif

#include "vst3effect.h"
#include "vst3/vst3_c_api.h"

namespace
{
constexpr int kBlockSize = 512;

#ifndef Q_OS_MAC
QString binaryPath(const QString &path)
{
    const QFileInfo info(path);
    if (info.isFile())
    {
        return info.absoluteFilePath();
    }
    if (!info.isDir() || !path.endsWith(QLatin1String(".vst3"), Qt::CaseInsensitive))
    {
        return {};
    }
    const QDir bundle(path);
    const QString name = info.fileName();
#ifdef Q_OS_WIN
#if defined(Q_PROCESSOR_X86_64)
    const QString arch = QStringLiteral("x86_64-win");
#elif defined(Q_PROCESSOR_ARM_64)
    const QString arch = QStringLiteral("arm64-win");
#else
    const QString arch = QStringLiteral("x86-win");
#endif
    const QString candidate = bundle.filePath(QStringLiteral("Contents/%1/%2").arg(arch, name));
#elif defined(Q_OS_LINUX)
#if defined(Q_PROCESSOR_X86_64)
    const QString arch = QStringLiteral("x86_64-linux");
#else
    const QString arch = QStringLiteral("%1-linux").arg(QSysInfo::currentCpuArchitecture());
#endif
    QString binaryName = name;
    binaryName.chop(5); // Remove the .vst3 bundle extension.
    const QString candidate = bundle.filePath(QStringLiteral("Contents/%1/%2.so").arg(arch, binaryName));
#else
    return {};
#endif
    if (QFileInfo::exists(candidate))
    {
        return candidate;
    }
    return {};
}
#endif

struct Module
{
#ifdef Q_OS_MAC
    CFBundleRef bundle = nullptr;
#else
    QLibrary library;
#endif
    Steinberg_IPluginFactory *factory = nullptr;
    using Exit = bool (*)();
    Exit exit = nullptr;
    bool entered = false;
#ifdef Q_OS_LINUX
    void *nativeHandle = nullptr;
#endif

    bool load(const QString &path, QString &error)
    {
#ifdef Q_OS_MAC
        const QFileInfo info(path);
        if (!info.isDir() || !path.endsWith(QLatin1String(".vst3"), Qt::CaseInsensitive))
        {
            error = QStringLiteral("VST3 bundle was not found in %1").arg(path);
            return false;
        }
        const QByteArray utf8Path = info.absoluteFilePath().toUtf8();
        CFStringRef cfPath =
            CFStringCreateWithCString(kCFAllocatorDefault, utf8Path.constData(), kCFStringEncodingUTF8);
        if (!cfPath)
        {
            error = QStringLiteral("Invalid VST3 bundle path: %1").arg(path);
            return false;
        }
        CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, cfPath, kCFURLPOSIXPathStyle, true);
        CFRelease(cfPath);
        if (!url)
        {
            error = QStringLiteral("Invalid VST3 bundle URL: %1").arg(path);
            return false;
        }
        bundle = CFBundleCreate(kCFAllocatorDefault, url);
        CFRelease(url);
        if (!bundle || !CFBundleLoadExecutable(bundle))
        {
            error = QStringLiteral("Could not load VST3 bundle: %1").arg(path);
            close();
            return false;
        }
        auto entry =
            reinterpret_cast<bool (*)(CFBundleRef)>(CFBundleGetFunctionPointerForName(bundle, CFSTR("bundleEntry")));
        exit = reinterpret_cast<Exit>(CFBundleGetFunctionPointerForName(bundle, CFSTR("bundleExit")));
        if (!entry || !exit || !entry(bundle))
        {
            error = QStringLiteral("VST3 bundleEntry failed or is missing");
            close();
            return false;
        }
        entered = true;
        auto getFactory = reinterpret_cast<Steinberg_IPluginFactory *(*)()>(
            CFBundleGetFunctionPointerForName(bundle, CFSTR("GetPluginFactory")));
#else
        const QString binary = binaryPath(path);
        if (binary.isEmpty())
        {
            error = QStringLiteral("VST3 binary was not found in %1").arg(path);
            return false;
        }
        library.setFileName(binary);
        if (!library.load())
        {
            error = library.errorString();
            return false;
        }
#ifdef Q_OS_WIN
        auto entry = reinterpret_cast<bool (*)()>(library.resolve("InitDll"));
        exit = reinterpret_cast<Exit>(library.resolve("ExitDll"));
        if (entry)
        {
            if (!entry())
            {
                error = QStringLiteral("VST3 InitDll failed");
                close();
                return false;
            }
            entered = true;
        }
#elif defined(Q_OS_LINUX)
        auto entry = reinterpret_cast<bool (*)(void *)>(library.resolve("ModuleEntry"));
        exit = reinterpret_cast<Exit>(library.resolve("ModuleExit"));
        // VST3 requires the dlopen handle in ModuleEntry; QLibrary does not expose it.
        nativeHandle = dlopen(binary.toLocal8Bit().constData(), RTLD_NOW | RTLD_LOCAL);
        if (!entry || !exit || !nativeHandle || !entry(nativeHandle))
        {
            error = QStringLiteral("VST3 ModuleEntry failed or is missing");
            close();
            return false;
        }
        entered = true;
#endif
        auto getFactory = reinterpret_cast<Steinberg_IPluginFactory *(*)()>(library.resolve("GetPluginFactory"));
#endif
        factory = getFactory ? getFactory() : nullptr;
        if (!factory)
        {
            error = QStringLiteral("VST3 GetPluginFactory failed");
            close();
            return false;
        }
        return true;
    }

    void close()
    {
        if (factory)
        {
            factory->lpVtbl->release(factory);
            factory = nullptr;
        }
        if (exit && entered)
        {
            exit();
        }
        exit = nullptr;
        entered = false;
#ifdef Q_OS_LINUX
        if (nativeHandle)
        {
            dlclose(nativeHandle);
            nativeHandle = nullptr;
        }
#endif
#ifdef Q_OS_MAC
        if (bundle)
        {
            CFBundleUnloadExecutable(bundle);
            CFRelease(bundle);
            bundle = nullptr;
        }
#else
        if (library.isLoaded())
        {
            library.unload();
        }
#endif
    }
    ~Module()
    {
        close();
    }
};

Steinberg_tresult SMTG_STDMETHODCALLTYPE hostQuery(void *self, const Steinberg_TUID iid, void **obj)
{
    if (!obj)
    {
        return Steinberg_kInvalidArgument;
    }
    if (std::memcmp(iid, Steinberg_FUnknown_iid, 16) == 0 ||
        std::memcmp(iid, Steinberg_Vst_IHostApplication_iid, 16) == 0)
    {
        *obj = self;
        return Steinberg_kResultOk;
    }
    *obj = nullptr;
    return Steinberg_kNoInterface;
}

Steinberg_uint32 SMTG_STDMETHODCALLTYPE hostAddRef(void *)
{
    return 1;
}

Steinberg_uint32 SMTG_STDMETHODCALLTYPE hostRelease(void *)
{
    return 1;
}

Steinberg_tresult SMTG_STDMETHODCALLTYPE hostName(void *, Steinberg_Vst_String128 name)
{
    constexpr char16_t text[] = u"SDRangel";
    std::memset(name, 0, 128 * sizeof(Steinberg_Vst_TChar));
    std::memcpy(name, text, sizeof(text));
    return Steinberg_kResultOk;
}

Steinberg_tresult SMTG_STDMETHODCALLTYPE hostCreate(void *, Steinberg_TUID, Steinberg_TUID, void **obj)
{
    if (obj)
    {
        *obj = nullptr;
    }
    return Steinberg_kNoInterface;
}
Steinberg_Vst_IHostApplicationVtbl hostVtbl = {hostQuery, hostAddRef, hostRelease, hostName, hostCreate};
Steinberg_Vst_IHostApplication host = {&hostVtbl};

struct ComponentHandler
{
    Steinberg_Vst_IComponentHandler iface;
    std::function<void(quint32, double)> queue;  // Forwards editor edits to this instance's processor.
    std::function<void(quint32, double)> onEdit;
};

Steinberg_tresult SMTG_STDMETHODCALLTYPE handlerQuery(void *self, const Steinberg_TUID iid, void **obj)
{
    if (!obj)
    {
        return Steinberg_kInvalidArgument;
    }
    if (std::memcmp(iid, Steinberg_FUnknown_iid, 16) == 0 ||
        std::memcmp(iid, Steinberg_Vst_IComponentHandler_iid, 16) == 0)
    {
        *obj = self;
        return Steinberg_kResultOk;
    }
    *obj = nullptr;
    return Steinberg_kNoInterface;
}
Steinberg_tresult SMTG_STDMETHODCALLTYPE handlerEdit(void *, Steinberg_Vst_ParamID)
{
    return Steinberg_kResultOk;
}
Steinberg_tresult SMTG_STDMETHODCALLTYPE handlerPerformEdit(void *self, Steinberg_Vst_ParamID id,
                                                            Steinberg_Vst_ParamValue value)
{
    auto *componentHandler = static_cast<ComponentHandler *>(self);
    if (!std::isfinite(value) || value < 0.0 || value > 1.0)
    {
        return Steinberg_kInvalidArgument;
    }
    if (componentHandler->queue)
    {
        componentHandler->queue(id, value);
    }
    if (componentHandler->onEdit)
    {
        componentHandler->onEdit(id, value);
    }
    return Steinberg_kResultOk;
}
Steinberg_tresult SMTG_STDMETHODCALLTYPE handlerRestart(void *, Steinberg_int32)
{
    return Steinberg_kResultOk;
}
Steinberg_Vst_IComponentHandlerVtbl handlerVtbl = {handlerQuery,       hostAddRef,  hostRelease,   handlerEdit,
                                                   handlerPerformEdit, handlerEdit, handlerRestart};

#ifdef QT_WIDGETS_LIB
struct EditorFrame
{
    Steinberg_IPlugFrame iface;
    std::function<Steinberg_tresult(Steinberg_IPlugView *, Steinberg_ViewRect *)> resize;
};

Steinberg_tresult SMTG_STDMETHODCALLTYPE frameQuery(void *self, const Steinberg_TUID iid, void **obj)
{
    if (!obj)
    {
        return Steinberg_kInvalidArgument;
    }
    if (std::memcmp(iid, Steinberg_FUnknown_iid, 16) == 0 || std::memcmp(iid, Steinberg_IPlugFrame_iid, 16) == 0)
    {
        *obj = self;
        return Steinberg_kResultOk;
    }
    *obj = nullptr;
    return Steinberg_kNoInterface;
}

Steinberg_tresult SMTG_STDMETHODCALLTYPE frameResize(void *self, Steinberg_IPlugView *view, Steinberg_ViewRect *size)
{
    auto *frame = static_cast<EditorFrame *>(self);
    return frame->resize ? frame->resize(view, size) : Steinberg_kResultFalse;
}

Steinberg_IPlugFrameVtbl frameVtbl = {frameQuery, hostAddRef, hostRelease, frameResize};
#endif

bool ok(Steinberg_tresult status)
{
    return status == Steinberg_kResultOk;
}

struct ParameterPoint
{
    Steinberg_Vst_IParamValueQueue iface;
    Steinberg_Vst_ParamID id;
    Steinberg_Vst_ParamValue value;
};
struct ParameterChanges
{
    Steinberg_Vst_IParameterChanges iface;
    ParameterPoint points[128];
    int count = 0;
};
Steinberg_tresult SMTG_STDMETHODCALLTYPE pointQuery(void *self, const Steinberg_TUID iid, void **obj)
{
    if (!obj)
    {
        return Steinberg_kInvalidArgument;
    }
    if (std::memcmp(iid, Steinberg_FUnknown_iid, 16) == 0 ||
        std::memcmp(iid, Steinberg_Vst_IParamValueQueue_iid, 16) == 0)
    {
        *obj = self;
        return Steinberg_kResultOk;
    }
    *obj = nullptr;
    return Steinberg_kNoInterface;
}
Steinberg_Vst_ParamID SMTG_STDMETHODCALLTYPE paramId(void *self)
{
    return static_cast<ParameterPoint *>(self)->id;
}
Steinberg_int32 SMTG_STDMETHODCALLTYPE pointCount(void *)
{
    return 1;
}
Steinberg_tresult SMTG_STDMETHODCALLTYPE getPoint(void *self, Steinberg_int32 index, Steinberg_int32 *offset,
                                                  Steinberg_Vst_ParamValue *value)
{
    if (index != 0 || !offset || !value)
    {
        return Steinberg_kInvalidArgument;
    }
    *offset = 0;
    *value = static_cast<ParameterPoint *>(self)->value;
    return Steinberg_kResultOk;
}
Steinberg_tresult SMTG_STDMETHODCALLTYPE addPoint(void *, Steinberg_int32, Steinberg_Vst_ParamValue, Steinberg_int32 *)
{
    return Steinberg_kNotImplemented;
}
Steinberg_Vst_IParamValueQueueVtbl pointVtbl = {pointQuery, hostAddRef, hostRelease, paramId,
                                                pointCount, getPoint,   addPoint};
Steinberg_tresult SMTG_STDMETHODCALLTYPE changesQuery(void *self, const Steinberg_TUID iid, void **obj)
{
    if (!obj)
    {
        return Steinberg_kInvalidArgument;
    }
    if (std::memcmp(iid, Steinberg_FUnknown_iid, 16) == 0 ||
        std::memcmp(iid, Steinberg_Vst_IParameterChanges_iid, 16) == 0)
    {
        *obj = self;
        return Steinberg_kResultOk;
    }
    *obj = nullptr;
    return Steinberg_kNoInterface;
}
Steinberg_int32 SMTG_STDMETHODCALLTYPE changeCount(void *self)
{
    return static_cast<ParameterChanges *>(self)->count;
}
Steinberg_Vst_IParamValueQueue *SMTG_STDMETHODCALLTYPE parameterData(void *self, Steinberg_int32 index)
{
    auto *changes = static_cast<ParameterChanges *>(self);
    return index >= 0 && index < changes->count ? &changes->points[index].iface : nullptr;
}
Steinberg_Vst_IParamValueQueue *SMTG_STDMETHODCALLTYPE addParameterData(void *, const Steinberg_Vst_ParamID *,
                                                                        Steinberg_int32 *)
{
    return nullptr;
}
Steinberg_Vst_IParameterChangesVtbl changesVtbl = {changesQuery, hostAddRef,    hostRelease,
                                                   changeCount,  parameterData, addParameterData};

// In-memory IBStream used to save and restore component and controller state.
struct MemoryStream
{
    Steinberg_IBStream iface;
    QByteArray data;
    qint64 position = 0;
};

Steinberg_tresult SMTG_STDMETHODCALLTYPE streamQuery(void *self, const Steinberg_TUID iid, void **obj)
{
    if (!obj) {
        return Steinberg_kInvalidArgument;
    }
    if (std::memcmp(iid, Steinberg_FUnknown_iid, 16) == 0 || std::memcmp(iid, Steinberg_IBStream_iid, 16) == 0)
    {
        *obj = self;
        return Steinberg_kResultOk;
    }
    *obj = nullptr;
    return Steinberg_kNoInterface;
}

Steinberg_tresult SMTG_STDMETHODCALLTYPE streamRead(void *self, void *buffer, Steinberg_int32 numBytes,
                                                    Steinberg_int32 *numBytesRead)
{
    auto *stream = static_cast<MemoryStream *>(self);
    if (!buffer || numBytes < 0) {
        return Steinberg_kInvalidArgument;
    }
    const qint64 available = std::max<qint64>(0, stream->data.size() - stream->position);
    const Steinberg_int32 count = static_cast<Steinberg_int32>(std::min<qint64>(numBytes, available));
    if (count > 0)
    {
        std::memcpy(buffer, stream->data.constData() + stream->position, count);
        stream->position += count;
    }
    if (numBytesRead) {
        *numBytesRead = count;
    }
    return Steinberg_kResultOk;
}

Steinberg_tresult SMTG_STDMETHODCALLTYPE streamWrite(void *self, void *buffer, Steinberg_int32 numBytes,
                                                     Steinberg_int32 *numBytesWritten)
{
    auto *stream = static_cast<MemoryStream *>(self);
    const qint64 end = stream->position + numBytes;
    if (!buffer || numBytes < 0 || end > 0x7fffffff) {
        return Steinberg_kInvalidArgument;
    }
    if (end > stream->data.size()) {
        stream->data.resize(static_cast<int>(end));
    }
    std::memcpy(stream->data.data() + stream->position, buffer, numBytes);
    stream->position = end;
    if (numBytesWritten) {
        *numBytesWritten = numBytes;
    }
    return Steinberg_kResultOk;
}

Steinberg_tresult SMTG_STDMETHODCALLTYPE streamSeek(void *self, Steinberg_int64 pos, Steinberg_int32 mode,
                                                    Steinberg_int64 *result)
{
    auto *stream = static_cast<MemoryStream *>(self);
    qint64 position = pos;
    if (mode == Steinberg_IBStream_IStreamSeekMode_kIBSeekCur) {
        position += stream->position;
    } else if (mode == Steinberg_IBStream_IStreamSeekMode_kIBSeekEnd) {
        position += stream->data.size();
    } else if (mode != Steinberg_IBStream_IStreamSeekMode_kIBSeekSet) {
        return Steinberg_kInvalidArgument;
    }
    if (position < 0) {
        return Steinberg_kInvalidArgument;
    }
    stream->position = position;
    if (result) {
        *result = position;
    }
    return Steinberg_kResultOk;
}

Steinberg_tresult SMTG_STDMETHODCALLTYPE streamTell(void *self, Steinberg_int64 *pos)
{
    if (!pos) {
        return Steinberg_kInvalidArgument;
    }
    *pos = static_cast<MemoryStream *>(self)->position;
    return Steinberg_kResultOk;
}

Steinberg_IBStreamVtbl streamVtbl = {streamQuery, hostAddRef, hostRelease, streamRead,
                                     streamWrite, streamSeek, streamTell};

MemoryStream makeStream(const QByteArray &data = QByteArray())
{
    MemoryStream stream;
    stream.iface.lpVtbl = &streamVtbl;
    stream.data = data;
    return stream;
}

// Saved state container: magic, version, class ID, component state, controller state.
constexpr quint32 kStateMagic = 0x53565354;
constexpr quint32 kStateVersion = 1;
} // namespace

struct Vst3Effect::Impl
{
    Module module;
    QByteArray classId;
    ComponentHandler handler = {{&handlerVtbl}, {}};
    std::function<void()> detachEditor;
    Steinberg_Vst_IComponent *component = nullptr;
    Steinberg_Vst_IAudioProcessor *processor = nullptr;
    Steinberg_Vst_IEditController *controller = nullptr;
    Steinberg_Vst_IConnectionPoint *componentConnection = nullptr;
    Steinberg_Vst_IConnectionPoint *controllerConnection = nullptr;
    bool componentConnected = false;
    bool controllerConnected = false;
    bool controllerInitialized = false;
    QVector<Vst3ParameterInfo> parameterInfos;
    QMutex parameterMutex;
    ParameterPoint pending[128] = {};
    int pendingCount = 0;
    bool initialized = false;
    bool active = false;
    bool processing = false;
    int inputChannels = 0;
    int outputChannels = 0;
    int sourceChannels = 0;
    int inputBusCount = 0;
    int outputBusCount = 0;
    bool sample64 = false;
    double sampleRate = 0.0;
    Steinberg_Vst_TSamples processedSamples = 0;
    double input64[2][kBlockSize] = {};
    double output64[2][kBlockSize] = {};
    float monoInput[kBlockSize] = {};

    bool queueParameter(quint32 id, double value)
    {
        QMutexLocker lock(&parameterMutex);
        for (int i = 0; i < pendingCount; ++i)
        {
            if (pending[i].id == id)
            {
                pending[i].value = value;
                return true;
            }
        }
        if (pendingCount >= 128)
        {
            return false;
        }
        pending[pendingCount].id = id;
        pending[pendingCount].value = value;
        ++pendingCount;
        return true;
    }

    void close()
    {
        if (detachEditor)
        {
            auto detach = std::move(detachEditor);
            detach();
        }
        if (processor && processing)
        {
            processor->lpVtbl->setProcessing(processor, 0);
        }
        processing = false;
        if (component && active)
        {
            component->lpVtbl->setActive(component, 0);
        }
        active = false;
        if (processor)
        {
            processor->lpVtbl->release(processor);
            processor = nullptr;
        }
        if (controllerConnection && controllerConnected)
        {
            controllerConnection->lpVtbl->disconnect(controllerConnection, componentConnection);
        }
        if (componentConnection && componentConnected)
        {
            componentConnection->lpVtbl->disconnect(componentConnection, controllerConnection);
        }
        controllerConnected = false;
        componentConnected = false;
        if (controllerConnection)
        {
            controllerConnection->lpVtbl->release(controllerConnection);
            controllerConnection = nullptr;
        }
        if (componentConnection)
        {
            componentConnection->lpVtbl->release(componentConnection);
            componentConnection = nullptr;
        }
        if (controller)
        {
            controller->lpVtbl->setComponentHandler(controller, nullptr);
            if (controllerInitialized)
            {
                controller->lpVtbl->terminate(controller);
            }
            controller->lpVtbl->release(controller);
            controller = nullptr;
        }
        controllerInitialized = false;
        parameterInfos.clear();
        pendingCount = 0;
        if (component)
        {
            if (initialized)
            {
                component->lpVtbl->terminate(component);
            }
            component->lpVtbl->release(component);
            component = nullptr;
        }
        initialized = false;
        classId.clear();
        module.close();
    }
    ~Impl()
    {
        close();
    }
};

#ifdef QT_WIDGETS_LIB
class Vst3Effect::EditorWidget : public QWidget
{
public:
    EditorWidget(Vst3Effect *effect, QWidget *parent) : QWidget(parent), m_effect(effect)
    {
        setAttribute(Qt::WA_NativeWindow);
        setFocusPolicy(Qt::StrongFocus);
        m_frame.iface.lpVtbl = &frameVtbl;
        m_frame.resize = [this](Steinberg_IPlugView *view, Steinberg_ViewRect *size)
        {
            return resizeFromPlugin(view, size);
        };
    }

    ~EditorWidget() override
    {
        detach();
        m_effect->m_impl->detachEditor = {};
    }

    bool attach(QString &error)
    {
        auto &impl = *m_effect->m_impl;
        if (!impl.controller)
        {
            error = QStringLiteral("VST3 effect has no editor controller");
            return false;
        }

        const char *platformType = nullptr;
#ifdef Q_OS_WIN
        platformType = Steinberg_kPlatformTypeHWND;
#elif defined(Q_OS_MAC)
        platformType = Steinberg_kPlatformTypeNSView;
#elif defined(Q_OS_LINUX)
        if (QGuiApplication::platformName() == QLatin1String("xcb"))
        {
            platformType = Steinberg_kPlatformTypeX11EmbedWindowID;
        }
#endif
        if (!platformType)
        {
            error = QStringLiteral("No native VST3 editor window is available on this display");
            return false;
        }

        m_view = impl.controller->lpVtbl->createView(impl.controller, Steinberg_Vst_ViewType_kEditor);
        if (!m_view)
        {
            error = QStringLiteral("This VST3 effect has no custom editor");
            return false;
        }
        if (!ok(m_view->lpVtbl->isPlatformTypeSupported(m_view, platformType)))
        {
            error = QStringLiteral("This VST3 editor does not support the current window system");
            detach();
            return false;
        }

        Steinberg_IPlugViewContentScaleSupport *scaleSupport = nullptr;
        if (ok(m_view->lpVtbl->queryInterface(m_view, Steinberg_IPlugViewContentScaleSupport_iid,
                                              reinterpret_cast<void **>(&scaleSupport))) &&
            scaleSupport)
        {
            scaleSupport->lpVtbl->setContentScaleFactor(scaleSupport, static_cast<float>(devicePixelRatioF()));
            scaleSupport->lpVtbl->release(scaleSupport);
        }

        Steinberg_ViewRect size = {};
        if (!ok(m_view->lpVtbl->getSize(m_view, &size)) || size.right <= size.left || size.bottom <= size.top)
        {
            size = {0, 0, 640, 400};
        }
        const qreal scale = nativeScale();
        setFixedSize(qMax(1, qRound((size.right - size.left) / scale)),
                     qMax(1, qRound((size.bottom - size.top) / scale)));
        if (!ok(m_view->lpVtbl->setFrame(m_view, &m_frame.iface)))
        {
            error = QStringLiteral("VST3 editor rejected the host window frame");
            detach();
            return false;
        }

        void *nativeParent = reinterpret_cast<void *>(static_cast<quintptr>(winId()));
        m_attached = true; // attached() may request a resize before it returns.
        if (!ok(m_view->lpVtbl->attached(m_view, nativeParent, platformType)))
        {
            error = QStringLiteral("VST3 editor could not attach to the host window");
            detach();
            return false;
        }
        impl.detachEditor = [this]()
        {
            detach();
        };
        return true;
    }

private:
    qreal nativeScale() const
    {
#ifdef Q_OS_MAC
        return 1.0; // NSView coordinates are logical points.
#else
        return devicePixelRatioF(); // HWND and X11 coordinates are physical pixels.
#endif
    }

    Steinberg_tresult resizeFromPlugin(Steinberg_IPlugView *view, Steinberg_ViewRect *size)
    {
        if (!m_attached || view != m_view || !size || m_resizing || size->right <= size->left ||
            size->bottom <= size->top)
        {
            return Steinberg_kResultFalse;
        }
        m_resizing = true;
        const qreal scale = nativeScale();
        setFixedSize(qMax(1, qRound((size->right - size->left) / scale)),
                     qMax(1, qRound((size->bottom - size->top) / scale)));
        const Steinberg_tresult result = m_view->lpVtbl->onSize(m_view, size);
        updateGeometry();
        m_resizing = false;
        return result;
    }

    void detach()
    {
        if (!m_view)
        {
            return;
        }
        Steinberg_IPlugView *view = m_view;
        m_view = nullptr;
        m_frame.resize = {};
        if (m_attached)
        {
            view->lpVtbl->removed(view);
        }
        m_attached = false;
        view->lpVtbl->setFrame(view, nullptr);
        view->lpVtbl->release(view);
    }

    Vst3Effect *m_effect;
    EditorFrame m_frame = {};
    Steinberg_IPlugView *m_view = nullptr;
    bool m_attached = false;
    bool m_resizing = false;
};
#endif

Vst3Effect::Vst3Effect() : m_impl(new Impl)
{
    Impl *impl = m_impl.get();
    m_impl->handler.queue = [impl](quint32 id, double value)
    {
        impl->queueParameter(id, value);
    };
}

Vst3Effect::~Vst3Effect()
{
    close();
}

QStringList Vst3Effect::modulePaths(const QStringList &extraPaths)
{
    QStringList paths = extraPaths;
    if (paths.isEmpty())
    {
#ifdef Q_OS_WIN
        paths << QDir(qEnvironmentVariable("ProgramFiles")).filePath(QStringLiteral("Common Files/VST3"));
        paths << QDir(qEnvironmentVariable("LOCALAPPDATA")).filePath(QStringLiteral("Programs/Common/VST3"));
        paths << QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("VST3"));
#elif defined(Q_OS_LINUX)
        paths << QDir::home().filePath(QStringLiteral(".vst3")) << QStringLiteral("/usr/lib/vst3")
              << QStringLiteral("/usr/lib64/vst3") << QStringLiteral("/usr/local/lib/vst3")
              << QStringLiteral("/usr/local/lib64/vst3");
        paths << QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("vst3"));
#elif defined(Q_OS_MAC)
        paths << QDir::home().filePath(QStringLiteral("Library/Audio/Plug-ins/VST3"))
              << QStringLiteral("/Library/Audio/Plug-ins/VST3")
              << QStringLiteral("/Network/Library/Audio/Plug-ins/VST3");
        paths << QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../VST3"));
#endif
    }
    QStringList modules;
    std::function<void(const QString &)> collect = [&](const QString &path)
    {
        const QFileInfo info(path);
        if (!info.exists())
        {
            return;
        }
        if (path.endsWith(QLatin1String(".vst3"), Qt::CaseInsensitive) && (info.isFile() || info.isDir()))
        {
            modules << info.absoluteFilePath();
            return;
        }
        if (!info.isDir() || info.isSymLink())
        {
            return;
        }
        const QFileInfoList entries = QDir(path).entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo &child : entries)
        {
            collect(child.absoluteFilePath());
        }
    };
    for (const QString &path : paths)
    {
        collect(path);
    }
    modules.removeDuplicates();
    return modules;
}

QVector<Vst3PluginInfo> Vst3Effect::discover(const QStringList &extraPaths)
{
    QVector<Vst3PluginInfo> result;
    for (const QString &path : modulePaths(extraPaths))
    {
        Module module;
        QString error;
        if (!module.load(path, error))
        {
            continue;
        }
        const int count = module.factory->lpVtbl->countClasses(module.factory);
        for (int i = 0; i < count; ++i)
        {
            Steinberg_PClassInfo info = {};
            if (!ok(module.factory->lpVtbl->getClassInfo(module.factory, i, &info)))
            {
                continue;
            }
            if (QByteArray(info.category, sizeof(info.category)).split('\0').first().trimmed() != "Audio Module Class")
            {
                continue;
            }
            Vst3PluginInfo plugin;
            plugin.modulePath = path;
            plugin.classId = QByteArray(info.cid, 16);
            plugin.name = QString::fromUtf8(QByteArray(info.name, sizeof(info.name)).split('\0').first());
            result.append(plugin);
        }
    }
    std::sort(result.begin(), result.end(),
              [](const auto &a, const auto &b)
              {
                  return a.name < b.name;
              });
    return result;
}

bool Vst3Effect::open(const QString &modulePath, const QByteArray &classId, int sampleRate, int sourceChannels,
                      QString &error, const QByteArray &state)
{
    close();
    if (classId.size() != 16 || sampleRate <= 0 || (sourceChannels != 1 && sourceChannels != 2))
    {
        error = QStringLiteral("Invalid VST3 selection or audio format");
        return false;
    }
    auto &v = *m_impl;
    if (!v.module.load(modulePath, error))
    {
        return false;
    }
    if (!ok(v.module.factory->lpVtbl->createInstance(v.module.factory, classId.constData(),
                                                     Steinberg_Vst_IComponent_iid,
                                                     reinterpret_cast<void **>(&v.component))) ||
        !v.component)
    {
        error = QStringLiteral("VST3 effect component could not be created");
        close();
        return false;
    }
    if (!ok(v.component->lpVtbl->initialize(v.component, reinterpret_cast<Steinberg_FUnknown *>(&host))))
    {
        error = QStringLiteral("VST3 effect initialization failed");
        close();
        return false;
    }
    v.initialized = true;
    v.classId = classId;
    if (!ok(v.component->lpVtbl->queryInterface(v.component, Steinberg_Vst_IAudioProcessor_iid,
                                                reinterpret_cast<void **>(&v.processor))) ||
        !v.processor)
    {
        error = QStringLiteral("Selected VST3 class has no audio processor");
        close();
        return false;
    }
    const auto controllerQuery = v.component->lpVtbl->queryInterface(v.component, Steinberg_Vst_IEditController_iid,
                                                                     reinterpret_cast<void **>(&v.controller));
    if (!ok(controllerQuery) || !v.controller)
    {
        Steinberg_TUID controllerId = {};
        const auto idStatus = v.component->lpVtbl->getControllerClassId(v.component, controllerId);
        const auto createStatus = ok(idStatus) ? v.module.factory->lpVtbl->createInstance(
                                                     v.module.factory, controllerId, Steinberg_Vst_IEditController_iid,
                                                     reinterpret_cast<void **>(&v.controller))
                                               : idStatus;
        if (ok(idStatus) && ok(createStatus) && v.controller)
        {
            const auto initStatus =
                v.controller->lpVtbl->initialize(v.controller, reinterpret_cast<Steinberg_FUnknown *>(&host));
            if (ok(initStatus))
            {
                v.controllerInitialized = true;
            }
            else
            {
                v.controller->lpVtbl->release(v.controller);
                v.controller = nullptr;
            }
        }
    }
    if (v.controller)
    {
        v.controller->lpVtbl->setComponentHandler(v.controller, &v.handler.iface);
        if (v.controllerInitialized)
        {
            v.component->lpVtbl->queryInterface(v.component, Steinberg_Vst_IConnectionPoint_iid,
                                                reinterpret_cast<void **>(&v.componentConnection));
            v.controller->lpVtbl->queryInterface(v.controller, Steinberg_Vst_IConnectionPoint_iid,
                                                 reinterpret_cast<void **>(&v.controllerConnection));
            if (v.componentConnection && v.controllerConnection)
            {
                const auto status1 =
                    v.componentConnection->lpVtbl->connect(v.componentConnection, v.controllerConnection);
                const auto status2 =
                    v.controllerConnection->lpVtbl->connect(v.controllerConnection, v.componentConnection);
                v.componentConnected = ok(status1);
                v.controllerConnected = ok(status2);
            }
        }
    }
    if (!state.isEmpty())
    {
        // Restore before reading parameters, so they reflect the saved state.
        QString stateError;
        if (!setState(state, stateError)) {
            qWarning("Vst3Effect::open: %s", qPrintable(stateError));
        }
    }
    if (v.controller)
    {
        const int count = std::max(0, std::min(4096, v.controller->lpVtbl->getParameterCount(v.controller)));
        for (int i = 0; i < count; ++i)
        {
            Steinberg_Vst_ParameterInfo info = {};
            if (!ok(v.controller->lpVtbl->getParameterInfo(v.controller, i, &info)))
            {
                continue;
            }
            Vst3ParameterInfo parameter;
            parameter.id = info.id;
            parameter.name =
                QString::fromUtf16(reinterpret_cast<const char16_t *>(info.title), 128).split(QChar(0)).first();
            parameter.defaultValue = info.defaultNormalizedValue;
            const double current = v.controller->lpVtbl->getParamNormalized(v.controller, info.id);
            parameter.currentValue =
                std::isfinite(current) && current >= 0.0 && current <= 1.0 ? current : parameter.defaultValue;
            parameter.steps = info.stepCount;
            parameter.readOnly = (info.flags & Steinberg_Vst_ParameterInfo_ParameterFlags_kIsReadOnly) != 0;
            parameter.hidden = (info.flags & Steinberg_Vst_ParameterInfo_ParameterFlags_kIsHidden) != 0;
            v.parameterInfos.append(parameter);
        }
    }
    v.inputBusCount = v.component->lpVtbl->getBusCount(v.component, Steinberg_Vst_MediaTypes_kAudio,
                                                       Steinberg_Vst_BusDirections_kInput);
    v.outputBusCount = v.component->lpVtbl->getBusCount(v.component, Steinberg_Vst_MediaTypes_kAudio,
                                                        Steinberg_Vst_BusDirections_kOutput);
    if (v.inputBusCount < 1 || v.inputBusCount > 16 || v.outputBusCount < 1 || v.outputBusCount > 16)
    {
        error = QStringLiteral("VST3 effect must have a main audio input and output bus");
        close();
        return false;
    }
    Steinberg_Vst_SpeakerArrangement arrangement =
        sourceChannels == 1 ? Steinberg_Vst_SpeakerArr_kMono : Steinberg_Vst_SpeakerArr_kStereo;
    v.processor->lpVtbl->setBusArrangements(v.processor, &arrangement, 1, &arrangement, 1);
    Steinberg_Vst_BusInfo input = {}, output = {};
    if (!ok(v.component->lpVtbl->getBusInfo(v.component, Steinberg_Vst_MediaTypes_kAudio,
                                            Steinberg_Vst_BusDirections_kInput, 0, &input)) ||
        !ok(v.component->lpVtbl->getBusInfo(v.component, Steinberg_Vst_MediaTypes_kAudio,
                                            Steinberg_Vst_BusDirections_kOutput, 0, &output)) ||
        input.channelCount < 1 || input.channelCount > 2 || output.channelCount < 1 || output.channelCount > 2)
    {
        error = QStringLiteral("VST3 effect requires an unsupported bus layout");
        close();
        return false;
    }
    v.inputChannels = input.channelCount;
    v.outputChannels = output.channelCount;
    v.sourceChannels = sourceChannels;
    v.sampleRate = sampleRate;
    v.processedSamples = 0;
    v.component->lpVtbl->activateBus(v.component, Steinberg_Vst_MediaTypes_kAudio, Steinberg_Vst_BusDirections_kInput,
                                     0, 1);
    v.component->lpVtbl->activateBus(v.component, Steinberg_Vst_MediaTypes_kAudio, Steinberg_Vst_BusDirections_kOutput,
                                     0, 1);
    for (int i = 1; i < v.inputBusCount; ++i)
    {
        v.component->lpVtbl->activateBus(v.component, Steinberg_Vst_MediaTypes_kAudio,
                                         Steinberg_Vst_BusDirections_kInput, i, 0);
    }
    for (int i = 1; i < v.outputBusCount; ++i)
    {
        v.component->lpVtbl->activateBus(v.component, Steinberg_Vst_MediaTypes_kAudio,
                                         Steinberg_Vst_BusDirections_kOutput, i, 0);
    }
    if (!ok(v.processor->lpVtbl->canProcessSampleSize(v.processor, Steinberg_Vst_SymbolicSampleSizes_kSample32)))
    {
        if (!ok(v.processor->lpVtbl->canProcessSampleSize(v.processor, Steinberg_Vst_SymbolicSampleSizes_kSample64)))
        {
            error = QStringLiteral("VST3 effect supports neither 32-bit nor 64-bit floating point audio");
            close();
            return false;
        }
        v.sample64 = true;
    }
    Steinberg_Vst_ProcessSetup setup = {Steinberg_Vst_ProcessModes_kRealtime,
                                        v.sample64 ? Steinberg_Vst_SymbolicSampleSizes_kSample64
                                                   : Steinberg_Vst_SymbolicSampleSizes_kSample32,
                                        kBlockSize, static_cast<double>(sampleRate)};
    if (!ok(v.processor->lpVtbl->setupProcessing(v.processor, &setup)) ||
        !ok(v.component->lpVtbl->setActive(v.component, 1)))
    {
        error = QStringLiteral("VST3 effect processing setup failed");
        close();
        return false;
    }
    v.active = true;
    if (!ok(v.processor->lpVtbl->setProcessing(v.processor, 1)))
    {
        error = QStringLiteral("VST3 effect could not start processing");
        close();
        return false;
    }
    v.processing = true;
    return true;
}

bool Vst3Effect::process(const float *left, const float *right, float *outLeft, float *outRight, int count,
                         QString &error)
{
    auto &v = *m_impl;
    if (!v.processing || count < 1 || count > kBlockSize)
    {
        error = QStringLiteral("VST3 effect is not ready");
        return false;
    }
    if (v.inputChannels == 1 && v.sourceChannels == 2 && right)
    {
        for (int i = 0; i < count; ++i)
        {
            v.monoInput[i] = 0.5f * (left[i] + right[i]);
        }
    }
    const float *firstInput = v.inputChannels == 1 && v.sourceChannels == 2 && right ? v.monoInput : left;
    float *in32[] = {const_cast<float *>(firstInput), const_cast<float *>(right ? right : left)};
    float *out32[] = {outLeft, outRight};
    double *in64[] = {v.input64[0], v.input64[1]};
    double *out64[] = {v.output64[0], v.output64[1]};
    if (v.sample64)
    {
        for (int i = 0; i < count; ++i)
        {
            v.input64[0][i] = firstInput[i];
            v.input64[1][i] = right ? right[i] : left[i];
        }
    }
    Steinberg_Vst_AudioBusBuffers inputs[16] = {}, outputs[16] = {};
    inputs[0].numChannels = v.inputChannels;
    outputs[0].numChannels = v.outputChannels;
    if (v.sample64)
    {
        inputs[0].Steinberg_Vst_AudioBusBuffers_channelBuffers64 = in64;
        outputs[0].Steinberg_Vst_AudioBusBuffers_channelBuffers64 = out64;
    }
    else
    {
        inputs[0].Steinberg_Vst_AudioBusBuffers_channelBuffers32 = in32;
        outputs[0].Steinberg_Vst_AudioBusBuffers_channelBuffers32 = out32;
    }
    Steinberg_Vst_ProcessData data = {};
    ParameterChanges changes = {};
    changes.iface.lpVtbl = &changesVtbl;
    {
        QMutexLocker lock(&v.parameterMutex);
        changes.count = v.pendingCount;
        for (int i = 0; i < changes.count; ++i)
        {
            changes.points[i] = v.pending[i];
            changes.points[i].iface.lpVtbl = &pointVtbl;
        }
        v.pendingCount = 0;
    }
    data.processMode = Steinberg_Vst_ProcessModes_kRealtime;
    data.symbolicSampleSize =
        v.sample64 ? Steinberg_Vst_SymbolicSampleSizes_kSample64 : Steinberg_Vst_SymbolicSampleSizes_kSample32;
    data.numSamples = count;
    data.numInputs = v.inputBusCount;
    data.numOutputs = v.outputBusCount;
    data.inputs = inputs;
    data.outputs = outputs;
    data.inputParameterChanges = &changes.iface;
    Steinberg_Vst_ProcessContext context = {};
    context.state = Steinberg_Vst_ProcessContext_StatesAndFlags_kPlaying |
                    Steinberg_Vst_ProcessContext_StatesAndFlags_kContTimeValid;
    context.sampleRate = v.sampleRate;
    context.projectTimeSamples = v.processedSamples;
    context.continousTimeSamples = v.processedSamples;
    context.tempo = 120.0;
    context.timeSigNumerator = 4;
    context.timeSigDenominator = 4;
    data.processContext = &context;
    if (!ok(v.processor->lpVtbl->process(v.processor, &data)))
    {
        error = QStringLiteral("VST3 audio processing failed");
        return false;
    }
    v.processedSamples += count;
    if (v.sample64)
    {
        for (int i = 0; i < count; ++i)
        {
            outLeft[i] = (outputs[0].silenceFlags & 1) ? 0.0f : static_cast<float>(v.output64[0][i]);
            outRight[i] = (outputs[0].silenceFlags & (v.outputChannels == 2 ? 2 : 1))
                              ? 0.0f
                              : static_cast<float>(v.output64[v.outputChannels == 2 ? 1 : 0][i]);
        }
    }
    else
    {
        if (outputs[0].silenceFlags & 1)
        {
            std::fill(outLeft, outLeft + count, 0.0f);
        }
        if (v.outputChannels == 2)
        {
            if (outputs[0].silenceFlags & 2)
            {
                std::fill(outRight, outRight + count, 0.0f);
            }
        }
        else
        {
            std::copy(outLeft, outLeft + count, outRight);
        }
    }
    return true;
}

void Vst3Effect::close()
{
    m_impl->close();
}

void Vst3Effect::setParameterEditCallback(std::function<void(quint32, double)> callback)
{
    m_impl->handler.onEdit = std::move(callback);
}

#ifdef QT_WIDGETS_LIB
QWidget *Vst3Effect::createEditorWidget(QWidget *parent, QString &error)
{
    if (m_impl->detachEditor)
    {
        error = QStringLiteral("The VST3 editor is already open");
        return nullptr;
    }
    auto *widget = new EditorWidget(this, parent);
    if (!widget->attach(error))
    {
        delete widget;
        return nullptr;
    }
    return widget;
}
#endif

unsigned int Vst3Effect::latencySamples() const
{
    return m_impl->processor ? m_impl->processor->lpVtbl->getLatencySamples(m_impl->processor) : 0;
}

QVector<Vst3ParameterInfo> Vst3Effect::parameters() const
{
    return m_impl->parameterInfos;
}

double Vst3Effect::parameterValue(quint32 id) const
{
    auto *controller = m_impl->controller;
    const double value = controller ? controller->lpVtbl->getParamNormalized(controller, id) : -1.0;
    return std::isfinite(value) && value >= 0.0 && value <= 1.0 ? value : -1.0;
}

QByteArray Vst3Effect::state(QString &error)
{
    auto &v = *m_impl;
    if (!v.component)
    {
        error = QStringLiteral("VST3 effect is not open");
        return QByteArray();
    }
    bool pending = false;
    {
        QMutexLocker lock(&v.parameterMutex);
        pending = v.pendingCount > 0;
    }
    if (pending && v.processing)
    {
        // Parameter changes reach the component only through process(), so deliver
        // any queued edits with a silent block before asking it for its state.
        float silence[2][kBlockSize] = {};
        float discard[2][kBlockSize];
        QString processError;
        process(silence[0], silence[1], discard[0], discard[1], kBlockSize, processError);
    }
    MemoryStream componentState = makeStream();
    if (!ok(v.component->lpVtbl->getState(v.component, &componentState.iface)))
    {
        error = QStringLiteral("VST3 effect could not save its state");
        return QByteArray();
    }
    MemoryStream controllerState = makeStream();
    if (v.controller && !ok(v.controller->lpVtbl->getState(v.controller, &controllerState.iface))) {
        controllerState.data.clear(); // Controller state is optional.
    }
    QByteArray result;
    QDataStream out(&result, QIODevice::WriteOnly);
    out << kStateMagic << kStateVersion << v.classId << componentState.data << controllerState.data;
    return result;
}

bool Vst3Effect::setState(const QByteArray &state, QString &error)
{
    auto &v = *m_impl;
    if (!v.component)
    {
        error = QStringLiteral("VST3 effect is not open");
        return false;
    }
    QDataStream in(state);
    quint32 magic = 0, version = 0;
    QByteArray classId, componentData, controllerData;
    in >> magic >> version >> classId >> componentData >> controllerData;
    if (in.status() != QDataStream::Ok || magic != kStateMagic || version != kStateVersion)
    {
        error = QStringLiteral("Saved VST3 state is invalid");
        return false;
    }
    if (classId != v.classId)
    {
        error = QStringLiteral("Saved VST3 state belongs to a different effect");
        return false;
    }
    MemoryStream componentState = makeStream(componentData);
    if (!ok(v.component->lpVtbl->setState(v.component, &componentState.iface)))
    {
        error = QStringLiteral("VST3 effect rejected its saved state");
        return false;
    }
    if (v.controller)
    {
        // The host must mirror the component state into the controller.
        MemoryStream mirroredState = makeStream(componentData);
        v.controller->lpVtbl->setComponentState(v.controller, &mirroredState.iface);
        if (!controllerData.isEmpty())
        {
            MemoryStream controllerState = makeStream(controllerData);
            v.controller->lpVtbl->setState(v.controller, &controllerState.iface);
        }
    }
    return true;
}

bool Vst3Effect::setParameter(quint32 id, double value)
{
    auto &v = *m_impl;
    if (!v.controller || value < 0.0 || value > 1.0)
    {
        return false;
    }
    const auto parameter = std::find_if(v.parameterInfos.cbegin(), v.parameterInfos.cend(),
                                        [id](const Vst3ParameterInfo &info)
                                        {
                                            return info.id == id && !info.readOnly;
                                        });
    if (parameter == v.parameterInfos.cend())
    {
        return false;
    }
    v.controller->lpVtbl->setParamNormalized(v.controller, id, value);
    return v.queueParameter(id, value);
}

QString Vst3Effect::parameterText(quint32 id, double value) const
{
    auto *controller = m_impl->controller;
    if (controller)
    {
        Steinberg_Vst_String128 text = {};
        if (ok(controller->lpVtbl->getParamStringByValue(controller, id, value, text)))
        {
            const QString formatted =
                QString::fromUtf16(reinterpret_cast<const char16_t *>(text), 128).split(QChar(0)).first();
            if (!formatted.isEmpty())
            {
                return formatted;
            }
        }
    }
    return QString::number(value, 'f', 3);
}
