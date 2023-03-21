///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_UNITS_H
#define INCLUDE_UNITS_H

#include <cmath>
#include <QString>
#include <QStringList>
#include <QRegExp>
#include <QDebug>

#include "export.h"

// Unit conversions
class SDRBASE_API Units
{
public:

    static inline float feetToMetres(float feet)
    {
        return feet * 0.3048f;
    }

    static inline int feetToIntegerMetres(float feet)
    {
        return (int)std::round(feetToMetres(feet));
    }

    static inline float metresToFeet(float metres)
    {
        return metres * 3.28084f;
    }

    static inline int metresToIntegerFeet(float metres)
    {
        return (int)std::round(metresToFeet(metres));
    }

    static inline float nauticalMilesToMetres(float nauticalMiles)
    {
        return nauticalMiles * 1855.0f;
    }

    static inline int nauticalMilesToIntegerMetres(float nauticalMiles)
    {
        return (int)std::round(nauticalMilesToMetres(nauticalMiles));
    }

    static float knotsToKPH(float knots)
    {
        return knots * 1.852f;
    }

    static int knotsToIntegerKPH(float knots)
    {
        return (int)std::round(knotsToKPH(knots));
    }

    static float knotsToMPH(float knots)
    {
        return knots * 1.15078f;
    }

    static int knotsToIntegerMPH(float knots)
    {
        return (int)std::round(knotsToMPH(knots));
    }

    static float knotsToMetresPerSecond(float knots)
    {
        return knots * 0.514444f;
    }

    static float kmpsToKPH(float kps)
    {
        return kps * (60.0 * 60.0);
    }

    static int kmpsToIntegerKPH(float kps)
    {
        return (int)std::round(kmpsToKPH(kps));
    }

    static float feetPerMinToMetresPerSecond(float fpm)
    {
        return fpm * 0.00508f;
    }

    static int feetPerMinToIntegerMetresPerSecond(float fpm)
    {
        return (int)std::round(feetPerMinToMetresPerSecond(fpm));
    }

    template <class T>
    static T degreesToRadians(T degrees)
    {
        return degrees * ((T)M_PI) / 180.0f;
    }

    template <class T>
    static T radiansToDegrees(T radians)
    {
        return radians * 180.0f / ((T)M_PI);
    }

    static float fahrenheitToCelsius(float fahrenheit)
    {
        return (fahrenheit - 32.0f) * (5.0f/9.0f);
    }

    static float celsiusToKelvin(float celsius)
    {
        return celsius + 273.15f;
    }

    static float inchesToMilimetres(float inches)
    {
        return inches * 25.4f;
    }

    static float milesToKilometres(float miles)
    {
        return miles * 1.60934f;
    }

    static float degreesMinutesSecondsToDecimal(int degrees, int minutes, float seconds)
    {
        float dec = std::abs(degrees) + minutes * 1.0f/60.0f + seconds * 1.0f/(60.0f*60.0f);
        if (degrees < 0)
            return -dec;
        else
            return dec;
    }

    static float hoursMinutesSecondsToDecimal(int hours, int minutes, float seconds)
    {
        return hours + minutes * 1.0f/60.0f + seconds * 1.0f/(60.0f*60.0f);
    }

