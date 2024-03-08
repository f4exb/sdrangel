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

// This source code file was last time modified by Arvo ES1JA on January 5th, 2019
// All changes are shown in the patch file coming together with the full JTDX source code.

#include "callsign.h"

const QRegularExpression Callsign::valid_callsign_regexp {R"([2,4-9]{1,1}[A-Z]{1,1}[0-9]{1,4}|[3]{1,1}[A-Z]{1,2}[0-9]{1,4}|[A-Z]{1,2}[0-9]{1,4})"};
const QRegularExpression Callsign::prefix_re {R"(^(?:(?<prefix>[2-9]{0,1}[A-Z]{1,2}[0-9]{1,4})?)?)"};

Q_GLOBAL_STATIC(Callsign, callsign)
Callsign *Callsign::instance()
{
    return callsign;
}

Callsign::Callsign()
{
    m_countryDat.init();
    m_countryDat.load();
}

Callsign::~Callsign()
{}

bool Callsign::is_callsign (QString const& callsign)
{
    if ((!callsign.at(1).isDigit() && callsign.size () == 2) || callsign == "F" || callsign == "G" || callsign == "I" || callsign == "K" || callsign == "W")
    {
        auto call = callsign + "0";
        return call.contains (valid_callsign_regexp);
    }
    else
    {
        return callsign.contains (valid_callsign_regexp);
    }
}

bool Callsign::is_compound_callsign (QString const& callsign)
{
    return callsign.contains ('/');
}

// split on first '/' and return the larger portion or the whole if
// there is no '/'
QString Callsign::base_callsign (QString callsign)
{
    auto slash_pos = callsign.indexOf('/');

    if (slash_pos >= 0)
    {
        auto right_size = callsign.size () - slash_pos - 1;

        if (right_size>= slash_pos) {
            callsign = callsign.mid(slash_pos + 1);
        } else {
            callsign = callsign.left(slash_pos);
        }
    }

    return callsign.toUpper();
}

// analyze the callsign and determine the effective prefix, returns
// the full call if no valid prefix (or prefix as a suffix) is specified
QString Callsign::effective_prefix(QString callsign)
{
    QStringList parts = callsign.split('/');

    if (parts.size() == 2 && (parts.at(1) == "P" || parts.at(1) == "A" || parts.at(1) == "MM"))  {
        return callsign;
    }

    auto prefix = parts.at(0);
    int size = prefix.size();
    int region = -1;

    for (int i = 1; i < parts.size(); ++i)
    {
        if (parts.at(i) == "AM")
        {
            prefix="1B1ABCD";
            size = prefix.size();
        }
        else if (is_callsign(parts.at(i)) && size > parts.at(i).size() && !((parts.at(i) == "LH" || parts.at(i) == "ND" || parts.at(i) == "AG" || parts.at(i) == "AE" || parts.at(i) == "KT") && i == (parts.size() -1)))
        {
            prefix = parts.at(i);
            size = prefix.size();
        }
        else {
            bool ok;
            auto reg = parts.at(i).toInt(&ok);

            if (ok) {
                region = reg;
            }
        }
    }

    auto const& match = prefix_re.match (prefix);
    auto shorted = match.captured ("prefix");

    if (shorted.isEmpty() || region > -1)
    {
        if (shorted.isEmpty())
        {
            if (region > -1) {
                shorted = prefix+"0";
            } else {
                shorted = prefix;
            }
        }

        if (region > -1) {
            shorted = shorted.left(shorted.size()-1) + QString::number(region);
        }

        return shorted.toUpper ();
    }
    else
    {
        return prefix;
    }
}

QString Callsign::striped_prefix (QString callsign)
{
    auto const& match = prefix_re.match(callsign);
    return match.captured("prefix");
}

CountryDat::CountryInfo Callsign::getCountryInfo(QString const& callsign)
{
    QString pf = callsign.toUpper();

    while (!pf.isEmpty ())
  	{
        CountryDat::CountryInfo country = CountryDat::nullCountry;

        if (pf.length() == callsign.length())
        {
            country = m_countryDat.getCountries().value("="+pf, country);

            if (country.country == CountryDat::nullCountry.country) {
                pf = effective_prefix(callsign);
            } else {
                return country;
            }
        }

        if (pf == "KG4" && callsign.length() != 5) {
            pf = "AA";
        }

        country = m_countryDat.getCountries().value(pf, country);

        if (country.country != CountryDat::nullCountry.country) {
	        return country;
        }

        pf = pf.left(pf.length()-1);
	}

    return CountryDat::nullCountry;
}
