///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_DO219_H
#define INCLUDE_DO219_H

// RCTA DO-219 / CPDLC decoder
class DO219Decoder {
    QByteArray m_bytes;
    int m_byteIndex;
    int m_bitIndex;
    bool m_cpdlc;
    bool m_overrun;
public:
    QString m_msgIdentificationNumber;
    QString m_msgReferenceNumber;
    QTime m_timeStamp;

    DO219Decoder() :
        m_byteIndex(0),
        m_bitIndex(0),
        m_cpdlc(true),
        m_overrun(false)
    {
    }

    QString decode(const QString &msg, bool uplink)
    {
        if (msg.isEmpty()) {
            return "";
        }
        m_bytes = QByteArray::fromHex(msg.toLatin1());
        QString s = uplink ? decodeATCUplinkMessage() : decodeATCDownlinkMessage();
        if (m_overrun) {
            s.append("\n(message truncated)");
        }
        return s;
    }

protected:
    int getBit()
    {
        // Data short of what the elements declare is routine, not just corrupt: each
        // block of a multi-block message is decoded on its own before reassembly. Past
        // the end, read zeros - which also ends any element count read from them.
        if (m_byteIndex >= m_bytes.size())
        {
            if (!m_overrun) {
                qDebug() << "DO219Decoder::getBit: Attempt to read beyond end of data";
            }
            m_overrun = true;
            return 0;
        }
        int bit = (m_bytes[m_byteIndex] >> (7 - m_bitIndex)) & 1;
        m_bitIndex++;
        if (m_bitIndex == 8)
        {
            m_bitIndex = 0;
            m_byteIndex++;
        }
        return bit;
    }

    int getBits(int numberOfBits)
    {
        int bits = 0;
        for (int i = 0; i < numberOfBits; i++)
        {
            bits <<= 1;
            bits |= getBit();
        }
        return bits;
    }

    QString decodeATCDownlinkMessage()
    {
        bool flag = getBits(1);

        QString s;
        decodeATCMessageHeader();
        s.append(decodeATCDownlinkMsgElementId());
        if (flag)
        {
            int size = getBits(2) + 1;
            for (int i = 0; i < size; i++) {
                s.append("\n" + decodeATCDownlinkMsgElementId());
            }
        }
        return s;
    }
    QString decodeATCUplinkMessage()
    {
        bool flag = getBits(1);
        QString s;
        decodeATCMessageHeader();
        s.append(decodeATCUplinkMsgElementId());
        if (flag)
        {
            int size = getBits(2) + 1;
            for (int i = 0; i < size; i++) {
                s.append("\n" + decodeATCUplinkMsgElementId());
            }
        }
        return s;
    }

    void decodeATCMessageHeader()
    {
        bool flag = getBits(1);
        bool flag1 = false;
        if (m_cpdlc) {
            flag1 = getBits(1);
        }
        m_msgIdentificationNumber = decodeMsgIdentificationNumber();
        if (flag) {
            m_msgReferenceNumber = decodeMsgReferenceNumber();
        }
        if (flag1) {
            m_timeStamp = QTime::fromString(decodeTimeHHMMSS());
        }
    }

    QString decodeTimeHHMMSS()
    {
        QString hours = decodeTimeHours();
        QString minutes = decodeTimeMinutes();
        QString seconds = decodeTimeSeconds();
        return hours + ":" + minutes + ":" + seconds;
    }

    QString decodeMsgIdentificationNumber()
    {
        int num = getBits(6);
        return QString::number(num);
    }

    QString decodeMsgReferenceNumber()
    {
        int num = getBits(6);
        return QString::number(num);
    }

