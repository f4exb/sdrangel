///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
//                                                                               //
// Based on code and docs by einergehtnochrein, rs1729 and bazjo                 //
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

#include <QDateTime>
#include <QVector3D>

#include "util/radiosonde.h"
#include "util/coordinates.h"

RS41Frame::RS41Frame(const QByteArray ba) :
    m_statusValid(false),
    m_batteryVoltage(0.0),
    m_pcbTemperature(0),
    m_humiditySensorHeating(0),
    m_transmitPower(0),
    m_maxSubframeNumber(0),
    m_subframeNumber(0),
    m_measValid(false),
    m_gpsInfoValid(false),
    m_posValid(false),
    m_latitude(0.0),
    m_longitude(0.0),
    m_height(0.0),
    m_bytes(ba),
    m_pressureCalibrated(false),
    m_temperatureCalibrated(false),
    m_humidityTemperatureCalibrated(false),
    m_humidityCalibrated(false)
{
    int length = getFrameLength(ba[RS41_OFFSET_FRAME_TYPE]);
    for (int i = RS41_OFFSET_BLOCK_0; i < length; )
    {
        uint8_t blockID = ba[i+0];
        uint8_t blockLength = ba[i+1];
        switch (blockID)
        {
        case RS41_ID_STATUS:
            decodeStatus(ba.mid(i+2, blockLength));
            break;
        case RS41_ID_MEAS:
            decodeMeas(ba.mid(i+2, blockLength));
            break;
        case RS41_ID_GPSINFO:
            decodeGPSInfo(ba.mid(i+2, blockLength));
            break;
        case RS41_ID_GPSRAW:
            break;
        case RS41_ID_GPSPOS:
            decodeGPSPos(ba.mid(i+2, blockLength));
            break;
        case RS41_ID_EMPTY:
            break;
        }
        i += 2 + blockLength + 2; // ID, length, data, CRC
    }
}

QString RS41Frame::toHex()
{
    return m_bytes.toHex();
}

uint16_t RS41Frame::getUInt16(const QByteArray ba, int offset) const
{
    return (ba[offset] & 0xff)
        | ((ba[offset+1] & 0xff) << 8);
}

uint32_t RS41Frame::getUInt24(const QByteArray ba, int offset) const
{
    return (ba[offset] & 0xff)
        | ((ba[offset+1] & 0xff) << 8)
        | ((ba[offset+2] & 0xff) << 16);
}

uint32_t RS41Frame::getUInt32(const QByteArray ba, int offset) const
{
    return (ba[offset] & 0xff)
        | ((ba[offset+1] & 0xff) << 8)
        | ((ba[offset+2] & 0xff) << 16)
        | ((ba[offset+3] & 0xff) << 24);
}

void RS41Frame::decodeStatus(const QByteArray ba)
{
    m_statusValid = true;
    m_frameNumber = getUInt16(ba, 0);
    m_serial = QString(ba.mid(0x2, 8));
    m_batteryVoltage = (ba[0xa] & 0xff) / 10.0;
    QStringList phases = {"Ground", "Ascent", "0x2", "Descent"};
    int phase = ba[0xd] & 0x3;
    m_flightPhase = phases[phase];
    m_batteryStatus = (ba[0xe] & 0x10) == 0 ? "OK" : "Low";
    m_pcbTemperature = (ba[0x10] & 0xff);
    m_humiditySensorHeating = getUInt16(ba, 0x13);
    m_transmitPower = ba[0x15] & 0xff;
    m_maxSubframeNumber = ba[0x16] & 0xff;
    m_subframeNumber = ba[0x17] & 0xff;
    m_subframe = ba.mid(0x18, 16);
}

