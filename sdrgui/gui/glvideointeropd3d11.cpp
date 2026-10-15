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

// D3D11 frames (Windows, D3D11VA decoding): the GPU's video processor deinterlaces and converts each NV12 frame to RGB
// into a texture of the same device, which is copied to one WGL_NV_DX_interop2 shares with OpenGL. Nothing goes
// through the CPU

#include "gui/glvideointerop.h"

#if defined(_WIN32) && !defined(__MINGW32__)

#include <map>

#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions>

using Microsoft::WRL::ComPtr;

namespace {

typedef HANDLE (WINAPI *DXOpenDevice)(void *dxDevice);
typedef BOOL (WINAPI *DXCloseDevice)(HANDLE device);
typedef HANDLE (WINAPI *DXRegisterObject)(HANDLE device, void *dxObject, GLuint name, GLenum type, GLenum access);
typedef BOOL (WINAPI *DXUnregisterObject)(HANDLE device, HANDLE object);
typedef BOOL (WINAPI *DXLockObjects)(HANDLE device, GLint count, HANDLE *objects);
typedef BOOL (WINAPI *DXUnlockObjects)(HANDLE device, GLint count, HANDLE *objects);

const GLenum accessReadOnly = 0x0000; // WGL_ACCESS_READ_ONLY_NV

class GLVideoInteropD3D11 : public GLVideoInterop
{
public:
    GLVideoInteropD3D11() :
        m_gl(nullptr),
        m_interopDevice(nullptr),
        m_width(0),
        m_height(0),
        m_pastFrames(0),
        m_canDeinterlace(false),
        m_glTexture(0),
        m_glObject(nullptr),
        m_locked(false),
        m_acquired(false),
        m_outputFrame(0),
        m_lastTexture(nullptr)
    {
    }

    ~GLVideoInteropD3D11() override
    {
        closeDevice();
    }

    bool init(QOpenGLContext *context)
    {
        m_gl = context->functions();
        m_open = reinterpret_cast<DXOpenDevice>(context->getProcAddress("wglDXOpenDeviceNV"));
        m_close = reinterpret_cast<DXCloseDevice>(context->getProcAddress("wglDXCloseDeviceNV"));
        m_register = reinterpret_cast<DXRegisterObject>(context->getProcAddress("wglDXRegisterObjectNV"));
        m_unregister = reinterpret_cast<DXUnregisterObject>(context->getProcAddress("wglDXUnregisterObjectNV"));
        m_dxLockObjects = reinterpret_cast<DXLockObjects>(context->getProcAddress("wglDXLockObjectsNV"));
        m_dxUnlockObjects = reinterpret_cast<DXUnlockObjects>(context->getProcAddress("wglDXUnlockObjectsNV"));
        return m_open && m_close && m_register && m_unregister && m_dxLockObjects && m_dxUnlockObjects;
    }

    GLVideoFrame::Type type() const override { return GLVideoFrame::D3D11; }
    int pastFrames() const override { return m_pastFrames; }
    bool canDeinterlace() const override { return m_canDeinterlace; }