    QString decodeATCUplinkMsgElementId()
    {
        int id = getBits(8);
        switch (id)
        {
        case 0:
            return "UNABLE";
        case 1:
            return "STANDBY";
        case 2:
            return "REQUEST DEFERRED";
        case 3:
            return "ROGER";
        case 4:
            return "AFFIRM";
        case 5:
            return "NEGATIVE";
        case 6:
            return "EXPECT " + decodeAltitude();
        case 7:
            return "EXPECT CLIMB AT " + decodeTime();
        case 8:
            return "EXPECT CLIMB AT " + decodePosition();
        case 9:
            return "EXPECT DESCENT AT " + decodeTime();
        case 10:
            return "EXPECT DESCENT AT " + decodePosition();
        case 11:
            return "EXPECT CRUISE CLIMB AT " + decodeTime();
        case 12:
            return "EXPECT CRUISE CLIMB AT " + decodePosition();
        case 13:
        {
            QString time = decodeTime();
            QString altitude = decodeAltitude();
            return "AT " + time + " EXPECT CLIMB TO " + altitude;
        }
        case 14:
        {
            QString position = decodePosition();
            QString altitude = decodeAltitude();
            return "AT " + position + " EXPECT CLIMB TO " + altitude;
        }
        case 15:
        {
            QString time = decodeTime();
            QString altitude = decodeAltitude();
            return "AT " + time + " EXPECT DESCENT TO " + altitude;
        }
        case 16:
        {
            QString position = decodePosition();
            QString altitude = decodeAltitude();
            return "AT " + position + " EXPECT DESCENT TO " + altitude;
        }
        case 17:
        {
            QString time = decodeTime();
            QString altitude = decodeAltitude();
            return "AT " + time + " EXPECT CRUISE CLIMB TO " + altitude;
        }
        case 18:
        {
            QString position = decodePosition();
            QString altitude = decodeAltitude();
            return "AT " + position + " EXPECT CRUISE CLIMB TO " + altitude;
        }
        case 19:
            return "MAINTAIN " + decodeAltitude();
        case 20:
            return "CLIMB AND MAINTAIN " + decodeAltitude();
        case 21:
        {
            QString time = decodeTime();
            QString altitude = decodeAltitude();
            return "AT " + time + " CLIMB AND MAINTAIN " + altitude;
        }
        case 22:
        {
            QString position = decodePosition();
            QString altitude = decodeAltitude();
            return "AT " + position + " CLIMB AND MAINTAIN " + altitude;
        }
        case 23:
            return "DESCEND AND MAINTAIN " + decodeAltitude();
        case 24:
        {
            QString time = decodeTime();
            QString altitude = decodeAltitude();
            return "AT " + time + " DESCEND AND MAINTAIN " + altitude;
        }
        case 25:
        {
            QString position = decodePosition();
            QString altitude = decodeAltitude();
            return "AT " + position + " DESCEND AND MAINTAIN " + altitude;
        }
        case 26:
        {
            QString altitude = decodeAltitude();
            QString time = decodeTime();
            return "CLIMB TO REACH " + altitude + " BY " + time;
        }
        case 27:
        {
            QString altitude = decodeAltitude();
            QString position = decodePosition();
            return "CLIMB TO REACH " + altitude + " BY " + position;
        }
        case 28:
        {
            QString altiutude = decodeAltitude();
            QString time = decodeTime();
            return "DESCEND TO REACH " + altiutude + " BY " + time;
        }
        case 29:
        {
            QString altiutude = decodeAltitude();
            QString position = decodePosition();
            return "DESCEND TO REACH " + altiutude + " BY " + position;
        }
        case 30:
        {
            QString altiutude1 = decodeAltitude();
            QString altiutude2 = decodeAltitude();
            return "MAINTAIN BLOCK " + altiutude1 + " TO " + altiutude2;
        }
        case 31:
        {
            QString altiutude1 = decodeAltitude();
            QString altiutude2 = decodeAltitude();
            return "CLIMB TO MAINTAIN BLOCK " + altiutude1 + " TO " + altiutude2;
        }
        case 32:
        {
            QString altiutude1 = decodeAltitude();
            QString altiutude2 = decodeAltitude();
            return "DESCEND TO MAINTAIN BLOCK " + altiutude1 + " TO " + altiutude2;
        }
        case 33:
            return "CRUISE " + decodeAltitude();
        case 34:
            return "CRUISE CLIMB TO " + decodeAltitude();
        case 35:
            return "CRUISE CLIMB ABOVE " + decodeAltitude();
        case 36:
            return "EXPEDITE CLIMB TO " + decodeAltitude();
        case 37:
            return "EXPEDITE CLIMB ABOVE " + decodeAltitude();
        case 38:
            return "IMMEDIATELY CLIMB TO " + decodeAltitude();
        case 39:
            return "IMMEDIATELY DESCEND TO " + decodeAltitude();
        case 40:
            return "IMMEDIATELY STOP CLIMB AT " + decodeAltitude();
        case 41:
            return "IMMEDIATELY STOP DESCENT AT " + decodeAltitude();
        case 42:
        {
            QString position = decodePosition();
            QString altitude = decodeAltitude();
            return "EXPECT TO CROSS " + position + " AT " + altitude;
        }
        case 43:
        {
            QString position = decodePosition();
            QString altitude = decodeAltitude();
            return "EXPECT TO CROSS " + position + " AT OR ABOVE " + altitude;
        }
        case 44:
        {
            QString position = decodePosition();
            QString altitude = decodeAltitude();
            return "EXPECT TO CROSS " + position + " AT OR BELOW " + altitude;
        }
        case 45:
        {
            QString position = decodePosition();
            QString altitude = decodeAltitude();
            return "EXPECT TO CROSS " + position + " AT AND MAINTAIN " + altitude;
        }
        case 46:
        {
            QString position = decodePosition();
            QString altitude = decodeAltitude();
            return "CROSS " + position + " AT " + altitude;
        }
        case 47:
        {
            QString position = decodePosition();
            QString altitude = decodeAltitude();
            return "CROSS " + position + " AT OR ABOVE " + altitude;
        }
        case 48:
        {
            QString position = decodePosition();
            QString altitude = decodeAltitude();
            return "CROSS " + position + " AT OR BELOW " + altitude;
        }
        case 49:
        {
            QString position = decodePosition();
            QString altitude = decodeAltitude();
            return "CROSS " + position + " AT AND MAINTAIN " + altitude;
        }
        case 50:
        {
            QString position = decodePosition();
            QString altitude1 = decodeAltitude();
            QString altitude2 = decodeAltitude();
            return "CROSS " + position + " BETWEEN " + altitude1 + " AND " + altitude2;
        }
        case 51:
        {
            QString position = decodePosition();
            QString time = decodeTime();
            return "CROSS " + position + " AT " + time;
        }
        case 52:
        {
            QString position = decodePosition();
            QString time = decodeTime();
            return "CROSS " + position + " AT OR BEFORE " + time;
        }
        case 53:
        {
            QString position = decodePosition();
            QString time = decodeTime();
            return "CROSS " + position + " AT OR AFTER " + time;
        }
        case 54:
        {
            QString position = decodePosition();
            QString time1 = decodeTime();
            QString time2 = decodeTime();
            return "CROSS " + position + " BETWEEN " + time1 + " AND " + time2;
        }
        case 55:
        {
            QString position = decodePosition();
            QString speed = decodeSpeed();
            return "CROSS " + position + " AT " + speed;
        }
        case 56:
        {
            QString position = decodePosition();
            QString speed = decodeSpeed();
            return "CROSS " + position + " AT OR LESS THAN " + speed;
        }
        case 57:
        {
            QString position = decodePosition();
            QString speed = decodeSpeed();
            return "CROSS " + position + " AT OR GREATER THAN " + speed;
        }
        case 58:
        {
            QString position = decodePosition();
            QString time = decodeTime();
            QString altitude = decodeAltitude();
            return "CROSS " + position + " AT " + time + " AT " + altitude;
        }
        case 59:
        {
            QString position = decodePosition();
            QString time = decodeTime();
            QString altitude = decodeAltitude();
            return "CROSS " + position + " AT OR BEOFRE " + time + " AT " + altitude;
        }
        case 60:
        {
            QString position = decodePosition();
            QString time = decodeTime();
            QString altitude = decodeAltitude();
            return "CROSS " + position + " AT OR AFTER " + time + " AT " + altitude;
        }
        case 61:
        {
            QString position = decodePosition();
            QString altitude = decodeAltitude();
            QString speed = decodeSpeed();
            return "CROSS " + position + " AT AND MAINTAIN " + altitude + " AT " + speed;
        }
        case 62:
        {
            QString time = decodeTime();
            QString position = decodePosition();
            QString altitude = decodeAltitude();
            return "AT " + time + " CROSS " + position + " AT AND MAINTAIN " + altitude;
        }
        case 63:
        {
            QString time = decodeTime();
            QString position = decodePosition();
            QString altitude = decodeAltitude();
            QString speed = decodeSpeed();
            return "AT " + time + " CROSS " + position + " AT AND MAINTAIN " + altitude + " AT " + speed;
        }
        case 64:
        {
            QString distanceOffset = decodeDistanceOffset();
            QString direction = decodeDirection();
            return "OFFSET " + distanceOffset + " " + direction + " OF ROUTE";
        }
        case 65:
        {
            QString position = decodePosition();
            QString distanceOffset = decodeDistanceOffset();
            QString direction = decodeDirection();
            return "AT " + position + " OFFSET " + distanceOffset + " " + direction + " OF ROUTE";
        }
        case 66:
        {
            QString time = decodeTime();
            QString distanceOffset = decodeDistanceOffset();
            QString direction = decodeDirection();
            return "AT " + time + " OFFSET " + distanceOffset + " " + direction + " OF ROUTE";
        }
        case 67:
            return "PROCEED BACK ON ROUTE";
        case 68:
            return "REJOIN ROUTE BY " + decodePosition();
        case 69:
            return "REJOIN ROUTE BY " + decodeTime();
        case 70:
            return "EXPECT BACK ON ROUTE BY " + decodePosition();
        case 71:
            return "EXPECT BACK ON ROUTE BY " + decodeTime();
        case 72:
            return "RESUME OWN NAVIGATION";
        case 73:
            return decodePredepartureClearance();
        case 74:
            return "PROCEED DIRECT TO " + decodePosition();
        case 75:
            return "WHEN ABLE PROCEED DIRECT TO " + decodePosition();
        case 76:
        {
            QString time = decodeTime();
            QString position = decodePosition();
            return "AT " + time + " PROCEED DIRECT TO " + position;
        }
        case 77:
        {
            QString position1 = decodePosition();
            QString position2 = decodePosition();
            return "AT " + position1 + " PROCEED DIRECT TO " + position2;
        }
        case 78:
        {
            QString altitude = decodeAltitude();
            QString position = decodePosition();
            return "AT " + altitude + " PROCEED DIRECT TO " + position;
        }
        case 79:
        {
            QString position = decodePosition();
            QString routeClearance = decodeRouteClearance();
            return "CLEARED TO " + position + " VIA " + routeClearance;
        }
        case 80:
            return "CLEARED " + decodeRouteClearance();
        case 81:
            return "CLEARED " + decodeProcedureName();
        case 82:
        {
            QString distanceOffset = decodeDistanceOffset();
            QString direction = decodeDirection();
            return "CLEARD TO DEVIATE UP TO " + distanceOffset + " " + direction + " OF ROUTE";
        }
        case 83:
        {
            QString position = decodePosition();
            QString routeClearance = decodeRouteClearance();
            return "AT " + position + " CLEARED " + routeClearance;
        }
        case 84:
        {
            QString position = decodePosition();
            QString procedureName = decodeProcedureName();
            return "AT " + position + " CLEARED " + procedureName;
        }
        case 85:
            return "EXPECT " + decodeRouteClearance();
        case 86:
        {
            QString position = decodePosition();
            QString routeClearance = decodeRouteClearance();
            return "AT " + position + " EXPECT " + routeClearance;
        }
        case 87:
            return "EXPECT DIRECT TO " + decodePosition();
        case 88:
        {
            QString position1 = decodePosition();
            QString position2 = decodePosition();
            return "AT " + position1 + " EXPECT DIRECT TO " + position2;
        }
        case 89:
        {
            QString time = decodeTime();
            QString position = decodePosition();
            return "AT " + time + " EXPECT DIRECT TO " + position;
        }
        case 90:
        {
            QString altitude = decodeAltitude();
            QString position = decodePosition();
            return "AT " + altitude + " EXPECT DIRECT TO " + position;
        }
        case 91:
        {
            QString position = decodePosition();
            QString altitude = decodeAltitude();
            return "HOLD AT " + position + " MAINTAIN " + altitude + " INBOUND TRACK";
        }
        case 92:
        {
            QString position = decodePosition();
            QString altitude = decodeAltitude();
            return "HOLD AT " + position + " AS PUBLISHED MAINTAIN " + altitude;
        }
        case 93:
            return "EXPECT FURTHER CLEARANCE AT " + decodeTime();
        case 94:
        {
            QString direction = decodeDirection();
            QString degrees = decodeDegrees();
            return "TURN " + direction + " HEADING " + degrees;
        }
        case 95:
        {
            QString direction = decodeDirection();
            QString degrees = decodeDegrees();
            return "TURN " + direction + " GROUND TRACK " + degrees;
        }
        case 96:
            return "FLY PRESENT HEADING";
        case 97:
        {
            QString position = decodePosition();
            QString degrees = decodeDegrees();
            return "AT " + position + " FLY HEADING " + degrees;
        }
        case 98:
        {
            QString direction = decodeDirection();
            QString degrees = decodeDegrees();
            return "IMMEDIATELY TURN " + direction + " HEADING " + degrees;
        }
        case 99:
            return "EXPECT " + decodeProcedureName();
        case 100:
        {
            QString time = decodeTime();
            QString speed = decodeSpeed();
            return "AT " + time + " EXPECT " + speed;
        }
        case 101:
        {
            QString position = decodePosition();
            QString speed = decodeSpeed();
            return "AT " + position + " EXPECT " + speed;
        }
        case 102:
        {
            QString altitude = decodeAltitude();
            QString speed = decodeSpeed();
            return "AT " + altitude + " EXPECT " + speed;
        }
        case 103:
        {
            QString time = decodeTime();
            QString speed1 = decodeSpeed();
            QString speed2 = decodeSpeed();
            return "AT " + time + " EXPECT " + speed1 + " TO " + speed2;
        }
        case 104:
        {
            QString position = decodePosition();
            QString speed1 = decodeSpeed();
            QString speed2 = decodeSpeed();
            return "AT " + position + " EXPECT " + speed1 + " TO " + speed2;
        }
        case 105:
        {
            QString altitude = decodeAltitude();
            QString speed1 = decodeSpeed();
            QString speed2 = decodeSpeed();
            return "AT " + altitude + " EXPECT " + speed1 + " TO " + speed2;
        }
        case 106:
            return "MAINTAIN " + decodeSpeed();
        case 107:
            return "MAINTAIN PRESENT SPEED";
        case 108:
            return "MAINTAIN " + decodeSpeed() + " OR GREATER";
        case 109:
            return "MAINTAIN " + decodeSpeed() + " OR LESS";
        case 110:
        {
            QString speed1 = decodeSpeed();
            QString speed2 = decodeSpeed();
            return "MAINTAIN " + speed1 + " THROUGH " + speed2;
        }
        case 111:
            return "INCREASE SPEED TO " + decodeSpeed();
        case 112:
            return "INCREASE SPEED TO " + decodeSpeed() + " OR GREATER";
        case 113:
            return "REDUCE SPEED TO " + decodeSpeed();
        case 114:
            return "REDUCE SPEED TO " + decodeSpeed() + " OR LESS";
        case 115:
            return "DO NOT EXCEED " + decodeSpeed();
        case 116:
            return "RESUME NORMAL SPEED";
        case 117:
        {
            QString icaoUnitName = decodeICAOUnitName();
            QString frequency = decodeFrequency();
            return "CONTACT " + icaoUnitName + " " + frequency;
        }
        case 118:
        {
            QString position = decodePosition();
            QString icaoUnitName = decodeICAOUnitName();
            QString frequency = decodeFrequency();
            return "AT " + position + " CONTACT " + icaoUnitName + " " + frequency;
        }
        case 119:
        {
            QString time = decodeTime();
            QString icaoUnitName = decodeICAOUnitName();
            QString frequency = decodeFrequency();
            return "AT " + time + " CONTACT " + icaoUnitName + " " + frequency;
        }
        case 120:
        {
            QString icaoUnitName = decodeICAOUnitName();
            QString frequency = decodeFrequency();
            return "MONITOR " + icaoUnitName + " " + frequency;
        }
        case 121:
        {
            QString position = decodePosition();
            QString icaoUnitName = decodeICAOUnitName();
            QString frequency = decodeFrequency();
            return "AT " + position + " MONITOR " + icaoUnitName + " " + frequency;
        }
        case 122:
        {
            QString time = decodeTime();
            QString icaoUnitName = decodeICAOUnitName();
            QString frequency = decodeFrequency();
            return "AT " + time + " MONITOR " + icaoUnitName + " " + frequency;
        }
        case 123:
            return "SQUAWK " + decodeBeaconCode();
        case 124:
            return "STOP SQUAWK";
        case 125:
            return "SQUAWK ALTITUDE";
        case 126:
            return "STOP ALTITUDE SQUAWK";
        case 127:
            return "REPORT BACK ON ROUTE";
        case 128:
            return "REPORT LEAVING " + decodeAltitude();
        case 129:
            return "REPORT LEVEL " + decodeAltitude();
        case 130:
            return "REPORT PASSING " + decodeAltitude();
        case 131:
            return "REPORT REAMINING FUEL AND SOULS ON BOARD";
        case 132:
            return "CONFIRM POSITION";
        case 133:
            return "CONFIRM ALTITUDE";
        case 134:
            return "CONFIRM SPEED";
        case 135:
            return "CONFIRM ASSIGNED ALTITUDE";
        case 136:
            return "CONFIRM ASSIGNED SPEED";
        case 137:
            return "CONFIRM ASSIGNED ROUTE";
        case 138:
            return "CONFIRM TIME OVER REPORTED WAYPOINT";
        case 139:
            return "CONFIRM REPORTED WAYPOINT";
        case 140:
            return "CONFIRM NEXT WAYPOINT";
        case 141:
            return "CONFIRM NEXT WAYPOINT ETA";
        case 142:
            return "CONFIRM ENSUING WAYPOINT";
        case 143:
            return "CONFIRM REQUEST";
        case 144:
            return "CONFIRM SQUAWK";
        case 145:
            return "CONFIRM HEADING";
        case 146:
            return "CONFIRM GROUND TRACK";
        case 147:
            return "REQUEST POSITION REPORT";
        case 148:
            return "WHEN CAN YOU ACCEPT " + decodeAltitude();
        case 149:
        {
            QString altitude = decodeAltitude();
            QString position = decodePosition();
            return "CAN YOU ACCEPT " + altitude + " AT " + position;
        }
        case 150:
        {
            QString altitude = decodeAltitude();
            QString time = decodeTime();
            return "CAN YOU ACCEPT " + altitude + " AT " + time;
        }
        case 151:
            return "WHEN CAN YOU ACCEPT " + decodeSpeed();
        case 152:
        {
            QString distanceOffset = decodeDistanceOffset();
            QString direction = decodeDirection();
            return "WHEN CAN YOU ACCEPT " + distanceOffset + " " + direction + " OFFSET";
        }
        case 153:
            return "ALTIMETER " + decodeAltimeter();
        case 154:
            return "RADAR SERVICES TERMINATED";
        case 155:
            return "RADAR CONTACT " + decodePosition();
        case 156:
            return "RADAR CONTACT LOST";
        case 157:
            return "CHECK STUCK MICROPHONE " + decodeFrequency();
        case 158:
            return "ATIS " + decodeATISCode();
        case 159:
            return "ERROR " + decodeErrorInformation();
        case 160:
            return "NEXT DATA AUTHORITY " + decodeICAOFacilityDesignation();
        case 161:
            return "END SERVICE";
        case 162:
            return "SERVICE UNAVAILABLE";
        case 163:
        {
            QString icaoFacilityDesignation = decodeICAOFacilityDesignation();
            QString tp4Table = decodeTp4Table();
            return icaoFacilityDesignation + " " + tp4Table;
        }
        case 164:
            return "WHEN READY";
        case 165:
            return "THEN";
        case 166:
            return "DUE TO TRAFFIC";
        case 167:
            return "DUE TO AIRSPACE RESTRICTION";
        case 168:
            return "DISREGARD";
        case 169:
            return decodeFreeText();
        case 170:
            return decodeFreeText();
        case 171:
            return "CLIMB AT " + decodeVerticalRate() + " MINIMUM";
        case 172:
            return "CLIMB AT " + decodeVerticalRate() + " MAXIMUM";
        case 173:
            return "DESCEND AT " + decodeVerticalRate() + " MINIMUM";
        case 174:
            return "DESCEND AT " + decodeVerticalRate() + " MAXIMUM";
        case 175:
            return "REPORT REACHING " + decodeAltitude();
        case 176:
            return "MAINTAIN OWN SEPERATION AND VMC";
        case 177:
            return "AT PILOTS DISCRETION";
        case 178:
            return decodeTrackDetailMsg();
        case 179:
            return "SQUAWK IDENT";
        case 180:
        {
            QString altitude1 = decodeAltitude();
            QString altitude2 = decodeAltitude();
            return "REPORT REACHING BLOCK " + altitude1 + " TO " + altitude2;
        }
        case 181:
        {
            QString toFrom = decodeToFrom();
            QString position = decodePosition();
            return "REPORT DISTANCE " + toFrom + " " + position;
        }
        case 182:
            return "CONFIRM ATIS CODE";
        default:
            return "UNKNOWN ATCUplinkMsgElementId: " + QString::number(id);
        }
    }

