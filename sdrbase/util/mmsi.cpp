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

#include <QResource>
#include <QDebug>

#include "util/mmsi.h"
#include "util/osndb.h"

// https://www.itu.int/en/ITU-R/terrestrial/fmd/Pages/mid.aspx
// Names used match up with names used for flags in ADS-B directory
QMap<int, QString> MMSI::m_mid = {
    {201, "albania"},
    {202, "andorra"},
    {203, "austria"},
    {204, "portugal"},
    {205, "belgium"},
    {206, "belarus"},
    {207, "bulgaria"},
    {208, "vatican_city"},
    {209, "cyprus"},
    {210, "cyprus"},
    {211, "germany"},
    {212, "cyprus"},
    {213, "georgia"},
    {214, "moldova"},
    {215, "malta"},
    {216, "armenia"},
    {218, "germany"},
    {219, "denmark"},
    {220, "denmark"},
    {224, "spain"},
    {225, "spain"},
    {226, "france"},
    {227, "france"},
    {228, "france"},
    {229, "malta"},
    {230, "finland"},
    {231, "denmark"},
    {232, "united_kingdom"},
    {233, "united_kingdom"},
    {234, "united_kingdom"},
    {235, "united_kingdom"},
    {236, "united_kingdom"},
    {237, "greece"},
    {238, "croatia"},
    {239, "greece"},
    {240, "greece"},
    {241, "greece"},
    {242, "morocco"},
    {243, "hungary"},
    {244, "netherlands"},
    {245, "netherlands"},
    {246, "netherlands"},
    {247, "italy"},
    {248, "malta"},
    {249, "malta"},
    {250, "ireland"},
    {251, "iceland"},
    {252, "liechtenstein"},
    {253, "luxembourg"},
    {254, "monaco"},
    {255, "portugal"},
    {256, "malta"},
    {257, "norway"},
    {258, "norway"},
    {259, "norway"},
    {261, "poland"},
    {262, "montenegro"},
    {263, "portugal"},
    {264, "romania"},
    {265, "sweden"},
    {266, "sweden"},
    {267, "slovakia"},
    {268, "san_marino"},
    {269, "switzerland"},
    {270, "czech_republic"},
    {271, "turkey"},
    {272, "ukraine"},
    {273, "russia"},
    {274, "macedonia"},
    {275, "latvia"},
    {276, "estonia"},
    {277, "slovenia"},
    {279, "serbia"},
    {301, "united_kingdom"},
    {303, "united_states"},
    {304, "antigua_and_barbuda"},
    {305, "antigua_and_barbuda"},
    {306, "netherlands"},
    {307, "netherlands"},
    {308, "bahamas"},
    {309, "bahamas"},
    {310, "bermuda"},
    {311, "bahamas"},
    {312, "belize"},
    {314, "barbados"},
    {316, "canada"},
    {319, "cayman_isles"},
    {321, "costa_rica"},
    {323, "cuba"},
    {325, "dominica"},
    {327, "dominican_republic"},
    {329, "france"},
    {330, "grenada"},
    {331, "denmark"}, // greenland
    {332, "guatemala"},
    {334, "honduras"},
    {336, "haiti"},
    {338, "united_states"},
    {339, "jamaica"},
    {341, "st_kitts_and_nevis"},
    {343, "st_lucia"},
    {345, "mexico"},
    {347, "france"}, // martinique
    {348, "united_kingdom"}, // montserrat
    {350, "nicaragua"},
    {351, "panama"},
    {352, "panama"},
    {353, "panama"},
    {354, "panama"},
    {355, "panama"},
    {356, "panama"},
    {357, "panama"},
    {358, "united_states"}, // puerto_rico
    {359, "el_salvador"},
    {361, "france"},
    {362, "trinidad_and_tobago"},
    {364, "turks_and_caicos"},
    {366, "united_states"},
    {367, "united_states"},
    {368, "united_states"},
    {369, "united_states"},
    {370, "panama"},
    {371, "panama"},
    {372, "panama"},
    {373, "panama"},
    {374, "panama"},
    {375, "st_vincent"},
    {376, "st_vincent"},
    {377, "st_vincent"},
    {378, "virgin_isles"},
    {401, "afghanistan"},
    {403, "saudi_arabia"},
    {405, "bangladesh"},
    {408, "bahrain"},
    {410, "bhutan"},
    {412, "china"},
    {413, "china"},
    {414, "china"},
    {416, "taiwan"},
    {417, "sri_lanka"},
    {419, "india"},
    {422, "iran"},
    {423, "azerbaijan"},
    {425, "iraq"},
    {428, "israel"},
    {431, "japan"},
    {432, "japan"},
    {434, "turkmenistan"},
    {436, "kazakhstan"},
    {437, "uzbekistan"},
    {438, "jordan"},
    {440, "korea_south"},
    {441, "korea_south"},
    {443, "palestine"},
    {445, "korea_north"},
    {447, "kuwait"},
    {450, "lebanon"},
    {451, "kyrgyzstan"},
    {453, "china"}, // macao
    {455, "maldives"},
    {457, "mongolia"},
    {459, "nepal"},
    {461, "oman"},
    {463, "pakistan"},
    {466, "qatar"},
    {468, "syria"},
    {470, "united_arab_emirates"},
    {471, "united_arab_emirates"},
    {472, "tajikistan"},
    {473, "yemen"},
    {474, "yemen"},
    {477, "hong_kong"},
    {478, "bosnia"},
    {501, "france"},
    {503, "australia"},
    {506, "myanmar"},
    {508, "brunei"},
    {510, "micronesia"},
    {511, "palau"},
    {512, "new_zealand"},
    {514, "cambodia"},
    {515, "cambodia"},
    {516, "australia"},
    {518, "cook_islands"},
    {520, "fiji"},
    {523, "australia"},
    {525, "indonesia"},
    {529, "kiribati"},
    {531, "laos"},
    {533, "malaysia"},
    {536, "united_states"},
    {538, "marshall islands"},
    {540, "france"},
    {542, "new_zealand"},
    {544, "nauru"},
    {546, "france"},
    {548, "philippines"},
    {550, "timorleste"},
    {553, "papua_new_guinea"},
    {555, "united_kingdom"},
    {557, "solomon_islands"},
    {559, "united_states"}, // american_samoa
    {561, "samoa"},
    {563, "singapore"},
    {564, "singapore"},
    {565, "singapore"},
    {566, "singapore"},
    {567, "thailand"},
    {570, "tonga"},
    {572, "tuvalu"},
    {574, "vietnam"},
    {576, "vanuatu"},
    {577, "vanuatu"},
    {578, "france"},
    {601, "south_africa"},
    {603, "angola"},
    {605, "algeria"},
    {607, "france"},
    {608, "united_kingdom"}, // ascension_island
    {609, "burundi"},
    {610, "benin"},
    {611, "botswana"},
    {612, "central_african_republic"},
    {613, "cameroun"}, // cameroon
    {615, "congoroc"},
    {616, "comoros"},
    {617, "cape_verde"},
    {618, "france"},
    {619, "ivory_coast"},
    {620, "comoros"},
    {621, "djibouti"},
    {622, "egypt"},
    {624, "ethiopia"},
    {625, "eritrea"},
    {626, "gabon"},
    {627, "ghana"},
    {629, "gambia"},
    {630, "guinea_bissau"},
    {631, "equatorial_guinea"},
    {632, "guinea"},
    {633, "burkina_faso"},
    {634, "kenya"},
    {635, "france"},
    {636, "liberia"},
    {637, "liberia"},
    {638, "south_sudan"},
    {642, "libya"},
    {644, "lesotho"},
    {645, "mauritius"},
    {647, "madagascar"},
    {649, "mali"},
    {650, "mozambique"},
    {654, "mauritania"},
    {655, "malawi"},
    {656, "niger"},
    {657, "nigeria"},
    {659, "namibia"},
    {660, "france"}, // reunion
    {661, "rwanda"},
    {662, "sudan"},
    {663, "senegal"},
    {664, "seychelles"},
    {665, "united_kingdom"}, // saint_helena
    {666, "somalia"},
    {667, "sierra_leone"},
    {668, "sao_tome_principe"},
    {669, "swaziland"}, // eswatini
    {670, "chad"},
    {671, "togo"},
    {672, "tunisia"},
    {674, "tanzania"},
    {675, "uganda"},
    {676, "congodrc"},
    {677, "tanzania"},
    {678, "zambia"},
    {679, "zimbabwe"},
    {701, "argentina"},
    {710, "brazil"},
    {720, "bolivia"},
    {725, "chile"},
    {730, "colombia"},
    {735, "ecuador"},
    {740, "falkland_isles"},
    {745, "france"}, // guiana
    {750, "guyana"},
    {755, "paraguay"},
    {760, "peru"},
    {765, "suriname"},
    {770, "uruguay"},
    {775, "venezuela"},
};

