///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include <QDebug>
#include <QMessageBox>

#include "sdrplaygui.h"

#include <device/devicesourceapi.h>
#include <dsp/filerecord.h>

#include "ui_sdrplaygui.h"
#include "gui/colormapper.h"
#include "gui/glspectrum.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"

// ====================================================================

unsigned int SDRPlayGui::m_bbGr[m_nbBands][m_nbGRValues] = {
        { // 10k - 12M
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, //   0 -   9
               10, 11, 12, 13, 14, 15, 16, 17, 18, 19, //  10 -  19
               20, 21, 22, 23, 24, 25, 26, 27, 28, 29, //  20 -  29
               30, 31, 32, 33, 34, 11, 12, 13, 14, 15, //  30 -  39
               16, 17, 18, 19, 20, 21, 22, 23, 24, 25, //  40 -  49
               26, 27, 28, 29, 30, 31, 32, 33, 34, 35, //  50 -  59
               36, 37, 38, 39, 40, 41, 42, 43, 44, 45, //  60 -  69
               46, 47, 48, 49, 50, 51, 52, 53, 54, 55, //  70 -  79
               56, 57, 58, 40, 41, 42, 43, 44, 45, 46, //  80 -  89
               47, 48, 49, 50, 51, 52, 53, 54, 55, 56, //  90 -  99
               57, 58, 59,                             // 100 - 102
        },
        { // 12 - 30M
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, //   0 -   9
               10, 11, 12, 13, 14, 15, 16, 17, 18, 19, //  10 -  19
               20, 21, 22, 23, 24, 25, 26, 27, 28, 29, //  20 -  29
               30, 31, 32, 33, 34, 11, 12, 13, 14, 15, //  30 -  39
               16, 17, 18, 19, 20, 21, 22, 23, 24, 25, //  40 -  49
               26, 27, 28, 29, 30, 31, 32, 33, 34, 35, //  50 -  59
               36, 37, 38, 39, 40, 41, 42, 43, 44, 45, //  60 -  69
               46, 47, 48, 49, 50, 51, 52, 53, 54, 55, //  70 -  79
               56, 57, 58, 40, 41, 42, 43, 44, 45, 46, //  80 -  89
               47, 48, 49, 50, 51, 52, 53, 54, 55, 56, //  90 -  99
               57, 58, 59,                             // 100 - 102
        },
        { // 30 - 60M
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, //   0 -   9
               10, 11, 12, 13, 14, 15, 16, 17, 18, 19, //  10 -  19
               20, 21, 22, 23, 24, 25, 26, 27, 28, 29, //  20 -  29
               30, 31, 32, 33, 34, 11, 12, 13, 14, 15, //  30 -  39
               16, 17, 18, 19, 20, 21, 22, 23, 24, 25, //  40 -  49
               26, 27, 28, 29, 30, 31, 32, 33, 34, 35, //  50 -  59
               36, 37, 38, 39, 40, 41, 42, 43, 44, 45, //  60 -  69
               46, 47, 48, 49, 50, 51, 52, 53, 54, 55, //  70 -  79
               56, 57, 58, 40, 41, 42, 43, 44, 45, 46, //  80 -  89
               47, 48, 49, 50, 51, 52, 53, 54, 55, 56, //  90 -  99
               57, 58, 59,                             // 100 - 102
        },
        { // 60 - 120M
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, //   0 -   9
               10, 11, 12, 13, 14, 15, 16, 17, 18, 19, //  10 -  19
               20, 21, 22, 23, 24, 25, 26, 27, 28,  5, //  20 -  29
                6,  7,  8,  9, 10, 11, 12, 13, 14, 15, //  30 -  39
               16, 17, 18, 19, 20, 21, 22, 23, 24, 25, //  40 -  49
               26, 27, 28, 29, 30, 31, 32, 33, 34, 35, //  50 -  59
               36, 37, 38, 39, 40, 41, 42, 43, 44, 45, //  60 -  69
               46, 47, 48, 49, 50, 51, 52, 53, 54, 55, //  70 -  79
               56, 57, 58, 40, 41, 42, 43, 44, 45, 46, //  80 -  89
               47, 48, 49, 50, 51, 52, 53, 54, 55, 56, //  90 -  99
               57, 58, 59,                             // 100 - 102
        },
        { // 120 - 250M
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, //   0 -   9
               10, 11, 12, 13, 14, 15, 16, 17, 18, 19, //  10 -  19
               20, 21, 22, 23, 24, 25, 26, 27, 28,  5, //  20 -  29
                6,  7,  8,  9, 10, 11, 12, 13, 14, 15, //  30 -  39
               16, 17, 18, 19, 20, 21, 22, 23, 24, 25, //  40 -  49
               26, 27, 28, 29, 30, 31, 32, 33, 34, 35, //  50 -  59
               36, 37, 38, 39, 40, 41, 42, 43, 44, 45, //  60 -  69
               46, 47, 48, 49, 50, 51, 52, 53, 54, 55, //  70 -  79
               56, 57, 58, 40, 41, 42, 43, 44, 45, 46, //  80 -  89
               47, 48, 49, 50, 51, 52, 53, 54, 55, 56, //  90 -  99
               57, 58, 59,                             // 100 - 102
        },
        { // 250 - 420M
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, //   0 -   9
               10, 11, 12, 13, 14, 15, 16, 17, 18, 19, //  10 -  19
               20, 21, 22, 23, 24, 25, 26, 27, 28, 29, //  20 -  29
               30, 31, 32, 33, 34, 11, 12, 13, 14, 15, //  30 -  39
               16, 17, 18, 19, 20, 21, 22, 23, 24, 25, //  40 -  49
               26, 27, 28, 29, 30, 31, 32, 33, 34, 35, //  50 -  59
               36, 37, 38, 39, 40, 41, 42, 43, 44, 45, //  60 -  69
               46, 47, 48, 49, 50, 51, 52, 53, 54, 55, //  70 -  79
               56, 57, 58, 40, 41, 42, 43, 44, 45, 46, //  80 -  89
               47, 48, 49, 50, 51, 52, 53, 54, 55, 56, //  90 -  99
               57, 58, 59,                             // 100 - 102
        },
        { // 420M - 1G
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, //   0 -   9
               10, 11,  5,  6,  7,  8,  9, 10, 11, 12, //  10 -  19
               13, 14, 15, 16, 17, 18, 19, 20, 21, 22, //  20 -  29
               23, 24, 25, 26, 27, 28, 29, 30, 31, 32, //  30 -  39
               33, 34, 35, 36, 37, 38, 39, 40, 41, 42, //  40 -  49
               43, 44, 45, 46, 47, 48, 49, 50, 51, 52, //  50 -  59
               53, 54, 55, 56, 57, 58, 40, 41, 42, 43, //  60 -  69
               44, 45, 46, 47, 48, 49, 50, 51, 52, 53, //  70 -  79
               54, 55, 56, 57, 58, 59, 59, 59, 59, 59, //  80 -  89
               59, 59, 59, 59, 59, 59, 59, 59, 59, 59, //  90 -  99
               59, 59, 59,                             // 100 - 102
        },
        { // 1 - 2G
                0,  1,  2,  3,  4,  5,  6,  7,  8,  9, //   0 -   9
               10, 11,  5,  6,  7,  8,  9, 10, 11, 12, //  10 -  19
               13, 14, 15, 16, 17, 18, 19, 20, 21, 22, //  20 -  29
               23, 24, 25, 26, 27, 28, 29, 30, 31, 32, //  30 -  39
               33, 34, 35, 36, 37, 38, 39, 40, 41, 42, //  40 -  49
               43, 44, 45, 46, 47, 48, 49, 50, 51, 52, //  50 -  59
               53, 54, 55, 56, 57, 58, 40, 41, 42, 43, //  60 -  69
               44, 45, 46, 47, 48, 49, 50, 51, 52, 53, //  70 -  79
               54, 55, 56, 57, 58, 59, 59, 59, 59, 59, //  80 -  89
               59, 59, 59, 59, 59, 59, 59, 59, 59, 59, //  90 -  99
               59, 59, 59,                             // 100 - 102
        }
};

