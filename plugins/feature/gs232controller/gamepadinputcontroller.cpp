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

#include <QDebug>
#include <QGamepadManager>

#include "gamepadinputcontroller.h"
#include "gamepadconfigurationdialog.h"

GamepadInputController::GamepadInputController(int deviceId) :
    m_gamepad(deviceId),
    m_rightX(0.0),
    m_rightY(0.0),
    m_leftX(0.0),
    m_leftY(0.0),
    m_configurationDialog(nullptr)
{
    connect(&m_gamepad, &QGamepad::axisRightXChanged, this, &GamepadInputController::axisRightXChanged);
    connect(&m_gamepad, &QGamepad::axisRightYChanged, this, &GamepadInputController::axisRightYChanged);
    connect(&m_gamepad, &QGamepad::axisLeftXChanged, this, &GamepadInputController::axisLeftXChanged);
    connect(&m_gamepad, &QGamepad::axisLeftYChanged, this, &GamepadInputController::axisLeftYChanged);
    connect(&m_gamepad, &QGamepad::buttonAChanged, this, &GamepadInputController::buttonAChanged);
    connect(&m_gamepad, &QGamepad::buttonBChanged, this, &GamepadInputController::buttonBChanged);
    connect(&m_gamepad, &QGamepad::buttonXChanged, this, &GamepadInputController::buttonXChanged);
    connect(&m_gamepad, &QGamepad::buttonYChanged, this, &GamepadInputController::buttonYChanged);
    connect(&m_gamepad, &QGamepad::buttonUpChanged, this, &GamepadInputController::buttonUpChanged);
    connect(&m_gamepad, &QGamepad::buttonDownChanged, this, &GamepadInputController::buttonDownChanged);
    connect(&m_gamepad, &QGamepad::buttonLeftChanged, this, &GamepadInputController::buttonLeftChanged);
    connect(&m_gamepad, &QGamepad::buttonRightChanged, this, &GamepadInputController::buttonRightChanged);
    connect(&m_gamepad, &QGamepad::buttonR1Changed, this, &GamepadInputController::buttonR1Changed);
    connect(&m_gamepad, &QGamepad::buttonL1Changed, this, &GamepadInputController::buttonL1Changed);
    connect(&m_gamepad, &QGamepad::buttonR3Changed, this, &GamepadInputController::buttonR3Changed);
    connect(&m_gamepad, &QGamepad::buttonL3Changed, this, &GamepadInputController::buttonL3Changed);
}

double GamepadInputController::getAxisValue(int axis)
{
    switch (axis)
    {
    case 0:
        return m_rightX;
    case 1:
        return m_rightY;
    case 2:
        return m_leftX;
    case 3:
        return m_leftY;
    }
    return 0.0;
}

int GamepadInputController::getNumberOfAxes() const
{
    return 4;
}

bool GamepadInputController::supportsConfiguration() const
{
    // Should only return true on Linux evdev or Android
#if defined(LINUX) || defined(ANDROID)
    return true;
#else
    return false;
#endif
}

void GamepadInputController::configure(InputControllerSettings *settings)
{
    if (!m_configurationDialog)
    {
        m_configurationDialog = new GamepadConfigurationDialog(&m_gamepad, settings, supportsConfiguration());
        connect(m_configurationDialog, &QDialog::finished, this, &GamepadInputController::configurationDialogClosed);
        m_configurationDialog->setAttribute(Qt::WA_DeleteOnClose, true);
        m_configurationDialog->setModal(false);
        m_configurationDialog->show();
    }
    else
    {
        m_configurationDialog->raise();
        m_configurationDialog->activateWindow();
    }
}

void GamepadInputController::configurationDialogClosed()
{
    m_configurationDialog = nullptr;
    emit configurationComplete();
}

void GamepadInputController::axisRightXChanged(double value)
{
    m_rightX = value;
}

void GamepadInputController::axisRightYChanged(double value)
{
    m_rightY = value;
}

void GamepadInputController::axisLeftXChanged(double value)
{
    m_leftX = value;
}

