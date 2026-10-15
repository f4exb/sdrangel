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

#ifndef MODEMDVB_DVBT2LDPC_H
#define MODEMDVB_DVBT2LDPC_H

#include <cstdint>

// Internal to modemdvb: vectorised LDPC decoders, each lane of a vector decoding a different codeword. Included by
// files compiled for instruction sets the CPU mightn't have, so only raw types here, no library code (see
// dvbt2ldpckernel.h)

namespace dvbt2 {

//! A code's graph and the buffers to decode it with, made by LDPC (dvbt2fec.cpp). The parity check matrix: the
//! variables (codeword bits) of each check, information bits first, then parity. Vectors are of the kernel's lanes
//! bytes, aligned to 64
struct LDPCKernelData
{
    int m_n;                    //!< Variables
    int m_nbChecks;
    const int *m_checkStart;    //!< Index into m_vars of each check's variables, and one past the last
    const int *m_vars;
    const int8_t *m_filler;     //!< m_n of 127: a codeword of zeros, for lanes without a block
    void *m_app;                //!< m_n vectors: each variable's LLR
    void *m_c2v;                //!< A vector for each of m_vars: each edge's message from its check. Needn't be set
    void *m_t;                  //!< The largest check degree's vectors, for the check being updated
    void *m_prefix;             //!< The same
};

//! A decoder for an instruction set
struct LDPCKernel
{
    int m_lanes;    //!< Codewords decoded at once, and the bytes of a vector
    //! LLRs are int8, positive for 0, decoded in place. Returns the iterations used, or maxIterations + 1 if not all
    //! the codewords satisfy the parity checks. Stops early if those that don't seem stuck, with a few errors.
    //! unconverged, if not null: the number of codewords that don't. quickBlocks: a bit for each block not worth more
    //! than a few iterations (probably damaged), given up then
    int (*m_decode)(const LDPCKernelData& data, int8_t *const *blocks, int nbBlocks, int maxIterations,
        int *unconverged, uint64_t quickBlocks);
};

extern const LDPCKernel ldpcKernelGeneric;

#if defined(DVBT2_LDPC_X86)
extern const LDPCKernel ldpcKernelSSE41;
extern const LDPCKernel ldpcKernelAVX2;
#elif defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(_M_ARM64)
#define DVBT2_LDPC_NEON
extern const LDPCKernel ldpcKernelNEON;
#endif

} // namespace dvbt2

#endif // MODEMDVB_DVBT2LDPC_H
