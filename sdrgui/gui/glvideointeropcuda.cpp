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

// CUDA frames (NVIDIA, NVDEC decoding, deinterlaced by bwdif_cuda): each NV12 plane is copied, within the GPU, into
// an OpenGL texture registered with CUDA. The CUDA driver is loaded when needed, so there's no dependency on it

#include "gui/glvideointerop.h"

#if defined(_WIN32) || (defined(__linux__) && !defined(ANDROID))

#include <cstddef>

#include <QDebug>
#include <QLibrary>
#include <QOpenGLContext>
#include <QOpenGLFunctions>

#ifndef GL_RED
#define GL_RED 0x1903
#endif
#ifndef GL_RG
#define GL_RG 0x8227
#endif
#ifndef GL_R8
#define GL_R8 0x8229
#endif
#ifndef GL_RG8
#define GL_RG8 0x822B
#endif

namespace {

// The parts of the CUDA driver API (cuda.h) used
typedef int CUresult;
typedef void *CUcontext;
typedef void *CUgraphicsResource;
typedef void *CUarray;
typedef void *CUstream;
typedef unsigned long long CUdeviceptr;

enum { CU_MEMORYTYPE_DEVICE = 2, CU_MEMORYTYPE_ARRAY = 3 };
enum { CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD = 2 };

struct CUDAMemcpy2D
{
    size_t srcXInBytes;
    size_t srcY;
    int srcMemoryType;
    const void *srcHost;
    CUdeviceptr srcDevice;
    CUarray srcArray;
    size_t srcPitch;
    size_t dstXInBytes;
    size_t dstY;
    int dstMemoryType;
    void *dstHost;
    CUdeviceptr dstDevice;
    CUarray dstArray;
    size_t dstPitch;
    size_t WidthInBytes;
    size_t Height;
};

#if defined(_WIN32)
#define CUDAAPI __stdcall
#else
#define CUDAAPI
#endif

typedef CUresult (CUDAAPI *CtxPushCurrent)(CUcontext);
typedef CUresult (CUDAAPI *CtxPopCurrent)(CUcontext *);
typedef CUresult (CUDAAPI *GraphicsGLRegisterImage)(CUgraphicsResource *, GLuint, GLenum, unsigned int);
typedef CUresult (CUDAAPI *GraphicsUnregisterResource)(CUgraphicsResource);
typedef CUresult (CUDAAPI *GraphicsMapResources)(unsigned int, CUgraphicsResource *, CUstream);
typedef CUresult (CUDAAPI *GraphicsUnmapResources)(unsigned int, CUgraphicsResource *, CUstream);
typedef CUresult (CUDAAPI *GraphicsSubResourceGetMappedArray)(CUarray *, CUgraphicsResource, unsigned int, unsigned int);
typedef CUresult (CUDAAPI *Memcpy2D)(const CUDAMemcpy2D *);

class GLVideoInteropCUDA : public GLVideoInterop
{
public:
    GLVideoInteropCUDA() :
        m_gl(nullptr),
        m_context(nullptr),
        m_width(0),
        m_height(0),
        m_textures{},
        m_resources{},
        m_acquired(false)
    {
    }

    ~GLVideoInteropCUDA() override
    {
        unregister();

        if (m_textures[0]) {
            m_gl->glDeleteTextures(2, m_textures);
        }
    }

    bool init(QOpenGLContext *context)
    {
#if defined(_WIN32)
        m_library.setFileName("nvcuda");
#else
        m_library.setFileName("cuda");
        m_library.setFileNameAndVersion("cuda", 1);
#endif
        if (!m_library.load()) {
            return false;
        }

        m_push = reinterpret_cast<CtxPushCurrent>(m_library.resolve("cuCtxPushCurrent_v2"));
        m_pop = reinterpret_cast<CtxPopCurrent>(m_library.resolve("cuCtxPopCurrent_v2"));
        m_register = reinterpret_cast<GraphicsGLRegisterImage>(m_library.resolve("cuGraphicsGLRegisterImage"));
        m_unregister = reinterpret_cast<GraphicsUnregisterResource>(m_library.resolve("cuGraphicsUnregisterResource"));
        m_map = reinterpret_cast<GraphicsMapResources>(m_library.resolve("cuGraphicsMapResources"));
        m_unmap = reinterpret_cast<GraphicsUnmapResources>(m_library.resolve("cuGraphicsUnmapResources"));
        m_mappedArray = reinterpret_cast<GraphicsSubResourceGetMappedArray>(m_library.resolve("cuGraphicsSubResourceGetMappedArray"));
        m_memcpy2D = reinterpret_cast<Memcpy2D>(m_library.resolve("cuMemcpy2D_v2"));

        if (!m_push || !m_pop || !m_register || !m_unregister || !m_map || !m_unmap || !m_mappedArray || !m_memcpy2D) {
            return false;
        }

        m_gl = context->functions();
        m_gl->glGenTextures(2, m_textures);
        return true;
    }

    GLVideoFrame::Type type() const override { return GLVideoFrame::CUDA; }