    // Also supports decimal degrees
    static bool degreeMinuteAndSecondsToDecimalDegrees(const QString& string, float& degrees)
    {
        QRegExp decimal("(-?[0-9]+(\\.[0-9]+)?)");
        if (decimal.exactMatch(string))
        {
             degrees = decimal.capturedTexts()[1].toFloat();
             return true;
        }
        QRegExp dms(QString("(-)?([0-9]+)[%1d](([0-9]+)['m](([0-9]+(\\.[0-9]+)?)[\"s])?)?").arg(QChar(0xb0)));
        if (dms.exactMatch(string))
        {
            float d = 0.0f;
            bool neg = false;
            for (int i = 0; i < dms.captureCount(); i++) {
                qDebug() << dms.capturedTexts()[i];
            }
            if (dms.captureCount() >= 1) {
                neg = dms.capturedTexts()[1] == "-";
            }
            if (dms.captureCount() >= 3) {
                d = dms.capturedTexts()[2].toFloat();
            }
            float m = 0.0f;
            if (dms.captureCount() >= 5) {
                m = dms.capturedTexts()[4].toFloat();
            }
            float s = 0.0f;
            if (dms.captureCount() >= 7) {
                s = dms.capturedTexts()[6].toFloat();
            }
            qDebug() << neg << d << m << s;
            degrees = d + m/60.0 + s/(60.0*60.0);
            if (neg) {
                degrees = -degrees;
            }
            return true;
        }
        return false;
    }

    static QString decimalDegreesToDegreeMinutesAndSeconds(float decimal, int secondsFieldWidth=5)
    {
        double v, d, m, s;
        int neg;

        v = decimal;
        neg = v < 0.0f;
        v = fabs(v);
        d = floor(v);
        v -= d;
        v *= 60.0;
        m = floor(v);
        v -= m;
        v *= 60.0;
        s = v;
        return QString("%1%2%3%4'%5\"").arg(neg ? "-" : "").arg((int)d).arg(QChar(0xb0)).arg((int)m, 2, 10, QChar('0')).arg(s, secondsFieldWidth, 'f', 2, QChar('0'));
    }

    static QString decimalDegreesToDegreesAndMinutes(float decimal)
    {
        double v, d, m;
        int neg;

        v = decimal;
        neg = v < 0.0f;
        v = fabs(v);
        d = floor(v);
        v -= d;
        v *= 60.0;
        m = round(v);
        if (m == 60)
        {
            if (neg) {
                d--;
            } else {
                d++;
            }
            m = 0;
        }
        return QString("%1%2%3%4'").arg(neg ? "-" : "").arg((int)d).arg(QChar(0xb0)).arg((int)m, 2, 10, QChar('0'));
    }

    static QString decimalDegreesToDegrees(float decimal)
    {
        double v, d;
        int neg;

        v = decimal;
        neg = v < 0.0f;
        v = fabs(v);
        d = round(v);
        return QString("%1%2%3").arg(neg ? "-" : "").arg((int)d).arg(QChar(0xb0));
    }

    static QString decimalHoursToHoursMinutesAndSeconds(float decimal, int precision=2)
    {
        double v, h, m, s;

        v = decimal;
        v = fabs(v);
        h = floor(v);
        v -= h;
        v *= 60.0;
        m = floor(v);
        v -= m;
        v *= 60.0;
        s = v;
        return QString("%1h%2m%3s").arg((int)h).arg((int)m, 2, 10, QChar('0')).arg(s, 2, 'f', precision, QChar('0'));
    }