    QString decodeATCDownlinkMsgElementId()
    {
        int id = getBits(8);
        switch (id)
        {
        case 0:
            return "WILCO";
        case 1:
            return "UNABLE";
        case 2:
            return "STANDBY";
        case 3:
            return "ROGER";
        case 4:
            return "AFFIRM";
        case 5:
            return "NEGATIVE";
        case 6:
            return "REQUEST " + decodeAltitude();
        case 7:
        {
            QString altitude1 = decodeAltitude();
            QString altitude2 = decodeAltitude();
            return "REQUEST BLOCK " + altitude1 + " TO " + altitude2;
        }
        case 8:
            return "REQUEST CRUISE CLIMB TO " + decodeAltitude();
        case 9:
            return "REQUEST CLIMB TO " + decodeAltitude();
        case 10:
            return "REQUEST DESCENT TO " + decodeAltitude();
        case 11:
        {
            QString position = decodePosition();
            QString altitude = decodeAltitude();
            return "AT " + position + " REQUEST CLIMB TO " + altitude;
        }
        case 12:
        {
            QString position = decodePosition();
            QString altitude = decodeAltitude();
            return "AT " + position + " REQUEST DESCENT TO " + altitude;
        }
        case 13:
        {
            QString time = decodeTime();
            QString altitude = decodeAltitude();
            return "AT " + time + " REQUEST CLIMB TO " + altitude;
        }
        case 14:
        {
            QString time = decodeTime();
            QString altitude = decodeAltitude();
            return "AT " + time + " REQUEST DESCENT TO " + altitude;
        }
        case 15:
        {
            QString distanceOffset = decodeDistanceOffset();
            QString direction = decodeDirection();
            return "REQUEST OFFSET " + distanceOffset + " " + direction + " OF ROUTE";
        }
        case 16:
        {
            QString position = decodePosition();
            QString distanceOffset = decodeDistanceOffset();
            QString direction = decodeDirection();
            return "AT " + position + " REQUEST OFFSET " + distanceOffset + " " + direction + " OF ROUTE";
        }
        case 17:
        {
            QString time = decodeTime();
            QString distanceOffset = decodeDistanceOffset();
            QString direction = decodeDirection();
            return "AT " + time + " REQUEST OFFSET " + distanceOffset + " " + direction + " OF ROUTE";
        }
        case 18:
            return "REQUEST " + decodeSpeed();
        case 19:
        {
            QString speed1 = decodeSpeed();
            QString speed2 = decodeSpeed();
            return "REQUEST " + speed1 + " TO " + speed2;
        }
        case 20:
            return "REQUEST VOICE CONTACT";
        case 21:
            return "REQUEST VOICE CONTACT " + decodeFrequency();
        case 22:
            return "REQUEST DIRECT TO " + decodePosition();
        case 23:
            return "REQUEST " + decodeProcedureName();
        case 24:
            return "REQUEST " + decodeRouteClearance();
        case 25:
            return "REQUEST CLEARANCE";
        case 26:
        {
            QString position = decodePosition();
            QString routeClearance = decodeRouteClearance();
            return "REQUEST WEATHER DEVIATION TO " + position + " VIA " + routeClearance;
        }
        case 27:
        {
            QString distanceOffset = decodeDistanceOffset();
            QString direction = decodeDirection();
            return "REQUEST WEATHER DEVIATION UP TO " + distanceOffset + " " + direction + " OF ROUTE";
        }
        case 28:
            return "LEAVING " + decodeAltitude();
        case 29:
            return "CLIMBING TO " + decodeAltitude();
        case 30:
            return "DESCENDING TO " + decodeAltitude();
        case 31:
            return "PASSING POSITION " + decodePosition();
        case 32:
            return "PRESENT ALTITUDE " + decodeAltitude();
        case 33:
            return "PRESENT POSITION " + decodePosition();
        case 34:
            return "PRESENT SPEED " + decodeSpeed();
        case 35:
            return "PRESENT HEADING " + decodeDegrees();
        case 36:
            return "PRESENT GROUND TRACK " + decodeDegrees();
        case 37:
            return "LEVEL " + decodeAltitude();
        case 38:
            return "ASSIGNED ALTITUDE " + decodeAltitude();
        case 39:
            return "ASSIGNED SPEED " + decodeSpeed();
        case 40:
            return "ASSIGNED ROUTE " + decodeRouteClearance();
        case 41:
            return "BACK ON ROUTE";
        case 42:
            return "NEXT WAYPOINT " + decodePosition();
        case 43:
            return "NEXT WAYPOINT ETA " + decodeTime();
        case 44:
            return "ENSUING WAYPOINT " + decodePosition();
        case 45:
            return "REPORTED WAYPOINT " + decodePosition();
        case 46:
            return "REPORTED WAYPOINT " + decodeTime();
        case 47:
            return "SQUAWKING " + decodeBeaconCode();
        case 48:
            return "POSITION REPORT " + decodePositionReport();
        case 49:
            return "WHEN CAN WE EXPECT " + decodeSpeed();
        case 50:
        {
            QString speed1 = decodeSpeed();
            QString speed2 = decodeSpeed();
            return "WHEN CAN WE EXPECT " + speed1 + " TO " + speed2;
        }
        case 51:
            return "WHEN CAN WE EXPECT BACK ON ROUTE";
        case 52:
            return "WHEN CAN WE EXPECT LOWER ALTITUDE";
        case 53:
            return "WHEN CAN WE EXPECT HIGHER ALTITUDE";
        case 54:
            return "WHEN CAN WE EXPECT CRUISE CLIMB TO " + decodeAltitude();
        case 55:
            return "PAN PAN PAN";
        case 56:
            return "MAYDAY MAYDAY MAYDAY";
        case 57:
        {
            QString remainingFuel = decodeRemainingFuel();
            QString remainingSouls = decodeRemainingSouls();
            return remainingFuel + " OF FUEL REMAINING AND " + remainingSouls + " SOULS ON BOARD";
        }
        case 58:
            return "CANCEL EMERGENCY";
        case 59:
        {
            QString position = decodePosition();
            QString routeClearance = decodeRouteClearance();
            return "DIVERTING TO " + position + " VIA " + routeClearance;
        }
        case 60:
        {
            QString distanceOffset = decodeDistanceOffset();
            QString direction = decodeDirection();
            return "OFFSETING " + distanceOffset + " " + direction + " OF ROUTE";
        }
        case 61:
            return "DESCENDING TO ALTITUDE " + decodeAltitude();
        case 62:
            return "ERROR " + decodeErrorInformation();
        case 63:
            return "NOT CURRET DATA AUTHORITY";
        case 64:
            return decodeICAOFacilityDesignation();
        case 65:
            return "DUE TO WEATHER";
        case 66:
            return "DUE TO AIRCRAFT PERFORAMNCE";
        case 67:
            return decodeFreeText();
        case 68:
            return decodeFreeText();
        case 69:
            return "REQUEST VMC DESCENT";
        case 70:
            return "REQUEST HEADING " + decodeDegrees();
        case 71:
            return "REQUEST GROUND TRACK " + decodeDegrees();
        case 72:
            return "REACHING ALTITUDE " + decodeAltitude();
        case 73:
            return decodeVersionNumber();
        case 74:
            return "MAINTAIN OWN SEPERATION AND VMC";
        case 75:
            return "AT PILOTS DISCRETION";
        case 76:
        {
            QString altitude1 = decodeAltitude();
            QString altitude2 = decodeAltitude();
            return "REACHING BLOCK " + altitude1 + " TO " + altitude2;
        }
        case 77:
        {
            QString altitude1 = decodeAltitude();
            QString altitude2 = decodeAltitude();
            return "ASSIGNED BLOCK " + altitude1 + " TO " + altitude2;
        }
        case 78:
        {
            QString time = decodeTime();
            QString distance = decodeDistance();
            QString toFrom = decodeToFrom();
            QString position = decodePosition();
            return "AT " + time + " " + distance + " " + toFrom + " " + position;
        }
        case 79:
            return "ATIS " + decodeATISCode();
        default:
            return "UNKNOWN ATCDownlinkMsgElementId: " + QString::number(id);
        }
    }

