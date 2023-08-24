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

#include <cmath>

#ifdef QT_GAMEPAD_FOUND
#include <QGamepadManager>
#include "gamepadinputcontroller.h"
#endif

#include "inputcontroller.h"

double InputController::getAxisCalibratedValue(int axis, InputControllerSettings *settings, bool highSensitvity)
{
    double value = getAxisValue(axis);
    double absValue = std::abs(value);
    double l = settings->m_deadzone[axis] / 100.0;
    if (absValue < l) {
        // Set to 0 if in deadzone
        value = 0.0;
    } else {
        // Rescale to [0,1] if outside of deadzone
        absValue = (absValue - l) / (1.0 - l);
        // Negate if original value was negative
        value = (value < 0.0) ? -absValue : absValue;
    }
    return value * (highSensitvity ? settings->m_highSensitivity : settings->m_lowSensitivity) / 100.0;
}

InputControllerManager* InputControllerManager::m_instance = nullptr;

QStringList InputControllerManager::getAllControllers()
{
#ifdef QT_GAMEPAD_FOUND
    return GamepadInputController::getAllControllers();
#else
    return {};
#endif
}

InputController* InputControllerManager::open(const QString& name)
{
#ifdef QT_GAMEPAD_FOUND
    return GamepadInputController::open(name);
#else
    return nullptr;
#endif
}

InputControllerManager* InputControllerManager::instance()
{
    if (!m_instance) {
        m_instance = new InputControllerManager();
    }
    return m_instance;
}

InputControllerManager::InputControllerManager()
{
#ifdef QT_GAMEPAD_FOUND
    connect(QGamepadManager::instance(), &QGamepadManager::connectedGamepadsChanged, this, &InputControllerManager::connectedGamepadsChanged);
#endif
}

void InputControllerManager::connectedGamepadsChanged()
{
    emit controllersChanged();
}
