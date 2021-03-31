#include "math.h"
#include "DVBS2.h"

#define CP 0x7FFF

void DVBS2::modulator_configuration()
{
        double r0,r1,r2,r3;
        double m = 1.0;
        if (m_format[0].constellation == M_32APSK)
            r0 = 0.9*m;// I am not sure why this needs to be 0.9 but 32APSK does not work if == 1.0
        else
            r0 = 1.0*m;// Don't use 0.9 for other mods, as that results in worse MER
        r1 = m;
	// BPSK
        m_bpsk[0][0].re = (short)((r0*cos(1*M_PI/4.0))*CP);
        m_bpsk[0][0].im = (short)((r0*sin(1*M_PI/4.0))*CP);
        m_bpsk[0][1].re = (short)((r0*cos(5*M_PI/4.0))*CP);
        m_bpsk[0][1].im = (short)((r0*sin(5*M_PI/4.0))*CP);

		m_bpsk[1][0].re = (short)((r0*cos(3*M_PI/4.0))*CP);
        m_bpsk[1][0].im = (short)((r0*sin(3*M_PI/4.0))*CP);
        m_bpsk[1][1].re = (short)((r0*cos(7*M_PI/4.0))*CP);
        m_bpsk[1][1].im = (short)((r0*sin(7*M_PI/4.0))*CP);

        // QPSK
        m_qpsk[0].re = (short)((r1*cos(M_PI/4.0))*CP);
        m_qpsk[0].im = (short)((r1*sin(M_PI/4.0))*CP);
        m_qpsk[1].re = (short)((r1*cos(7*M_PI/4.0))*CP);
        m_qpsk[1].im = (short)((r1*sin(7*M_PI/4.0))*CP);
        m_qpsk[2].re = (short)((r1*cos(3*M_PI/4.0))*CP);
        m_qpsk[2].im = (short)((r1*sin(3*M_PI/4.0))*CP);
        m_qpsk[3].re = (short)((r1*cos(5*M_PI/4.0))*CP);
        m_qpsk[3].im = (short)((r1*sin(5*M_PI/4.0))*CP);

	// 8PSK
        m_8psk[0].re = (short)((r1*cos(M_PI/4.0))*CP);
        m_8psk[0].im = (short)((r1*sin(M_PI/4.0))*CP);
        m_8psk[1].re = (short)((r1*cos(0.0))*CP);
        m_8psk[1].im = (short)((r1*sin(0.0))*CP);
        m_8psk[2].re = (short)((r1*cos(4*M_PI/4.0))*CP);
        m_8psk[2].im = (short)((r1*sin(4*M_PI/4.0))*CP);
        m_8psk[3].re = (short)((r1*cos(5*M_PI/4.0))*CP);
        m_8psk[3].im = (short)((r1*sin(5*M_PI/4.0))*CP);
        m_8psk[4].re = (short)((r1*cos(2*M_PI/4.0))*CP);
        m_8psk[4].im = (short)((r1*sin(2*M_PI/4.0))*CP);
        m_8psk[5].re = (short)((r1*cos(7*M_PI/4.0))*CP);
        m_8psk[5].im = (short)((r1*sin(7*M_PI/4.0))*CP);
        m_8psk[6].re = (short)((r1*cos(3*M_PI/4.0))*CP);
        m_8psk[6].im = (short)((r1*sin(3*M_PI/4.0))*CP);
        m_8psk[7].re = (short)((r1*cos(6*M_PI/4.0))*CP);
        m_8psk[7].im = (short)((r1*sin(6*M_PI/4.0))*CP);

        // 16 APSK
        r2 = m;
        switch( m_format[0].code_rate )
        {
            case CR_2_3:
                r1 = r2/3.15;
                break;
            case CR_3_4:
                r1 = r2/2.85;
                break;
            case CR_4_5:
                r1 = r2/2.75;
                break;
            case CR_5_6:
                r1 = r2/2.70;
                break;
            case CR_8_9:
                r1 = r2/2.60;
                break;
            case CR_9_10:
                r1 = r2/2.57;
                break;
            default:
                // Illegal
                r1 = 0;
                break;
        }

        m_16apsk[0].re  = (short)((r2*cos(M_PI/4.0))*CP);
        m_16apsk[0].im  = (short)((r2*sin(M_PI/4.0))*CP);
        m_16apsk[1].re  = (short)((r2*cos(-M_PI/4.0))*CP);
        m_16apsk[1].im  = (short)((r2*sin(-M_PI/4.0))*CP);
        m_16apsk[2].re  = (short)((r2*cos(3*M_PI/4.0))*CP);
        m_16apsk[2].im  = (short)((r2*sin(3*M_PI/4.0))*CP);
        m_16apsk[3].re  = (short)((r2*cos(-3*M_PI/4.0))*CP);
        m_16apsk[3].im  = (short)((r2*sin(-3*M_PI/4.0))*CP);
        m_16apsk[4].re  = (short)((r2*cos(M_PI/12.0))*CP);
        m_16apsk[4].im  = (short)((r2*sin(M_PI/12.0))*CP);
        m_16apsk[5].re  = (short)((r2*cos(-M_PI/12.0))*CP);
        m_16apsk[5].im  = (short)((r2*sin(-M_PI/12.0))*CP);
        m_16apsk[6].re  = (short)((r2*cos(11*M_PI/12.0))*CP);
        m_16apsk[6].im  = (short)((r2*sin(11*M_PI/12.0))*CP);
        m_16apsk[7].re  = (short)((r2*cos(-11*M_PI/12.0))*CP);
        m_16apsk[7].im  = (short)((r2*sin(-11*M_PI/12.0))*CP);
        m_16apsk[8].re  = (short)((r2*cos(5*M_PI/12.0))*CP);
        m_16apsk[8].im  = (short)((r2*sin(5*M_PI/12.0))*CP);
        m_16apsk[9].re  = (short)((r2*cos(-5*M_PI/12.0))*CP);
        m_16apsk[9].im  = (short)((r2*sin(-5*M_PI/12.0))*CP);
        m_16apsk[10].re = (short)((r2*cos(7*M_PI/12.0))*CP);
        m_16apsk[10].im = (short)((r2*sin(7*M_PI/12.0))*CP);
        m_16apsk[11].re = (short)((r2*cos(-7*M_PI/12.0))*CP);
        m_16apsk[11].im = (short)((r2*sin(-7*M_PI/12.0))*CP);
        m_16apsk[12].re = (short)((r1*cos(M_PI/4.0))*CP);
        m_16apsk[12].im = (short)((r1*sin(M_PI/4.0))*CP);
        m_16apsk[13].re = (short)((r1*cos(-M_PI/4.0))*CP);
        m_16apsk[13].im = (short)((r1*sin(-M_PI/4.0))*CP);
        m_16apsk[14].re = (short)((r1*cos(3*M_PI/4.0))*CP);
        m_16apsk[14].im = (short)((r1*sin(3*M_PI/4.0))*CP);
        m_16apsk[15].re = (short)((r1*cos(-3*M_PI/4.0))*CP);
        m_16apsk[15].im = (short)((r1*sin(-3*M_PI/4.0))*CP);
        // 32 APSK
        r3 = m;
		switch( m_format[0].code_rate )
        {
            case CR_3_4:
                r1 = r3/5.27;
                r2 = r1*2.84;
                break;
            case CR_4_5:
                r1 = r3/4.87;
                r2 = r1*2.72;
                break;
            case CR_5_6:
                r1 = r3/4.64;
                r2 = r1*2.64;
                break;
            case CR_8_9:
                r1 = r3/4.33;
                r2 = r1*2.54;
                break;
            case CR_9_10:
                r1 = r3/4.30;
                r2 = r1*2.53;
                break;
            default:
                // Illegal
                r1 = 0;
                r2 = 0;
                break;
        }

		m_32apsk[0].re  = (short)((r2*cos(M_PI/4.0))*CP);
        m_32apsk[0].im  = (short)((r2*sin(M_PI/4.0))*CP);
        m_32apsk[1].re  = (short)((r2*cos(5*M_PI/12.0))*CP);
        m_32apsk[1].im  = (short)((r2*sin(5*M_PI/12.0))*CP);
        m_32apsk[2].re  = (short)((r2*cos(-M_PI/4.0))*CP);
        m_32apsk[2].im  = (short)((r2*sin(-M_PI/4.0))*CP);
        m_32apsk[3].re  = (short)((r2*cos(-5*M_PI/12.0))*CP);
        m_32apsk[3].im  = (short)((r2*sin(-5*M_PI/12.0))*CP);
        m_32apsk[4].re  = (short)((r2*cos(3*M_PI/4.0))*CP);
        m_32apsk[4].im  = (short)((r2*sin(3*M_PI/4.0))*CP);
        m_32apsk[5].re  = (short)((r2*cos(7*M_PI/12.0))*CP);
        m_32apsk[5].im  = (short)((r2*sin(7*M_PI/12.0))*CP);
        m_32apsk[6].re  = (short)((r2*cos(-3*M_PI/4.0))*CP);
        m_32apsk[6].im  = (short)((r2*sin(-3*M_PI/4.0))*CP);
        m_32apsk[7].re  = (short)((r2*cos(-7*M_PI/12.0))*CP);
        m_32apsk[7].im  = (short)((r2*sin(-7*M_PI/12.0))*CP);
        m_32apsk[8].re  = (short)((r3*cos(M_PI/8.0))*CP);
        m_32apsk[8].im  = (short)((r3*sin(M_PI/8.0))*CP);
        m_32apsk[9].re  = (short)((r3*cos(3*M_PI/8.0))*CP);
        m_32apsk[9].im  = (short)((r3*sin(3*M_PI/8.0))*CP);
        m_32apsk[10].re = (short)((r3*cos(-M_PI/4.0))*CP);
        m_32apsk[10].im = (short)((r3*sin(-M_PI/4.0))*CP);
        m_32apsk[11].re = (short)((r3*cos(-M_PI/2.0))*CP);
        m_32apsk[11].im = (short)((r3*sin(-M_PI/2.0))*CP);
        m_32apsk[12].re = (short)((r3*cos(3*M_PI/4.0))*CP);
        m_32apsk[12].im = (short)((r3*sin(3*M_PI/4.0))*CP);
        m_32apsk[13].re = (short)((r3*cos(M_PI/2.0))*CP);
        m_32apsk[13].im = (short)((r3*sin(M_PI/2.0))*CP);
        m_32apsk[14].re = (short)((r3*cos(-7*M_PI/8.0))*CP);
        m_32apsk[14].im = (short)((r3*sin(-7*M_PI/8.0))*CP);
        m_32apsk[15].re = (short)((r3*cos(-5*M_PI/8.0))*CP);
        m_32apsk[15].im = (short)((r3*sin(-5*M_PI/8.0))*CP);
        m_32apsk[16].re = (short)((r2*cos(M_PI/12.0))*CP);
        m_32apsk[16].im = (short)((r2*sin(M_PI/12.0))*CP);
        m_32apsk[17].re = (short)((r1*cos(M_PI/4.0))*CP);
        m_32apsk[17].im = (short)((r1*sin(M_PI/4.0))*CP);
        m_32apsk[18].re = (short)((r2*cos(-M_PI/12.0))*CP);
        m_32apsk[18].im = (short)((r2*sin(-M_PI/12.0))*CP);
        m_32apsk[19].re = (short)((r1*cos(-M_PI/4.0))*CP);
        m_32apsk[19].im = (short)((r1*sin(-M_PI/4.0))*CP);
        m_32apsk[20].re = (short)((r2*cos(11*M_PI/12.0))*CP);
        m_32apsk[20].im = (short)((r2*sin(11*M_PI/12.0))*CP);
        m_32apsk[21].re = (short)((r1*cos(3*M_PI/4.0))*CP);
        m_32apsk[21].im = (short)((r1*sin(3*M_PI/4.0))*CP);
        m_32apsk[22].re = (short)((r2*cos(-11*M_PI/12.0))*CP);
        m_32apsk[22].im = (short)((r2*sin(-11*M_PI/12.0))*CP);
        m_32apsk[23].re = (short)((r1*cos(-3*M_PI/4.0))*CP);
        m_32apsk[23].im = (short)((r1*sin(-3*M_PI/4.0))*CP);
        m_32apsk[24].re = (short)((r3*cos(0.0))*CP);
        m_32apsk[24].im = (short)((r3*sin(0.0))*CP);
        m_32apsk[25].re = (short)((r3*cos(M_PI/4.0))*CP);
        m_32apsk[25].im = (short)((r3*sin(M_PI/4.0))*CP);
        m_32apsk[26].re = (short)((r3*cos(-M_PI/8.0))*CP);
        m_32apsk[26].im = (short)((r3*sin(-M_PI/8.0))*CP);
        m_32apsk[27].re = (short)((r3*cos(-3*M_PI/8.0))*CP);
        m_32apsk[27].im = (short)((r3*sin(-3*M_PI/8.0))*CP);
        m_32apsk[28].re = (short)((r3*cos(7*M_PI/8.0))*CP);
        m_32apsk[28].im = (short)((r3*sin(7*M_PI/8.0))*CP);
        m_32apsk[29].re = (short)((r3*cos(5*M_PI/8.0))*CP);
        m_32apsk[29].im = (short)((r3*sin(5*M_PI/8.0))*CP);
        m_32apsk[30].re = (short)((r3*cos(M_PI))*CP);
        m_32apsk[30].im = (short)((r3*sin(M_PI))*CP);
        m_32apsk[31].re = (short)((r3*cos(-3*M_PI/4.0))*CP);
        m_32apsk[31].im = (short)((r3*sin(-3*M_PI/4.0))*CP);
}
