///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef PLUGINS_CHANNELRX_DEMODDSD_DSDDEMODBAUDRATES_H_
#define PLUGINS_CHANNELRX_DEMODDSD_DSDDEMODBAUDRATES_H_

class DSDDemodBaudRates
{
public:
    static unsigned int getRate(unsigned int rate_index);
    static unsigned int getRateIndex(unsigned int rate);
    static unsigned int getDefaultRate() { return m_rates[m_defaultRateIndex]; }
    static unsigned int getDefaultRateIndex() { return m_defaultRateIndex; }
    static unsigned int getNbRates() { return m_nb_rates; }
private:
    static unsigned int m_nb_rates;
    static unsigned int m_rates[2];
    static unsigned int m_defaultRateIndex;
};

#endif /* PLUGINS_CHANNELRX_DEMODDSD_DSDDEMODBAUDRATES_H_ */