    bool acquire(const GLVideoFrame& frame, const GLVideoFrame *const *past, int nbPast, bool deinterlace,
        GLuint textures[2], Layout& layout) override
    {
        ID3D11Device *device = static_cast<ID3D11Device *>(frame.m_d3d11Device);
        ID3D11Texture2D *texture = static_cast<ID3D11Texture2D *>(frame.m_d3d11Texture);
        m_acquired = false;

        if (!device || !texture || !openDevice(device) || !setupProcessor(frame.m_width, frame.m_height)) {
            return false;
        }

        // If the video processor can't deinterlace, the frame's shown as it is: the renderer then has the decoder
        // deinterlace in memory, while deinterlacing's on
        const bool deinterlaceFrame = deinterlace && frame.m_interlaced && m_canDeinterlace;

        if (texture != m_lastTexture)
        {
            m_inputViews.clear(); // A new decoder: its texture array
            m_lastTexture = texture;
        }

        if (frame.m_lock) {
            frame.m_lock();
        }

        bool ok = false;
        ID3D11VideoProcessorInputView *input = inputView(texture, frame.m_d3d11Index);
        ID3D11VideoProcessorInputView *pastViews[4];
        int nbPastViews = 0;

        for (int i = 0; deinterlaceFrame && (i < nbPast) && (i < 4); i++)
        {
            ID3D11Texture2D *pastTexture = static_cast<ID3D11Texture2D *>(past[i]->m_d3d11Texture);

            if (pastTexture == texture)
            {
                ID3D11VideoProcessorInputView *view = inputView(pastTexture, past[i]->m_d3d11Index);

                if (view) {
                    pastViews[nbPastViews++] = view;
                }
            }
        }

        if (input)
        {
            const D3D11_VIDEO_FRAME_FORMAT format = !deinterlaceFrame ? D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE
                : frame.m_topFieldFirst ? D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST : D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST;
            m_videoContext->VideoProcessorSetStreamFrameFormat(m_processor.Get(), 0, format);
            // Only the picture, unscaled: the decoder's textures (and so the past frames', of the same array) are
            // larger, aligned to its blocks (as 1080 lines in 1088), and the padding would otherwise be scaled in
            const RECT rect = {0, 0, (LONG) frame.m_width, (LONG) frame.m_height};
            m_videoContext->VideoProcessorSetStreamSourceRect(m_processor.Get(), 0, TRUE, &rect);
            m_videoContext->VideoProcessorSetStreamDestRect(m_processor.Get(), 0, TRUE, &rect);
            m_videoContext->VideoProcessorSetOutputTargetRect(m_processor.Get(), TRUE, &rect);
            // A frame out for each frame in, as bwdif's send_frame
            m_videoContext->VideoProcessorSetStreamOutputRate(m_processor.Get(), 0, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_HALF, FALSE, nullptr);
            m_videoContext->VideoProcessorSetStreamAutoProcessingMode(m_processor.Get(), 0, FALSE);

            D3D11_VIDEO_PROCESSOR_COLOR_SPACE in = {};
            // BT.2020 needs ID3D11VideoContext1's DXGI colour spaces: BT.709 is the nearest of these
            in.YCbCr_Matrix = frame.m_matrix == GLVideoFrame::BT601 ? 0 : 1;
            in.Nominal_Range = frame.m_fullRange ? D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_0_255 : D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_16_235;
            m_videoContext->VideoProcessorSetStreamColorSpace(m_processor.Get(), 0, &in);
            D3D11_VIDEO_PROCESSOR_COLOR_SPACE out = {};
            out.RGB_Range = 0; // 0 to 255
            m_videoContext->VideoProcessorSetOutputColorSpace(m_processor.Get(), &out);

            D3D11_VIDEO_PROCESSOR_STREAM stream = {};
            stream.Enable = TRUE;
            stream.InputFrameOrField = 0;
            stream.PastFrames = nbPastViews;
            stream.ppPastSurfaces = nbPastViews > 0 ? pastViews : nullptr;
            stream.pInputSurface = input;
            const HRESULT hr = m_videoContext->VideoProcessorBlt(m_processor.Get(), m_outputView.Get(), m_outputFrame++, 1, &stream);
            ok = SUCCEEDED(hr);

            if (ok) {
                m_context->CopyResource(m_shared.Get(), m_output.Get());
            }

            m_context->Flush();

            if (!ok) {
                qWarning("GLVideoInteropD3D11: VideoProcessorBlt failed: 0x%08lx", (unsigned long) hr);
            }
        }

        if (frame.m_unlock) {
            frame.m_unlock();
        }

        if (!ok) {
            return false;
        }

        if (!m_dxLockObjects(m_interopDevice, 1, &m_glObject))
        {
            qWarning("GLVideoInteropD3D11: wglDXLockObjectsNV failed");
            return false;
        }

        m_locked = true;
        m_acquired = true;
        textures[0] = m_glTexture;
        textures[1] = 0;
        layout = LayoutRGBA;
        return true;
    }

    // The shared texture keeps the last frame processed
    bool reacquire(GLuint textures[2], Layout& layout) override
    {
        if (!m_acquired || !m_glObject || m_locked) {
            return false;
        }

        if (!m_dxLockObjects(m_interopDevice, 1, &m_glObject))
        {
            qWarning("GLVideoInteropD3D11: wglDXLockObjectsNV failed");
            return false;
        }

        m_locked = true;
        textures[0] = m_glTexture;
        textures[1] = 0;
        layout = LayoutRGBA;
        return true;
    }

    void release() override
    {
        if (m_locked)
        {
            m_dxUnlockObjects(m_interopDevice, 1, &m_glObject);
            m_locked = false;
        }
    }