void RS41Frame::decodeMeas(const QByteArray ba)
{
    m_measValid = true;
    m_tempMain = getUInt24(ba, 0x0);
    m_tempRef1 = getUInt24(ba, 0x3);
    m_tempRef2 = getUInt24(ba, 0x6);
    m_humidityMain = getUInt24(ba, 0x9);
    m_humidityRef1 = getUInt24(ba, 0xc);
    m_humidityRef2 = getUInt24(ba, 0xf);
    m_humidityTempMain = getUInt24(ba, 0x12);
    m_humidityTempRef1 = getUInt24(ba, 0x15);
    m_humidityTempRef2 = getUInt24(ba, 0x18);
    m_pressureMain = getUInt24(ba, 0x1b);
    m_pressureRef1 = getUInt24(ba, 0x1e);
    m_pressureRef2 = getUInt24(ba, 0x21);
    m_pressureTemp = getUInt16(ba, 0x26) / 100.0f;
}

void RS41Frame::decodeGPSInfo(const QByteArray ba)
{
    m_gpsInfoValid = true;
    uint16_t gpsWeek = getUInt16(ba, 0x0);
    uint32_t gpsTimeOfWeek = getUInt32(ba, 0x2);  // Milliseconds
    QDateTime epoch(QDate(1980, 1, 6), QTime(0, 0, 0), Qt::OffsetFromUTC, 18); // GPS doesn't count leap seconds
    m_gpsDateTime = epoch.addDays(gpsWeek*7).addMSecs(gpsTimeOfWeek);
}

void RS41Frame::decodeGPSPos(const QByteArray ba)
{
    m_satellitesUsed = ba[0x12] & 0xff;
    if (m_satellitesUsed > 0)
    {
        m_posValid = true;
        int32_t ecefX = (int32_t)getUInt32(ba, 0x0);
        int32_t ecefY = (int32_t)getUInt32(ba, 0x4);
        int32_t ecefZ = (int32_t)getUInt32(ba, 0x8);
        // Convert cm to m
        // Convert to latitude, longitude and altitude
        Coordinates::ecefToGeodetic(ecefX / 100.0, ecefY / 100.0, ecefZ / 100.0, m_latitude, m_longitude, m_height);
        int32_t velX = (int16_t)getUInt16(ba, 0xc);
        int32_t velY = (int16_t)getUInt16(ba, 0xe);
        int32_t velZ = (int16_t)getUInt16(ba, 0x10);
        // Convert cm/s to m/s
        // Calculate speed / heading
        Coordinates::ecefVelToSpeedHeading(m_latitude, m_longitude, velX / 100.0, velY / 100.0, velZ / 100.0, m_speed, m_verticalRate, m_heading);
    }
}

// Find the water vapor saturation pressure for a given temperature.
float waterVapourSaturationPressure(float tCelsius)
{
    // Convert to Kelvin
    float T = tCelsius + 273.15f;

    // Correction
    T = - 0.4931358f
        + (1.0f + 4.6094296e-3f) * T
        - 1.3746454e-5f * T * T
        + 1.2743214e-8f * T * T * T;

    // Hyland and Wexler equation
    float p = expf(-5800.2206f / T
                  + 1.3914993f
                  + 6.5459673f * logf(T)
                  - 4.8640239e-2f * T
                  + 4.1764768e-5f * T * T
                  - 1.4452093e-8f * T * T * T);

    // Scale result to hPa
    return p / 100.0f;
}

float calcT(int f, int f1, int f2, float r1, float r2, float *poly, float *cal)
{
    /*float g = (float)(f2-f1) / (r2-r1);       // gain
    float Rb = (f1*r2-f2*r1) / (float)(f2-f1); // offset
    float Rc = f/g - Rb;
    float R = Rc * cal[0];
    float T = (poly[0] + poly[1]*R + poly[2]*R*R + cal[1])*(1.0 + cal[2]);
    return T;
    */

    // Convert integer measurement to scale factor
    float s = (f-f1)/(float)(f2-f1);

    // Calculate resistance (scale between two reference resistors)
    float rUncal = r1 + (r2 - r1) * s;
    float r = rUncal * cal[0];

    // Convert resistance to temperature
    float tUncal = poly[0] + poly[1]*r + poly[2]*r*r;

    // Correct temperature (5th order polynomial)
    float tCal = 0.0f;
    for (int i = 6; i > 0; i--)
    {
        tCal *= tUncal;
        tCal += cal[i];
    }
    tCal += tUncal;

    return tCal;
}