    QString decodeAltitude()
    {
        int type = getBits(3);
        switch (type)
        {
        case 0:
            return decodeAltitudeQNH();
        case 1:
            return decodeAltitudeQNHMeters();
        case 2:
            return decodeAltitudeQFE();
        case 3:
            return decodeAltitudeQFEMeters();
        case 4:
            return decodeAltitudeGNSSFeet();
        case 5:
            return decodeAltitudeGNSSMeters();
        case 6:
            return decodeAltitudeFlightLevel();
        case 7:
            return decodeAltitudeFlightLevelMetric();
        default:
            return QString("UNKOWNN ALTITUDE TYPE %1").arg(type);
        }
    }

    QString decodeAltitudeQNH()
    {
        int v = getBits(12);
        return QString("%1 FT").arg(v * 10);
    }

    QString decodeAltitudeQNHMeters()
    {
        int v = getBits(14);
        return QString("%1 M").arg(v);
    }

    QString decodeAltitudeQFE()
    {
        int v = getBits(12);
        return QString("QFE %1 FT").arg(v * 10);
    }

    QString decodeAltitudeQFEMeters()
    {
        int v = getBits(13);
        return QString("QFE %1 M").arg(v);
    }

    QString decodeAltitudeGNSSFeet()
    {
        int v = getBits(18);
        return QString("GNSS %1 FT").arg(v);
    }

