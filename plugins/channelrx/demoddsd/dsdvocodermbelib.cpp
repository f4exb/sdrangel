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

#include <algorithm>
#include <cmath>
#include <cstring>

#ifdef DSDDEMOD_HAS_MBELIB
#include <mbevocoder/mbevocoder.h>
#endif

#include "dsp/dv/dvambe.h"
#include "dsp/dv/dvdtmf.h"

#include "dsdvocodermbelib.h"

#ifdef DSDDEMOD_HAS_MBELIB

struct DSDVocoderMbelib::State
{
    mbe_parms m_current;
    mbe_parms m_previous;
    mbe_parms m_previousEnhanced;
};

DSDVocoderMbelib::DSDVocoderMbelib() :
    m_state(new State())
{
    reset();
}

DSDVocoderMbelib::~DSDVocoderMbelib()
{
    delete m_state;
}

bool DSDVocoderMbelib::isAvailable()
{
    return true;
}

const char *DSDVocoderMbelib::getVersion()
{
    return mbe_versionString();
}

void DSDVocoderMbelib::reset()
{
    mbe_initMbeParms(&m_state->m_current, &m_state->m_previous, &m_state->m_previousEnhanced);
}

//! Bit i of the frame's bytes: LSB first for D-STAR, MSB first for the others
static int hardBit(const DSDVoiceFrame& frame, int i, bool lsbFirst)
{
    return (frame.m_bytes[i / 8] >> (lsbFirst ? (i % 8) : (7 - i % 8))) & 1;
}

static mbe_soft_bit softBit(int bit, float llr)
{
    // The hard decision from the frame's bytes, the LLR's magnitude the reliability (saturating at
    // 255). The LLR alone rounded to an integer made every |LLR| < 0.5 a 0 bit whatever its sign:
    // D-STAR's weak bits were flipped before the decoder saw them
    return mbe_softBitFromHard(bit, (uint8_t) std::min(255.0f, std::fabs(llr)));
}

bool DSDVocoderMbelib::decode(const DSDVoiceFrame& frame, float *audio, Result& result)
{
    mbe_process_result r;
    mbe_initProcessResult(&r);
    result = Result();
    int status = -1;

    switch (frame.m_rate)
    {
    case DSDVoiceFrame::RateAMBE3600x2400:
    {
        // D-STAR: the bytes hold the bits LSB first, the LLRs follow the MSB first numbering
        mbe_soft_bit bits[72];

        for (int i = 0; i < 72; i++) {
            bits[i] = softBit(hardBit(frame, i, true), frame.m_llr[8 * (i / 8) + 7 - (i % 8)]);
        }

        mbe_soft_bit fr[4][24];
        mbe_ambeSoftFrameFromBits(MBE_LAYOUT_DSTAR, bits, fr);
        char d[49];
        status = mbe_processAmbe3600x2400SoftFramef(audio, &r, fr, d, &m_state->m_current, &m_state->m_previous, &m_state->m_previousEnhanced);
        result.m_protectedBits = 47;
        break;
    }
    case DSDVoiceFrame::RateAMBE3600x2450:
    {
        mbe_soft_bit bits[72];

        for (int i = 0; i < 72; i++) {
            bits[i] = softBit(hardBit(frame, i, false), frame.m_llr[i]);
        }

        mbe_soft_bit fr[4][24];
        mbe_ambeSoftFrameFromBits(MBE_LAYOUT_DMR, bits, fr);
        char d[49];
        status = mbe_processAmbe3600x2450SoftFramef(audio, &r, fr, d, &m_state->m_current, &m_state->m_previous, &m_state->m_previousEnhanced);
        result.m_protectedBits = 47;
        break;
    }
    case DSDVoiceFrame::RateAMBE2450:
    {
        // YSF V/D type 2: the 49 parameter bits as a DVSI raw word, the protocol's own code already used
        char d[49];
        mbe_dvsiToAmbe2450Data(frame.m_bytes, d);
        status = mbe_processAmbe2450Dataf(audio, &r, d, &m_state->m_current, &m_state->m_previous, &m_state->m_previousEnhanced);
        break;
    }
    case DSDVoiceFrame::RateIMBE7200x4400:
    {
        mbe_soft_bit bits[144];

        for (int i = 0; i < 144; i++) {
            bits[i] = softBit(hardBit(frame, i, false), frame.m_llr[i]);
        }

        mbe_soft_bit fr[8][23];
        mbe_imbeSoftFrameFromBits(MBE_LAYOUT_P25P1, bits, fr);
        char d[88];
        status = mbe_processImbe7200x4400SoftFramef(audio, &r, fr, d, &m_state->m_current, &m_state->m_previous, &m_state->m_previousEnhanced);
        result.m_protectedBits = 137;
        break;
    }
    default:
        break;
    }

    if (status < 0)
    {
        std::fill(audio, audio + m_samplesPerFrame, 0.0f);
        return false;
    }

    result.m_correctedBits = std::max(0, r.total_errors);
    result.m_muted = (r.flags & (MBE_PROCESS_FLAG_MUTE | MBE_PROCESS_FLAG_REPEAT | MBE_PROCESS_FLAG_ERASURE)) != 0;
    result.m_tone = (r.flags & MBE_PROCESS_FLAG_TONE) != 0;
    return true;
}