float calcU(int cInt, int cMin, int cMax, float c1, float c2, float T, float HT, float *capCal, float *matrixCal)
{
    //qDebug() << "cInt " << cInt << " cMin " << cMin << " cMax " << cMax << " c1 " << c1 << " c2 " << c2 << " T " << T << " HT " << HT << " capCal[0] " << capCal[0] << " capCal[1] " << capCal[1];
  /*
    float a0 = 7.5f;
    float a1 = 350.0f / capCal[0];
    float fh = (cInt-cMin) / (float)(cMax-cMin);
    float rh = 100.0f * (a1*fh - a0);
    float T0 = 0.0f;
    float T1 = -25.0f;
    rh += T0 - T/5.5;
    if (T < T1) {
        rh *= 1.0 + (T1-T)/90.0;
    }
    if (rh < 0.0) {
        rh = 0.0;
    }
    if (rh > 100.0) {
        rh = 100.0;
    }
    if (T < -273.0) {
        rh = -1.0;
    }

    qDebug() << "RH old method: " << rh;  */


    // Convert integer measurement to scale factor
    float s = (cInt - cMin) / (float)(cMax-cMin);

    // Calculate capacitance (scale between two reference caps)
    float cUncal = c1 + (c2 - c1) * s;
    float cCal = (cUncal / capCal[0] - 1.0f) * capCal[1];
    float uUncal = 0.0f;
    float t = (HT - 20.0f) / 180.0f;
    float f1 = 1.0f;
    for (int i = 0; i < 7; i++)
    {
        float f2 = 1.0;
        for (int j = 0; j < 6; j++)
        {
            uUncal += f1 * f2 * matrixCal[i*6+j];
            f2 *= t;
        }
        f1 *= cCal;
    }

    // Adjust for difference in outside air temperature and the humidty sensor temperature
    float uCal = uUncal * waterVapourSaturationPressure(T) / waterVapourSaturationPressure(HT);

    // Ensure within range of 0..100%
    uCal = std::min(100.0f, uCal);
    uCal = std::max(0.0f, uCal);

    return uCal;
}

float calcP(int f, int f1, int f2, float pressureTemp, float *cal)
{
    // Convert integer measurement to scale factor
    float s = (f-f1) / (float)(f2-f1);

    float t = pressureTemp;
    float t2 = t * t;
    float t3 = t2 * t;

    float poly[6];
    poly[0] = cal[0] + cal[7] * t + cal[11] * t2 + cal[15] * t3;
    poly[1] = cal[1] + cal[8] * t + cal[12] * t2 + cal[16] * t3;
    poly[2] = cal[2] + cal[9] * t + cal[13] * t2 + cal[17] * t3;
    poly[3] = cal[3] + cal[10] * t + cal[14] * t2;
    poly[4] = cal[4];
    poly[5] = cal[5];

    float p = cal[6] / s;
    float p2 = p * p;
    float p3 = p2 * p;
    float p4 = p3 * p;
    float p5 = p4 * p;

    float pCal = poly[0] + poly[1] * p + poly[2] * p2 + poly[3] * p3 + poly[4] * p4 + poly[5] * p5;

    return pCal;
}

float RS41Frame::getPressureFloat(const RS41Subframe *subframe)
{
    if (!m_pressureCalibrated) {
        calcPressure(subframe);
    }
    return m_pressure;
}

QString RS41Frame::getPressureString(const RS41Subframe *subframe)
{
    if (!m_pressureCalibrated) {
        calcPressure(subframe);
    }
    return m_pressureString;
}

