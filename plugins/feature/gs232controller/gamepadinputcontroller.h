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

class GamepadConfigurationDialog;

class GamepadInputController : public InputController {

public:

    GamepadInputController(int deviceId);
    double getAxisValue(int axis) override;
    int getNumberOfAxes() const override;
    bool supportsConfiguration() const override;
    void configure(InputControllerSettings *settings) override;

    static QStringList getAllControllers();
    static GamepadInputController* open(const QString& name);

private slots:

    void configurationDialogClosed();
    void axisRightXChanged(double value);
    void axisRightYChanged(double value);
    void axisLeftXChanged(double value);
    void axisLeftYChanged(double value);
    void buttonAChanged(bool value);
    void buttonBChanged(bool value);
    void buttonXChanged(bool value);
    void buttonYChanged(bool value);
    void buttonUpChanged(bool value);
    void buttonDownChanged(bool value);
    void buttonLeftChanged(bool value);
    void buttonRightChanged(bool value);
    void buttonL1Changed(bool value);
    void buttonR1Changed(bool value);
    void buttonL3Changed(bool value);
    void buttonR3Changed(bool value);

private:

    QGamepad m_gamepad;
    double m_rightX;
    double m_rightY;
    double m_leftX;
    double m_leftY;
    GamepadConfigurationDialog *m_configurationDialog;

};

#endif // INCLUDE_FEATURE_GAMEPADINPUTCONTROLLER_H_

