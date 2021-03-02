/*
LDPC interleavers handler

Copyright 2018 Ahmet Inan <xdsopl@gmail.com>
*/

#include <iostream>
#include <cstring>
#include "testbench.h"
#include "interleaver.h"

namespace ldpctool {

Interleaver<code_type> *create_interleaver(char *modulation, char *standard, char prefix, int number)
{
	if (!strcmp(standard, "S2")) {
		if (!strcmp(modulation, "8PSK")) {
			typedef MUX3<code_type, 0, 1, 2> _012;
			typedef MUX3<code_type, 2, 1, 0> _210;
			if (prefix == 'B') {
				switch (number) {
				case 5:
					return new BITL<code_type, PITL0<code_type, 64800>, _210>();
				default:
					return new BITL<code_type, PITL0<code_type, 64800>, _012>();
				}
			}
			if (prefix == 'C') {
				switch (number) {
				case 5:
					return new BITL<code_type, PITL0<code_type, 16200>, _210>();
				default:
					return new BITL<code_type, PITL0<code_type, 16200>, _012>();
				}
			}
		}
	}
	if (!strcmp(standard, "S2X")) {
		if (!strcmp(modulation, "8PSK")) {
			typedef MUX3<code_type, 0, 1, 2> _012;
			typedef MUX3<code_type, 1, 0, 2> _102;
			if (prefix == 'B') {
				switch (number) {
				case 7:
				case 8:
				case 9:
					return new BITL<code_type, PITL0<code_type, 64800>, _102>();
				default:
					return new BITL<code_type, PITL0<code_type, 64800>, _012>();
				}
			}
			if (prefix == 'C') {
				switch (number) {
				case 4:
				case 5:
				case 6:
				case 7:
					return new BITL<code_type, PITL0<code_type, 16200>, _102>();
				default:
					return new BITL<code_type, PITL0<code_type, 16200>, _012>();
				}
			}
		}
	}
	if (!strcmp(standard, "T2")) {
		if (!strcmp(modulation, "QPSK")) {
			if (prefix == 'B') {
				switch (number) {
				case 8:
					return new BITL<code_type, PITL<code_type, 16200, 30>, MUX0<code_type>>();
				case 9:
					return new BITL<code_type, PITL<code_type, 16200, 27>, MUX0<code_type>>();
				}
			}
		}
		if (!strcmp(modulation, "QAM16")) {
			typedef MUX8<code_type, 7, 1, 4, 2, 5, 3, 6, 0> _71425360;
			if (prefix == 'A') {
				typedef MUX8<code_type, 0, 5, 1, 2, 4, 7, 3, 6> _05124736;
				typedef CT8<code_type, 0, 0, 2, 4, 4, 5, 7, 7> CT;
				switch (number) {
				case 1:
					return new BITL<code_type, PCTITL<code_type, 64800, 90, CT>, _71425360>();
				case 2:
					return new BITL<code_type, PCTITL<code_type, 64800, 72, CT>, _05124736>();
				case 3:
					return new BITL<code_type, PCTITL<code_type, 64800, 60, CT>, _71425360>();
				case 4:
					return new BITL<code_type, PCTITL<code_type, 64800, 45, CT>, _71425360>();
				case 5:
					return new BITL<code_type, PCTITL<code_type, 64800, 36, CT>, _71425360>();
				case 6:
					return new BITL<code_type, PCTITL<code_type, 64800, 30, CT>, _71425360>();
				}
			}
			if (prefix == 'B') {
				typedef MUX8<code_type, 6, 0, 3, 4, 5, 2, 1, 7> _60345217;
				typedef MUX8<code_type, 7, 5, 4, 0, 3, 1, 2, 6> _75403126;
				typedef CT8<code_type, 0, 0, 0, 1, 7, 20, 20, 21> CT;
				switch (number) {
				case 1:
					return new BITL<code_type, PCTITL<code_type, 16200, 36, CT>, _71425360>();
				case 2:
					return new BITL<code_type, PCTITL<code_type, 16200, 25, CT>, _71425360>();
				case 3:
					return new BITL<code_type, PCTITL<code_type, 16200, 18, CT>, _71425360>();
				case 4:
					return new BITL<code_type, PCTITL<code_type, 16200, 15, CT>, _71425360>();
				case 5:
					return new BITL<code_type, PCTITL<code_type, 16200, 12, CT>, _71425360>();
				case 6:
					return new BITL<code_type, PCTITL<code_type, 16200, 10, CT>, _71425360>();
				case 7:
					return new BITL<code_type, PCTITL<code_type, 16200, 8, CT>, _71425360>();
				case 8:
					return new BITL<code_type, PCTITL<code_type, 16200, 30, CT>, _60345217>();
				case 9:
					return new BITL<code_type, PCTITL<code_type, 16200, 27, CT>, _75403126>();
				}
			}
		}
		if (!strcmp(modulation, "QAM64")) {
			typedef MUX12<code_type, 11, 7, 3, 10, 6, 2, 9, 5, 1, 8, 4, 0> _11731062951840;
			if (prefix == 'A') {
				typedef MUX12<code_type, 2, 7, 6, 9, 0, 3, 1, 8, 4, 11, 5, 10> _27690318411510;
				typedef CT12<code_type, 0, 0, 2, 2, 3, 4, 4, 5, 5, 7, 8, 9> CT;
				switch (number) {
				case 1:
					return new BITL<code_type, PCTITL<code_type, 64800, 90, CT>, _11731062951840>();
				case 2:
					return new BITL<code_type, PCTITL<code_type, 64800, 72, CT>, _27690318411510>();
				case 3:
					return new BITL<code_type, PCTITL<code_type, 64800, 60, CT>, _11731062951840>();
				case 4:
					return new BITL<code_type, PCTITL<code_type, 64800, 45, CT>, _11731062951840>();
				case 5:
					return new BITL<code_type, PCTITL<code_type, 64800, 36, CT>, _11731062951840>();
				case 6:
					return new BITL<code_type, PCTITL<code_type, 64800, 30, CT>, _11731062951840>();
				}
			}
			if (prefix == 'B') {
				typedef MUX12<code_type, 4, 2, 0, 5, 6, 1, 3, 7, 8, 9, 10, 11> _42056137891011;
				typedef MUX12<code_type, 4, 0, 1, 6, 2, 3, 5, 8, 7, 10, 9, 11> _40162358710911;
				typedef CT12<code_type, 0, 0, 0, 2, 2, 2, 3, 3, 3, 6, 7, 7> CT;
				switch (number) {
				case 1:
					return new BITL<code_type, PCTITL<code_type, 16200, 36, CT>, _11731062951840>();
				case 2:
					return new BITL<code_type, PCTITL<code_type, 16200, 25, CT>, _11731062951840>();
				case 3:
					return new BITL<code_type, PCTITL<code_type, 16200, 18, CT>, _11731062951840>();
				case 4:
					return new BITL<code_type, PCTITL<code_type, 16200, 15, CT>, _11731062951840>();
				case 5:
					return new BITL<code_type, PCTITL<code_type, 16200, 12, CT>, _11731062951840>();
				case 6:
					return new BITL<code_type, PCTITL<code_type, 16200, 10, CT>, _11731062951840>();
				case 7:
					return new BITL<code_type, PCTITL<code_type, 16200, 8, CT>, _11731062951840>();
				case 8:
					return new BITL<code_type, PCTITL<code_type, 16200, 30, CT>, _42056137891011>();
				case 9:
					return new BITL<code_type, PCTITL<code_type, 16200, 27, CT>, _40162358710911>();
				}
			}
		}
		if (!strcmp(modulation, "QAM256")) {
			if (prefix == 'A') {
				typedef MUX16<code_type, 15, 1, 13, 3, 8, 11, 9, 5, 10, 6, 4, 7, 12, 2, 14, 0> _1511338119510647122140;
				typedef MUX16<code_type, 2, 11, 3, 4, 0, 9, 1, 8, 10, 13, 7, 14, 6, 15, 5, 12> _2113409181013714615512;
				typedef MUX16<code_type, 7, 2, 9, 0, 4, 6, 13, 3, 14, 10, 15, 5, 8, 12, 11, 1> _7290461331410155812111;
				typedef CT16<code_type, 0, 2, 2, 2, 2, 3, 7, 15, 16, 20, 22, 22, 27, 27, 28, 32> CT;
				switch (number) {
				case 1:
					return new BITL<code_type, PCTITL<code_type, 64800, 90, CT>, _1511338119510647122140>();
				case 2:
					return new BITL<code_type, PCTITL<code_type, 64800, 72, CT>, _2113409181013714615512>();
				case 3:
					return new BITL<code_type, PCTITL<code_type, 64800, 60, CT>, _7290461331410155812111>();
				case 4:
					return new BITL<code_type, PCTITL<code_type, 64800, 45, CT>, _1511338119510647122140>();
				case 5:
					return new BITL<code_type, PCTITL<code_type, 64800, 36, CT>, _1511338119510647122140>();
				case 6:
					return new BITL<code_type, PCTITL<code_type, 64800, 30, CT>, _1511338119510647122140>();
				}
			}
			if (prefix == 'B') {
				typedef MUX8<code_type, 7, 3, 1, 5, 2, 6, 4, 0> _73152640;
				typedef MUX8<code_type, 4, 0, 1, 2, 5, 3, 6, 7> _40125367;
				typedef MUX8<code_type, 4, 0, 5, 1, 2, 3, 6, 7> _40512367;
				typedef CT8<code_type, 0, 0, 0, 1, 7, 20, 20, 21> CT;
				switch (number) {
				case 1:
					return new BITL<code_type, PCTITL<code_type, 16200, 36, CT>, _73152640>();
				case 2:
					return new BITL<code_type, PCTITL<code_type, 16200, 25, CT>, _73152640>();
				case 3:
					return new BITL<code_type, PCTITL<code_type, 16200, 18, CT>, _73152640>();
				case 4:
					return new BITL<code_type, PCTITL<code_type, 16200, 15, CT>, _73152640>();
				case 5:
					return new BITL<code_type, PCTITL<code_type, 16200, 12, CT>, _73152640>();
				case 6:
					return new BITL<code_type, PCTITL<code_type, 16200, 10, CT>, _73152640>();
				case 7:
					return new BITL<code_type, PCTITL<code_type, 16200, 8, CT>, _73152640>();
				case 8:
					return new BITL<code_type, PCTITL<code_type, 16200, 30, CT>, _40125367>();
				case 9:
					return new BITL<code_type, PCTITL<code_type, 16200, 27, CT>, _40512367>();
				}
			}
		}
	}
	std::cerr << "using noop interleaver." << std::endl;
	return new ITL0<code_type>();
}

} // namespace ldpctool
