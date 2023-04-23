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

#ifndef INCLUDE_FEATURE_GAMEPADINPUTCONTROLLER_H_
#define INCLUDE_FEATURE_GAMEPADINPUTCONTROLLER_H_

#include "inputcontroller.h"

#include <QGamepad>

class GamepadInputController : public InputController {

public:

    GamepadInputController(int deviceId);
    double getAxisValue(int axis) override;
    int getNumberOfAxes() const override;

    static QStringList getAllControllers();
    static GamepadInputController* open(const QString& name);

private slots:

    void axisRightXChanged(double value);
    void axisRightYChanged(double value);
    void axisLeftXChanged(double value);
    void axisLeftYChanged(double value);

private:

    QGamepad m_gamepad;
    double m_rightX;
    double m_rightY;
    double m_leftX;
    double m_leftY;
};

#endif // INCLUDE_FEATURE_GAMEPADINPUTCONTROLLER_H_

