///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_UTIL_DSC_H
#define INCLUDE_UTIL_DSC_H

#include "export.h"

#include <QByteArray>
#include <QString>
#include <QDateTime>

// Digital Select Calling
// https://www.itu.int/dms_pubrec/itu-r/rec/m/R-REC-M.493-15-201901-I!!PDF-E.pdf

class SDRBASE_API DSCDecoder {

public:

    void init(int offset);
    bool decodeBits(int bits);
    QByteArray getMessage() const { return m_bytes; }
    int getErrors() const { return m_errors; }

    static int m_maxBytes;

private:

    static const int BUFFER_SIZE = 3;
    signed char m_buf[3];
    enum State {
        PHASING,
        FILL_DX,
        FILL_RX,
        DX,
        RX,
        DX_EOS,
        RX_EOS,
        DONE,
        NO_EOS
    } m_state;
    int m_idx;
    int m_errors;
    int m_phaseIdx;
    bool m_eos;
    static const signed char m_expectedSymbols[];

    QByteArray m_bytes;

    bool decodeSymbol(signed char symbol);
    static signed char bitsToSymbol(unsigned int bits);
    static unsigned char reverse(unsigned char b);
    signed char selectSymbol(signed char dx, signed char rx);

};

class SDRBASE_API DSCMessage {
public:

    enum FormatSpecifier {
        GEOGRAPHIC_CALL = 102,
        DISTRESS_ALERT = 112,
        GROUP_CALL = 114,
        ALL_SHIPS = 116,
        SELECTIVE_CALL = 120,
        AUTOMATIC_CALL = 123
    };

    enum Category {
        ROUTINE = 100,
        SAFETY = 108,
        URGENCY = 110,
        DISTRESS = 112
    };

    enum FirstTelecommand {
        F3E_G3E_ALL_MODES_TP = 100,
        F3E_G3E_DUPLEX_TP = 101,
        POLLING = 103,
        UNABLE_TO_COMPLY = 104,
        END_OF_CALL = 105,
        DATA = 106,
        J3E_TP = 109,
        DISTRESS_ACKNOWLEDGEMENT = 110,
        DISTRESS_ALERT_RELAY = 112,
        F1B_J2B_TTY_FEC = 113,
        F1B_J2B_TTY_AQR = 115,
        TEST = 118,
        POSITION_UPDATE = 121,
        NO_INFORMATION = 126
    };

    enum SecondTelecommand {
        NO_REASON = 100,
        CONGESTION = 101,
        BUSY = 102,
        QUEUE = 103,
        BARRED = 104,
        NO_OPERATOR = 105,
        OPERATOR_UNAVAILABLE = 106,
        EQUIPMENT_DISABLED = 107,
        UNABLE_TO_USE_CHANNEL = 108,
        UNABLE_TO_USE_MODE = 109,
        NOT_PARTIES_TO_CONFLICT = 110,
        MEDICAL_TRANSPORTS = 111,
        PAY_PHONE = 112,
        FAX = 113,
        NO_INFORMATION_2 = 126
    };

    enum DistressNature {
        FIRE = 100,
        FLOODING = 101,
        COLLISION = 102,
        GROUNDING = 103,
        LISTING = 104,
        SINKING = 105,
        ADRIFT = 106,
        UNDESIGNATED = 107,
        ABANDONING_SHIP = 108,
        PIRACY = 109,
        MAN_OVERBOARD = 110,
        EPIRB = 112
    };

    enum EndOfSignal {
        REQ = 117,
        ACK = 122,
        EOS = 127
    };

    static QMap<FormatSpecifier, QString> m_formatSpecifierStrings;
    static QMap<FormatSpecifier, QString> m_formatSpecifierShortStrings;
    static QMap<Category, QString> m_categoryStrings;
    static QMap<Category, QString> m_categoryShortStrings;
    static QMap<FirstTelecommand, QString> m_telecommand1Strings;
    static QMap<FirstTelecommand, QString> m_telecommand1ShortStrings;
    static QMap<SecondTelecommand, QString> m_telecommand2Strings;
    static QMap<SecondTelecommand, QString> m_telecommand2ShortStrings;
    static QMap<DistressNature, QString> m_distressNatureStrings;
    static QMap<EndOfSignal, QString> m_endOfSignalStrings;
    static QMap<EndOfSignal, QString> m_endOfSignalShortStrings;

    FormatSpecifier m_formatSpecifier;
    bool m_formatSpecifierMatch;
    QString m_address;
    bool m_hasAddress;
    int m_addressLatitude;      // For GEOGRAPHIC_CALL
    int m_addressLongitude;
    int m_addressLatAngle;
    int m_addressLonAngle;

    Category m_category;
    bool m_hasCategory;
    QString m_selfId;
    FirstTelecommand m_telecommand1;
    bool m_hasTelecommand1;
    SecondTelecommand m_telecommand2;
    bool m_hasTelecommand2;

    QString m_distressId;
    bool m_hasDistressId;

    DistressNature m_distressNature;
    bool m_hasDistressNature;

    QString m_position;
    bool m_hasPosition;

    int m_frequency1;  // Rx
    bool m_hasFrequency1;
    QString m_channel1;
    bool m_hasChannel1;
    int m_frequency2;   // Tx
    bool m_hasFrequency2;
    QString m_channel2;
    bool m_hasChannel2;

    QString m_number; // Phone number
    bool m_hasNumber;

    QTime m_time;
    bool m_hasTime;

    FirstTelecommand m_subsequenceComms;
    bool m_hasSubsequenceComms;

    EndOfSignal m_eos;
    signed char m_ecc; // Error checking code (parity)
    signed char m_calculatedECC;
    bool m_eccOk;
    bool m_valid; // Data is within defined values

    QDateTime m_dateTime; // Date/time when received
    QByteArray m_data;

    DSCMessage(const QByteArray& data, QDateTime dateTime);
    QString toString(const QString separator = " ") const;
    QString toYaddNetFormat(const QString& id, qint64 frequency) const;
    QString formatSpecifier(bool shortString=false) const;
    QString category(bool shortString=false) const;

    static QString telecommand1(FirstTelecommand telecommand, bool shortString=false);
    static QString telecommand2(SecondTelecommand telecommand, bool shortString=false);
    static QString distressNature(DistressNature nature);
    static QString endOfSignal(EndOfSignal eos, bool shortString=false);

protected:

    QString symbolsToDigits(const QByteArray data, int startIdx, int length);
    QString formatCoordinates(int latitude, int longitude);
    void decode(const QByteArray& data);
    void checkECC(const QByteArray& data);
    void decodeFrequency(const QByteArray& data, int& idx, int& frequency, QString& channel);
    QString formatAddress(const QString &address) const;
    QString formatCoordinates(const QString& coords);

};

#endif /* INCLUDE_UTIL_DSC_H */
