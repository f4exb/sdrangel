#ifndef _FCDPRO_HID_CMD_H_
#define _FCDPRO_HID_CMD_H_

#define FCDPRO_HID_CMD_GET_IF_RSSI      104 // Returns 1 byte unsigned IF RSSI, -35dBm ~=0, -10dBm ~=70.
#define FCDPRO_HID_CMD_GET_PLL_LOCK     105 // Returns 1 bit, true if locked

#define FCDPRO_HID_CMD_SET_DC_CORR      106 // Send with 2 byte unsigned I DC correction followed by 2 byte unsigned Q DC correction. 32768 is the default centre value.
#define FCDPRO_HID_CMD_GET_DC_CORR      107 // Returns 2 byte unsigned I DC correction followed by 2 byte unsigned Q DC correction. 32768 is the default centre value.
#define FCDPRO_HID_CMD_SET_IQ_CORR      108 // Send with 2 byte signed phase correction followed by 2 byte unsigned gain correction. 0 is the default centre value for phase correction, 32768 is the default centre value for gain.
#define FCDPRO_HID_CMD_GET_IQ_CORR      109 // Returns 2 byte signed phase correction followed by 2 byte unsigned gain correction. 0 is the default centre value for phase correction, 32768 is the default centre value for gain.

#define FCDPRO_HID_CMD_SET_LNA_GAIN     110 // Send a 1 byte value, see enums for reference
#define FCDPRO_HID_CMD_SET_LNA_ENHANCE  111
#define FCDPRO_HID_CMD_SET_BAND         112
#define FCDPRO_HID_CMD_SET_RF_FILTER    113
#define FCDPRO_HID_CMD_SET_MIXER_GAIN   114
#define FCDPRO_HID_CMD_SET_BIAS_CURRENT 115
#define FCDPRO_HID_CMD_SET_MIXER_FILTER 116
#define FCDPRO_HID_CMD_SET_IF_GAIN1     117
#define FCDPRO_HID_CMD_SET_IF_GAIN_MODE 118
#define FCDPRO_HID_CMD_SET_IF_RC_FILTER 119
#define FCDPRO_HID_CMD_SET_IF_GAIN2     120
#define FCDPRO_HID_CMD_SET_IF_GAIN3     121
#define FCDPRO_HID_CMD_SET_IF_FILTER    122
#define FCDPRO_HID_CMD_SET_IF_GAIN4     123
#define FCDPRO_HID_CMD_SET_IF_GAIN5     124
#define FCDPRO_HID_CMD_SET_IF_GAIN6     125

#define FCDPRO_HID_CMD_GET_LNA_GAIN     150 // Retrieve a 1 byte value, see enums for reference
#define FCDPRO_HID_CMD_GET_LNA_ENHANCE  151
#define FCDPRO_HID_CMD_GET_BAND         152
#define FCDPRO_HID_CMD_GET_RF_FILTER    153
#define FCDPRO_HID_CMD_GET_MIXER_GAIN   154
#define FCDPRO_HID_CMD_GET_BIAS_CURRENT 155
#define FCDPRO_HID_CMD_GET_MIXER_FILTER 156
#define FCDPRO_HID_CMD_GET_IF_GAIN1     157
#define FCDPRO_HID_CMD_GET_IF_GAIN_MODE 158
#define FCDPRO_HID_CMD_GET_IF_RC_FILTER 159
#define FCDPRO_HID_CMD_GET_IF_GAIN2     160
#define FCDPRO_HID_CMD_GET_IF_GAIN3     161
#define FCDPRO_HID_CMD_GET_IF_FILTER    162
#define FCDPRO_HID_CMD_GET_IF_GAIN4     163
#define FCDPRO_HID_CMD_GET_IF_GAIN5     164
#define FCDPRO_HID_CMD_GET_IF_GAIN6     165

#define FCDPRO_HID_CMD_I2C_SEND_BYTE    200
#define FCDPRO_HID_CMD_I2C_RECEIVE_BYTE 201

typedef enum 
{
	TLGE_N5_0DB=0,
	TLGE_N2_5DB=1,
	TLGE_P0_0DB=4,
	TLGE_P2_5DB=5,
	TLGE_P5_0DB=6,
	TLGE_P7_5DB=7,
	TLGE_P10_0DB=8,
	TLGE_P12_5DB=9,
	TLGE_P15_0DB=10,
	TLGE_P17_5DB=11,
	TLGE_P20_0DB=12,
	TLGE_P25_0DB=13,
	TLGE_P30_0DB=14
} fcdpro_tuner_lna_gains;

typedef enum
{
  TLEE_OFF=0,
  TLEE_0=1,
  TLEE_1=3,
  TLEE_2=5,
  TLEE_3=7
} fcdpro_tuner_lna_enhance;

typedef enum
{
  TBE_VHF2,
  TBE_VHF3,
  TBE_UHF,
  TBE_LBAND
} fcdpro_tuner_bands;

