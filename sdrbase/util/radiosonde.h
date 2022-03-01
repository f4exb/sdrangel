///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_RADIOSONDE_H
#define INCLUDE_RADIOSONDE_H

#include <cstdint>
#include <algorithm>
#include <iterator>

#include <QtCore>
#include <QDateTime>

#include "util/units.h"

#include "export.h"

#define RS41_LENGTH_STD           320
#define RS41_LENGTH_EXT           518

#define RS41_OFFSET_RS            0x08
#define RS41_OFFSET_FRAME_TYPE    0x38
#define RS41_OFFSET_BLOCK_0       0x39

#define RS41_FRAME_STD            0x0f
#define RS41_FRAME_EXT            0xf0

#define RS41_ID_STATUS            0x79
#define RS41_ID_MEAS              0x7a
#define RS41_ID_GPSINFO           0x7c
#define RS41_ID_GPSRAW            0x7d
#define RS41_ID_GPSPOS            0x7b
#define RS41_ID_EMPTY             0x76

#define RS41_RS_N                 255
#define RS41_RS_K                 231
#define RS41_RS_2T                (RS41_RS_N-RS41_RS_K)
#define RS41_RS_INTERLEAVE        2
#define RS41_RS_DATA              (264/RS41_RS_INTERLEAVE)
#define RS41_RS_PAD               (RS41_RS_K-RS41_RS_DATA)

class RS41Subframe;

// Frame of data transmitted by RS41 radiosonde
class SDRBASE_API RS41Frame {
public:

    // Status
    bool m_statusValid;
    uint16_t m_frameNumber;             // Increments every frame
    QString m_serial;                   // Serial number
    float m_batteryVoltage;             // In volts
    QString m_flightPhase;              // On ground, ascent, descent
    QString m_batteryStatus;            // OK or Low
    uint8_t m_pcbTemperature;           // In degrees C
    uint16_t m_humiditySensorHeating;   // 0..1000
    uint8_t m_transmitPower;            // 0..7
    uint8_t m_maxSubframeNumber;
    uint8_t m_subframeNumber;
    QByteArray m_subframe;              // 16 bytes of subframe

    // Meas
    bool m_measValid;
    uint32_t m_tempMain;
    uint32_t m_tempRef1;
    uint32_t m_tempRef2;
    uint32_t m_humidityMain;
    uint32_t m_humidityRef1;
    uint32_t m_humidityRef2;
    uint32_t m_humidityTempMain;
    uint32_t m_humidityTempRef1;
    uint32_t m_humidityTempRef2;
    uint32_t m_pressureMain;
    uint32_t m_pressureRef1;
    uint32_t m_pressureRef2;
    float m_pressureTemp;               // Pressure sensor module temperature - In degrees C

    // GPSInfo
    bool m_gpsInfoValid;
    QDateTime m_gpsDateTime;

    // GPSPos
    bool m_posValid;
    double m_latitude;                  // In degrees
    double m_longitude;                 // In degrees
    double m_height;                    // In metres
    double m_speed;                     // In m/s
    double m_heading;                   // In degreees
    double m_verticalRate;              // In m/s
    int m_satellitesUsed;

    RS41Frame(const QByteArray ba);
    ~RS41Frame() {}
    QString toHex();
    void decodeStatus(const QByteArray ba);
    void decodeMeas(const QByteArray ba);
    void decodeGPSInfo(const QByteArray ba);
    void decodeGPSPos(const QByteArray ba);

    float getPressureFloat(const RS41Subframe *subframe);
    QString getPressureString(const RS41Subframe *subframe);
    float getTemperatureFloat(const RS41Subframe *subframe);
    QString getTemperatureString(const RS41Subframe *subframe);
    float getHumidityTemperatureFloat(const RS41Subframe *subframe);
    float getHumidityFloat(const RS41Subframe *subframe);
    QString getHumidityString(const RS41Subframe *subframe);

    static RS41Frame* decode(const QByteArray ba);
    static int getFrameLength(int frameType);

protected:
    uint16_t getUInt16(const QByteArray ba, int offset) const;
    uint32_t getUInt24(const QByteArray ba, int offset) const;
    uint32_t getUInt32(const QByteArray ba, int offset) const;

    void calcPressure(const RS41Subframe *subframe);
    void calcTemperature(const RS41Subframe *subframe);
    void calcHumidityTemperature(const RS41Subframe *subframe);
    void calcHumidity(const RS41Subframe *subframe);

    QByteArray m_bytes;

    float m_pressure;
    QString m_pressureString;
    bool m_pressureCalibrated;
    float m_temperature;
    QString m_temperatureString;
    bool m_temperatureCalibrated;
    float m_humidityTemperature;
    bool m_humidityTemperatureCalibrated;
    float m_humidity;
    QString m_humidityString;
    bool m_humidityCalibrated;

};

// RS41 subframe holding calibration data collected from multiple RS51Frames
class SDRBASE_API RS41Subframe {
public:

    RS41Subframe();
    void update(RS41Frame *message);
    bool hasTempCal() const;
    bool getTempCal(float &r1, float &r2, float *poly, float *cal) const;
    bool hasHumidityCal() const;
    bool getHumidityCal(float &c1, float &c2, float *capCal, float *calMatrix) const;
    bool hasHumidityTempCal() const;
    bool getHumidityTempCal(float &r1, float &r2, float *poly, float *cal) const;
    bool hasPressureCal() const;
    bool getPressureCal(float *cal) const;
    QString getType() const;
    QString getFrequencyMHz() const;
    QString getBurstKillStatus() const;
    QString getBurstKillTimer() const;

protected:

    bool m_subframeValid[51];
    QByteArray m_subframe;

    uint16_t getUInt16(int offset) const;
    float getFloat(int offset) const;

};

#endif // INCLUDE_RADIOSONDE_H
