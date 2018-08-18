///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon instance                                                            //
//                                                                               //
// SDRdaemon is a detached SDR front end that handles the interface with a       //
// physical device and sends or receives the I/Q samples stream to or from a     //
// SDRangel instance via UDP. It is controlled via a Web REST API.               //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QSettings>

#include "sdrdaemonpreferences.h"
#include "sdrdaemonsettings.h"

SDRDaemonSettings::SDRDaemonSettings()
{
    resetToDefaults();
}

SDRDaemonSettings::~SDRDaemonSettings()
{}

void SDRDaemonSettings::load()
{
    QSettings s;
    m_preferences.deserialize(qUncompress(QByteArray::fromBase64(s.value("preferences").toByteArray())));
}

void SDRDaemonSettings::save() const
{
    QSettings s;
    s.setValue("preferences", qCompress(m_preferences.serialize()).toBase64());
}

void SDRDaemonSettings::resetToDefaults()
{
    m_preferences.resetToDefaults();
}
