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

#ifndef INCLUDE_FEATURE_INPUTCONTROLLER_H_
#define INCLUDE_FEATURE_INPUTCONTROLLER_H_

#include <QObject>

#include "inputcontrollersettings.h"

#define INPUTCONTROLLER_BUTTON_RIGHT_TOP        0           // Y / triangle
#define INPUTCONTROLLER_BUTTON_RIGHT_BOTTOM     1           // A / X
#define INPUTCONTROLLER_BUTTON_RIGHT_LEFT       2           // X / Square
#define INPUTCONTROLLER_BUTTON_RIGHT_RIGHT      3           // B / Circle

#define INPUTCONTROLLER_BUTTON_LEFT_UP          4           // D-Pad
#define INPUTCONTROLLER_BUTTON_LEFT_DOWN        5
#define INPUTCONTROLLER_BUTTON_LEFT_LEFT        6
#define INPUTCONTROLLER_BUTTON_LEFT_RIGHT       7

#define INPUTCONTROLLER_BUTTON_R1               8           // On top of controller
#define INPUTCONTROLLER_BUTTON_L1               9
#define INPUTCONTROLLER_BUTTON_R3               10          // Sticks pushed
#define INPUTCONTROLLER_BUTTON_L3               11

class InputController : public QObject {
    Q_OBJECT
public:

    // Called every ~50ms
    // axis 0-3. 0=Az/X, 1=El/Y, 2=Az Offset, 3=El Offset
    // value returned should be current axis position in range [-1,1]
    virtual double getAxisValue(int axis) = 0;
    // Gets axis value applying deadzone and sensitivity
    double getAxisCalibratedValue(int axis, InputControllerSettings *settings, bool highSensitivity);
    virtual int getNumberOfAxes() const = 0;
    virtual bool supportsConfiguration() const { return false; }
    virtual void configure(InputControllerSettings *settings) { (void) settings; };

signals:
    void buttonChanged(int button, bool released);
    void configurationComplete();

};

class InputControllerManager : public QObject {
    Q_OBJECT
public:

    static QStringList getAllControllers();
    static InputController* open(const QString& name);
    static InputControllerManager* instance();

signals:

    void controllersChanged();

private slots:
    void connectedGamepadsChanged();

private:
    InputControllerManager();

    static InputControllerManager *m_instance;

};


#endif // INCLUDE_FEATURE_INPUTCONTROLLER_H_