unsigned int SDRPlayGui::m_lnaGr[m_nbBands][m_nbGRValues] = {
        { // 10k - 12M
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //   0 -   9
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  10 -  19
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  20 -  29
                0,  0,  0,  0,  0, 24, 24, 24, 24, 24, //  30 -  39
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  40 -  49
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  50 -  59
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  60 -  69
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  70 -  79
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  80 -  89
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  90 -  99
               24, 24, 24,                             // 100 - 102
        },
        { // 12 - 30M
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //   0 -   9
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  10 -  19
                 0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  20 -  29
                 0,  0,  0,  0,  0, 24, 24, 24, 24, 24, //  30 -  39
                24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  40 -  49
                24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  50 -  59
                24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  60 -  69
                24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  70 -  79
                24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  80 -  89
                24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  90 -  99
                24, 24, 24,                             // 100 - 102
        },
        { // 30 - 60M
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //   0 -   9
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  10 -  19
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  20 -  29
                0,  0,  0,  0,  0, 24, 24, 24, 24, 24, //  30 -  39
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  40 -  49
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  50 -  59
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  60 -  69
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  70 -  79
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  80 -  89
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  90 -  99
               24, 24, 24,                             // 100 - 102
        },
        { // 60 - 120M
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //   0 -   9
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  10 -  19
                0,  0,  0,  0,  0,  0,  0,  0,  0, 24, //  20 -  29
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  30 -  39
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  40 -  49
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  50 -  59
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  60 -  69
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  70 -  79
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  80 -  89
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  90 -  99
               24, 24, 24,                             // 100 - 102
        },
        { // 120 - 250M
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //   0 -   9
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  10 -  19
                0,  0,  0,  0,  0,  0,  0,  0,  0, 24, //  20 -  29
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  30 -  39
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  40 -  49
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  50 -  59
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  60 -  69
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  70 -  79
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  80 -  89
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  90 -  99
               24, 24, 24,                             // 100 - 102
        },
        { // 250 - 420M

                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //   0 -   9
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  10 -  19
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  20 -  29
                0,  0,  0,  0,  0, 24, 24, 24, 24, 24, //  30 -  39
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  40 -  49
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  50 -  59
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  60 -  69
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  70 -  79
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  80 -  89
               24, 24, 24, 24, 24, 24, 24, 24, 24, 24, //  90 -  99
               24, 24, 24,                             // 100 - 102
        },
        { // 420M - 1G
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //   0 -   9
                0,  0,  7,  7,  7,  7,  7,  7,  7,  7, //  10 -  19
                7,  7,  7,  7,  7,  7,  7,  7,  7,  7, //  20 -  29
                7,  7,  7,  7,  7,  7,  7,  7,  7,  7, //  30 -  39
                7,  7,  7,  7,  7,  7,  7,  7,  7,  7, //  40 -  49
                7,  7,  7,  7,  7,  7,  7,  7,  7,  7, //  50 -  59
                7,  7,  7,  7,  7,  7,  7,  7,  7,  7, //  60 -  69
                7,  7,  7,  7,  7,  7,  7,  7,  7,  7, //  70 -  79
                7,  7,  7,  7,  7,  7,  7,  7,  7,  7, //  80 -  89
                7,  7,  7,  7,  7,  7,  7,  7,  7,  7, //  90 -  99
                7,  7,  7,                             // 100 - 102
        },
        { // 1 - 2G
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //   0 -   9
                7,  7,  7,  7,  7,  7,  7,  7,  7,  7, //  10 -  19
                7,  7,  7,  7,  7,  7,  7,  7,  7,  7, //  20 -  29
                7,  7,  7,  7,  7,  7,  7,  7,  7,  7, //  30 -  39
                7,  7,  7,  7,  7,  7,  7,  7,  7,  7, //  40 -  49
                7,  7,  7,  7,  7,  7,  7,  7,  7,  7, //  50 -  59
                7,  7,  7,  7,  7,  7,  7,  7,  7,  7, //  60 -  69
                7,  7,  7,  7,  7,  7,  7,  7,  7,  7, //  70 -  79
                7,  7,  7,  7,  7,  7,  7,  7,  7,  7, //  80 -  89
                7,  7,  7,  7,  7,  7,  7,  7,  7,  7, //  90 -  99
                7,  7,  7,                             // 100 - 102
        }
};

