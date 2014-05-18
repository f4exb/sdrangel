#include "pidcontroller.h"

PIDController::PIDController() :
	m_p(0.0),
	m_i(0.0),
	m_d(0.0),
	m_int(0.0),
	m_diff(0.0)
{
}

void PIDController::setup(Real p, Real i, Real d)
{
	m_p = p;
	m_i = i;
	m_d = d;
}
