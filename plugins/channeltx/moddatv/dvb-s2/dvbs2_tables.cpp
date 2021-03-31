#include "DVBS2.h"
//
// Modcod error correction
//

const unsigned long DVBS2::g[6]=
{
    0x55555555,0x33333333,0x0F0F0F0F,0x00FF00FF,0x0000FFFF,0xFFFFFFFF
};

const int DVBS2::ph_scram_tab[64]=
{
	0,1,1,1,0,0,0,1,1,0,0,1,1,1,0,1,1,0,0,0,0,0,1,1,1,1,0,0,1,0,0,1,
	0,1,0,1,0,0,1,1,0,1,0,0,0,0,1,0,0,0,1,0,1,1,0,1,1,1,1,1,1,0,1,0
};
const int DVBS2::ph_sync_seq[26]=
{
	0,1,1,0,0,0,1,1,0,1,0,0,1,0,1,1,1,0,1,0,0,0,0,0,1,0
};
