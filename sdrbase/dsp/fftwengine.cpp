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

#include <QElapsedTimer>
#include "dsp/fftwengine.h"

FFTWEngine::FFTWEngine(const QString& fftWisdomFileName) :
    m_fftWisdomFileName(fftWisdomFileName),
	m_plans(),
	m_currentPlan(nullptr),
    m_reuse(true)
{
}

FFTWEngine::~FFTWEngine()
{
	freeAll();
}

void FFTWEngine::configure(int n, bool inverse)
{
    if (m_reuse)
    {
        for (Plans::const_iterator it = m_plans.begin(); it != m_plans.end(); ++it)
        {
            if (((*it)->n == n) && ((*it)->inverse == inverse))
            {
                m_currentPlan = *it;
                return;
            }
        }
    }

	m_currentPlan = new Plan;
	m_currentPlan->n = n;
	m_currentPlan->inverse = inverse;
	m_currentPlan->in = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * n);
	m_currentPlan->out = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * n);
	QElapsedTimer t;
	t.start();
    m_globalPlanMutex.lock();

    if (m_fftWisdomFileName.size() > 0)
    {
        int rc = fftwf_import_wisdom_from_filename(m_fftWisdomFileName.toStdString().c_str());

        if (rc == 0) { // that's an error (undocumented)
            qInfo("FFTWEngine::configure: importing from FFTW wisdom file: '%s' failed", qPrintable(m_fftWisdomFileName));
        } else {
            qDebug("FFTWEngine::configure: successfully imported from FFTW wisdom file: '%s'", qPrintable(m_fftWisdomFileName));
        }
    }
    else
    {
        qDebug("FFTWEngine::configure: no FFTW wisdom file");
    }

	m_currentPlan->plan = fftwf_plan_dft_1d(n, m_currentPlan->in, m_currentPlan->out, inverse ? FFTW_BACKWARD : FFTW_FORWARD, FFTW_PATIENT);
    m_globalPlanMutex.unlock();

    qDebug("FFT: creating FFTW plan (n=%d,%s) took %lld ms", n, inverse ? "inverse" : "forward", t.elapsed());
	m_plans.push_back(m_currentPlan);
}

void FFTWEngine::transform()
{
	if(m_currentPlan != NULL)
		fftwf_execute(m_currentPlan->plan);
}

Complex* FFTWEngine::in()
{
	if(m_currentPlan != NULL)
		return reinterpret_cast<Complex*>(m_currentPlan->in);
	else return NULL;
}

Complex* FFTWEngine::out()
{
	if(m_currentPlan != NULL)
		return reinterpret_cast<Complex*>(m_currentPlan->out);
	else return NULL;
}

QMutex FFTWEngine::m_globalPlanMutex;

void FFTWEngine::freeAll()
{
	for(Plans::iterator it = m_plans.begin(); it != m_plans.end(); ++it) {
		fftwf_destroy_plan((*it)->plan);
		fftwf_free((*it)->in);
		fftwf_free((*it)->out);
		delete *it;
	}
	m_plans.clear();
}