    // The input views hold the decoder's texture array, and the device (and the interop's) the decoder's device.
    // Opened again for the next frame
    void releaseFrames() override
    {
        closeDevice();
    }

    void abandon() override
    {
        // The registration, OpenGL texture and interop device go with the context. Then only Direct3D's references
        m_locked = false;
        m_glObject = nullptr;
        m_glTexture = 0;
        m_interopDevice = nullptr;
        closeDevice();
    }

private:
    bool openDevice(ID3D11Device *device)
    {
        if (device == m_device.Get()) {
            return m_interopDevice != nullptr;
        }

        closeDevice();
        m_device = device;
        m_interopDevice = m_open(device);

        if (!m_interopDevice)
        {
            qWarning("GLVideoInteropD3D11: wglDXOpenDeviceNV failed");
            return false;
        }

        device->GetImmediateContext(&m_context);

        if (FAILED(device->QueryInterface(IID_PPV_ARGS(&m_videoDevice))) || FAILED(m_context.As(&m_videoContext)))
        {
            qWarning("GLVideoInteropD3D11: no video device");
            return false;
        }

        return true;
    }

    void closeProcessor()
    {
        release();

        if (m_glObject)
        {
            m_unregister(m_interopDevice, m_glObject);
            m_glObject = nullptr;
        }

        if (m_glTexture)
        {
            m_gl->glDeleteTextures(1, &m_glTexture);
            m_glTexture = 0;
        }

        m_inputViews.clear();
        m_acquired = false;
        m_canDeinterlace = false;
        m_outputView.Reset();
        m_output.Reset();
        m_shared.Reset();
        m_processor.Reset();
        m_enumerator.Reset();
        m_width = 0;
        m_height = 0;
    }

    void closeDevice()
    {
        closeProcessor();

        if (m_interopDevice)
        {
            m_close(m_interopDevice);
            m_interopDevice = nullptr;
        }

        m_videoContext.Reset();
        m_videoDevice.Reset();
        m_context.Reset();
        m_device.Reset();
        m_lastTexture = nullptr;
    }

    // A video processor that can deinterlace, preferably adaptively (with previous frames), and its output texture,
    // shared with OpenGL
    bool setupProcessor(int width, int height)
    {
        if ((width == m_width) && (height == m_height) && m_processor) {
            return true;
        }

        closeProcessor();
        D3D11_VIDEO_PROCESSOR_CONTENT_DESC desc = {};
        desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
        desc.InputFrameRate = {25, 1};
        desc.InputWidth = width;
        desc.InputHeight = height;
        desc.OutputFrameRate = {25, 1};
        desc.OutputWidth = width;
        desc.OutputHeight = height;
        desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

        if (FAILED(m_videoDevice->CreateVideoProcessorEnumerator(&desc, &m_enumerator))) {
            return false;
        }

        D3D11_VIDEO_PROCESSOR_CAPS caps;
        m_enumerator->GetVideoProcessorCaps(&caps);
        UINT index = 0;
        int best = -1;

        for (UINT i = 0; i < caps.RateConversionCapsCount; i++)
        {
            D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS rate;

            if (FAILED(m_enumerator->GetVideoProcessorRateConversionCaps(i, &rate))) {
                continue;
            }

            const int score = (rate.ProcessorCaps & D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_ADAPTIVE) ? 2
                : (rate.ProcessorCaps & D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BOB) ? 1 : 0;

            if (score > best)
            {
                best = score;
                index = i;
                m_pastFrames = (int) std::min<UINT>(rate.PastFrames, 4);
            }
        }

        if (FAILED(m_videoDevice->CreateVideoProcessor(m_enumerator.Get(), index, &m_processor))) {
            return false;
        }

        UINT support = 0;
        DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM;

        if (FAILED(m_enumerator->CheckVideoProcessorFormat(format, &support)) || !(support & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT)) {
            format = DXGI_FORMAT_R8G8B8A8_UNORM;
        }

        D3D11_TEXTURE2D_DESC texture = {};
        texture.Width = width;
        texture.Height = height;
        texture.MipLevels = 1;
        texture.ArraySize = 1;
        texture.Format = format;
        texture.SampleDesc.Count = 1;
        texture.Usage = D3D11_USAGE_DEFAULT;
        texture.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        if (FAILED(m_device->CreateTexture2D(&texture, nullptr, &m_output))) {
            return false;
        }

        D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC view = {};
        view.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;

        if (FAILED(m_videoDevice->CreateVideoProcessorOutputView(m_output.Get(), m_enumerator.Get(), &view, &m_outputView))) {
            return false;
        }

        // The video processor can't write to a texture registered with OpenGL: NVIDIA's driver (2026) crashes later
        // (in its worker thread), and OpenGL sees nothing. So its output is copied to another texture, that is
        texture.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        if (FAILED(m_device->CreateTexture2D(&texture, nullptr, &m_shared))) {
            return false;
        }

        m_gl->glGenTextures(1, &m_glTexture);
        m_glObject = m_register(m_interopDevice, m_shared.Get(), m_glTexture, GL_TEXTURE_2D, accessReadOnly);

        if (!m_glObject)
        {
            qWarning("GLVideoInteropD3D11: wglDXRegisterObjectNV failed");
            return false;
        }

        m_gl->glBindTexture(GL_TEXTURE_2D, m_glTexture);
        m_gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        m_gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        m_gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        m_gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        m_gl->glBindTexture(GL_TEXTURE_2D, 0);
        m_width = width;
        m_height = height;
        m_canDeinterlace = best > 0;
        qDebug("GLVideoInteropD3D11: %dx%d, deinterlacing %s, %d past frames", width, height,
            best == 2 ? "adaptive" : best == 1 ? "bob" : "none", m_pastFrames);
        return true;
    }

