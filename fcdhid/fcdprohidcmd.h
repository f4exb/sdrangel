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

#endif // _FCDPRO_HID_CMD_H_