bool DSDVocoderMbelib::channelErrors(const DSDVoiceFrame& frame, int& errors, int& bits)
{
    // Decode hard, re-encode the parameters and compare with what was received: every bit the
    // codes corrected differs, and nothing else can (the unprotected bits are the received ones).
    // mbelib's own correction counts undercount - by about 40% against the harness's known
    // channel errors - and its later blocks are not corrected in place to compare against. An
    // uncorrectable C0 counts as 4 errors, the least it takes
    switch (frame.m_rate)
    {
    case DSDVoiceFrame::RateAMBE3600x2400:
    case DSDVoiceFrame::RateAMBE3600x2450:
    {
        const bool dstar = frame.m_rate == DSDVoiceFrame::RateAMBE3600x2400;
        const mbe_layout layout = dstar ? MBE_LAYOUT_DSTAR : MBE_LAYOUT_DMR;
        char hard[72], fr[4][24], d[49], again[72];

        for (int i = 0; i < 72; i++) {
            hard[i] = (char) hardBit(frame, i, dstar);
        }

        mbe_ambeFrameFromBits(layout, hard, fr);
        int status = dstar ? mbe_decodeAmbe3600x2400Frame(fr, d, nullptr) : mbe_decodeAmbe3600x2450Frame(fr, d, nullptr);
        bits = 47;

        if (status < 0)
        {
            errors = 4;
            return true;
        }

        dstar ? mbe_encodeAmbe3600x2400Frame(d, fr) : mbe_encodeAmbe3600x2450Frame(d, fr);
        mbe_ambeFrameToBits(layout, fr, again);
        errors = 0;

        for (int i = 0; i < 72; i++) {
            errors += (hard[i] & 1) != (again[i] & 1);
        }

        return true;
    }
    case DSDVoiceFrame::RateIMBE7200x4400:
    {
        char hard[144], fr[8][23], d[88], again[144];

        for (int i = 0; i < 144; i++) {
            hard[i] = (char) hardBit(frame, i, false);
        }

        mbe_imbeFrameFromBits(MBE_LAYOUT_P25P1, hard, fr);
        int status = mbe_decodeImbe7200x4400Frame(fr, d, nullptr);
        bits = 137;

        if (status < 0)
        {
            errors = 4;
            return true;
        }

        mbe_encodeImbe7200x4400Frame(d, fr);
        mbe_imbeFrameToBits(MBE_LAYOUT_P25P1, fr, again);
        errors = 0;

        for (int i = 0; i < 144; i++) {
            errors += (hard[i] & 1) != (again[i] & 1);
        }

        return true;
    }
    default:
        return false;
    }
}

