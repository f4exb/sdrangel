/*
 * agc.cpp
 *
 *  Created on: Sep 7, 2015
 *      Author: f4exb
 */

#include "dsp/agc.h"


AGC::AGC() :
	m_u0(1.0),
	m_R(1.0),
	m_moving_average(),
	m_historySize(0),
	m_count(0)
{}

AGC::AGC(int historySize, Real R) :
	m_u0(1.0),
	m_R(R),
	m_moving_average(historySize, m_R),
	m_historySize(historySize),
	m_count(0)
{}

AGC::~AGC()
{}

void AGC::resize(int historySize, Real R)
{
	m_R = R;
	m_moving_average.resize(historySize, R);
	m_historySize = historySize;
	m_count = 0;
}

Real AGC::getValue()
{
	return m_u0;
}

Real AGC::getAverage()
{
	return m_moving_average.average();
}

MagSquaredAGC::MagSquaredAGC() :
	AGC()
{}

MagSquaredAGC::MagSquaredAGC(int historySize, Real R) :
	AGC(historySize, R)
{}

MagSquaredAGC::~MagSquaredAGC()
{}

void MagSquaredAGC::feed(Complex& ci)
{
	Real magsq = ci.real()*ci.real() + ci.imag()*ci.imag();
	m_moving_average.feed(magsq);
	m_u0 = m_R / m_moving_average.average();
	ci *= m_u0;
}


MagAGC::MagAGC() :
	AGC()
{}

MagAGC::MagAGC(int historySize, Real R) :
	AGC(historySize, R)
{}

MagAGC::~MagAGC()
{}

void MagAGC::feed(Complex& ci)
{
	Real mag = sqrt(ci.real()*ci.real() + ci.imag()*ci.imag());
	m_moving_average.feed(mag);
	m_u0 = m_R / m_moving_average.average();
	ci *= m_u0;
}


AlphaAGC::AlphaAGC() :
	AGC(),
	m_alpha(0.5),
	m_squelchOpen(true)
{}

AlphaAGC::AlphaAGC(int historySize, Real R) :
	AGC(historySize, R),
	m_alpha(0.5),
	m_squelchOpen(true)
{}


AlphaAGC::AlphaAGC(int historySize, Real R, Real alpha) :
	AGC(historySize, R),
	m_alpha(alpha),
	m_squelchOpen(true)
{}

AlphaAGC::~AlphaAGC()
{}

void AlphaAGC::resize(int historySize, Real R, Real alpha)
{
	 m_R = R;
	 m_alpha = alpha;
	 m_squelchOpen = true;
	 m_moving_average.resize(historySize, R);
}

void AlphaAGC::feed(Complex& ci)
{
	Real magsq = ci.real()*ci.real() + ci.imag()*ci.imag();

	if (m_squelchOpen && (magsq))
	{
		m_moving_average.feed(m_moving_average.average() - m_alpha*(m_moving_average.average() - magsq));
	}
	else
	{
		//m_squelchOpen = true;
		m_moving_average.feed(magsq);
	}
	ci *= m_u0;
}
