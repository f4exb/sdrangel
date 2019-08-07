///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
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

#include <QByteArray>

#include "export.h"
#include "dsp/dsptypes.h"
#include "settings/serializable.h"

class SDRBASE_API GLSpectrumSettings : public Serializable
{
public:
    enum AveragingMode
    {
        AvgModeNone,
        AvgModeMoving,
        AvgModeFixed,
        AvgModeMax
    };

	int m_fftSize;
	int m_fftOverlap;
	int m_fftWindow;
	Real m_refLevel;
	Real m_powerRange;
	int m_decay;
	int m_decayDivisor;
	int m_histogramStroke;
	int m_displayGridIntensity;
	int m_displayTraceIntensity;
	bool m_displayWaterfall;
	bool m_invertedWaterfall;
    Real m_waterfallShare;
	bool m_displayMaxHold;
	bool m_displayCurrent;
	bool m_displayHistogram;
	bool m_displayGrid;
	bool m_invert;
	AveragingMode m_averagingMode;
	int m_averagingIndex;
	int m_averagingMaxScale; //!< Max power of 10 multiplier to 2,5,10 base ex: 2 -> 2,5,10,20,50,100,200,500,1000
	unsigned int m_averagingNb;
	bool m_linear; //!< linear else logarithmic scale

    GLSpectrumSettings();
	virtual ~GLSpectrumSettings();
    void resetToDefaults();

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    static int getAveragingMaxScale(AveragingMode averagingMode);
    static int getAveragingValue(int averagingIndex, AveragingMode averagingMode);
    static int getAveragingIndex(int averagingValue, AveragingMode averagingMode);
};
