///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
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


#ifndef PLUGINS_CHANNEL_BFM_RDSDEMOD_H_
#define PLUGINS_CHANNEL_BFM_RDSDEMOD_H_

#include "dsp/dsptypes.h"

class RDSDemod
{
public:
	RDSDemod();
	~RDSDemod();

	void process(Real rdsSample, Real pilotPhaseSample);

protected:
	Real filter_lp_2400_iq(Real in, int iqIndex);
	int sign(Real a);
	void biphase(Real acc);
	void print_delta(char b);
	void output_bit(char b);

private:
	Real m_xv[2][2+1];
	Real m_yv[2][2+1];
	Real m_rdsBB;
	Real m_rdsBB_1;
	Real m_rdsClockPhase;
	Real m_rdsClockOffset;
	Real m_rdsClockLO;
	Real m_rdsClockLO_1;
	int m_numSamples;
	Real m_acc;
	Real m_acc_1;
	int m_counter;
	int m_readingFrame;
	int m_totErrors[2];
	int m_dbit;
};

#endif /* PLUGINS_CHANNEL_BFM_RDSDEMOD_H_ */
