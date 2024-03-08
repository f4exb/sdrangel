///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

// This source code file was last time modified by Arvo ES1JA on 20190224
// All changes are shown in the patch file coming together with the full JTDX source code.

/*
#Sov Mil Order of Malta:   15:  28:  EU:   41.90:   -12.43:    -1.0:  1A:
    #1A;
#Spratly Islands:          26:  50:  AS:    9.88:  -114.23:    -8.0:  1S:
    #1S,9M0,BV9S;
#Monaco:                   14:  27:  EU:   43.73:    -7.40:    -1.0:  3A:
    #3A;
#Heard Island:             39:  68:  AF:  -53.08:   -73.50:    -5.0:  VK0H:
    #=VK0IR;
#Macquarie Island:         30:  60:  OC:  -54.60:  -158.88:   -10.0:  VK0M:
    #=VK0KEV;
#Cocos-Keeling:            29:  54:  OC:  -12.15:   -96.82:    -6.5:  VK9C:
    #AX9C,AX9Y,VH9C,VH9Y,VI9C,VI9Y,VJ9C,VJ9Y,VK9C,VK9Y,VL9C,VL9Y,VM9C,VM9Y,
    #VN9C,VN9Y,VZ9C,VZ9Y,=VK9AA;
*/



#include "countrydat.h"
#include <QFile>
#include <QTextStream>

const CountryDat::CountryInfo CountryDat::nullCountry = CountryDat::CountryInfo{"", "?", "where?", "", ""};

