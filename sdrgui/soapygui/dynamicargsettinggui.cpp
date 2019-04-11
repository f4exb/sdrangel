///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include "dynamicargsettinggui.h"

DynamicArgSettingGUI::DynamicArgSettingGUI(ArgInfoGUI *argSettingGUI, const QString& name, QObject *parent) :
    QObject(parent),
    m_argSettingGUI(argSettingGUI),
    m_name(name)
{
    connect(m_argSettingGUI, SIGNAL(valueChanged()), this, SLOT(processValueChanged()));
}

DynamicArgSettingGUI::~DynamicArgSettingGUI()
{
    disconnect(m_argSettingGUI, SIGNAL(valueChanged()), this, SLOT(processValueChanged()));
}

void DynamicArgSettingGUI::processValueChanged()
{
    if (m_argSettingGUI->getValueType() == ArgInfoGUI::ArgInfoValueBool) {
        emit valueChanged(m_name, QVariant(m_argSettingGUI->getBoolValue()));
    } else if (m_argSettingGUI->getValueType() == ArgInfoGUI::ArgInfoValueInt) {
        emit valueChanged(m_name, QVariant(m_argSettingGUI->getIntValue()));
    } else if (m_argSettingGUI->getValueType() == ArgInfoGUI::ArgInfoValueFloat) {
        emit valueChanged(m_name, QVariant(m_argSettingGUI->getFloatValue()));
    } else if (m_argSettingGUI->getValueType() == ArgInfoGUI::ArgInfoValueString) {
        emit valueChanged(m_name, QVariant(m_argSettingGUI->getStringValue()));
    }
}

QVariant DynamicArgSettingGUI::getValue() const
{
    if (m_argSettingGUI->getValueType() == ArgInfoGUI::ArgInfoValueBool) {
        return QVariant(m_argSettingGUI->getBoolValue());
    } else if (m_argSettingGUI->getValueType() == ArgInfoGUI::ArgInfoValueInt) {
        return QVariant(m_argSettingGUI->getIntValue());
    } else if (m_argSettingGUI->getValueType() == ArgInfoGUI::ArgInfoValueFloat) {
        return QVariant(m_argSettingGUI->getFloatValue());
    } else if (m_argSettingGUI->getValueType() == ArgInfoGUI::ArgInfoValueString) {
        return QVariant(m_argSettingGUI->getStringValue());
    } else {
        return QVariant(false);
    }
}

void DynamicArgSettingGUI::setValue(const QVariant& value)
{
    bool ok = false;

    if (value.type() == QVariant::Bool)
    {
        m_argSettingGUI->setBoolValue(value.toBool());
    }
    else if (value.type() == QVariant::Int)
    {
        int newValue = value.toInt(&ok);

        if (ok) {
            m_argSettingGUI->setIntValue(newValue);
        }
    }
    else if (value.type() == QVariant::Double)
    {
        double newValue = value.toDouble(&ok);

        if (ok) {
            m_argSettingGUI->setFloatValue(newValue);
        }
    }
    else if (value.type() == QVariant::String)
    {
        m_argSettingGUI->setStringValue(value.toString());
    }
}
