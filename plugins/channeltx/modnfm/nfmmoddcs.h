///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_CHANNELTX_MODNFM_NFMMODDCS_H_
#define PLUGINS_CHANNELTX_MODNFM_NFMMODDCS_H_

class NFMModDCS
{
public:
    NFMModDCS();
    ~NFMModDCS();

    void reset();
    void setDCS(int code);
    void setPositive(bool positive);
    void setSampleRate(int sampleRate);
    int next(); //!< +1/-1 sample

private:
    int m_shift; //!< current frequency shift: -1 or 1
    int m_dcsWord[23]; //!< current DCS word in transmit order including parity and filler 11 + 3 + 9
    float m_step;
    bool m_positive;
    float m_bitPerStep;
    static const float m_codeRate;
};


#endif // PLUGINS_CHANNELTX_MODNFM_NFMMODDCS_H_