float RS41Frame::getTemperatureFloat(const RS41Subframe *subframe)
{
    if (!m_temperatureCalibrated) {
        calcTemperature(subframe);
    }
    return m_temperature;
}

QString RS41Frame::getTemperatureString(const RS41Subframe *subframe)
{
    if (!m_temperatureCalibrated) {
        calcTemperature(subframe);
    }
    return m_temperatureString;
}

void RS41Frame::calcPressure(const RS41Subframe *subframe)
{
    float cal[18];

    if (m_pressureMain == 0)
    {
        m_pressure = 0.0f;
        m_pressureString = "";
        return;
    }

    m_pressureCalibrated = subframe->getPressureCal(cal);

    m_pressure = calcP(m_pressureMain, m_pressureRef1, m_pressureRef2, m_pressureTemp, cal);

    // RS41 pressure resolution of 0.01hPa
    m_pressureString = QString::number(m_pressure, 'f', 2);

    if (!m_pressureCalibrated) {
        m_pressureString = m_pressureString + "U"; // U for uncalibrated
    }
}

void RS41Frame::calcTemperature(const RS41Subframe *subframe)
{
    float r1, r2;
    float poly[3];
    float cal[7];

    if (m_tempMain == 0)
    {
        m_temperature = 0.0f;
        m_temperatureString = "";
        return;
    }

    m_temperatureCalibrated = subframe->getTempCal(r1, r2, poly, cal);

    m_temperature = calcT(m_tempMain, m_tempRef1, m_tempRef2,
                          r1, r2,
                          poly, cal);

    // RS41 temperature resolution of 0.01C
    m_temperatureString = QString::number(m_temperature, 'f', 2);

    if (!m_temperatureCalibrated) {
        m_temperatureString = m_temperatureString + "U"; // U for uncalibrated
    }
}

float RS41Frame::getHumidityTemperatureFloat(const RS41Subframe *subframe)
{
    if (!m_humidityTemperatureCalibrated) {
        calcHumidityTemperature(subframe);
    }
    return m_humidityTemperature;
}

void RS41Frame::calcHumidityTemperature(const RS41Subframe *subframe)
{
    float r1, r2;
    float poly[3];
    float cal[7];

    if (m_humidityTempMain == 0)
    {
        m_humidityTemperature = 0.0f;
        return;
    }

    m_humidityTemperatureCalibrated = subframe->getHumidityTempCal(r1, r2, poly, cal);

    m_humidityTemperature = calcT(m_humidityTempMain, m_humidityTempRef1, m_humidityTempRef2,
                                  r1, r2,
                                  poly, cal);
}

float RS41Frame::getHumidityFloat(const RS41Subframe *subframe)
{
    if (!m_humidityCalibrated) {
        calcHumidity(subframe);
    }
    return m_humidity;
}

QString RS41Frame::getHumidityString(const RS41Subframe *subframe)
{
    if (!m_humidityCalibrated) {
        calcHumidity(subframe);
    }
    return m_humidityString;
}

void RS41Frame::calcHumidity(const RS41Subframe *subframe)
{
    float c1, c2;
    float capCal[2];
    float calMatrix[7*6];

    if (m_humidityMain == 0)
    {
        m_humidity = 0.0f;
        m_humidityString = "";
        return;
    }

    float temperature = getTemperatureFloat(subframe);
    float humidityTemperature = getHumidityTemperatureFloat(subframe);

    bool humidityCalibrated = subframe->getHumidityCal(c1, c2, capCal, calMatrix);

    m_humidityCalibrated = m_temperatureCalibrated && m_humidityTemperatureCalibrated && humidityCalibrated;

    m_humidity = calcU(m_humidityMain, m_humidityRef1, m_humidityRef2,
                       c1, c2,
                       temperature, humidityTemperature,
                       capCal, calMatrix);

    // RS41 humidity resolution of 0.1%
    m_humidityString = QString::number(m_humidity, 'f', 1);

    if (!m_humidityCalibrated) {
        m_humidityString = m_humidityString + "U"; // U for uncalibrated
    }
}