    QString decodeAltitudeGNSSMeters()
    {
        int v = getBits(16);
        return QString("GNSS %1 M").arg(v);
    }

    QString decodeAltitudeFlightLevel()
    {
        int v = getBits(10);
        return QString("F%1").arg((v+30), 3, 10, QChar('0'));
    }

    QString decodeAltitudeFlightLevelMetric()
    {
        int v = getBits(11);
        return QString("S%1").arg((v+10), 3, 10, QChar('0'));
    }

    QString decodeATISCode()
    {
        return decodeIA5String(5);
    }

    QString decodeAltimeter()
    {
        bool flag = getBits(1);
        if (flag) {
            return decodeAltimeterEnglish();
        } else {
            return decodeAltimeterMetric();
        }
    }

    QString decodeAltimeterEnglish()
    {
        bool alt = getBits(10);
        return QString("%1").arg((alt+2200)/100.0, 0, 'f', 2);
    }

    QString decodeAltimeterMetric()
    {
        bool alt = getBits(13);
        return QString("%1 HPA").arg((alt+7500)/10.0, 0, 'f', 1);
    }

    QString decodePredepartureClearance()
    {
        bool flag0 = getBits(1);
        bool flag1 = getBits(1);
        bool flag2 = getBits(2);
        QString s = decodeAircraftFlightIdentification();
        if (flag0) {
            s.append(" " + decodeAircraftType());
        }
        if (flag1) {
            s.append(" " + decodeAircraftEquipmentCode());
        }
        s.append(" " + decodeTime());
        s.append(" " + decodeRouteClearance());
        if (flag2) {
            s.append(" " + decodeAltitude());
        }
        s.append(" " + decodeFrequencyDeparture());
        s.append(" " + decodeBeaconCode());
        s.append(" " + decodePDCRevision());
        return s;
    }

    QString decodeFrequencyDeparture()
    {
        return decodeFrequencyVHF();
    }

    QString decodePDCRevision()
    {
        int rev = getBits(4);
        return QString::number(rev + 1);
    }

    QString decodeAircraftFlightIdentification()
    {
        int size = getBits(3) + 2;
        return decodeIA5String(size);
    }

    QString decodeAircraftType()
    {
        int size = getBits(3) + 2;
        return decodeIA5String(size);
    }

    QString decodeAircraftEquipmentCode()
    {
        QString s = decodeComNavApproachEquipmentAvailable();
        int size = getBits(4) + 1;
        for (int i = 0; i < size; i++) {
            s.append(" " + decodeComNavEquipmentStatus());
        }
        s.append(" " + decodeSSREquipmentAvailable());
        return s;
    }