void CountryDat::init()
{
    _countries.clear();
    _name.clear();
    _name.insert("where?","where?");
    _name.insert("Sov Mil Order of Malta","Sov Mil Order of Malta");
    _name.insert("Spratly Islands","Spratly Is.");
    _name.insert("Monaco","Monaco");
    _name.insert("Agalega & St. Brandon","Agalega & St. Brandon");
    _name.insert("Mauritius","Mauritius");
    _name.insert("Rodriguez Island","Rodriguez Is.");
    _name.insert("Equatorial Guinea","Equatorial Guinea");
    _name.insert("Annobon Island","Annobon Is.");
    _name.insert("Fiji","Fiji");
    _name.insert("Conway Reef","Conway Reef");
    _name.insert("Rotuma Island","Rotuma Is.");
    _name.insert("Kingdom of Eswatini","Kingdom of Eswatini");
    _name.insert("Tunisia","Tunisia");
    _name.insert("Vietnam","Vietnam");
    _name.insert("Guinea","Guinea");
    _name.insert("Bouvet","Bouvet");
    _name.insert("Peter 1 Island","Peter 1 Is.");
    _name.insert("Azerbaijan","Azerbaijan");
    _name.insert("Georgia","Georgia");
    _name.insert("Montenegro","Montenegro");
    _name.insert("Sri Lanka","Sri Lanka");
    _name.insert("ITU HQ","ITU HQ");
    _name.insert("United Nations HQ","United Nations HQ");
    _name.insert("Vienna Intl Ctr","Vienna Intl Ctr");
    _name.insert("Timor - Leste","Timor - Leste");
    _name.insert("Israel","Israel");
    _name.insert("Libya","Libya");
    _name.insert("Cyprus","Cyprus");
    _name.insert("Tanzania","Tanzania");
    _name.insert("Nigeria","Nigeria");
    _name.insert("Madagascar","Madagascar");
    _name.insert("Mauritania","Mauritania");
    _name.insert("Niger","Niger");
    _name.insert("Togo","Togo");
    _name.insert("Samoa","Samoa");
    _name.insert("Uganda","Uganda");
    _name.insert("Kenya","Kenya");
    _name.insert("Senegal","Senegal");
    _name.insert("Jamaica","Jamaica");
    _name.insert("Yemen","Yemen");
    _name.insert("Lesotho","Lesotho");
    _name.insert("Malawi","Malawi");
    _name.insert("Algeria","Algeria");
    _name.insert("Barbados","Barbados");
    _name.insert("Maldives","Maldives");
    _name.insert("Guyana","Guyana");
    _name.insert("Croatia","Croatia");
    _name.insert("Ghana","Ghana");
    _name.insert("Malta","Malta");
    _name.insert("Zambia","Zambia");
    _name.insert("Kuwait","Kuwait");
    _name.insert("Sierra Leone","Sierra Leone");
    _name.insert("West Malaysia","W. Malaysia");
    _name.insert("East Malaysia","E. Malaysia");
    _name.insert("Nepal","Nepal");
    _name.insert("Dem. Rep. of the Congo","Dem. Rep. of the Congo");
    _name.insert("Burundi","Burundi");
    _name.insert("Singapore","Singapore");
    _name.insert("Rwanda","Rwanda");
    _name.insert("Trinidad & Tobago","Trinidad & Tobago");
    _name.insert("Botswana","Botswana");
    _name.insert("Tonga","Tonga");
    _name.insert("Oman","Oman");
    _name.insert("Bhutan","Bhutan");
    _name.insert("United Arab Emirates","United Arab Emirates");
    _name.insert("Qatar","Qatar");
    _name.insert("Bahrain","Bahrain");
    _name.insert("Pakistan","Pakistan");
    _name.insert("Scarborough Reef","Scarborough Reef");
    _name.insert("Taiwan","Taiwan");
    _name.insert("Pratas Island","Pratas Is.");
    _name.insert("China","China");
    _name.insert("Nauru","Nauru");
    _name.insert("Andorra","Andorra");
    _name.insert("The Gambia","The Gambia");
    _name.insert("Bahamas","Bahamas");
    _name.insert("Mozambique","Mozambique");
    _name.insert("Chile","Chile");
    _name.insert("San Felix & San Ambrosio","San Felix & San Ambrosio");
    _name.insert("Easter Island","Easter Is.");
    _name.insert("Juan Fernandez Islands","Juan Fernandez Is.");
    _name.insert("Antarctica","Antarctica");
    _name.insert("Cuba","Cuba");
    _name.insert("Morocco","Morocco");
    _name.insert("Bolivia","Bolivia");
    _name.insert("Portugal","Portugal");
    _name.insert("Madeira Islands","Madeira Is.");
    _name.insert("Azores","Azores");
    _name.insert("Uruguay","Uruguay");
    _name.insert("Sable Island","Sable Is.");
    _name.insert("St. Paul Island","St. Paul Is.");
    _name.insert("Angola","Angola");
    _name.insert("Cape Verde","Cape Verde");
    _name.insert("Comoros","Comoros");
    _name.insert("Fed. Rep. of Germany","Germany");
    _name.insert("Philippines","Philippines");
    _name.insert("Eritrea","Eritrea");
    _name.insert("Palestine","Palestine");
    _name.insert("North Cook Islands","N. Cook Is.");
    _name.insert("South Cook Islands","S. Cook Is.");
    _name.insert("Niue","Niue");
    _name.insert("Bosnia-Herzegovina","Bosnia-Herzegovina");
    _name.insert("Spain","Spain");
    _name.insert("Balearic Islands","Balearic Is.");
    _name.insert("Canary Islands","Canary Is.");
    _name.insert("Ceuta & Melilla","Ceuta & Melilla");
    _name.insert("Ireland","Ireland");
    _name.insert("Armenia","Armenia");
    _name.insert("Liberia","Liberia");
    _name.insert("Iran","Iran");
    _name.insert("Moldova","Moldova");
    _name.insert("Estonia","Estonia");
    _name.insert("Ethiopia","Ethiopia");
    _name.insert("Belarus","Belarus");
    _name.insert("Kyrgyzstan","Kyrgyzstan");
    _name.insert("Tajikistan","Tajikistan");
    _name.insert("Turkmenistan","Turkmenistan");
    _name.insert("France","France");
    _name.insert("Guadeloupe","Guadeloupe");
    _name.insert("Mayotte","Mayotte");
    _name.insert("St. Barthelemy","St. Barthelemy");
    _name.insert("New Caledonia","New Caledonia");
    _name.insert("Chesterfield Islands","Chesterfield Is.");
    _name.insert("Martinique","Martinique");
    _name.insert("French Polynesia","Fr. Polynesia");
    _name.insert("Austral Islands","Austral Is.");
    _name.insert("Clipperton Island","Clipperton Is.");
    _name.insert("Marquesas Islands","Marquesas Is.");
    _name.insert("St. Pierre & Miquelo","St. Pierre & Miquelo");
    _name.insert("Reunion Island","Reunion Is.");
    _name.insert("St. Martin","St. Martin");
    _name.insert("Glorioso Islands","Glorioso Is.");
    _name.insert("Juan de Nova, Europa","Juan de Nova, Europa");
    _name.insert("Tromelin Island","Tromelin Is.");
    _name.insert("Crozet Island","Crozet Is.");
    _name.insert("Kerguelen Islands","Kerguelen Is.");
    _name.insert("Amsterdam & St. Paul Is.","Amsterdam & St. Paul Is.");
    _name.insert("Wallis & Futuna Islands","Wallis & Futuna Is.");
    _name.insert("French Guiana","Fr. Guiana");
    _name.insert("England","England");
    _name.insert("Isle of Man","Isle of Man");
    _name.insert("Northern Ireland","N. Ireland");
    _name.insert("Jersey","Jersey");
    _name.insert("Shetland Islands","Shetland Is.");
    _name.insert("Scotland","Scotland");
    _name.insert("Guernsey","Guernsey");
    _name.insert("Wales","Wales");
    _name.insert("Solomon Islands","Solomon Is.");
    _name.insert("Temotu Province","Temotu Province");
    _name.insert("Hungary","Hungary");
    _name.insert("Switzerland","Switzerland");
    _name.insert("Liechtenstein","Liechtenstein");
    _name.insert("Ecuador","Ecuador");
    _name.insert("Galapagos Islands","Galapagos Is.");
    _name.insert("Haiti","Haiti");
    _name.insert("Dominican Republic","Dominican Rep.");
    _name.insert("Colombia","Colombia");
    _name.insert("San Andres & Providencia","San Andres & Providencia");
    _name.insert("Malpelo Island","Malpelo Is.");
    _name.insert("Republic of Korea","Rep. of Korea");
    _name.insert("Panama","Panama");
    _name.insert("Honduras","Honduras");
    _name.insert("Thailand","Thailand");
    _name.insert("Vatican City","Vatican City");
    _name.insert("Saudi Arabia","Saudi Arabia");
    _name.insert("Italy","Italy");
    _name.insert("African Italy","AF Italy");
    _name.insert("Sardinia","Sardinia");
    _name.insert("Sicily","Sicily");
    _name.insert("Djibouti","Djibouti");
    _name.insert("Grenada","Grenada");
    _name.insert("Guinea-Bissau","Guinea-Bissau");
    _name.insert("St. Lucia","St. Lucia");
    _name.insert("Dominica","Dominica");
    _name.insert("St. Vincent","St. Vincent");
    _name.insert("Japan","Japan");
    _name.insert("Minami Torishima","Minami Torishima");
    _name.insert("Ogasawara","Ogasawara");
    _name.insert("Mongolia","Mongolia");
    _name.insert("Svalbard","Svalbard");
    _name.insert("Bear Island","Bear Is.");
    _name.insert("Jan Mayen","Jan Mayen");
    _name.insert("Jordan","Jordan");
    _name.insert("United States","U.S.A.");
    _name.insert("Guantanamo Bay","Guantanamo Bay");
    _name.insert("Mariana Islands","Mariana Is.");
    _name.insert("Baker & Howland Islands","Baker & Howland Is.");
    _name.insert("Guam","Guam");
    _name.insert("Johnston Island","Johnston Is.");
    _name.insert("Midway Island","Midway Is.");
    _name.insert("Palmyra & Jarvis Islands","Palmyra & Jarvis Is.");
    _name.insert("Hawaii","Hawaii");
    _name.insert("Kure Island","Kure Is.");
    _name.insert("American Samoa","American Samoa");
    _name.insert("Swains Island","Swains Is.");
    _name.insert("Wake Island","Wake Is.");
    _name.insert("Alaska","Alaska");
    _name.insert("Navassa Island","Navassa Is.");
    _name.insert("US Virgin Islands","US Virgin Is.");
    _name.insert("Puerto Rico","Puerto Rico");
    _name.insert("Desecheo Island","Desecheo Is.");
    _name.insert("Norway","Norway");
    _name.insert("Argentina","Argentina");
    _name.insert("Luxembourg","Luxembourg");
    _name.insert("Lithuania","Lithuania");
    _name.insert("Bulgaria","Bulgaria");
    _name.insert("Peru","Peru");
    _name.insert("Lebanon","Lebanon");
    _name.insert("Austria","Austria");
    _name.insert("Finland","Finland");
    _name.insert("Aland Islands","Aland Is.");
    _name.insert("Market Reef","Market Reef");
    _name.insert("Czech Republic","Czech Rep.");
    _name.insert("Slovak Republic","Slovak Rep.");
    _name.insert("Belgium","Belgium");
    _name.insert("Greenland","Greenland");
    _name.insert("Faroe Islands","Faroe Is.");
    _name.insert("Denmark","Denmark");
    _name.insert("Papua New Guinea","Papua New Guinea");
    _name.insert("Aruba","Aruba");
    _name.insert("DPR of Korea","DPR of Korea");
    _name.insert("Netherlands","Netherlands");
    _name.insert("Curacao","Curacao");
    _name.insert("Bonaire","Bonaire");
    _name.insert("Saba & St. Eustatius","Saba & St. Eustatius");
    _name.insert("Sint Maarten","Sint Maarten");
    _name.insert("Brazil","Brazil");
    _name.insert("Fernando de Noronha","Fernando de Noronha");
    _name.insert("St. Peter & St. Paul","St. Peter & St. Paul");
    _name.insert("Trindade & Martim Vaz","Trindade & Martim Vaz");
    _name.insert("Suriname","Suriname");
    _name.insert("Franz Josef Land","Franz Josef Land");
    _name.insert("Western Sahara","Western Sahara");
    _name.insert("Bangladesh","Bangladesh");
    _name.insert("Slovenia","Slovenia");
    _name.insert("Seychelles","Seychelles");
    _name.insert("Sao Tome & Principe","Sao Tome & Principe");
    _name.insert("Sweden","Sweden");
    _name.insert("Poland","Poland");
    _name.insert("Sudan","Sudan");
    _name.insert("Egypt","Egypt");
    _name.insert("Greece","Greece");
    _name.insert("Mount Athos","Mount Athos");
    _name.insert("Dodecanese","Dodecanese");
    _name.insert("Crete","Crete");
    _name.insert("Tuvalu","Tuvalu");
    _name.insert("Western Kiribati","W. Kiribati");
    _name.insert("Central Kiribati","C. Kiribati");
    _name.insert("Eastern Kiribati","E. Kiribati");
    _name.insert("Banaba Island","Banaba Is.");
    _name.insert("Somalia","Somalia");
    _name.insert("San Marino","San Marino");
    _name.insert("Palau","Palau");
    _name.insert("Asiatic Turkey","AS Turkey");
    _name.insert("European Turkey","EU Turkey");
    _name.insert("Iceland","Iceland");
    _name.insert("Guatemala","Guatemala");
    _name.insert("Costa Rica","Costa Rica");
    _name.insert("Cocos Island","Cocos Is.");
    _name.insert("Cameroon","Cameroon");
    _name.insert("Corsica","Corsica");
    _name.insert("Central African Republic","C. African Rep.");
    _name.insert("Republic of the Congo","Rep. of the Congo");
    _name.insert("Gabon","Gabon");
    _name.insert("Chad","Chad");
    _name.insert("Cote d'Ivoire","Cote d'Ivoire");
    _name.insert("Benin","Benin");
    _name.insert("Mali","Mali");
    _name.insert("European Russia","EU Russia");
    _name.insert("Kaliningrad","Kaliningrad");
    _name.insert("Asiatic Russia","AS Russia");
    _name.insert("Uzbekistan","Uzbekistan");
    _name.insert("Kazakhstan","Kazakhstan");
    _name.insert("Ukraine","Ukraine");
    _name.insert("Antigua & Barbuda","Antigua & Barbuda");
    _name.insert("Belize","Belize");
    _name.insert("St. Kitts & Nevis","St. Kitts & Nevis");
    _name.insert("Namibia","Namibia");
    _name.insert("Micronesia","Micronesia");
    _name.insert("Marshall Islands","Marshall Is.");
    _name.insert("Brunei Darussalam","Brunei Darussalam");
    _name.insert("Canada","Canada");
    _name.insert("Australia","Australia");
    _name.insert("Heard Island","Heard Is.");
    _name.insert("Macquarie Island","Macquarie Is.");
    _name.insert("Cocos (Keeling) Islands","Cocos (Keeling) Is.");
    _name.insert("Lord Howe Island","Lord Howe Is.");
    _name.insert("Mellish Reef","Mellish Reef");
    _name.insert("Norfolk Island","Norfolk Is.");
    _name.insert("Willis Island","Willis Is.");
    _name.insert("Christmas Island","Christmas Is.");
    _name.insert("Anguilla","Anguilla");
    _name.insert("Montserrat","Montserrat");
    _name.insert("British Virgin Islands","British Virgin Is.");
    _name.insert("Turks & Caicos Islands","Turks & Caicos Is.");
    _name.insert("Pitcairn Island","Pitcairn Is.");
    _name.insert("Ducie Island","Ducie Is.");
    _name.insert("Falkland Islands","Falkland Is.");
    _name.insert("South Georgia Island","S. Georgia Is.");
    _name.insert("South Shetland Islands","S. Shetland Is.");
    _name.insert("South Orkney Islands","S. Orkney Is.");
    _name.insert("South Sandwich Islands","S. Sandwich Is.");
    _name.insert("Bermuda","Bermuda");
    _name.insert("Chagos Islands","Chagos Is.");
    _name.insert("Hong Kong","Hong Kong");
    _name.insert("India","India");
    _name.insert("Andaman & Nicobar Is.","Andaman & Nicobar Is.");
    _name.insert("Lakshadweep Islands","Lakshadweep Is.");
    _name.insert("Mexico","Mexico");
    _name.insert("Revillagigedo","Revillagigedo");
    _name.insert("Burkina Faso","Burkina Faso");
    _name.insert("Cambodia","Cambodia");
    _name.insert("Laos","Laos");
    _name.insert("Macao","Macao");
    _name.insert("Myanmar","Myanmar");
    _name.insert("Afghanistan","Afghanistan");
    _name.insert("Indonesia","Indonesia");
    _name.insert("Iraq","Iraq");
    _name.insert("Vanuatu","Vanuatu");
    _name.insert("Syria","Syria");
    _name.insert("Latvia","Latvia");
    _name.insert("Nicaragua","Nicaragua");
    _name.insert("Romania","Romania");
    _name.insert("El Salvador","El Salvador");
    _name.insert("Serbia","Serbia");
    _name.insert("Venezuela","Venezuela");
    _name.insert("Aves Island","Aves Is.");
    _name.insert("Zimbabwe","Zimbabwe");
    _name.insert("North Macedonia","N. Macedonia");
    _name.insert("Republic of Kosovo","Rep. of Kosovo");
    _name.insert("Republic of South Sudan","Rep. of S. Sudan");
    _name.insert("Albania","Albania");
    _name.insert("Gibraltar","Gibraltar");
    _name.insert("UK Base Areas on Cyprus","UK Base Areas on Cyprus");
    _name.insert("St. Helena","St. Helena");
    _name.insert("Ascension Island","Ascension Is.");
    _name.insert("Tristan da Cunha & Gough","Tristan da Cunha & Gough");
    _name.insert("Cayman Islands","Cayman Is.");
    _name.insert("Tokelau Islands","Tokelau Is.");
    _name.insert("New Zealand","New Zealand");
    _name.insert("Chatham Islands","Chatham Is.");
    _name.insert("Kermadec Islands","Kermadec Is.");
    _name.insert("N.Z. Subantarctic Is.","N.Z. Subantarctic Is.");
    _name.insert("Paraguay","Paraguay");
    _name.insert("South Africa","S. Africa");
    _name.insert("Pr. Edward & Marion Is.","Pr. Edward & Marion Is.");
}