    bool acquire(const GLVideoFrame& frame, const GLVideoFrame *const *, int, bool, GLuint textures[2], Layout& layout) override
    {
        CUcontext context = static_cast<CUcontext>(frame.m_cudaContext);
        m_acquired = false;

        if (!context || (m_push(context) != 0)) {
            return false;
        }

        bool ok = setup(context, frame.m_cudaContextOwner, frame.m_width, frame.m_height);

        if (ok)
        {
            ok = m_map(2, m_resources, nullptr) == 0;

            if (ok)
            {
                for (int plane = 0; (plane < 2) && ok; plane++)
                {
                    CUarray array = nullptr;
                    ok = m_mappedArray(&array, m_resources[plane], 0, 0) == 0;

                    if (ok)
                    {
                        CUDAMemcpy2D copy = {};
                        copy.srcMemoryType = CU_MEMORYTYPE_DEVICE;
                        copy.srcDevice = frame.m_cudaPlane[plane];
                        copy.srcPitch = frame.m_cudaPitch[plane];
                        copy.dstMemoryType = CU_MEMORYTYPE_ARRAY;
                        copy.dstArray = array;
                        copy.WidthInBytes = plane == 0 ? frame.m_width : 2 * ((frame.m_width + 1) / 2);
                        copy.Height = plane == 0 ? frame.m_height : (frame.m_height + 1) / 2;
                        ok = m_memcpy2D(&copy) == 0;
                    }
                }

                m_unmap(2, m_resources, nullptr);
            }
        }

        CUcontext popped;
        m_pop(&popped);

        if (!ok)
        {
            qWarning("GLVideoInteropCUDA: copy to OpenGL failed");
            return false;
        }

        m_acquired = true;
        textures[0] = m_textures[0];
        textures[1] = m_textures[1];
        layout = LayoutNV12;
        return true;
    }

    // The textures keep the last frame copied
    bool reacquire(GLuint textures[2], Layout& layout) override
    {
        if (!m_acquired) {
            return false;
        }

        textures[0] = m_textures[0];
        textures[1] = m_textures[1];
        layout = LayoutNV12;
        return true;
    }

    void abandon() override
    {
        // The registrations go with the CUDA context, the textures with the OpenGL context
        m_resources[0] = m_resources[1] = nullptr;
        m_textures[0] = m_textures[1] = 0;
        unregister();
    }

private:
    // In the context they were registered in, which is kept alive until they are unregistered
    void unregister()
    {
        if (m_context && (m_resources[0] || m_resources[1]) && (m_push(m_context) == 0))
        {
            for (CUgraphicsResource& resource : m_resources)
            {
                if (resource) {
                    m_unregister(resource);
                }
            }

            CUcontext popped;
            m_pop(&popped);
        }

        m_resources[0] = m_resources[1] = nullptr;
        m_acquired = false;
        m_context = nullptr;
        m_contextOwner.reset();
        m_width = 0;
        m_height = 0;
    }

    // Textures of the frame's size, registered with its context. In the context
    bool setup(CUcontext context, const std::shared_ptr<void>& contextOwner, int width, int height)
    {
        if ((width == m_width) && (height == m_height) && (context == m_context) && m_resources[0] && m_resources[1]) {
            return true;
        }

        unregister();
        m_context = context;
        m_contextOwner = contextOwner;

        const int uvWidth = (width + 1) / 2, uvHeight = (height + 1) / 2;

        for (int plane = 0; plane < 2; plane++)
        {
            m_gl->glBindTexture(GL_TEXTURE_2D, m_textures[plane]);
            m_gl->glTexImage2D(GL_TEXTURE_2D, 0, plane == 0 ? GL_R8 : GL_RG8, plane == 0 ? width : uvWidth,
                plane == 0 ? height : uvHeight, 0, plane == 0 ? GL_RED : GL_RG, GL_UNSIGNED_BYTE, nullptr);
            m_gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            m_gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            m_gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            m_gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            if (m_register(&m_resources[plane], m_textures[plane], GL_TEXTURE_2D, CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD) != 0)
            {
                qWarning("GLVideoInteropCUDA: cuGraphicsGLRegisterImage failed");
                m_resources[plane] = nullptr;
                return false;
            }
        }

        m_gl->glBindTexture(GL_TEXTURE_2D, 0);
        m_width = width;
        m_height = height;
        return true;
    }

    QLibrary m_library;
    CtxPushCurrent m_push = nullptr;
    CtxPopCurrent m_pop = nullptr;
    GraphicsGLRegisterImage m_register = nullptr;
    GraphicsUnregisterResource m_unregister = nullptr;
    GraphicsMapResources m_map = nullptr;
    GraphicsUnmapResources m_unmap = nullptr;
    GraphicsSubResourceGetMappedArray m_mappedArray = nullptr;
    Memcpy2D m_memcpy2D = nullptr;
    QOpenGLFunctions *m_gl;
    CUcontext m_context;                    //!< Of the resources
    std::shared_ptr<void> m_contextOwner;   //!< Keeps m_context alive while resources are registered in it
    int m_width;
    int m_height;
    GLuint m_textures[2];
    CUgraphicsResource m_resources[2];
    bool m_acquired;                        //!< The textures have the last frame acquired
};

} // namespace

std::unique_ptr<GLVideoInterop> createGLVideoInteropCUDA(QOpenGLContext *context)
{
    // Registering textures needs R8 and RG8, so desktop OpenGL 3 or core
    if (context->isOpenGLES() || ((context->format().majorVersion() < 3) && !context->hasExtension("GL_ARB_texture_rg"))) {
        return nullptr;
    }

    std::unique_ptr<GLVideoInteropCUDA> interop(new GLVideoInteropCUDA());

    if (!interop->init(context)) {
        return nullptr;
    }

    return std::unique_ptr<GLVideoInterop>(interop.release());
}

#else

std::unique_ptr<GLVideoInterop> createGLVideoInteropCUDA(QOpenGLContext *)
{
    return nullptr;
}

#endif
