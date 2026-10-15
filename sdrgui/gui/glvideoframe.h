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

#ifndef SDRGUI_GUI_GLVIDEOFRAME_H_
#define SDRGUI_GUI_GLVIDEOFRAME_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

// A video frame for GLVideoWidget / GLVideoRenderer: YUV 4:2:0 in memory, or on the GPU as a native handle, which
// the renderer imports into OpenGL without copying it through the CPU. Whatever keeps the frame's memory valid (e.g.
// a decoder's frame reference) is held by a subclass, and released when the last pointer to the frame goes. No
// dependency on any decoder, so usable by any video plugin
// An image drawn over the picture, such as subtitles: premultiplied RGBA pixels (m_width x m_height), at (m_x, m_y)
// of a canvas (m_canvasWidth x m_canvasHeight, e.g. a DVB subtitle display) that is stretched over the picture
struct GLVideoSubtitle
{
    int m_x = 0;
    int m_y = 0;
    int m_width = 0;
    int m_height = 0;
    int m_canvasWidth = 720;
    int m_canvasHeight = 576;
    std::vector<uint8_t> m_rgba;
};

typedef std::shared_ptr<const GLVideoSubtitle> GLVideoSubtitlePtr;

class GLVideoFrame
{
public:
    enum Type
    {
        Planar420,      //!< Y, U, V planes in memory (e.g. AV_PIX_FMT_YUV420P)
        NV12,           //!< Y, interleaved UV planes in memory
        D3D11,          //!< NV12 texture array slice of a D3D11 device (Windows, D3D11VA)
        DMABuf,         //!< NV12 as DMA-BUF planes (Linux, VAAPI)
        IOSurface,      //!< NV12 IOSurface (macOS, VideoToolbox)
        CUDA,           //!< NV12 in CUDA device memory (NVIDIA)
        NbTypes
    };

    enum Matrix { BT601, BT709, BT2020 };   //!< BT2020 non-constant luminance

    //! Where 4:2:0 chroma samples are, relative to luma: left is co-sited with even columns, between rows (MPEG-2,
    //! H.264 and HEVC's default), centre between columns too (MPEG-1, JPEG), top left co-sited with even rows too
    enum ChromaLocation { ChromaLeft, ChromaCenter, ChromaTopLeft };

    virtual ~GLVideoFrame() {}

    Type m_type = Planar420;
    int m_width = 0;
    int m_height = 0;
    double m_sampleAspectRatio = 1.0;   //!< Pixel width / height: displayed width is m_width * this
    Matrix m_matrix = BT601;
    ChromaLocation m_chromaLocation = ChromaLeft;   //!< Not used for D3D11 frames, as the video processor converts them
    bool m_fullRange = false;           //!< Else limited (16 to 235)
    bool m_interlaced = false;          //!< For deinterlacing by the renderer, which only D3D11 frames need
    bool m_topFieldFirst = true;
    double m_timestamp = 0.0;           //!< Seconds, for information
    GLVideoSubtitlePtr m_subtitle;      //!< Drawn over the picture, if set. Frames share it while it's shown

    // Planar420 and NV12 in memory
    const uint8_t *m_data[3] = {};
    int m_linesize[3] = {};

    // D3D11: a slice of an NV12 texture array, of a device whose immediate context the decoder uses too, so it's
    // used between m_lock() and m_unlock()
    void *m_d3d11Device = nullptr;      //!< ID3D11Device *
    void *m_d3d11Texture = nullptr;     //!< ID3D11Texture2D *
    int m_d3d11Index = 0;
    std::function<void()> m_lock;
    std::function<void()> m_unlock;

    // DMABuf: Y (DRM_FORMAT_R8) and UV (DRM_FORMAT_GR88) planes
    int m_dmaBufFd[2] = {-1, -1};
    int m_dmaBufOffset[2] = {};
    int m_dmaBufPitch[2] = {};
    uint32_t m_dmaBufFourcc[2] = {};
    uint64_t m_dmaBufModifier[2] = {};
    bool m_dmaBufHasModifier = false;
    // Which surface (as a VASurfaceID) of which pool (as a decoder's frames context) the planes are, so the display
    // can keep them imported for when the surface comes again, rather than import them for each frame (their file
    // descriptors differ each time). And what keeps the pool alive (so its address can't be reused for another) while
    // the display keeps them. m_dmaBufPool null if not known: then imported for each frame
    uint64_t m_dmaBufSurface = 0;
    const void *m_dmaBufPool = nullptr;
    std::shared_ptr<void> m_dmaBufPoolOwner;

    // IOSurface (IOSurfaceRef), NV12 (biplanar)
    void *m_ioSurface = nullptr;

    // CUDA: Y and UV device pointers (CUdeviceptr), in a context (CUcontext), and what keeps the context alive (e.g. a
    // reference to the decoder's device), for the display to hold while it has textures registered in the context, which
    // would otherwise be destroyed with the decoder and its last frame
    uint64_t m_cudaPlane[2] = {};
    int m_cudaPitch[2] = {};
    void *m_cudaContext = nullptr;
    std::shared_ptr<void> m_cudaContextOwner;

    bool isGPU() const { return (m_type != Planar420) && (m_type != NV12); }
};

typedef std::shared_ptr<GLVideoFrame> GLVideoFramePtr;

#endif // SDRGUI_GUI_GLVIDEOFRAME_H_
