///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software, you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY, without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "ctcssfrequencies.h"

// The 51 tones from various standards (https://en.wikipedia.org/wiki/Continuous_Tone-Coded_Squelch_System)
const float CTCSSFrequencies::m_Freqs[] = {
   67.0, // 0
   69.3,
   71.9,
   74.4,
   77.0,
   79.7,
   82.5,
   85.4,
   88.5,
   91.5,
   94.8, // 10
   97.4,
   100.0,
   103.5,
   107.2,
   110.9,
   114.8,
   118.8,
   123.0,
   127.3,
   131.8, // 20
   136.5,
   141.3,
   146.2,
   150.0,
   151.4,
   156.7,
   159.8,
   162.2,
   165.5,
   167.9, // 30
   171.3,
   173.8,
   177.3,
   179.9,
   183.5,
   186.2,
   189.9,
   192.8,
   196.6,
   199.5, // 40
   203.5,
   206.5,
   210.7,
   218.1,
   225.7,
   229.1,
   233.6,
   241.8,
   250.3,
   254.1, // 50
};

const int CTCSSFrequencies::m_nbFreqs = 51;