    QString decodeComNavApproachEquipmentAvailable()
    {
        int d = getBits(1);
        return d == 0 ? "Y" : "N";
    }

    QString decodeComNavEquipmentStatus()
    {
        int d = getBits(4);
        switch (d)
        {
        case 0:
            return "A"; // Loran A
        case 1:
            return "C"; // Loran C
        case 2:
            return "D"; // DME
        case 3:
            return "E"; // DECCA
        case 4:
            return "F"; // ADF
        case 5:
            return "G"; // GNSS
        case 6:
            return "H"; // hfRTF
        case 7:
            return "I"; // INS
        case 8:
            return "L"; // ILS
        case 9:
            return "M"; // Omega
        case 10:
            return "O"; // VOR
        case 11:
            return "P"; // Doppler
        case 12:
            return "R"; // RNAV
        case 13:
            return "T"; // TACAN
        case 14:
            return "U"; // UHF RTF
        case 15:
            return "V"; // VHF RTF
        default:
            return QString("UNKNOWN EQUIPMENT STATUS %1").arg(d);
        }
    }

    QString decodeSSREquipmentAvailable()
    {
        int d = getBits(3);
        switch (d)
        {
        case 0:
            return "N"; // nil
        case 1:
            return "A";
        case 2:
            return "C";
        case 3:
            return "X";
        case 4:
            return "P";
        case 5:
            return "I";
        case 6:
            return "D";
        default:
            return QString("UNKNOWN SSR EQUIPMENT %1").arg(d);
        }
    }

    QString decodeTp4Table()
    {
        getBits(1); // Consumed to keep the bit position right; nothing to display
        return "";
    }

    QString decodeTrackDetailMsg()
    {
        return ""; // FIMXE
    }

    QString decodeDegrees()
    {
        int choice = getBits(1);
        if (choice == 0) {
            return decodeDegreesMagnetic();
        } else {
            return decodeDegreesTrue();
        }
    }

    QString decodeDegreesMagnetic()
    {
        int degrees = getBits(9);
        return QString("%1").arg(degrees, 3, 10, QChar('0'));
    }

    QString decodeDegreesTrue()
    {
        int degrees = getBits(9);
        return QString("%1 T").arg(degrees, 3, 10, QChar('0'));
    }

    QString decodeDirection()
    {
        int dir = getBits(4);
        const QStringList dirs = {"L", "R", "L OR R", "N", "S", "E", "W", "NE", "NW", "SE", "SW"};
        if (dir >= dirs.size()) {
            return QString("UNKNOWN DIRECTION %1").arg(dir);
        } else {
            return dirs[dir];
        }
    }
    QString decodeDistance()
    {
        int unit = getBits(1);
        if (unit == 0) {
            return decodeDistanceNM();
        } else {
            return decodeDistanceKM();
        }
    }

    QString decodeDistanceNM()
    {
        int d = getBits(14);
        return QString("%1 NM").arg((d + 1) / 10.0f, 0, 'f', 1);
    }

    QString decodeDistanceKM()
    {
        int d = getBits(10);
        return QString("%1 KM").arg(d + 1);
    }

    QString decodeDistanceOffset()
    {
        int unit = getBits(1);
        if (unit == 0) {
            return decodeDistanceOffsetNM();
        } else {
            return decodeDistanceOffsetKM();
        }
    }

    QString decodeDistanceOffsetNM()
    {
        int d = getBits(7);
        return QString("%1 NM").arg(d + 1);
    }

    QString decodeDistanceOffsetKM()
    {
        int d = getBits(8);
        return QString("%1 KM").arg(d + 1);
    }

    QString decodeErrorInformation()
    {
        int error = getBits(5);
        QStringList errors = {
            "Application Error",
            "Duplicate Msg Identification Number",
            "Unrecognized Msg Reference Number",
            "End Service With Pending Msgs",
            "End Serivce With No Valid Response",
            "Insufficient Msg Storage Capacity",
            "No Available Msg Identification Number",
            "Command Termination",
            "Insufficient Data",
            "Unexpected Data",
            "Invalid Data"
        };
        if (error >= errors.size()) {
            return QString("Reserved Error Msg %1").arg(error);
        } else {
            return errors[error];
        }
    }

    QString decodeFixName()
    {
        int size = getBits(3) + 1;
        return decodeIA5String(size);
    }

    QString decodeFreeText()
    {
        int size = getBits(8) + 1;
        return decodeIA5String(size);
    }

    QString decodeFrequency()
    {
        int choice = getBits(2);
        switch (choice)
        {
        case 0:
            return decodeFrequencyHF();
        case 1:
            return decodeFrequencyVHF();
        case 2:
            return decodeFrequencyUHF();
        case 3:
            return decodeFrequencySatChannel();
        default:
            return QString("UNKNOWN FREQUENCY %1").arg(choice);
        }
    }

    QString decodeFrequencyHF()
    {
        int f = getBits(15);
        return QString("%1 KHZ").arg(f + 2850);
    }

    QString decodeFrequencyVHF()
    {
        int f = getBits(15);
        float freq = (117000.0f + f) / 1000.0f;
        return QString("%1").arg(freq, 7, 'f', 3);
    }

    QString decodeFrequencyUHF()
    {
        int f = getBits(18);
        float freq = (225000.0f + f) / 1000.0f;
        return QString("%1").arg(freq, 7, 'f', 3);
    }

    QString decodeFrequencySatChannel()
    {
        QString channel;
        for (int i = 0; i < 12; i++) {
            channel = channel + decodeDigit();
        }
        return channel;
    }

    QString decodeIA5String(int size)
    {
        QString s;
        for (int i = 0; i < size; i++) {
            s.append(decodeIA5Char());
        }
        return s;
    }

    QChar decodeIA5Char()
    {
        int bits = getBits(7);
        return QChar(bits);
    }

    QString decodeDigit()
    {
        int bits = getBits(4);
        return QString::number(bits);
    }

    QString decodeAirport()
    {
        return decodeIA5String(4);
    }

    QString decodeBeaconCode()
    {
        QString s;
        for (int i = 0; i < 4; i++) {
            s.append(decodeBeaconOctalDigit());
        }
        return s;
    }

    QString decodeBeaconOctalDigit()
    {
        int bits = getBits(3);
        return QString::number(bits);
    }

    QString decodeICAOUnitName()
    {
        // DO-219 spec is inconsistent in it's description of this element - page 22 appears to be wrong
        QString icaoFacilityIndentification = decodeICAOFacilityIdentification();
        QString icaoFacilityFunction = decodeICAOFacilityFunction();
        return icaoFacilityIndentification + " " + icaoFacilityFunction;
    }

    QString decodeICAOFacilityIdentification()
    {
        int type = getBits(1);
        if (type == 0) {
            return decodeICAOFacilityDesignation();
        } else {
            return decodeICAOFacilityName();
        }
    }

    QString decodeICAOFacilityDesignation()
    {
        return decodeIA5String(4);
    }

    QString decodeICAOFacilityName()
    {
        int size = getBits(4) + 3;
        return decodeIA5String(size);
    }

    QString decodeICAOFacilityFunction()
    {
        int func = getBits(3);
        const QStringList funcs = {
            "CENTER",
            "APPROACH",
            "TOWER",
            "FINAL",
            "GROUND CONTROL",
            "CLEARANCE DELIVERY",
            "DEPARTURE",
            "CONTROL"
        };
        return funcs[func];
    }