char DSDVocoderMbelib::dtmfKey(const DSDVoiceFrame& frame)
{
    char d[49];
    uint8_t params[49];
    int toneId = -1;

    switch (frame.m_rate)
    {
    case DSDVoiceFrame::RateAMBE3600x2400:
    case DSDVoiceFrame::RateAMBE3600x2450:
    {
        const bool dstar = frame.m_rate == DSDVoiceFrame::RateAMBE3600x2400;
        char hard[72], fr[4][24];

        for (int i = 0; i < 72; i++) {
            hard[i] = (char) hardBit(frame, i, dstar);
        }

        mbe_ambeFrameFromBits(dstar ? MBE_LAYOUT_DSTAR : MBE_LAYOUT_DMR, hard, fr);
        mbe_process_result r;
        mbe_initProcessResult(&r);
        const int total = dstar ? mbe_decodeAmbe3600x2400Frame(fr, d, &r) : mbe_decodeAmbe3600x2450Frame(fr, d, &r);

        if ((total < 0) || (dstar ? ((r.c0_errors >= 2) || (total >= 3)) : (total >= 6))) {
            return 0;
        }

        for (int i = 0; i < 49; i++) {
            params[i] = (uint8_t) (d[i] & 1);
        }

        toneId = dstar ? DVDTMF::ambe2400ToneId(params) : DVDTMF::ambe2450ToneId(params);
        break;
    }
    case DSDVoiceFrame::RateAMBE2450:
        // YSF V/D type 2: the parameters as a DVSI raw word, the protocol's own code already used
        mbe_dvsiToAmbe2450Data(frame.m_bytes, d);

        for (int i = 0; i < 49; i++) {
            params[i] = (uint8_t) (d[i] & 1);
        }

        toneId = DVDTMF::ambe2450ToneId(params);
        break;
    default:
        return 0;
    }

    return DVDTMF::toneIdKey(toneId);
}

#else

struct DSDVocoderMbelib::State {};

DSDVocoderMbelib::DSDVocoderMbelib() : m_state(nullptr) {}
DSDVocoderMbelib::~DSDVocoderMbelib() {}
bool DSDVocoderMbelib::isAvailable() { return false; }
const char *DSDVocoderMbelib::getVersion() { return "none"; }
void DSDVocoderMbelib::reset() {}

bool DSDVocoderMbelib::decode(const DSDVoiceFrame& frame, float *audio, Result& result)
{
    (void) frame;
    result = Result();
    std::fill(audio, audio + m_samplesPerFrame, 0.0f);
    return false;
}

char DSDVocoderMbelib::dtmfKey(const DSDVoiceFrame& frame)
{
    // AMBE+2 through sdrbase's Golay decoders; D-STAR's needs mbelib's
    uint8_t params[49];

    if (frame.m_rate == DSDVoiceFrame::RateAMBE3600x2450)
    {
        const int corrected = DVAMBE::bytesToParams(frame.m_bytes, params);
        return (corrected >= 0) && (corrected < 6) ? DVDTMF::toneIdKey(DVDTMF::ambe2450ToneId(params)) : 0;
    }

    if (frame.m_rate == DSDVoiceFrame::RateAMBE2450)
    {
        DVAMBE::dvsiToParams(frame.m_bytes, params);
        return DVDTMF::toneIdKey(DVDTMF::ambe2450ToneId(params));
    }

    return 0;
}

bool DSDVocoderMbelib::channelErrors(const DSDVoiceFrame& frame, int& errors, int& bits)
{
    // AMBE+2 through sdrbase's Golay decoders; the others need mbelib's
    if (frame.m_rate != DSDVoiceFrame::RateAMBE3600x2450) {
        return false;
    }

    uint8_t params[49];
    int corrected = DVAMBE::bytesToParams(frame.m_bytes, params);
    errors = corrected >= 0 ? corrected : 4;
    bits = 47;
    return true;
}

#endif
