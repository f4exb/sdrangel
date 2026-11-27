///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026                                                                //
// written by Robin Getz <rgetz503@gmail.com>                                        //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////

#include "releaseupdateservice.h"

#ifdef ENABLE_RELEASE_CHECK_GITHUB
#include "releaseupdateservicegithub.h"
#endif

ReleaseUpdateService *createReleaseUpdateService(QObject *parent)
{
#ifdef ENABLE_RELEASE_CHECK_GITHUB
    return new ReleaseUpdateServiceGitHub(parent);
#else
    return nullptr;
#endif
}
