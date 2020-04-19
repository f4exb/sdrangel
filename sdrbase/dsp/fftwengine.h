///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB                              //
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

#ifndef INCLUDE_FFTWENGINE_H
#define INCLUDE_FFTWENGINE_H

#include <QMutex>
#include <QString>

#include <fftw3.h>
#include <list>
#include "dsp/fftengine.h"
#include "export.h"

class SDRBASE_API FFTWEngine : public FFTEngine {
public:
	FFTWEngine(const QString& fftWisdomFileName);
	virtual ~FFTWEngine();

	virtual void configure(int n, bool inverse);
	virtual void transform();

	virtual Complex* in();
	virtual Complex* out();

    virtual void setReuse(bool reuse) { m_reuse = reuse; }

protected:
	static QMutex m_globalPlanMutex;
    QString m_fftWisdomFileName;

	struct Plan {
		int n;
		bool inverse;
		fftwf_plan plan;
		fftwf_complex* in;
		fftwf_complex* out;
	};
	typedef std::list<Plan*> Plans;
	Plans m_plans;
	Plan* m_currentPlan;
    bool m_reuse;

	void freeAll();
};

#endif // INCLUDE_FFTWENGINE_H
