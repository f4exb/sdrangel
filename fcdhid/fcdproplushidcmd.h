#ifndef _FCD_PRO_PLUS_HID_CMD_H_
#define _FCD_PRO_PLUS_HID_CMD_H_

#define FCDPROPLUS_HID_CMD_SET_LNA_GAIN     110 // Send one byte, 1 on, 0 off
#define FCDPROPLUS_HID_CMD_SET_RF_FILTER    113 // Send one byte enum, see TUNERRFFILTERENUM
#define FCDPROPLUS_HID_CMD_SET_MIXER_GAIN   114 // Send one byte, 1 on, 0 off
#define FCDPROPLUS_HID_CMD_SET_IF_GAIN      117 // Send one byte value, valid value 0 to 59 (dB)
#define FCDPROPLUS_HID_CMD_SET_IF_FILTER    122 // Send one byte enum, see TUNERIFFILTERENUM
#define FCDPROPLUS_HID_CMD_SET_BIAS_TEE     126 // Send one byte, 1 on, 0 off

#define FCDPROPLUS_HID_CMD_GET_LNA_GAIN     150 // Returns one byte, 1 on, 0 off
#define FCDPROPLUS_HID_CMD_GET_RF_FILTER    153 // Returns one byte enum, see TUNERRFFILTERENUM
#define FCDPROPLUS_HID_CMD_GET_MIXER_GAIN   154 // Returns one byte, 1 on, 0 off
#define FCDPROPLUS_HID_CMD_GET_IF_GAIN      157 // Returns one byte value, valid value 0 to 59 (dB)
#define FCDPROPLUS_HID_CMD_GET_IF_FILTER    162 // Returns one byte enum, see TUNERIFFILTERENUM
#define FCDPROPLUS_HID_CMD_GET_BIAS_TEE     166 // Returns one byte, 1 on, 0 off

#define FCD_RESET                    255 // Reset to bootloader

typedef enum 
{
	TRFE_0_4,
	TRFE_4_8,
	TRFE_8_16,
	TRFE_16_32,
	TRFE_32_75,
	TRFE_75_125,
	TRFE_125_250,
	TRFE_145,
	TRFE_410_875,
	TRFE_435,
	TRFE_875_2000
} fcdproplus_tuner_rf_filters;

typedef enum
{
  TIFE_200KHZ=0,
  TIFE_300KHZ=1,
  TIFE_600KHZ=2,
  TIFE_1536KHZ=3,
  TIFE_5MHZ=4,
  TIFE_6MHZ=5,
  TIFE_7MHZ=6,
  TIFE_8MHZ=7
} fcdproplus_tuner_if_filters;

#endif // _FCD_PRO_PLUS_HID_CMD_H_