QString CountryDat::_extractName(const QString line)
{
    int s1 = line.indexOf(':');

    if (s1>=0)
    {
        QString name = line.left(s1);
        return _name.value(name,name);
    }

    return "";
}

QString CountryDat::_extractMasterPrefix(const QString line)
{
    int s1 = line.lastIndexOf(' ');
    int s2 = line.lastIndexOf(':');

    if (s1 >= 0 && s1 < s2)
    {
        QString pfx = line.mid(s1, s2-s1);
        return pfx.toUpper();
    }

    return "";
}

QString CountryDat::_extractContinent(const QString line)
{
    int s1;
    s1 = line.indexOf(':');

    if (s1>=0)
    {
        s1 = line.indexOf(':',s1+1);

        if (s1>=0)
        {
            s1 = line.indexOf(':',s1+1);

            if (s1>=0)
            {
                s1 = line.indexOf(':',s1+1);

                if (s1>=0)
                {
                     QString cont = line.mid(s1-2, 2);
                     return cont;
                }
            }
        }
    }

    return "";
}

QString CountryDat::_extractITUZ(const QString line)
{
    int s1;
    s1 = line.indexOf(':');

    if (s1>=0)
    {
        s1 = line.indexOf(':',s1+1);

        if (s1>=0)
        {
            s1 = line.indexOf(':',s1+1);

            if (s1>=0)
            {
                QString cont = line.mid(s1-2, 2);

                if (cont.size() == 1) {
                    cont = " " + cont;
                }

                return cont;
            }
        }
    }

    return "";
}

