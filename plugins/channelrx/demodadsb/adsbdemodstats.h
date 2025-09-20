///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_ADSBDEMODSTATS_H
#define INCLUDE_ADSBDEMODSTATS_H

#include <QtCore>

struct ADSBDemodStats {

    qint64 m_correlatorMatches; //!< Total number of correlator matches
    qint64 m_adsbFrames;        //!< How many ADS-B frames with correct CRCs
    qint64 m_modesFrames;       //!< How many non-ADS-B Mode-S frames with correct CRCs
    qint64 m_preambleFails;     //!< How many non-ADS-B Mode S frames with errors in preamble
    qint64 m_crcFails;          //!< How many frames we've demoded with incorrect CRCs
    qint64 m_typeFails;         //!< How many frames we've demoded with unknown type (DF) so we can't check CRC
    qint64 m_invalidFails;      //!< How many frames we've demoded with reserved bits set
    qint64 m_icaoFails;         //!< How many frames we've demoded with reserved ICAO

    ADSBDemodStats()
    {
        reset();
    }

    void reset()
    {
        m_correlatorMatches = 0;
        m_adsbFrames = 0;
        m_modesFrames = 0;
        m_preambleFails = 0;
        m_crcFails = 0;
        m_typeFails = 0;
        m_invalidFails = 0;
        m_icaoFails = 0;
    }

};

#endif // INCLUDE_ADSBDEMODSTATS_H
