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

#include <QEvent>
#include <QHelpEvent>
#include <QToolTip>
#include <QDebug>

#include "util/ourairportsdb.h"

#include "acarsmessageview.h"

// Explanations common to every view. One table for all of them, rather than a copy in
// each: there is a view in every ACARS demodulator and more in the Aircraft feature
static const QHash<QString, QString>& builtInAcronyms()
{
    static const QHash<QString, QString> acronyms({
        {"ACARS", "Aircraft Communications Addressing and Reporting System"},
        {"ACK", "Acknowledge"},
        {"ADS", "Automatic Dependent Surveillance"},
        {"ADT", "Approved Departure Time"},
        {"APCH", "Approach"},
        {"ARINC", "Aeronautical Radio, Incorporated"},
        {"AFN", "ATS Facilities Notification"},
        {"ATC", "Air Traffic Control"},
        {"ATS", "Air Traffic Services"},
        {"ATIS", "Automated Terminal Information Service"},
        {"ARR", "Arrival"},
        {"BTN", "Between"},
        {"CAA", "Civil Aviation Authority"},
        {"CPC", "Controller to Pilot Communications"},
        {"CRC", "Cyclic Redundancy Check"},
        {"CPDLC", "Controller-Pilot Data Link Communications"},
        {"CTOT", "Calculated Take Off Time"},
        {"DEP", "Departure"},
        {"DSP", "Datalink Service Provider"},
        {"EFB", "Electronic Flight Bag"},
        {"ETA", "Estimated Time of Arrival"},
        {"FANS", "Future Air Navigation System"},
        {"FL", "Flight Level"},
        {"FMC", "Flight Management Computer"},
        {"FMS", "Flight Management System"},
        {"GNSS", "Global Navigations Satellite System"},
        {"HF", "High Frequency"},
        {"HFDL", "High Frequency Data Link"},
        {"IATA", "International Air Transport Association"},
        {"ICAO", " International Civil Aviation Organization"},
        {"IFR", "Instrument Flight Rules"},
        {"ILS", "Instrument Landing System"},
        {"IMI", "Imbedded Message Identifier"},
        {"MIAM", "Media Independent Aircraft Messaging"},
        {"MDI", "Minimum Departure Interval"},
        {"MFI", "Message Function Indentifier"},
        {"MTI", "Message Text Identifier"},
        {"MU", "Management Unit"},
        {"NAT", "North Atlantic Track"},
        {"NAK", "Non Acknowledgement"},
        {"NAV", "Navigation"},
        {"NDB", "Non-Directional Beacon"},
        {"NOTAM", "Notice to Airmen"},
        {"QNH", "Barometer setting"},
        {"RWY", "Runway"},
        {"SATCOM", "Satellite Communications"},
        {"SDU", "Satellite Data Unit"},
        {"SID", "Standard Instrument Departure"},
        {"SMI", "Standard Message Identifier"},
        {"STAR", "Standard Terminal Arrival Route"}, 
        {"SITA", "Society Internationale de Telecommunications Aeronautiques"},
        {"TCAS", "Traffic Alert/Collision Avoidance System"},
        {"TEI", "Text Element Identifier"},
        {"TWI", "Terminal Weather Information"},
        {"TWIP", "Terminal Weather Information for Pilots"},
        {"UTC", "Coordinated Universal Time"},
        {"VDL", "VHF Digital Link"},
        {"VHF", "Very High Frequency"},
        {"VFR", "Visual Flight Rules"},
        {"WILCO", "Will Comply"},
        {"WXRQ", "Weather Request"},
        
        // METAR
        {"SKC", "No Cloud/Sky Clear"},
        {"NCD", "No Cloud Detected"},
        {"CLR", "No Clouds below 12,000 ft"},
        {"NSC", "No Signficant Cloud"},
        {"FEW", "Few = 1-2 oktas"},
        {"SCT", "Scattered = 3-4 oktas"},
        {"BKN", "Broken = 5-7 oktas"},
        {"OVC", "Overcast = 8 oktas"},
        {"TCU", "Towering Cumulus Cloud"},
        {"CB", "Cumulonimbus cloud"},
        {"VV", "Vertical Visibility"},
        {"VRB", "Variable"},
        {"VIS", "Visibility"},
        {"WS", "Wind Shear"},
        {"WND", "Wind"},
        {"SFC", "Surface"},
        {"OHD", "Overhead"},
        {"LTG", "Lightning"},
        {"KT", "Knots"},
        {"CAVOK", "Ceiling and Visibilty OK"},
        {"NR", "Not Reported"},
        
        // TEI codes from ARINC 620 Append B
        {"AC", "Estimated time approach clearance"},
        {"AD", "Aerodrome of concern or arrival"},
        {"AL", "Altitude or flight level"},
        {"AN", "Aircraft number"},
        {"AP", "Aircraft located at an airport"},
        {"AR", "Arrival runway"},
        {"AU", "Auxiliary power unit (APU)"},
        {"BC", "Billing Code"},
        {"BF", "Boarded fuel"},
        {"CL", "Cruising level"},
        {"CP", "Cargo and Payload Information"},
        {"CZ", "Cruising speed"},
        {"DA", "Aerodrome of departure"},
        {"DP", "Dew point"},
        {"DS", "Destination station"},
        {"DT", "Communication service information"},
        {"DV", "Identification of aircraft being diverted"},
        {"ED", "Estimated time of departure"},
        {"EN", "Endurance"},
        {"EO", "Estimated time over"},
        {"FB", "Fuel on board"},
        {"FC", "Estimated further clearance"},
        {"FD", "Fuel over destination"},
        {"FI", "Flight identification"},
        {"GL", "Approximate geographic location of aircraft"},
        {"HD", "Aircraft heading"},
        {"IC", "Aircraft icing"},
        {"IN", "IN Time"},
        {"LA", "Identification of officer landing aircraft"},
        {"LP", "Log Page"},
        {"LR", "Identification landing category"},
        {"MA", "Message assurance"},
        {"MN", "Maintenance"},
        {"NL", "Number of landings"},
        {"NP", "Next report point"},
        {"OF", "OFF Time"},
        {"ON", "ON Time"},
        {"OS", "Other supplementary information"},
        {"OT", "OUT Time"},
        {"OV", "Present location"},
        {"PB", "Number of persons on board"},
        {"PD", "Point of departure"},
        {"QN", "Altimeter setting"},
        {"RD", "Departure runway"},
        {"RF", "Request flight level"},
        {"RI", "Return in time"},
        {"RM", "Remarks"},
        {"RO", "Return on time"},
        {"RT", "Route information"},
        {"SA", "Alternative aerodrome"},
        {"SI", "Special communication addressing instruction"},
        {"SK", "Sky conditions"},
        {"SL", "SELCAL code"},
        {"SP", "Significant point"},
        {"TA", "Static air temperature"},
        {"TB", "Turbulence"},
        {"TM", "Surface air temperature"},
        {"TO", "Time over"},
        {"TP", "Transmission Path"},
        {"VR", "Runway visual range"},
        {"WV", "Wind information"},
        {"WX", "Weather report"},
        {"WI", "Weather"},
        {"ZW", "Zero fuel weight"},
        
        // SMIs from ARINC 620 Table C-2        
        {"HJK", "Hijack Situation Report"},
        {"AVR", "Voice Contact Request"},
        {"GVR", "Voice Go-ahead"},
        {"AEP", "Alternate Aircrew Initiated Position Report"},
        {"TIS", "ATIS Request"},
        {"AEP", "Aircrew Initiated Position Report"},
        {"WXR", "Weather Request"},
        //{"ETA", "Aircrew Revision to Previous ETA/Diversion Report"},
        {"AGM", "Airline Designated Downlink"},
        {"ENG", "Aircrew Initiated Engine Data/Takeoff Thrust Report"},
        {"AGM", "Aircrew Entered Miscellaneous Message"},
        {"SIT", "Emergency Message"},
        {"CLX", "Oceanic Clearance"},
        {"CLD", "Departure Clearance"},
        {"FSM", "Flight Systems Message"},
        {"RAR", "Request ADS Reports"},
        {"FTU", "Free Text from ATC"},
        {"DDS", "Deliver Departure Slot"},
        {"DAI", "ATIS Report"},
        {"AFU", "ATS Facility Notification"},
        {"TWI", "Terminal Weather Information for Pilots"},
        {"PBC", "Pushback Clearance"},
        {"ETC", "Expected Taxi Clearance"},
        {"CPR", "CPC Command Response"},
        {"RCL", "Request Oceanic Clearance"},
        {"CLA", "Oceanic Clearance Readback"},
        {"RCD", "Request Departure Clearance"},
        {"CDA", "Departure Clearance Readback"},
        {"POS", "Waypoint Position Report"},
        {"PAR", "Provide ADS Report"},
        {"FTD", "Free Text to ATC"},
        {"RDS", "Request Departure Slot"},
        {"RAI", "Request ATIS Report"},
        {"AFD", "ATS Facility Notification"},
        {"TWR", "Terminal Weather Information for Pilots"},
        {"PBR", "Pushback Clearance Request"},
        {"ETR", "Expected Taxi Clearance Request"},
        {"CPL", "CPC Log-on/Log-off Request"},
        {"CWR", "CPC WILCO/UNABLE Response"},
        {"CP0", "Undesignated Cockpit/Cabin Printer Messages, All Call"},
        {"AGM", "Designated Cockpit/Cabin Printer Messages"},
        {"JDI", "De-Icing"},
        {"JDL", "Data Loading"},
        {"EML", "Internet E-Mail Message"},
        {"EMS", "Internet E-Mail Message/DSP Service"},
        {"EM1", "Left Engine Monitoring Unit Messages"},
        {"EM2", "Right Engine Monitoring Unit Messages"},
        {"WXM", "Meteorological Report"},
        {"ICE", "Icing Report"},
        {"WXC", "Meteorological Command/Report"},
        {"REJ", "Undelivered Uplink Report"},
        {"KBM", "Loopback Request/Response"},
        {"JLB", "Cabin E-Logbook"},
        {"JLC", "Cabin E-Logbook"},
        {"JLS", "Technical (Cockpit) E-Logbook"},
        {"JLT", "Technical (Cockpit) E-Logbook"},
        {"AGM", "Departure/Arrival Report"},
        {"SVC", "Unable to Deliver Uplink Messages"},
        {"DLA", "Delay Message"},
        {"DIV", "Diversion Report"},
        {"JRE", "Refuel � Administrative and General Purpose"},
        {"JRF", "Refuel � CG Targeting"},
        {"NSR", "Network Statistics Report/Request"},
        {"NPR", "VHF Performance Report/Request"},
        {"APR", "LRU Configuration Report/Request"},
        {"MED", "Media Advisory"},
        {"TEB", "Turbulence Event Report"},
        
        // HFDL
        {"GS", "Ground Station"},

        // Loadsheet        
        {"DOW", "Dry Operating Weight"},
        {"ZFW", "Zero Fuel Weight"},
        {"ZFWT", "Zero Fuel Weight Total"},
        {"TOF", "Fake-Off Fuel"},
        {"TOW", "Take-Off Weight"},
        {"LAW", "Landing Weight"},
        {"LDW", "Landing Weight"},
        {"TIF", "Trip Fuel"},
        {"PAX", "Passengers"},
        {"MACZFW", "Mean Aerodynamic Chord Zero Fuel Weight"},
        {"MACTOW", "Mean Aerodynamic Chord Take-Off Weight"},
        {"TOMAC", "Take-Off Mean Aerodynamic Chord"},
        {"LIZFW", "Loading Index at Zero Fuel Weight"},
        {"LITOW", "Loading Index at Take-Off Weight"},
        {"NOTOC", "Notification to Captain"},
        {"TOCG", "Take-Off Centre of Gravity"},
        {"RMWT", "Ramp/Taxi Weight"},
        {"TOWT", "Take-Off Weight"},
        {"STAB", "Stabilizer"},


    });
    return acronyms;
}

