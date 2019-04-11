///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
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

#ifndef DATVVIDEOPLAYER_H
#define DATVVIDEOPLAYER_H

#include "leansdr/framework.h"
#include "datvideostream.h"

namespace leansdr
{

template<typename T> struct datvvideoplayer: runnable
{
    datvvideoplayer(scheduler *sch, pipebuf<T> &_in, DATVideostream * objVideoStream) :
        runnable(sch, _in.name), in(_in), m_objVideoStream(objVideoStream)
    {
    }

    void run()
    {
        int size = in.readable() * sizeof(T);

        if (!size) {
            return;
        }

        int nw = m_objVideoStream->pushData((const char *) in.rd(), size);

        if (!nw)
        {
            fatal("leansdr::datvvideoplayer::run: pipe");
            return;
        }

        if (nw < 0)
        {
            fatal("leansdr::datvvideoplayer::run: write");
            return;
        }

        if (nw % sizeof(T))
        {
            fatal("leansdr::datvvideoplayer::run: partial write");
            return;
        }

        in.read(nw / sizeof(T));
    }

private:
    pipereader<T> in;
    DATVideostream * m_objVideoStream;
};

}

#endif // DATVVIDEOPLAYER_H
