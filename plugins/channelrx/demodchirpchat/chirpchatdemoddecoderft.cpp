///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#include "chirpchatdemodsettings.h"
#include "chirpchatdemoddecoderft.h"

#ifndef HAS_FT8
void ChirpChatDemodDecoderFT::decodeSymbols(
    const std::vector<std::vector<float>>& mags, // vector of symbols magnitudes
    int nbSymbolBits, //!< number of bits per symbol
    QString& msg,     //!< formatted message
    QString& call1,   //!< 1st callsign or shorthand
    QString& call2,   //!< 2nd callsign
    QString& loc,     //!< locator, report or shorthand
    bool& reply       //!< true if message is a reply report
)
{
    qWarning("ChirpChatDemodDecoderFT::decodeSymbols: not implemented");
}
#else

#include "ft8.h"
#include "packing.h"

void ChirpChatDemodDecoderFT::decodeSymbols(
    const std::vector<std::vector<float>>& mags, // vector of symbols magnitudes
    int nbSymbolBits, //!< number of bits per symbol
    std::string& msg,     //!< formatted message
    std::string& call1,   //!< 1st callsign or shorthand
    std::string& call2,   //!< 2nd callsign
    std::string& loc,     //!< locator, report or shorthand
    bool& reply,          //!< true if message is a reply report
    int& payloadParityStatus,
    bool& payloadCRCStatus
)
{
    if (mags.size()*nbSymbolBits < 174)
    {
        qWarning("ChirpChatDemodDecoderFT::decodeSymbols: insufficient number of symbols for FT payload");
        return;
    }

    FT8::FT8Params params;
    int r174[174];
    std::string comments;
    payloadParityStatus = (int) ChirpChatDemodSettings::ParityOK;
    payloadCRCStatus = false;
    std::vector<std::vector<float>> magsp = mags;

    qDebug("ChirpChatDemodDecoderFT::decodeSymbols: try decode with symbol shift 0");
    int res = decodeWithShift(params, magsp, nbSymbolBits, r174, comments);

    if (res == 0)
    {
        std::vector<std::vector<float>> magsn = mags;
        int shiftcount = 0;

        while ((res == 0) && (shiftcount < 7))
        {
            qDebug("ChirpChatDemodDecoderFT::decodeSymbols: try decode with symbol shift %d", shiftcount + 1);
            res = decodeWithShift(params, magsp, nbSymbolBits, r174, comments, 1);

            if (res == 0)
            {
                qDebug("ChirpChatDemodDecoderFT::decodeSymbols: try decode with symbol shift -%d", shiftcount + 1);
                res = decodeWithShift(params, magsn, nbSymbolBits, r174, comments, -1);
            }

            shiftcount++;
        }
    }

    if (res == 0)
    {
        if (comments == "LDPC fail")
        {
            qWarning("ChirpChatDemodDecoderFT::decodeSymbols: LDPC failed");
            payloadParityStatus = (int) ChirpChatDemodSettings::ParityError;
        }
        else if (comments == "OSD fail")
        {
            qWarning("ChirpChatDemodDecoderFT::decodeSymbols: OSD failed");
            payloadParityStatus = (int) ChirpChatDemodSettings::ParityError;
        }
        else if (comments == "CRC fail")
        {
            qWarning("ChirpChatDemodDecoderFT::decodeSymbols: CRC failed");
        }
        else
        {
            qWarning("ChirpChatDemodDecoderFT::decodeSymbols: decode failed for unknown reason");
            payloadParityStatus = (int) ChirpChatDemodSettings::ParityUndefined;
        }

        return;
    }

    payloadCRCStatus = true;
    FT8::Packing packing;
    std::string msgType;
    msg = packing.unpack(r174, call1, call2, loc, msgType);
    reply = false;

    if ((msgType == "0.3") || (msgType == "0.3")) {
        reply = r174[56] != 0;
    }
    if ((msgType == "1") || (msgType == "2")) {
        reply = r174[58] != 0;
    }
    if ((msgType == "3")) {
        reply = r174[57] != 0;
    }
    if ((msgType == "5")) {
        reply = r174[34] != 0;
    }
}

int ChirpChatDemodDecoderFT::decodeWithShift(
    FT8::FT8Params& params,
    std::vector<std::vector<float>>& mags,
    int nbSymbolBits,
    int *r174,
    std::string& comments,
    int shift
)
{
    if (shift > 0)
    {
        for (unsigned int si = 0; si < mags.size(); si++)
        {
            for (int bini = (1<<nbSymbolBits) - 1; bini > 0; bini--)
            {
                float x = mags[si][bini - 1];
                mags[si][bini - 1] = mags[si][bini];
                mags[si][bini] = x;
            }
        }
    }

    if (shift < 0)
    {
        for (unsigned int si = 0; si < mags.size(); si++)
        {
            for (int bini = 0; bini < (1<<nbSymbolBits) - 1; bini++)
            {
                float x = mags[si][bini + 1];
                mags[si][bini + 1] = mags[si][bini];
                mags[si][bini] = x;
            }
        }
    }

    float *lls = new float[mags.size()*nbSymbolBits]; // bits log likelihoods (>0 for 0, <0 for 1)
    std::fill(lls, lls+mags.size()*nbSymbolBits, 0.0);
    FT8::FT8::soft_decode_mags(params, mags, nbSymbolBits, lls);
    deinterleave174(lls);
    int ret = FT8::FT8::decode(lls, r174, params, 0, comments);
    delete[] lls;
    return ret;
}

void ChirpChatDemodDecoderFT::deinterleave174(float ll174[])
{
    // 174 = 2*3*29
    float t174[174];
    std::copy(ll174, ll174+174, t174);

    for (int i = 0; i < 174; i++) {
        ll174[i] = t174[(i%6)*29 + (i%29)];
    }
}

#endif // HAS_FT8
