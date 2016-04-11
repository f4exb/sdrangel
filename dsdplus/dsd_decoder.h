///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef DSDPLUS_DSD_DECODER_H_
#define DSDPLUS_DSD_DECODER_H_

#include "dsd_opts.h"
#include "dsd_state.h"
#include "dsd_filters.h"
#include "dsd_mbe.h"
#include "dmr_voice.h"
#include "dmr_data.h"
#include "dstar.h"

/*
 * Frame sync patterns
 */
#define INV_P25P1_SYNC "333331331133111131311111"
#define P25P1_SYNC     "111113113311333313133333"

#define X2TDMA_BS_VOICE_SYNC "113131333331313331113311"
#define X2TDMA_BS_DATA_SYNC  "331313111113131113331133"
#define X2TDMA_MS_DATA_SYNC  "313113333111111133333313"
#define X2TDMA_MS_VOICE_SYNC "131331111333333311111131"

#define DSTAR_HD       "131313131333133113131111"
#define INV_DSTAR_HD   "313131313111311331313333"
#define DSTAR_SYNC     "313131313133131113313111"
#define INV_DSTAR_SYNC "131313131311313331131333"

#define NXDN_MS_DATA_SYNC      "313133113131111333"
#define INV_NXDN_MS_DATA_SYNC  "131311331313333111"
#define NXDN_MS_VOICE_SYNC     "313133113131113133"
#define INV_NXDN_MS_VOICE_SYNC "131311331313331311"
#define INV_NXDN_BS_DATA_SYNC  "131311331313333131"
#define NXDN_BS_DATA_SYNC      "313133113131111313"
#define INV_NXDN_BS_VOICE_SYNC "131311331313331331"
#define NXDN_BS_VOICE_SYNC     "313133113131113113"

#define DMR_BS_DATA_SYNC  "313333111331131131331131"
#define DMR_BS_VOICE_SYNC "131111333113313313113313"
#define DMR_MS_DATA_SYNC  "311131133313133331131113"
#define DMR_MS_VOICE_SYNC "133313311131311113313331"

#define INV_PROVOICE_SYNC    "31313111333133133311331133113311"
#define PROVOICE_SYNC        "13131333111311311133113311331133"
#define INV_PROVOICE_EA_SYNC "13313133113113333311313133133311"
#define PROVOICE_EA_SYNC     "31131311331331111133131311311133"

namespace DSDplus
{

class DSDDecoder
{
    friend class DSDMBEDecoder;
    friend class DSDDMRVoice;
    friend class DSDDMRData;
    friend class DSDDstar;
public:
    typedef enum
    {
        DSDLookForSync,
        DSDNoSync,
        DSDprocessFrame,
        DSDprocessNXDNVoice,
        DSDprocessNXDNData,
        DSDprocessDSTAR,
        DSDprocessDSTAR_HD,
        DSDprocessDMRvoice,
        DSDprocessDMRdata,
        DSDprocessX2TDMAvoice,
        DSDprocessX2TDMAdata,
        DSDprocessProVoice,
        DSDprocessUnknown
    } DSDFSMState;

    DSDDecoder();
    ~DSDDecoder();

    void run(short sample);

    short *getAudio(int& nbSamples)
    {
        nbSamples = m_state.audio_out_nb_samples;
        return m_state.audio_out_buf;
    }

    void resetAudio()
    {
        m_state.audio_out_nb_samples = 0;
        m_state.audio_out_buf_p = m_state.audio_out_buf;
    }

    DSDOpts *getOpts() { return &m_opts; }
    DSDState *getState() { return &m_state; }

private:
    bool pushSample(short sample, int have_sync); //!< push a new sample into the decoder. Returns true if a new symbol is available
    int getDibit(); //!< get dibit from the last retrieved symbol. Returns the dibit as its dibit value: 0,1,2,3
    int getFrameSync();
    void resetSymbol();
    void resetFrameSync();
    void printFrameSync(const char *frametype, int offset, char *modulation);
    void noCarrier();
    void printFrameInfo();
    void processFrame();
    static int comp(const void *a, const void *b);

    DSDOpts m_opts;
    DSDState m_state;
    DSDFSMState m_fsmState;
    // symbol engine
    int m_symbol;      //!< the last retrieved symbol
    int m_sampleIndex; //!< the current sample index for the symbol in progress
    int m_sum;
    int m_count;
    // sync engine:
    int m_sync; //!< The current sync type
    int m_dibit, m_synctest_pos, m_lastt;
    char m_synctest[25];
    char m_synctest18[19];
    char m_synctest32[33];
    char m_modulation[8];
    char *m_synctest_p;
    char m_synctest_buf[10240];
    int m_lmin, m_lmax, m_lidx;
    int m_lbuf[24], m_lbuf2[24];
    int m_lsum;
    char m_spectrum[64];
    int m_t;
    int m_hasSync; //!< tells whether we are in synced phase
    // Other
    DSDFilters m_dsdFilters;
    // MBE decoder
    DSDMBEDecoder m_mbeDecoder;
    // Frame decoders
    DSDDMRVoice m_dsdDMRVoice;
    DSDDMRData m_dsdDMRData;
    DSDDstar m_dsdDstar;
};

} // namespace dsdplus

#endif /* DSDPLUS_DSD_DECODER_H_ */
