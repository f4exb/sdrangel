///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef DEVICES_PLUTOSDR_DEVICEPLUTOSDRPARAMS_H_
#define DEVICES_PLUTOSDR_DEVICEPLUTOSDRPARAMS_H_

#include <string>

#include "export.h"

class DEVICES_API DevicePlutoSDRBox;

/**
 * This structure refers to one physical device shared among parties (logical devices represented by
 * the DeviceAPI with single Rx or Tx stream type).
 * It allows storing information on the common resources in one place and is shared among participants.
 * There is only one copy that is constructed by the first participant and destroyed by the last.
 * A participant knows it is the first or last by checking the lists of buddies (Rx + Tx).
 */
class DEVICES_API DevicePlutoSDRParams
{
public:
    DevicePlutoSDRParams();
    ~DevicePlutoSDRParams();

    bool open(const std::string& serial);
    bool openURI(const std::string& uri);
    void close();

    DevicePlutoSDRBox *getBox() { return m_box; }

private:
    DevicePlutoSDRBox *m_box;
};


#endif /* DEVICES_PLUTOSDR_DEVICEPLUTOSDRPARAMS_H_ */