typedef enum
{
  // Band 0, VHF II
  TRFE_LPF268MHZ=0,
  TRFE_LPF299MHZ=8,
  // Band 1, VHF III
  TRFE_LPF509MHZ=0,
  TRFE_LPF656MHZ=8,
  // Band 2, UHF
  TRFE_BPF360MHZ=0,
  TRFE_BPF380MHZ=1,
  TRFE_BPF405MHZ=2,
  TRFE_BPF425MHZ=3,
  TRFE_BPF450MHZ=4,
  TRFE_BPF475MHZ=5,
  TRFE_BPF505MHZ=6,
  TRFE_BPF540MHZ=7,
  TRFE_BPF575MHZ=8,
  TRFE_BPF615MHZ=9,
  TRFE_BPF670MHZ=10,
  TRFE_BPF720MHZ=11,
  TRFE_BPF760MHZ=12,
  TRFE_BPF840MHZ=13,
  TRFE_BPF890MHZ=14,
  TRFE_BPF970MHZ=15,
  // Band 2, L band
  TRFE_BPF1300MHZ=0,
  TRFE_BPF1320MHZ=1,
  TRFE_BPF1360MHZ=2,
  TRFE_BPF1410MHZ=3,
  TRFE_BPF1445MHZ=4,
  TRFE_BPF1460MHZ=5,
  TRFE_BPF1490MHZ=6,
  TRFE_BPF1530MHZ=7,
  TRFE_BPF1560MHZ=8,
  TRFE_BPF1590MHZ=9,
  TRFE_BPF1640MHZ=10,
  TRFE_BPF1660MHZ=11,
  TRFE_BPF1680MHZ=12,
  TRFE_BPF1700MHZ=13,
  TRFE_BPF1720MHZ=14,
  TRFE_BPF1750MHZ=15
} fcdpro_rf_filters;

typedef enum
{
  TMGE_P4_0DB=0,
  TMGE_P12_0DB=1
} fcdpro_tuner_mixer_gains;

typedef enum
{
  TBCE_LBAND=0,
  TBCE_1=1,
  TBCE_2=2,
  TBCE_VUBAND=3
} fcdpro_tuner_bias_currents;

typedef enum
{
  TMFE_27_0MHZ=0,
  TMFE_4_6MHZ=8,
  TMFE_4_2MHZ=9,
  TMFE_3_8MHZ=10,
  TMFE_3_4MHZ=11,
  TMFE_3_0MHZ=12,
  TMFE_2_7MHZ=13,
  TMFE_2_3MHZ=14,
  TMFE_1_9MHZ=15
} fcdpro_tuner_mixer_filters;

typedef enum
{
  TIG1E_N3_0DB=0,
  TIG1E_P6_0DB=1
} fcdpro_tuner_if_gains1;

typedef enum
{
  TIGME_LINEARITY=0,
  TIGME_SENSITIVITY=1
} fcdpro_tuner_if_gain_modes;

typedef enum
{
  TIRFE_21_4MHZ=0,
  TIRFE_21_0MHZ=1,
  TIRFE_17_6MHZ=2,
  TIRFE_14_7MHZ=3,
  TIRFE_12_4MHZ=4,
  TIRFE_10_6MHZ=5,
  TIRFE_9_0MHZ=6,
  TIRFE_7_7MHZ=7,
  TIRFE_6_4MHZ=8,
  TIRFE_5_3MHZ=9,
  TIRFE_4_4MHZ=10,
  TIRFE_3_4MHZ=11,
  TIRFE_2_6MHZ=12,
  TIRFE_1_8MHZ=13,
  TIRFE_1_2MHZ=14,
  TIRFE_1_0MHZ=15
} fcdpro_tuner_if_rc_filters;

typedef enum
{
  TIG2E_P0_0DB=0,
  TIG2E_P3_0DB=1,
  TIG2E_P6_0DB=2,
  TIG2E_P9_0DB=3
} fcdpro_tuner_if_gains2;

typedef enum
{
  TIG3E_P0_0DB=0,
  TIG3E_P3_0DB=1,
  TIG3E_P6_0DB=2,
  TIG3E_P9_0DB=3
} fcdpro_tuner_if_gains3;

typedef enum
{
  TIG4E_P0_0DB=0,
  TIG4E_P1_0DB=1,
  TIG4E_P2_0DB=2
} fcdpro_tuner_if_gains4;

typedef enum
{
  TIFE_5_50MHZ=0,
  TIFE_5_30MHZ=1,
  TIFE_5_00MHZ=2,
  TIFE_4_80MHZ=3,
  TIFE_4_60MHZ=4,
  TIFE_4_40MHZ=5,
  TIFE_4_30MHZ=6,
  TIFE_4_10MHZ=7,
  TIFE_3_90MHZ=8,
  TIFE_3_80MHZ=9,
  TIFE_3_70MHZ=10,
  TIFE_3_60MHZ=11,
  TIFE_3_40MHZ=12,
  TIFE_3_30MHZ=13,
  TIFE_3_20MHZ=14,
  TIFE_3_10MHZ=15,
  TIFE_3_00MHZ=16,
  TIFE_2_95MHZ=17,
  TIFE_2_90MHZ=18,
  TIFE_2_80MHZ=19,
  TIFE_2_75MHZ=20,
  TIFE_2_70MHZ=21,
  TIFE_2_60MHZ=22,
  TIFE_2_55MHZ=23,
  TIFE_2_50MHZ=24,
  TIFE_2_45MHZ=25,
  TIFE_2_40MHZ=26,
  TIFE_2_30MHZ=27,
  TIFE_2_28MHZ=28,
  TIFE_2_24MHZ=29,
  TIFE_2_20MHZ=30,
  TIFE_2_15MHZ=31
} fcdpro_tuner_if_filters;

typedef enum
{
  TIG5E_P3_0DB=0,
  TIG5E_P6_0DB=1,
  TIG5E_P9_0DB=2,
  TIG5E_P12_0DB=3,
  TIG5E_P15_0DB=4
} fcdpro_tuner_if_gains5;

typedef enum
{
  TIG6E_P3_0DB=0,
  TIG6E_P6_0DB=1,
  TIG6E_P9_0DB=2,
  TIG6E_P12_0DB=3,
  TIG6E_P15_0DB=4
} fcdpro_tuner_if_gains6;

#endif // _FCDPRO_HID_CMD_H_
