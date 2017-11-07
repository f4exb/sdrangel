///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
//                                                                               //
// OpenGL interface modernization.                                               //
// See: http://doc.qt.io/qt-5/qopenglshaderprogram.html                          //
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

#ifndef PLUGINS_CHANNELRX_DEMODATV_ATVSCREENINTERFACE_H_
#define PLUGINS_CHANNELRX_DEMODATV_ATVSCREENINTERFACE_H_

class ATVScreenInterface
{
public:
    ATVScreenInterface() :
        m_blnRenderImmediate(false)
    {}

    virtual ~ATVScreenInterface() {}

    virtual void resizeATVScreen(int intCols __attribute__((unused)), int intRows __attribute__((unused))) {}
    virtual void renderImage(unsigned char * objData __attribute__((unused))) {}
    virtual bool selectRow(int intLine __attribute__((unused))) { return false; }
    virtual bool setDataColor(int intCol __attribute__((unused)), int intRed __attribute__((unused)), int intGreen __attribute__((unused)), int intBlue __attribute__((unused))) { return false; }
    void setRenderImmediate(bool blnRenderImmediate) { m_blnRenderImmediate = blnRenderImmediate; }

protected:
    bool m_blnRenderImmediate;

};



#endif /* PLUGINS_CHANNELRX_DEMODATV_ATVSCREENINTERFACE_H_ */
