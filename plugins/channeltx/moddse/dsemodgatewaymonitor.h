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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODGATEWAYMONITOR_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODGATEWAYMONITOR_H_

#include <array>

#include <QByteArray>
#include <QString>

#include "dsemodtxsession.h"
#include "dsdvocodermbelib.h"

/** Decode a copy of DMR or D-STAR gateway voice without changing the relayed frames. */
class DSEModGatewayMonitor
{
public:
    static bool isAvailable() { return DSDVocoderMbelib::isAvailable(); }
    void reset() { m_vocoder.reset(); }
    static bool extract(const DSEModTxSession::Frame& frame,
        std::array<DSDVoiceFrame, 3>& voices, int& count, QString& error);
    //! Empty PCM means that the frame carried no speech. PCM is s16le mono at 8 kHz.
    bool decode(const DSEModTxSession::Frame& frame, QByteArray& pcm, QString& error);

private:
    DSDVocoderMbelib m_vocoder;
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODGATEWAYMONITOR_H_