    QString decodeLatitudeLongitude()
    {
        QString latitude = decodeLatitude();
        QString longitude = decodeLongitude();
        return latitude + " " + longitude;
    }

    QString decodeLatitude()
    {
        bool flag = getBits(1);
        QString s = decodeLatitudeDegrees();
        if (flag) {
            s.append(" " + decodeMinutesLatLon());
        }
        s.append(" " + decodeLatitudeDirection());
        return s;
    }

    QString decodeLatitudeDegrees()
    {
        int degrees = getBits(7);
        return QString::number(degrees);
    }

    QString decodeMinutesLatLon()
    {
        int minutes = getBits(10);
        return QString::number(minutes / 10.0f, 'f', 1);
    }

    QString decodeLatitudeDirection()
    {
        int dir = getBits(1);
        return dir == 0 ? "N" : "S";
    }

    QString decodeLongitude()
    {
        bool flag = getBits(1);
        QString s = decodeLongitudeDegrees();
        if (flag) {
            s.append(" " + decodeMinutesLatLon());
        }
        s.append(" " + decodeLongitudeDirection());
        return s;
    }

    QString decodeLongitudeDegrees()
    {
        int degrees = getBits(8);
        return QString::number(degrees);
    }

    QString decodeLongitudeDirection()
    {
        int dir = getBits(1);
        return dir == 0 ? "E" : "W";
    }

    QString decodePlaceBearingDistance()
    {
        bool flag = getBits(1);
        QString s = decodeFixName();
        if (flag) {
            s.append(" " + decodeLatitudeLongitude());
        }
        s.append(" " + decodeDegrees());
        s.append(" " + decodeDistance());
        return s;
    }

    QString decodeProcedureName()
    {
        bool flag0 = getBits(1);
        QString procedureType = decodeProcedureType();
        QString procedure = decodeProcedure();
        QString s = procedureType + " " + procedure;
        if (flag0) {
            s.append(decodeProcedureTransistion());
        }
        return s;
    }

    QString decodeProcedureType()
    {
        int type = getBits(2);
        const QStringList types = {"Arrival", "Approach", "Departure"};
        if (type >= types.size()) {
            return QString("UNKNOWN PROCEDURE TYPE %1").arg(type);
        } else {
            return types[type];
        }
    }

    QString decodeProcedure()
    {
        int size = getBits(3) + 1;
        return decodeIA5String(size);
    }

    QString decodeProcedureTransistion()
    {
        int size = getBits(3) + 1;
        return decodeIA5String(size);
    }

    QString decodePosition()
    {
        int type = getBits(3);
        switch (type)
        {
        case 0:
            return decodeFixName();
        case 1:
            return decodeNavAid();
        case 2:
            return decodeAirport();
        case 3:
            return decodeLatitudeLongitude();
        case 4:
            return decodePlaceBearingDistance();
        default:
            return QString("UNKNOWN POSITION %1").arg(type);
        }
    }

    QString decodeNavAid()
    {
        int size = getBits(2) + 1;
        return decodeIA5String(size);
    }

    QString decodePositionReport()
    {
        bool flag0 = getBits(1);
        bool flag1 = getBits(1);
        bool flag2 = getBits(1);
        bool flag3 = getBits(1);
        bool flag4 = getBits(1);
        bool flag5 = getBits(1);
        bool flag6 = getBits(1);
        bool flag7 = getBits(1);
        bool flag8 = getBits(1);
        bool flag9 = getBits(1);
        bool flag10 = getBits(1);
        bool flag11 = getBits(1);
        bool flag12 = getBits(1);
        bool flag13 = getBits(1);
        bool flag14 = getBits(1);
        bool flag15 = getBits(1);
        QString s = "\nPositionCurrent: " + decodePosition();
        s.append("\nTimeAtPositionCurrent: " + decodeTime());
        s.append("\nAltitude: " + decodeAltitude());
        if (flag0) {
            s.append("\nFixNext: " + decodePosition());
        }
        if (flag1) {
            s.append("\nTimeAtFixNext: " + decodeTime());
        }
        if (flag2) {
            s.append("\nFixNextPlusOne: " + decodePosition());
        }
        if (flag3) {
            s.append("\nTimeAtDestination: " + decodeTime());
        }
        if (flag4) {
            s.append("\nRemainingFuel" + decodeRemainingFuel());
        }
        if (flag5) {
            s.append("\nTemperature: " + decodeTemperature());
        }
        if (flag6) {
            s.append("\nWinds: " + decodeWinds());
        }
        if (flag7) {
            s.append("\nTurbulence: " + decodeTurbulence());
        }
        if (flag8) {
            s.append("\nIcing: " + decodeIcing());
        }
        if (flag9) {
            s.append("\nSpeed: " + decodeSpeed());
        }
        if (flag10) {
            s.append("\nSpeedGround: " + decodeSpeedGround());
        }
        if (flag11) {
            s.append("\nVerticalChange: " + decodeVerticalChange());
        }
        if (flag12) {
            s.append("\nTrackAngle: " + decodeDegrees());
        }
        if (flag13) {
            s.append("\nTrueHeading: " + decodeDegrees());
        }
        if (flag14) {
            s.append("\nDistance: " + decodeDistance());
        }
        if (flag15) {
            s.append("\nSupplementaryInformation: " + decodeSupplementaryInformation());
        }
        return s;
    }

    QString decodeTemperature()
    {
        bool scale = getBits(1);
        if (scale == 0) {
            return decodeTemperatureC();
        } else {
            return decodeTemperatureF();
        }
    }

    QString decodeTemperatureC()
    {
        int temp = getBits(7);
        return QString("%1 C").arg(temp - 80);
    }

    QString decodeTemperatureF()
    {
        int temp = getBits(8);
        return QString("%1 F").arg(temp - 105);
    }

    QString decodeWinds()
    {
        QString windDirection = decodeWindDirection();
        QString windSpeed = decodeWindSpeed();
        return windDirection + windSpeed;
    }

    QString decodeWindDirection()
    {
        int dir = getBits(9);
        return QString("%1/").arg(dir + 1);
    }

    QString decodeWindSpeed()
    {
        int units = getBits(1);
        if (units == 0) {
            return decodeWindSpeedEnglish();
        } else {
            return decodeWindSpeedMetric();
        }
    }

    QString decodeWindSpeedEnglish()
    {
        int speed = getBits(8);
        return QString("%1 KNTS").arg(speed);
    }

    QString decodeWindSpeedMetric()
    {
        int speed = getBits(9);
        return QString("%1 KPH").arg(speed);
    }

    QString decodeTurbulence()
    {
        int turb = getBits(2);
        const QStringList turbs = {"LIGHT", "MODERATE", "SEVERE"};
        if (turb >= turbs.size()) {
            return QString("UNKNOWN TURBULENCE %1").arg(turb);
        } else {
            return turbs[turb];
        }
    }

    QString decodeIcing()
    {
        int icing = getBits(2);
        const QStringList icings = {"TRACE", "LIGHT", "MODERATE", "SEVERE"};
        if (icing >= icings.size()) {
            return QString("UNKNOWN ICING %1").arg(icing);
        } else {
            return icings[icing];
        }
    }

    QString decodeVerticalChange()
    {
        QString verticalDirection = decodeVerticalDirection();
        QString verticalRate = decodeVerticalRate();
        return verticalDirection + " " + verticalRate;
    }

    QString decodeVerticalDirection()
    {
        int dir = getBits(1);
        if (dir == 0) {
            return "UP";
        } else {
            return "DOWN";
        }
    }

