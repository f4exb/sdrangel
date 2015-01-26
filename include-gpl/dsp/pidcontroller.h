#ifndef INCLUDE_PIDCONTROLLER_H
#define INCLUDE_PIDCONTROLLER_H

#include "dsp/dsptypes.h"

class PIDController {
private:
	Real m_p;
	Real m_i;
	Real m_d;
	Real m_int;
	Real m_diff;

public:
	PIDController();

	void setup(Real p, Real i, Real d);

	Real feed(Real v)
	{
		m_int += v * m_i;
		Real d = m_d * (m_diff - v);
		m_diff = v;
		return (v * m_p) + m_int + d;
	}
};

#endif // INCLUDE_PIDCONTROLLER_H