RS41Frame* RS41Frame::decode(const QByteArray ba)
{
    return new RS41Frame(ba);
}

int RS41Frame::getFrameLength(int frameType)
{
    return frameType == RS41_FRAME_STD ? RS41_LENGTH_STD : RS41_LENGTH_EXT;
}

RS41Subframe::RS41Subframe() :
    m_subframe(51*16, (char)0)
{
    for (int i = 0; i < 51; i++) {
        m_subframeValid[i] = false;
    }
}

// Update subframe with subframe data from received message
void RS41Subframe::update(RS41Frame *message)
{
    m_subframeValid[message->m_subframeNumber] = true;
    int offset = message->m_subframeNumber * 16;
    for (int i = 0; i < 16; i++) {
        m_subframe[offset+i] = message->m_subframe[i];
    }
}

// Indicate if we have all the required temperature calibration data
bool RS41Subframe::hasTempCal() const
{
    return m_subframeValid[3] && m_subframeValid[4] && m_subframeValid[5] && m_subframeValid[6] && m_subframeValid[7];
}

// Get temperature calibration data
// r1, r2 - Temperature reference resistances (Ohms)
// poly - Resistance to temperature 2nd order polynomial
bool RS41Subframe::getTempCal(float &r1, float &r2, float *poly, float *cal) const
{
    if (hasTempCal())
    {
        r1 = getFloat(0x3d);
        r2 = getFloat(0x41);
        for (int i = 0; i < 3; i++) {
            poly[i] = getFloat(0x4d + i * 4);
        }
        for (int i = 0; i < 7; i++) {
            cal[i] = getFloat(0x59 + i * 4);
        }
        return true;
    }
    else
    {
        // Use default values
        r1 = 750.0f;
        r2 = 1100.0f;
        poly[0] = -243.9108f;
        poly[1] = 0.187654f;
        poly[2] = 8.2e-06f;
        cal[0] = 1.279928f;
        for (int i = 1; i < 7; i++) {
            cal[i] = 0.0f;
        }
        return false;
    }
}

// Indicate if we have all the required humidty calibration data
bool RS41Subframe::hasHumidityCal() const
{
    return m_subframeValid[4] && m_subframeValid[7]
            && m_subframeValid[8] && m_subframeValid[9] && m_subframeValid[0xa] && m_subframeValid[0xb]
            && m_subframeValid[0xc] && m_subframeValid[0xd] && m_subframeValid[0xe] && m_subframeValid[0xf]
            && m_subframeValid[0x10] && m_subframeValid[0x11] && m_subframeValid[0x12];
}

// Get humidty calibration data
bool RS41Subframe::getHumidityCal(float &c1, float &c2, float *capCal, float *calMatrix) const
{
    if (hasHumidityCal())
    {
        c1 = getFloat(0x45);
        c2 = getFloat(0x49);
        for (int i = 0; i < 2; i++) {
            capCal[i] = getFloat(0x75 + i * 4);
        }
        for (int i = 0; i < 7*6; i++) {
            calMatrix[i] = getFloat(0x7d + i * 4);
        }
        return true;
    }
    else
    {
        // Use default values
        c1 = 0.0f;
        c2 = 47.0f;
        capCal[0] = 45.9068f;
        capCal[1] = 4.92924f;
        static const float calMatrixDefault[7*6] = {
             -0.002586f, -2.24367f,   9.92294f,   -3.61913f,   54.3554f,    -93.3012f,
             51.7056f,   38.8709f,    209.437f,   -378.437f,   9.17326f,    19.5301f,
             150.257f,   -150.907f,   -280.315f,  182.293f,    3247.39f,    4083.65f,
             -233.568f,  345.375f,    200.217f,   -388.246f,   -3617.66f,   0.0f,
             225.841f,   -233.051f,   0.0f,       0.0f,        0.0f,        0.0f,
             -93.0635f,  0.0f,        0.0f,       0.0f,        0.0f,        0.0f,
              0.0f,      0.0f,        0.0f,       0.0f,        0.0f,        0.0f
        };
        std::copy(calMatrixDefault, calMatrixDefault + 7*6, calMatrix);
        return false;
    }
}