QString MMSI::getMID(const QString &mmsi)
{
    if (mmsi.startsWith("00") || mmsi.startsWith("99") || mmsi.startsWith("98")) {
        return mmsi.mid(2, 3);
    } else if (mmsi.startsWith("0") || mmsi.startsWith("8")) {
        return mmsi.mid(1, 3);
    } else if (mmsi.startsWith("111")) {
        return mmsi.mid(3, 3);
    } else {
        return mmsi.left(3);
    }
}

QString MMSI::getCountry(const QString &mmsi)
{
    return m_mid[MMSI::getMID(mmsi).toInt()];
}

void MMSI::checkFlags()
{
    // Loop through all MIDs and check to see if we have a flag icon
    for (auto id : m_mid.keys())
    {
        QString country = m_mid.value(id);
        QString path = QString(":/flags/%1.bmp").arg(country);
        QResource res(path);
        if (!res.isValid()) {
            qDebug() << "MMSI::checkFlags: Resource invalid " << path;
        }
    }
}

QIcon *MMSI::getFlagIcon(const QString &mmsi)
{
    QString country = getCountry(mmsi);
    return AircraftInformation::getFlagIcon(country);
}

QString MMSI::getFlagIconURL(const QString &mmsi)
{
    QString country = getCountry(mmsi);
    return AircraftInformation::getFlagIconURL(country);
}