unsigned int SDRPlayGui::m_mixerGr[m_nbBands][m_nbGRValues] = {
        { // 10k - 12M
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //   0 -   9
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  10 -  19
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  20 -  29
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  30 -  39
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  40 -  49
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  50 -  59
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  60 -  69
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  70 -  79
                0,  0,  0, 19, 19, 19, 19, 19, 19, 19, //  80 -  89
               19, 19, 19, 19, 19, 19, 19, 19, 19, 19, //  90 -  99
               19, 19, 19,                             // 100 - 102
        },
        { // 12 - 30M
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //   0 -   9
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  10 -  19
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  20 -  29
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  30 -  39
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  40 -  49
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  50 -  59
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  60 -  69
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  70 -  79
                0,  0,  0, 19, 19, 19, 19, 19, 19, 19, //  80 -  89
               19, 19, 19, 19, 19, 19, 19, 19, 19, 19, //  90 -  99
               19, 19, 19,                             // 100 - 102
        },
        { // 30 - 60M
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //   0 -   9
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  10 -  19
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  20 -  29
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  30 -  39
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  40 -  49
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  50 -  59
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  60 -  69
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  70 -  79
                0,  0,  0, 19, 19, 19, 19, 19, 19, 19, //  80 -  89
               19, 19, 19, 19, 19, 19, 19, 19, 19, 19, //  90 -  99
               19, 19, 19,                             // 100 - 102
        },
        { // 60 - 120M
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //   0 -   9
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  10 -  19
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  20 -  29
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  30 -  39
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  40 -  49
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  50 -  59
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  60 -  69
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  70 -  79
                0,  0,  0, 19, 19, 19, 19, 19, 19, 19, //  80 -  89
               19, 19, 19, 19, 19, 19, 19, 19, 19, 19, //  90 -  99
               19, 19, 19,                             // 100 - 102
        },
        { // 120 - 250M
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //   0 -   9
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  10 -  19
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  20 -  29
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  30 -  39
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  40 -  49
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  50 -  59
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  60 -  69
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  70 -  79
                0,  0,  0, 19, 19, 19, 19, 19, 19, 19, //  80 -  89
               19, 19, 19, 19, 19, 19, 19, 19, 19, 19, //  90 -  99
               19, 19, 19,                             // 100 - 102
        },
        { // 250 - 420M
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //   0 -   9
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  10 -  19
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  20 -  29
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  30 -  39
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  40 -  49
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  50 -  59
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  60 -  69
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  70 -  79
                0,  0,  0, 19, 19, 19, 19, 19, 19, 19, //  80 -  89
               19, 19, 19, 19, 19, 19, 19, 19, 19, 19, //  90 -  99
               19, 19, 19,                             // 100 - 102
        },
        { // 420M - 1G
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //   0 -   9
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  10 -  19
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  20 -  29
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  30 -  39
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  40 -  49
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  50 -  59
                0,  0,  0,  0,  0, 19, 19, 19, 19, 19, //  60 -  69
               19, 19, 19, 19, 19, 19, 19, 19, 19, 19, //  70 -  79
               19, 19, 19, 19, 19, 19, 19, 19, 19, 19, //  80 -  89
               19, 19, 19, 19, 19, 19, 19, 19, 19, 19, //  90 -  99
               19, 19, 19,                             // 100 - 102
        },
        { // 1 - 2G
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //   0 -   9
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  10 -  19
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  20 -  29
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  30 -  39
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  40 -  49
                0,  0,  0,  0,  0,  0,  0,  0,  0,  0, //  50 -  59
                0,  0,  0,  0,  0, 19, 19, 19, 19, 19, //  60 -  69
               19, 19, 19, 19, 19, 19, 19, 19, 19, 19, //  70 -  79
               19, 19, 19, 19, 19, 19, 19, 19, 19, 19, //  80 -  89
               19, 19, 19, 19, 19, 19, 19, 19, 19, 19, //  90 -  99
               19, 19, 19,                             // 100 - 102
        }
};