    QString decodeVerticalRate()
    {
        int units = getBits(1);
        if (units == 0) {
            return decodeVerticalRateEnglish();
        } else {
            return decodeVerticalRateMetric();
        }
    }

    QString decodeVerticalRateEnglish()
    {
        int rate = getBits(6);
        return QString("%1 FT/MIN").arg(rate * 100);
    }

    QString decodeVerticalRateMetric()
    {
        int rate = getBits(8);
        return QString("%1 M/MIN").arg(rate * 10);
    }

    QString decodeSpeed()
    {
        int type = getBits(3);
        switch (type)
        {
        case 0:
            return decodeSpeedIndicated();
        case 1:
            return decodeSpeedIndicatedMetric();
        case 2:
            return decodeSpeedTrue();
        case 3:
            return decodeSpeedTrueMetric();
        case 4:
            return decodeSpeedGround();
        case 5:
            return decodeSpeedGroundMetric();
        case 6:
            return decodeSpeedMach();
        case 7:
            return decodeSpeedMachLarge();
        default:
            return QString("UKNOWN SPEED TYPE %1").arg(type);
        }
    }

    QString decodeSpeedIndicated()
    {
        int speed = getBits(5);
        return QString("%1 IAS").arg((speed + 7) * 10);
    }

    QString decodeSpeedIndicatedMetric()
    {
        int speed = getBits(7);
        return QString("%1 KMIAS").arg((speed + 10) * 10);
    }

    QString decodeSpeedTrue()
    {
        int speed = getBits(6);
        return QString("%1 TAS").arg((speed + 7) * 10);
    }

    QString decodeSpeedTrueMetric()
    {
        int speed = getBits(7);
        return QString("%1 KMTAS").arg((speed + 10) * 10);
    }

    QString decodeSpeedGround()
    {
        int speed = getBits(6);
        return QString("%1 GS").arg((speed + 7) * 10);
    }

    QString decodeSpeedGroundMetric()
    {
        int speed = getBits(8);
        return QString("%1 KMGS").arg((speed + 10) * 10);
    }

    QString decodeSpeedMach()
    {
        int speed = getBits(5);
        return QString("%1M").arg((speed + 61) / 100.0f, 0, 'f', 2);
    }

    QString decodeSpeedMachLarge()
    {
        int speed = getBits(9);
        return QString("%1M").arg((speed + 93) / 100.0f, 0, 'f', 2);
    }

    QString decodeTime()
    {
        QString timeHours = decodeTimeHours();
        QString timeMinutes = decodeTimeMinutes();
        return timeHours + ":" + timeMinutes;
    }

    QString decodeTimeHours()
    {
        int h = getBits(5);
        return QString("%1").arg(h, 2, 10, QChar('0'));
    }

    QString decodeTimeMinutes()
    {
        int m = getBits(6);
        return QString("%1").arg(m, 2, 10, QChar('0'));
    }

    QString decodeTimeSeconds()
    {
        int s = getBits(6);
        return QString("%1").arg(s, 2, 10, QChar('0'));
    }

    QString decodeToFrom()
    {
        int dir = getBits(1);
        if (dir == 0) {
            return "TO";
        } else {
            return "FROM";
        }
    }

    QString decodeRemainingFuel()
    {
        QString timeHours = decodeTimeHours();
        QString timeMinutes = decodeTimeMinutes();
        return timeHours + ":" + timeMinutes;
    }

    QString decodeRouteClearance()
    {
        bool flag0 = getBits(1);
        bool flag1 = getBits(1);
        bool flag2 = getBits(1);
        bool flag3 = getBits(1);
        bool flag4 = getBits(1);
        bool flag5 = getBits(1);
        bool flag6 = getBits(1);
        bool flag7 = getBits(1);
        bool flag8 = getBits(1);
        bool flag9 = getBits(1);
        QString s;
        if (flag0) {
            s.append("\nAirportDeparture: " + decodeAirport());
        }
        if (flag1) {
            s.append("\nAirportDestination: " + decodeAirport());
        }
        if (flag2) {
            s.append("\nRunwayDeparture: " + decodeRunway());
        }
        if (flag3) {
            s.append("\nProcedureDeparture: " + decodeProcedureName());
        }
        if (flag4) {
            s.append("\nRunwayArrival: " + decodeRunway());
        }
        if (flag5) {
            s.append("\nProcedureApproach: " + decodeProcedureName());
        }
        if (flag6) {
            s.append("\nProcedureArrival: " + decodeProcedureName());
        }
        if (flag7) {
            s.append("\nAirwayIntercept: " + decodeAirwayIntercept());
        }
        if (flag8) {
            int size = getBits(7) + 1;
            s.append("\nRouteInformation: ");
            for (int i = 0; i < size; i++) {
                s.append(decodeRouteInformation() + " ");
            }
        }
        if (flag9) {
            s.append("\nRouteInformationAdditional: " + decodeRouteInformationAdditional());
        }
        return s;
    }

    QString decodePublishedIdentifier()
    {
        bool flag = getBits(1);
        QString s = decodeFixName();
        if (flag) {
            s.append(" " + decodeLatitudeLongitude());
        }
        return s;
    }

    QString decodeRemainingSouls()
    {
        int souls = getBits(10);
        return QString::number(souls + 1);
    }

    QString decodeVersionNumber()
    {
        int version = getBits(4);
        return QString::number(version);
    }

    QString decodeRunway()
    {
        QString runwayDirection = decodeRunwayDirection();
        QString runwayConfiguration = decodeRunwayConfiguration();
        return runwayDirection + runwayConfiguration;
    }

    QString decodeRunwayDirection()
    {
        int dir = getBits(6);
        return QString::number(dir);
    }

    QString decodeRunwayConfiguration()
    {
        int cfg = getBits(2);
        QStringList cfgs = {"L", "R", "C", ""};
        return cfgs[cfg];
    }

    QString decodeAirwayIntercept()
    {
        int size = getBits(3) + 1;
        return decodeIA5String(size);
    }

    QString decodeRouteInformation()
    {
        int type = getBits(3);
        switch (type)
        {
        case 0:
            return decodePublishedIdentifier();
        case 1:
            return decodeLatitudeLongitude();
        case 2:
            return decodePlaceBearingPlaceBearing();
        case 3:
            return decodePlaceBearingDistance();
        case 4:
            return decodeAirwayIdentifier();
        case 5:
            return decodeTrackDetail();
        default:
            return QString("UNKOWN ROUTE INFORMATION TYPE %1").arg(type);
        }
    }

    QString decodePlaceBearing()
    {
        bool flag = getBits(1);
        QString s = decodeFixName();
        if (flag) {
            s.append(" " + decodeLatitudeLongitude());
        }
        s.append(" " + decodeDegrees());
        return s;
    }

    QString decodePlaceBearingPlaceBearing()
    {
        QString placeBearing1 = decodePlaceBearing();
        QString placeBearing2 = decodePlaceBearing();
        return placeBearing1 + " " + placeBearing2;
    }

    QString decodeAirwayIdentifier()
    {
        int size = getBits(3) + 1;
        return decodeIA5String(size);
    }

    QString decodeTrackDetail()
    {
        int size = getBits(7) + 1;
        QString s;
        for (int i = 0; i < size; i++) {
            s.append(" " + decodeLatitudeLongitude());
        }
        return s;
    }

    QString decodeTrackName()
    {
        int size = getBits(2) + 3;
        return decodeIA5String(size);
    }

    QString decodeSupplementaryInformation()
    {
        return "";
    }

    QString decodeRouteInformationAdditional()
    {
        return "";
    }

};

#endif // INCLUDE_DO219_H