void GamepadInputController::axisLeftYChanged(double value)
{
    m_leftY = value;
}

void GamepadInputController::buttonAChanged(bool value)
{
    emit buttonChanged(INPUTCONTROLLER_BUTTON_RIGHT_BOTTOM, !value);
}

void GamepadInputController::buttonBChanged(bool value)
{
    emit buttonChanged(INPUTCONTROLLER_BUTTON_RIGHT_RIGHT, !value);
}

void GamepadInputController::buttonXChanged(bool value)
{
    emit buttonChanged(INPUTCONTROLLER_BUTTON_RIGHT_LEFT, !value);
}

void GamepadInputController::buttonYChanged(bool value)
{
    emit buttonChanged(INPUTCONTROLLER_BUTTON_RIGHT_TOP, !value);
}

void GamepadInputController::buttonUpChanged(bool value)
{
    emit buttonChanged(INPUTCONTROLLER_BUTTON_LEFT_UP, !value);
}

void GamepadInputController::buttonDownChanged(bool value)
{
    emit buttonChanged(INPUTCONTROLLER_BUTTON_LEFT_DOWN, !value);
}

void GamepadInputController::buttonLeftChanged(bool value)
{
    emit buttonChanged(INPUTCONTROLLER_BUTTON_LEFT_LEFT, !value);
}

void GamepadInputController::buttonRightChanged(bool value)
{
    emit buttonChanged(INPUTCONTROLLER_BUTTON_LEFT_RIGHT, !value);
}

void GamepadInputController::buttonL1Changed(bool value)
{
    emit buttonChanged(INPUTCONTROLLER_BUTTON_L1, !value);
}

void GamepadInputController::buttonR1Changed(bool value)
{
    emit buttonChanged(INPUTCONTROLLER_BUTTON_R1, !value);
}

void GamepadInputController::buttonL3Changed(bool value)
{
    emit buttonChanged(INPUTCONTROLLER_BUTTON_L3, !value);
}

void GamepadInputController::buttonR3Changed(bool value)
{
    emit buttonChanged(INPUTCONTROLLER_BUTTON_R3, !value);
}

QStringList GamepadInputController::getAllControllers()
{
    QStringList names;
    QGamepadManager *gamepadManager = QGamepadManager::instance();

    if (gamepadManager)
    {
        const QList<int> gamepads = gamepadManager->connectedGamepads();
        for (const auto gamepad : gamepads)
        {
            QString name;
            if (gamepadManager->gamepadName(gamepad).isEmpty()) {
                name = QString("Gamepad %1").arg(gamepad);
            } else {
                name = gamepadManager->gamepadName(gamepad);
            }
            qDebug() << "GamepadInputController::getAllControllers: Gamepad: " << gamepad << "name:" << gamepadManager->gamepadName(gamepad) << " connected " << gamepadManager->isGamepadConnected(gamepad);
            names.append(name);
        }
        if (gamepads.size() == 0) {
            qDebug() << "GamepadInputController::getAllControllers: No gamepads";
        }
    }
    else
    {
        qDebug() << "GamepadInputController::getAllControllers: No gamepad manager";
    }
    return names;
}

GamepadInputController* GamepadInputController::open(const QString& name)
{
    GamepadInputController *inputController = nullptr;
    QGamepadManager *gamepadManager = QGamepadManager::instance();

    if (gamepadManager)
    {
        const QList<int> gamepads = gamepadManager->connectedGamepads();
        for (const auto gamepad : gamepads)
        {
            QString gamepadName;
            if (gamepadManager->gamepadName(gamepad).isEmpty()) {
                gamepadName = QString("Gamepad %1").arg(gamepad);
            } else {
                gamepadName = gamepadManager->gamepadName(gamepad);
            }
            if (name == gamepadName)
            {
                inputController = new GamepadInputController(gamepad);
                if (inputController)
                {
                    qDebug() << "GamepadInputController::open: Opened gamepad " << name;
                }
                else
                {
                    qDebug() << "GamepadInputController::open: Failed to open gamepad: " << gamepad;
                }
            }
        }
    }

    return inputController;
}