QString CountryDat::_extractCQZ(const QString line)
{
    int s1;
    s1 = line.indexOf(':');

    if (s1>=0)
    {
        s1 = line.indexOf(':',s1+1);

        if (s1>=0)
        {
            QString cont = line.mid(s1-2, 2);

            if (cont.size() == 1) {
                cont = " " + cont;
            }

            return cont;
        }
    }

    return "";
}

QString CountryDat::_removeBrackets(QString &line, const QString a, const QString b)
{
    QString res = "";
    int s1 = line.indexOf(a);

    while (s1 >= 0)
    {
        int s2 = line.indexOf(b);
        res += line.mid(s1+1,s2-s1-1);
        line = line.left(s1) + line.mid(s2+1,-1);
        s1 = line.indexOf(a);
    }

    return res;
}

QStringList CountryDat::_extractPrefix(QString &line, bool &more)
{
    QString a;
    line = line.remove(" \n");
    line = line.replace(" ","");
    a = _removeBrackets(line,"<",">");
    a = _removeBrackets(line,"~","~");
    int s1 = line.indexOf(';');
    more = true;

    if (s1 >= 0)
    {
        line = line.left(s1);
        more = false;
    }

    QStringList r = line.split(',');

    return r;
}


void CountryDat::load()
{
    _countries.clear();
    QFile inputFile(":/data/cty.dat");

    if (inputFile.open(QIODevice::ReadOnly))
    {
        QTextStream in(&inputFile);

        while (!in.atEnd())
        {
            QString line1 = in.readLine();

            if (!in.atEnd())
            {
                QString line2 = in.readLine();
                QString masterPrefix, country, cqz, ituz, continent;
                cqz = _extractCQZ(line1);
                ituz = _extractITUZ(line1);
                continent = _extractContinent(line1);
                masterPrefix = _extractMasterPrefix(line1).trimmed();
                country = _extractName(line1).trimmed();

                if (!masterPrefix.isEmpty() || !country.isEmpty())
                {
                    bool more = true;
                    QStringList prefixs;

                    while (more)
                    {
                        QStringList p = _extractPrefix(line2,more);
                        prefixs += p;

                        if (more) {
                            line2 = in.readLine();
                        }
                    }

                    QString p,_cqz,_ituz,_continent;

                    foreach(p,prefixs)
                    {
                        if (!p.isEmpty())
                        {
                            _cqz = _removeBrackets(p,"(",")");

                            if (_cqz.isEmpty()) {
                                _cqz = cqz;
                            }
                            if (_cqz.size() == 1) {
                                _cqz = "0" + _cqz;
                            }

                            _ituz = _removeBrackets(p,"[","]");

                            if (_ituz.isEmpty()) {
                                _ituz = ituz;
                            }
                            if (_ituz.size() == 1) {
                                _ituz = "0" + _ituz;
                            }

                            _continent = _removeBrackets(p,"{","}");

                            if (_continent.isEmpty()) {
                                _continent = continent;
                            }

                            _countries.insert(p, CountryInfo{_continent, masterPrefix, country, _cqz, _ituz});
                        }
                    }
                }
            }
        }

        inputFile.close();
    }
}