// ====================================================================

SDRPlayGui::SDRPlayGui(DeviceSourceAPI *deviceAPI, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::SDRPlayGui),
    m_deviceAPI(deviceAPI)
{
    ui->setupUi(this);
    ui->centerFrequency->setColorMapper(ColorMapper(ColorMapper::ReverseGold));
    ui->centerFrequency->setValueRange(7, 10U, 12000U);

    ui->fBand->clear();
    for (int i = 0; i < SDRPlayBands::getNbBands(); i++)
    {
        ui->fBand->addItem(SDRPlayBands::getBandName(i));
    }

    ui->ifFrequency->clear();
    for (int i = 0; i < SDRPlayIF::getNbIFs(); i++)
    {
        ui->ifFrequency->addItem(QString::number(SDRPlayIF::getIF(i)));
    }

    ui->mirDCCorr->clear();
    for (int i = 0; i < SDRPlayDCCorr::getNbDCCorrs(); i++)
    {
        ui->mirDCCorr->addItem(SDRPlayDCCorr::getDCCorrName(i));
    }

    ui->samplerate->clear();
    for (int i = 0; i < SDRPlaySampleRates::getNbRates(); i++)
    {
        ui->samplerate->addItem(QString::number(SDRPlaySampleRates::getRate(i)));
    }

    ui->bandwidth->clear();
    for (int i = 0; i < SDRPlayBandwidths::getNbBandwidths(); i++)
    {
        ui->bandwidth->addItem(QString::number(SDRPlayBandwidths::getBandwidth(i)));
    }

    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
    connect(&m_statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
    m_statusTimer.start(500);

    displaySettings();

    m_sampleSource = new SDRPlayInput(m_deviceAPI);
    m_deviceAPI->setSource(m_sampleSource);

    connect(m_sampleSource->getOutputMessageQueueToGUI(), SIGNAL(messageEnqueued()), this, SLOT(handleSourceMessages()));
    m_deviceAPI->setSource(m_sampleSource);

    char recFileNameCStr[30];
    sprintf(recFileNameCStr, "test_%d.sdriq", m_deviceAPI->getDeviceUID());
    m_fileSink = new FileRecord(std::string(recFileNameCStr));
    m_deviceAPI->addSink(m_fileSink);

    connect(m_deviceAPI->getDeviceOutputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleDSPMessages()), Qt::QueuedConnection);
}