QString MMSI::getCategory(const QString &mmsi)
{
    switch (mmsi[0].toLatin1())
    {
    case '0':
        if (mmsi.startsWith("00")) {
            return "Coast";
        } else {
            return "Group"; // Group of ships
        }
    case '1':
        // Search and rescue
        if (mmsi[6] == '1') {
            return "SAR Aircraft";
        } else if (mmsi[6] == '5') {
            return "SAR Helicopter";
        } else {
            return "SAR";
        }
    case '8':
        return "Handheld";
    case '9':
        if (mmsi.startsWith("970"))
        {
            return "SAR";
        }
        else if (mmsi.startsWith("972"))
        {
            return "Man overboard";
        }
        else if (mmsi.startsWith("974"))
        {
            return "EPIRB"; // Emergency Becaon
        }
        else if (mmsi.startsWith("979"))
        {
            return "AMRD"; // Autonomous
        }
        else if (mmsi.startsWith("98"))
        {
            return "Craft with parent ship";
        }
        else if (mmsi.startsWith("99"))
        {
            if (mmsi[5] == '1') {
                return "Physical AtoN";
            } else if (mmsi[5] == '6') {
                return "Virtual AtoN";
            } else if (mmsi[5] == '8') {
                return "Mobile AtoN";
            } else {
                return "AtoN"; // Aid-to-navigation
            }
        }
        break;
    default:
        return "Ship"; // Vessel better?
    }
    return "Unknown";
}