AcarsMessageView::AcarsMessageView(QWidget *parent) :
    QTextEdit(parent)
{
    setMouseTracking(true);
}
    
bool AcarsMessageView::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip)
    {
        QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);
        QTextCursor cursor = cursorForPosition(helpEvent->pos());
        cursor.select(QTextCursor::WordUnderCursor);
        QString text = cursor.selectedText();
        // Remove trailing digits from METAR
        while (text.size() > 0 && text.right(1)[0].isDigit()) {
            text = text.left(text.size() - 1);
        }
        const QString meaning = explanation(text);
        if (!meaning.isEmpty())
        {
            QToolTip::showText(helpEvent->globalPos(), QString("%1 - %2").arg(text).arg(meaning));
        }
        else
        {
            if (!text.isEmpty()) {
                qDebug() << "No tooltip for " << text;
            }
            QToolTip::hideText();
        }
        return true;
    }
    return QTextEdit::event(event);
}

void AcarsMessageView::addAcronym(const QString &acronym, const QString &explanation)
{
    m_acronym.insert(acronym, explanation);
}

// Airports are looked up in the shared airport database when a tooltip is wanted. Copying
// every ICAO ident in to each view's table cost ~4.5 MB a view, ~117 MB with 26 views
QString AcarsMessageView::explanation(const QString& text) const
{
    if (text.isEmpty()) {
        return QString();
    }

    auto it = m_acronym.constFind(text);
    if (it != m_acronym.constEnd()) {
        return it.value();
    }

    it = builtInAcronyms().constFind(text);
    if (it != builtInAcronyms().constEnd()) {
        return it.value();
    }

    QSharedPointer<const QHash<QString, AirportInformation *>> airportsByIdent = OurAirportsDB::getAirportsByIdent();
    if (airportsByIdent)
    {
        const AirportInformation *airport = airportsByIdent->value(text);
        if (airport) {
            return airport->m_name;
        }
    }

    return QString();
}        