    // Input views of the decoder's texture array slices, made once each
    ID3D11VideoProcessorInputView *inputView(ID3D11Texture2D *texture, int index)
    {
        auto it = m_inputViews.find(index);

        if (it != m_inputViews.end()) {
            return it->second.Get();
        }

        D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC desc = {};
        desc.FourCC = 0;
        desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipSlice = 0;
        desc.Texture2D.ArraySlice = index;
        ComPtr<ID3D11VideoProcessorInputView> view;

        if (FAILED(m_videoDevice->CreateVideoProcessorInputView(texture, m_enumerator.Get(), &desc, &view)))
        {
            qWarning("GLVideoInteropD3D11: CreateVideoProcessorInputView failed");
            return nullptr;
        }

        m_inputViews[index] = view;
        return view.Get();
    }

    QOpenGLFunctions *m_gl;
    DXOpenDevice m_open = nullptr;
    DXCloseDevice m_close = nullptr;
    DXRegisterObject m_register = nullptr;
    DXUnregisterObject m_unregister = nullptr;
    DXLockObjects m_dxLockObjects = nullptr;    //!< Of the interop's objects, unlike GLVideoFrame::m_lock (the decoder's)
    DXUnlockObjects m_dxUnlockObjects = nullptr;

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<ID3D11VideoDevice> m_videoDevice;
    ComPtr<ID3D11VideoContext> m_videoContext;
    HANDLE m_interopDevice;
    ComPtr<ID3D11VideoProcessorEnumerator> m_enumerator;
    ComPtr<ID3D11VideoProcessor> m_processor;
    ComPtr<ID3D11Texture2D> m_output;
    ComPtr<ID3D11Texture2D> m_shared; // A copy of m_output, shared with OpenGL
    ComPtr<ID3D11VideoProcessorOutputView> m_outputView;
    std::map<int, ComPtr<ID3D11VideoProcessorInputView>> m_inputViews;
    int m_width;
    int m_height;
    int m_pastFrames;
    bool m_canDeinterlace;  //!< Else interlaced frames that are to be deinterlaced are shown as they are
    GLuint m_glTexture;
    HANDLE m_glObject;
    bool m_locked;
    bool m_acquired;        //!< m_shared has the last frame acquired
    UINT m_outputFrame;
    ID3D11Texture2D *m_lastTexture;
};

} // namespace

std::unique_ptr<GLVideoInterop> createGLVideoInteropD3D11(QOpenGLContext *context)
{
    if (context->isOpenGLES()) {
        return nullptr;
    }

    std::unique_ptr<GLVideoInteropD3D11> interop(new GLVideoInteropD3D11());

    if (!interop->init(context)) {
        return nullptr;
    }

    return std::unique_ptr<GLVideoInterop>(interop.release());
}

#else

std::unique_ptr<GLVideoInterop> createGLVideoInteropD3D11(QOpenGLContext *)
{
    return nullptr;
}

#endif