    // Try to convert a string to latitude and longitude. Returns false if not recognised format.
    // https://en.wikipedia.org/wiki/ISO_6709 specifies a standard syntax
    // We support both decimal and DMS formats
    static bool stringToLatitudeAndLongitude(const QString& string, float& latitude, float& longitude)
    {
        QRegExp decimal("(-?[0-9]+(\\.[0-9]+)?) *,? *(-?[0-9]+(\\.[0-9]+)?)");
        if (decimal.exactMatch(string))
        {
             latitude = decimal.capturedTexts()[1].toFloat();
             longitude = decimal.capturedTexts()[3].toFloat();
             return true;
        }
        QRegExp dms(QString("([0-9]+)[%1d]([0-9]+)['m]([0-9]+(\\.[0-9]+)?)[\"s]([NS]) *,? *([0-9]+)[%1d]([0-9]+)['m]([0-9]+(\\.[0-9]+)?)[\"s]([EW])").arg(QChar(0xb0)));
        if (dms.exactMatch(string))
        {
             float latD = dms.capturedTexts()[1].toFloat();
             float latM = dms.capturedTexts()[2].toFloat();
             float latS = dms.capturedTexts()[3].toFloat();
             bool north = dms.capturedTexts()[5] == "N";
             float lonD = dms.capturedTexts()[6].toFloat();
             float lonM = dms.capturedTexts()[7].toFloat();
             float lonS = dms.capturedTexts()[8].toFloat();
             bool east = dms.capturedTexts()[10] == "E";
             latitude = latD + latM/60.0 + latS/(60.0*60.0);
             if (!north)
                 latitude = -latitude;
             longitude = lonD + lonM/60.0 + lonS/(60.0*60.0);
             if (!east)
                 longitude = -longitude;
             return true;
        }
        QRegExp dms2(QString("([0-9]+)([NS])([0-9]{2})([0-9]{2}) *,?([0-9]+)([EW])([0-9]{2})([0-9]{2})"));
        if (dms2.exactMatch(string))
        {
             float latD = dms2.capturedTexts()[1].toFloat();
             bool north = dms2.capturedTexts()[2] == "N";
             float latM = dms2.capturedTexts()[3].toFloat();
             float latS = dms2.capturedTexts()[4].toFloat();
             float lonD = dms2.capturedTexts()[5].toFloat();
             bool east = dms2.capturedTexts()[6] == "E";
             float lonM = dms2.capturedTexts()[7].toFloat();
             float lonS = dms2.capturedTexts()[8].toFloat();
             latitude = latD + latM/60.0 + latS/(60.0*60.0);
             if (!north)
                 latitude = -latitude;
             longitude = lonD + lonM/60.0 + lonS/(60.0*60.0);
             if (!east)
                 longitude = -longitude;
             return true;
        }
        // 512255.5900N 0024400.6105W as used on aviation charts
        QRegExp dms3(QString("(\\d{2})(\\d{2})((\\d{2})(\\.\\d+)?)([NS]) *,?(\\d{3})(\\d{2})((\\d{2})(\\.\\d+)?)([EW])"));
        if (dms3.exactMatch(string))
        {
             float latD = dms3.capturedTexts()[1].toFloat();
             float latM = dms3.capturedTexts()[2].toFloat();
             float latS = dms3.capturedTexts()[3].toFloat();
             bool north = dms3.capturedTexts()[6] == "N";
             float lonD = dms3.capturedTexts()[7].toFloat();
             float lonM = dms3.capturedTexts()[8].toFloat();
             float lonS = dms3.capturedTexts()[9].toFloat();
             bool east = dms3.capturedTexts()[12] == "E";
             latitude = latD + latM/60.0 + latS/(60.0*60.0);
             if (!north)
                 latitude = -latitude;
             longitude = lonD + lonM/60.0 + lonS/(60.0*60.0);
             if (!east)
                 longitude = -longitude;
             return true;
        }
        return false;
    }

    static float solarFluxUnitsToJansky(float sfu)
    {
        return sfu * 10000.0f;
    }

    template <class T>
    static T solarFluxUnitsToWattsPerMetrePerHertz(T sfu)
    {
        return sfu * 1e-22f;
    }

    template <class T>
    static T wattsPerMetrePerHertzToSolarFluxUnits(T w)
    {
        return w / 1e-22f;
    }

    template <class T>
    static T wattsPerMetrePerHertzToJansky(T w)
    {
        return w / 1e-26f;
    }

    template <class T>
    static T noiseFigureToNoiseTemp(T nfdB, T refTempK=T(290.0))
    {
        return refTempK * (std::pow(T(10.0), nfdB/T(10.0)) - T(1.0));
    }

    template <class T>
    static T noiseTempToNoiseFigureTo(T tempK, T refTempK=T(290.0))
    {
        return T(10.0) * std::log10(tempK/refTempK+T(1.0));
    }

};

#endif // INCLUDE_UNITS_H