// Indicate if we have all the required humidty temperature sensor calibration data
bool RS41Subframe::hasHumidityTempCal() const
{
    return m_subframeValid[3] && m_subframeValid[4] && m_subframeValid[0x12] && m_subframeValid[0x13] && m_subframeValid[0x14];
}

// Get humidty temperature sensor calibration data
bool RS41Subframe::getHumidityTempCal(float &r1, float &r2, float *poly, float *cal) const
{
    if (hasHumidityTempCal())
    {
        r1 = getFloat(0x3d);
        r2 = getFloat(0x41);
        for (int i = 0; i < 3; i++) {
             poly[i] = getFloat(0x125 + i * 4);
        }
        for (int i = 0; i < 7; i++) {
            cal[i] = getFloat(0x131 + i * 4);
        }
        return true;
    }
    else
    {
        // Use default values
        r1 = 750.0f;
        r2 = 1100.0f;
        poly[0] = -243.9108f;
        poly[1] = 0.187654f;
        poly[2] = 8.2e-06f;
        cal[0] = 1.279928f;
        for (int i = 1; i < 7; i++) {
            cal[i] = 0.0f;
        }
        return false;
    }
}

// Indicate if we have all the required pressure calibration data
bool RS41Subframe::hasPressureCal() const
{
    return m_subframeValid[0x25] && m_subframeValid[0x26] && m_subframeValid[0x27]
            && m_subframeValid[0x28] && m_subframeValid[0x29] && m_subframeValid[0x2a];
}

// Get pressure calibration data
bool RS41Subframe::getPressureCal(float *cal) const
{
    if (hasPressureCal())
    {
        for (int i = 0; i < 18; i++) {
            cal[i] = getFloat(0x25e + i * 4);
        }
        return true;
    }
    else
    {
        // Use default values - TODO: Need to obtain from inflight device
        for (int i = 0; i < 18; i++) {
            cal[i] = 0.0f;
        }
        return false;
    }
}

// Get type of RS41. E.g. "RS41-SGP"
QString RS41Subframe::getType() const
{
    if (m_subframeValid[0x21] & m_subframeValid[0x22])
    {
        return QString(m_subframe.mid(0x218, 10)).trimmed();
    }
    else
    {
        return "RS41";
    }
}

// Get transmission frequency in MHz
QString RS41Subframe::getFrequencyMHz() const
{
    if (m_subframeValid[0])
    {
        uint8_t lower = m_subframe[2] & 0xff;
        uint8_t upper = m_subframe[3] & 0xff;
        float freq = 400.0 + (upper + (lower / 255.0)) * 0.04;
        return QString::number(freq, 'f', 3);
    }
    else
    {
        return "";
    }
}

QString RS41Subframe::getBurstKillStatus() const
{
    if (m_subframeValid[2])
    {
        uint8_t status = m_subframe[0x2b];
        return status == 0 ? "Inactive" : "Active";
    }
    else
    {
        return "";
    }
}

// Seconds until power-off once active
QString RS41Subframe::getBurstKillTimer() const
{
    if (m_subframeValid[0x31])
    {
        uint16_t secs = getUInt16(0x316);
        QTime t(0, 0, 0);
        t = t.addSecs(secs);
        return t.toString("hh:mm:ss");
    }
    else
    {
        return "";
    }
}

uint16_t RS41Subframe::getUInt16(int offset) const
{
    return (m_subframe[offset] & 0xff) | ((m_subframe[offset+1] & 0xff) << 8);
}

float RS41Subframe::getFloat(int offset) const
{
    float f;
    // Assumes host is little endian with 32-bit float
    memcpy(&f, m_subframe.data() + offset, 4);
    return f;
}