SDRPlayGui::~SDRPlayGui()
{
    delete ui;
}

void SDRPlayGui::destroy()
{
    delete this;
}

void SDRPlayGui::setName(const QString& name)
{
    setObjectName(name);
}

QString SDRPlayGui::getName() const
{
    return objectName();
}

void SDRPlayGui::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    sendSettings();
}

qint64 SDRPlayGui::getCenterFrequency() const
{
    return m_settings.m_centerFrequency;
}

void SDRPlayGui::setCenterFrequency(qint64 centerFrequency)
{
    m_settings.m_centerFrequency = centerFrequency;
    displaySettings();
    sendSettings();
}

QByteArray SDRPlayGui::serialize() const
{
    return m_settings.serialize();
}

bool SDRPlayGui::deserialize(const QByteArray& data)
{
    if(m_settings.deserialize(data))
    {
        displaySettings();
        sendSettings();
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

bool SDRPlayGui::handleMessage(const Message& message)
{
    if (SDRPlayInput::MsgReportSDRPlay::match(message))
    {
        displaySettings();
        return true;
    }
    else
    {
        return false;
    }
}

void SDRPlayGui::handleDSPMessages()
{
    Message* message;

    while ((message = m_deviceAPI->getDeviceOutputMessageQueue()->pop()) != 0)
    {
        qDebug("SDRPlayGui::handleDSPMessages: message: %s", message->getIdentifier());

        if (DSPSignalNotification::match(*message))
        {
            DSPSignalNotification* notif = (DSPSignalNotification*) message;
            m_sampleRate = notif->getSampleRate();
            m_deviceCenterFrequency = notif->getCenterFrequency();
            qDebug("SDRPlayGui::handleDSPMessages: SampleRate:%d, CenterFrequency:%llu", notif->getSampleRate(), notif->getCenterFrequency());
            updateSampleRateAndFrequency();
            m_fileSink->handleMessage(*notif); // forward to file sink

            delete message;
        }
    }
}

void SDRPlayGui::handleSourceMessages()
{
    Message* message;

    while ((message = m_sampleSource->getOutputMessageQueueToGUI()->pop()) != 0)
    {
        qDebug("SDRPlayGui::HandleSourceMessages: message: %s", message->getIdentifier());

        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

void SDRPlayGui::updateSampleRateAndFrequency()
{
    m_deviceAPI->getSpectrum()->setSampleRate(m_sampleRate);
    m_deviceAPI->getSpectrum()->setCenterFrequency(m_deviceCenterFrequency);
    ui->deviceRateText->setText(tr("%1k").arg((float)m_sampleRate / 1000));
}


void SDRPlayGui::displaySettings()
{
    ui->centerFrequency->setValue(m_settings.m_centerFrequency / 1000);

    ui->ppm->setValue(m_settings.m_LOppmTenths);
    ui->ppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));

    ui->samplerate->setCurrentIndex(m_settings.m_devSampleRateIndex);

    ui->dcOffset->setChecked(m_settings.m_dcBlock);
    ui->iqImbalance->setChecked(m_settings.m_iqCorrection);

    ui->fBand->setCurrentIndex(m_settings.m_frequencyBandIndex);
    ui->ifFrequency->setCurrentIndex(m_settings.m_ifFrequencyIndex);
    ui->mirDCCorr->setCurrentIndex(m_settings.m_mirDcCorrIndex);
    ui->mirDCCorrTrackTime->setValue(m_settings.m_mirDcCorrTrackTimeIndex);
    ui->mirDCCorrTrackTimeText->setText(tr("%1us").arg(m_settings.m_mirDcCorrIndex * 3));
    ui->samplerate->setCurrentIndex(m_settings.m_devSampleRateIndex);

    ui->decim->setCurrentIndex(m_settings.m_log2Decim);
    ui->fcPos->setCurrentIndex((int) m_settings.m_fcPos);

    ui->gr->setValue(m_settings.m_gainRedctionIndex);

    char grStrChr[20];

    sprintf(grStrChr, "%03d:%02d-%02d-%02d",
            m_settings.m_gainRedctionIndex,
            24 - m_lnaGr[m_settings.m_frequencyBandIndex][m_settings.m_gainRedctionIndex],
            19 - m_mixerGr[m_settings.m_frequencyBandIndex][m_settings.m_gainRedctionIndex],
            59 - m_bbGr[m_settings.m_frequencyBandIndex][m_settings.m_gainRedctionIndex]);

    ui->grText->setText(QString(grStrChr));
}

void SDRPlayGui::sendSettings()
{
    if(!m_updateTimer.isActive())
        m_updateTimer.start(100);
}

void SDRPlayGui::updateHardware()
{
    qDebug() << "SDRPlayGui::updateHardware";
    SDRPlayInput::MsgConfigureSDRPlay* message = SDRPlayInput::MsgConfigureSDRPlay::create( m_settings);
    m_sampleSource->getInputMessageQueue()->push(message);
    m_updateTimer.stop();
}

void SDRPlayGui::updateStatus()
{
    int state = m_deviceAPI->state();

    if(m_lastEngineState != state)
    {
        switch(state)
        {
            case DSPDeviceSourceEngine::StNotStarted:
                ui->startStop->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
                break;
            case DSPDeviceSourceEngine::StIdle:
                ui->startStop->setStyleSheet("QToolButton { background-color : blue; }");
                break;
            case DSPDeviceSourceEngine::StRunning:
                ui->startStop->setStyleSheet("QToolButton { background-color : green; }");
                break;
            case DSPDeviceSourceEngine::StError:
                ui->startStop->setStyleSheet("QToolButton { background-color : red; }");
                QMessageBox::information(this, tr("Message"), m_deviceAPI->errorMessage());
                break;
            default:
                break;
        }

        m_lastEngineState = state;
    }
}

void SDRPlayGui::on_centerFrequency_changed(quint64 value)
{
    m_settings.m_centerFrequency = value * 1000;
    sendSettings();
}

void SDRPlayGui::on_ppm_valueChanged(int value)
{
    m_settings.m_LOppmTenths = value;
    ui->ppmText->setText(QString("%1").arg(QString::number(m_settings.m_LOppmTenths/10.0, 'f', 1)));
    sendSettings();
}

void SDRPlayGui::on_dcOffset_toggled(bool checked)
{
    m_settings.m_dcBlock = checked;
    sendSettings();
}

void SDRPlayGui::on_iqImbalance_toggled(bool checked)
{
    m_settings.m_iqCorrection = checked;
    sendSettings();
}

void SDRPlayGui::on_fBand_currentIndexChanged(int index)
{
    ui->centerFrequency->setValueRange(
            7,
            SDRPlayBands::getBandLow(index),
            SDRPlayBands::getBandHigh(index));

    ui->centerFrequency->setValue((SDRPlayBands::getBandLow(index)+SDRPlayBands::getBandHigh(index)) / 2);
    m_settings.m_centerFrequency = (SDRPlayBands::getBandLow(index)+SDRPlayBands::getBandHigh(index)) * 500;
    m_settings.m_frequencyBandIndex = index;

    sendSettings();
}

void SDRPlayGui::on_mirDCCorr_currentIndexChanged(int index)
{
    m_settings.m_mirDcCorrIndex = index;
    sendSettings();
}

void SDRPlayGui::on_mirDCCorrTrackTime_valueChanged(int value)
{
    m_settings.m_mirDcCorrTrackTimeIndex = value;
    ui->mirDCCorrTrackTimeText->setText(tr("%1us").arg(m_settings.m_mirDcCorrTrackTimeIndex * 3));
    sendSettings();
}

void SDRPlayGui::on_bandwidth_currentIndexChanged(int index)
{
    m_settings.m_bandwidthIndex = index;
    sendSettings();
}

void SDRPlayGui::on_samplerate_currentIndexChanged(int index)
{
    m_settings.m_devSampleRateIndex = index;
    sendSettings();
}

void SDRPlayGui::on_decim_currentIndexChanged(int index)
{
    m_settings.m_log2Decim = index;
    sendSettings();
}

void SDRPlayGui::on_fcPos_currentIndexChanged(int index)
{
    m_settings.m_fcPos = (SDRPlaySettings::fcPos_t) index;
    sendSettings();
}

void SDRPlayGui::on_gr_valueChanged(int value)
{
    m_settings.m_gainRedctionIndex = value;

    char grStrChr[20];

    sprintf(grStrChr, "%03d:%02d-%02d-%02d",
            m_settings.m_gainRedctionIndex,
            24 - m_lnaGr[m_settings.m_frequencyBandIndex][m_settings.m_gainRedctionIndex],
            19 - m_mixerGr[m_settings.m_frequencyBandIndex][m_settings.m_gainRedctionIndex],
            59 - m_bbGr[m_settings.m_frequencyBandIndex][m_settings.m_gainRedctionIndex]);

    ui->grText->setText(QString(grStrChr));

    sendSettings();
}

void SDRPlayGui::on_startStop_toggled(bool checked)
{
    if (checked)
    {
        if (m_deviceAPI->initAcquisition())
        {
            m_deviceAPI->startAcquisition();
            DSPEngine::instance()->startAudio();
        }
    }
    else
    {
        m_deviceAPI->stopAcquisition();
        DSPEngine::instance()->stopAudio();
    }
}

void SDRPlayGui::on_record_toggled(bool checked)
{
    if (checked)
    {
        ui->record->setStyleSheet("QToolButton { background-color : red; }");
        m_fileSink->startRecording();
    }
    else
    {
        ui->record->setStyleSheet("QToolButton { background:rgb(79,79,79); }");
        m_fileSink->stopRecording();
    }
}

// ====================================================================

unsigned int SDRPlaySampleRates::m_rates[m_nb_rates] = {
        2000,   // 0
        2048,   // 1
        2304,   // 2
        2400,   // 3
        3072,   // 4
        3200,   // 5
        4000,   // 6
        4096,   // 7
        4608,   // 8
        4800,   // 9
        5000,   // 10
        6000,   // 11
        6144,   // 12
        8000,   // 13
        8192,   // 14
        9216,   // 15
        9600,   // 16
        10000,  // 17
};

unsigned int SDRPlaySampleRates::getRate(unsigned int rate_index)
{
    if (rate_index < m_nb_rates)
    {
        return m_rates[rate_index];
    }
    else
    {
        return m_rates[0];
    }
}

unsigned int SDRPlaySampleRates::getRateIndex(unsigned int rate)
{
    for (unsigned int i=0; i < m_nb_rates; i++)
    {
        if (rate == m_rates[i])
        {
            return i;
        }
    }

    return 0;
}

unsigned int SDRPlaySampleRates::getNbRates()
{
    return SDRPlaySampleRates::m_nb_rates;
}

// ====================================================================

unsigned int SDRPlayBandwidths::m_bw[m_nb_bw] = {
        200,    // 0
        300,    // 1
        600,    // 2
        1536,   // 3
        5000,   // 4
        6000,   // 5
        7000,   // 6
        8000,   // 7
};

unsigned int SDRPlayBandwidths::getBandwidth(unsigned int bandwidth_index)
{
    if (bandwidth_index < m_nb_bw)
    {
        return m_bw[bandwidth_index];
    }
    else
    {
        return m_bw[0];
    }
}

unsigned int SDRPlayBandwidths::getBandwidthIndex(unsigned int bandwidth)
{
    for (unsigned int i=0; i < m_nb_bw; i++)
    {
        if (bandwidth == m_bw[i])
        {
            return i;
        }
    }

    return 0;
}

unsigned int SDRPlayBandwidths::getNbBandwidths()
{
    return SDRPlayBandwidths::m_nb_bw;
}

// ====================================================================

unsigned int SDRPlayIF::m_if[m_nb_if] = {
        0,      // 0
        450,    // 1
        1620,   // 2
        2048,   // 3
};

unsigned int SDRPlayIF::getIF(unsigned int if_index)
{
    if (if_index < m_nb_if)
    {
        return m_if[if_index];
    }
    else
    {
        return m_if[0];
    }
}

unsigned int SDRPlayIF::getIFIndex(unsigned int iff)
{
    for (unsigned int i=0; i < m_nb_if; i++)
    {
        if (iff == m_if[i])
        {
            return i;
        }
    }

    return 0;
}

unsigned int SDRPlayIF::getNbIFs()
{
    return SDRPlayIF::m_nb_if;
}

// ====================================================================

/** Lower frequency bound in kHz inclusive */
unsigned int SDRPlayBands::m_bandLow[m_nb_bands] = {
        10,       // 0
        12000,    // 1
        30000,    // 2
        60000,    // 3
        120000,   // 4
        250000,   // 5
        420000,   // 6
        1000000,  // 7
};

/** Lower frequency bound in kHz exclusive */
unsigned int SDRPlayBands::m_bandHigh[m_nb_bands] = {
        12000,    // 0
        30000,    // 1
        60000,    // 2
        120000,   // 3
        250000,   // 4
        420000,   // 5
        1000000,  // 6
        2000000,  // 7
};

const char* SDRPlayBands::m_bandName[m_nb_bands] = {
        "10k-12M",    // 0
        "12-30M",     // 1
        "30-60M",     // 2
        "60-120M",    // 3
        "120-250M",   // 4
        "250-420M",   // 5
        "420M-1G",    // 6
        "1-2G",       // 7
};

QString SDRPlayBands::getBandName(unsigned int band_index)
{
    if (band_index < m_nb_bands)
    {
        return QString(m_bandName[band_index]);
    }
    else
    {
        return QString(m_bandName[0]);
    }
}

unsigned int SDRPlayBands::getBandLow(unsigned int band_index)
{
    if (band_index < m_nb_bands)
    {
        return m_bandLow[band_index];
    }
    else
    {
        return m_bandLow[0];
    }
}

unsigned int SDRPlayBands::getBandHigh(unsigned int band_index)
{
    if (band_index < m_nb_bands)
    {
        return m_bandHigh[band_index];
    }
    else
    {
        return m_bandHigh[0];
    }
}


unsigned int SDRPlayBands::getNbBands()
{
    return SDRPlayBands::m_nb_bands;
}

// ====================================================================

const char* SDRPlayDCCorr::m_dcCorrName[m_nb_dcCorrs] = {
        "None",   // 0
        "Per1",   // 1
        "Per2",   // 2
        "Per3",   // 3
        "1Shot",  // 4
        "Cont",   // 5
};

QString SDRPlayDCCorr::getDCCorrName(unsigned int dcCorr_index)
{
    if (dcCorr_index < m_nb_dcCorrs)
    {
        return QString(m_dcCorrName[dcCorr_index]);
    }
    else
    {
        return QString(m_dcCorrName[0]);
    }
}

unsigned int SDRPlayDCCorr::getNbDCCorrs()
{
    return SDRPlayDCCorr::m_nb_dcCorrs;
}
