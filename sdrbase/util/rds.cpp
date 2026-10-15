///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2019, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2016 Ziga S <ziga.svetina@gmail.com>                            //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include "rds.h"

/* page 77, Annex F in the standard */
const char * const RDS::m_programmeTypes[RDS::m_nbProgrammeTypes] = {
	"None",
	"News",
	"Current Affairs",
	"Information",
	"Sport",
	"Education",
	"Drama",
	"Cultures",
	"Science",
	"Varied Speech",
	"Pop Music",
	"Rock Music",
	"Easy Listening",
	"Light Classics M",
	"Serious Classics",
	"Other Music",
	"Weather & Metr",
	"Finance",
	"Children’s Progs",
	"Social Affairs",
	"Religion",
	"Phone In",
	"Travel & Touring",
	"Leisure & Hobby",
	"Jazz Music",
	"Country Music",
	"National Music",
	"Oldies Music",
	"Folk Music",
	"Documentary",
	"Alarm Test",
	"Alarm-Alarm!"
};
