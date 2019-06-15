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

#include <math.h>
#include <QHBoxLayout>

#include "ui_arginfogui.h"
#include "arginfogui.h"

ArgInfoGUI::ArgInfoGUI(ArgInfoType type, ArgInfoValueType valueType, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ArgInfoGUI),
    m_type(type),
    m_valueType(valueType),
    m_boolValue(false),
    m_intValue(0),
    m_floatValue(0.0),
    m_hasRange(false),
    m_minValue(0.0),
    m_maxValue(0.0)
{
    ui->setupUi(this);
    QHBoxLayout *layout = ui->argLayout;

    if ((m_valueType == ArgInfoValueInt) || (m_valueType == ArgInfoValueFloat))
    {
        if (m_type == ArgInfoContinuous) {
            ui->argEdit->setAlignment(Qt::AlignRight);
        } else if (m_type == ArgInfoDiscrete) {
            ui->argCombo->setLayoutDirection(Qt::RightToLeft);
        }
    }

    if (m_type != ArgInfoBinary)
    {
        layout->removeWidget(ui->argCheck);
        delete ui->argCheck;
    }

    if (m_type != ArgInfoContinuous)
    {
        layout->removeWidget(ui->argEdit);
        delete ui->argEdit;
    }

    if (m_type != ArgInfoDiscrete)
    {
        layout->removeWidget(ui->argCombo);
        delete ui->argCombo;
    }
}

ArgInfoGUI::~ArgInfoGUI()
{
    delete ui;
}

void ArgInfoGUI::setRange(double min, double max)
{
    m_hasRange = true;
    m_minValue = min;
    m_maxValue = max;
}

void ArgInfoGUI::setLabel(const QString& text)
{
    ui->argLabel->setText(text);
}

void ArgInfoGUI::setToolTip(const QString& text)
{
    if (m_type == ArgInfoBinary) {
        ui->argCheck->setToolTip(text);
    } else if (m_type == ArgInfoContinuous) {
        ui->argEdit->setToolTip(text);
    } else if (m_type == ArgInfoDiscrete) {
        ui->argCombo->setToolTip(text);
    }
}

void ArgInfoGUI::setUnits(const QString& units)
{
    ui->argUnits->setText(units);
}

void ArgInfoGUI::setBoolValue(bool value)
{
    if (m_valueType == ArgInfoValueBool)
    {
        m_boolValue = value;
        updateUIFromBool();
    }
    else if (m_valueType == ArgInfoValueInt)
    {
        m_intValue = setIntegerValue(value ? 1 : 0);
        updateUIFromInt();
    }
    else if (m_valueType == ArgInfoValueFloat)
    {
        m_floatValue = setDoubleValue(value ? 1.0 : 0.0);
        updateUIFromFloat();
    }
    else if (m_valueType == ArgInfoValueString)
    {
        m_stringValue = QString(value ? "true" : "false");
        updateUIFromString();
    }
}

void ArgInfoGUI::addBoolValue(const QString& text, bool value)
{
    if (m_type == ArgInfoDiscrete) {
        ui->argCombo->addItem(text, QVariant(value));
    }
}

void ArgInfoGUI::setIntValue(int value)
{
    if (m_valueType == ArgInfoValueBool)
    {
        m_boolValue = (value != 0);
        updateUIFromBool();
    }
    else if (m_valueType == ArgInfoValueInt)
    {
        m_intValue = setIntegerValue(value);
        updateUIFromInt();
    }
    else if (m_valueType == ArgInfoValueFloat)
    {
        m_floatValue = setDoubleValue(value);
        updateUIFromFloat();
    }
    else if (m_valueType == ArgInfoValueString)
    {
        m_stringValue = QString("%1").arg(value);
        updateUIFromString();
    }
}

void ArgInfoGUI::addIntValue(const QString& text, int value)
{
    if (m_type == ArgInfoDiscrete) {
        ui->argCombo->addItem(text, QVariant(setIntegerValue(value)));
    }
}

void ArgInfoGUI::setFloatValue(double value)
{
    if (m_valueType == ArgInfoValueBool)
    {
        m_boolValue = (value != 0.0);
        updateUIFromBool();
    }
    else if (m_valueType == ArgInfoValueInt)
    {
        m_intValue = setIntegerValue(value);
        updateUIFromInt();
    }
    else if (m_valueType == ArgInfoValueFloat)
    {
        m_floatValue = setDoubleValue(value);
        updateUIFromFloat();
    }
    else if (m_valueType == ArgInfoValueString)
    {
        m_stringValue = QString("%1").arg(value);
        updateUIFromString();
    }
}

void ArgInfoGUI::addFloatValue(const QString& text, double value)
{
    if (m_type == ArgInfoDiscrete) {
        ui->argCombo->addItem(text, QVariant(setDoubleValue(value)));
    }
}

void ArgInfoGUI::setStringValue(const QString& value)
{
    if (m_valueType == ArgInfoValueBool)
    {
        m_boolValue = (value == "true");
        updateUIFromBool();
    }
    else if (m_valueType == ArgInfoValueInt)
    {
        int intValue = atoi(value.toStdString().c_str());
        m_intValue = setIntegerValue(intValue);
        updateUIFromInt();
    }
    else if (m_valueType == ArgInfoValueFloat)
    {
        double doubleValue = atof(value.toStdString().c_str());
        m_floatValue = setDoubleValue(doubleValue);
        updateUIFromFloat();
    }
    else if (m_valueType == ArgInfoValueString)
    {
        m_stringValue = value;
        updateUIFromString();
    }
}

void ArgInfoGUI::addStringValue(const QString& text, const QString& value)
{
    if (m_type == ArgInfoDiscrete) {
        ui->argCombo->addItem(text, QVariant(value));
    }
}

int ArgInfoGUI::setIntegerValue(int value)
{
    if (m_hasRange) {
        return value < round(m_minValue) ? round(m_minValue) : value > round(m_maxValue) ? round(m_maxValue) : value;
    } else {
        return value;
    }
}

double ArgInfoGUI::setDoubleValue(double value)
{
    if (m_hasRange) {
        return value < m_minValue ? m_minValue : value > m_maxValue ? m_maxValue : value;
    } else {
        return value;
    }
}

void ArgInfoGUI::updateUIFromBool()
{
    if (m_type == ArgInfoBinary)
    {
        ui->argCheck->blockSignals(true);
        ui->argCheck->setChecked(m_boolValue);
        ui->argCheck->blockSignals(false);
    }
    else if (m_type == ArgInfoContinuous)
    {
        ui->argEdit->blockSignals(true);
        ui->argEdit->setText(QString(m_boolValue ? "true" : "false"));
        ui->argEdit->blockSignals(false);
    }
    else if (m_type == ArgInfoDiscrete)
    {
        if (ui->argCombo->count() > 1)
        {
            ui->argCombo->blockSignals(true);
            ui->argCombo->setCurrentIndex(m_boolValue ? 0 : 1);
            ui->argCombo->blockSignals(false);
        }
    }
}

void ArgInfoGUI::updateUIFromInt()
{
    if (m_type == ArgInfoBinary)
    {
        ui->argCheck->blockSignals(true);
        ui->argCheck->setChecked(m_intValue == 0 ? false : true);
        ui->argCheck->blockSignals(false);
    }
    else if (m_type == ArgInfoContinuous)
    {
        ui->argEdit->blockSignals(true);
        ui->argEdit->setText(QString("%1").arg(m_intValue));
        ui->argEdit->blockSignals(false);
    }
    else if (m_type == ArgInfoDiscrete)
    {
        for (int i = 0; i < ui->argCombo->count(); i++)
        {
            if (ui->argCombo->itemData(i).type() == QVariant::Int)
            {
                bool ok = false;
                QVariant v = ui->argCombo->itemData(i);

                if (m_intValue >= v.toInt(&ok) && ok)
                {
                    ui->argCombo->blockSignals(true);
                    ui->argCombo->setCurrentIndex(i);
                    ui->argCombo->blockSignals(false);
                    break;
                }
            }
        }
    }
}

void ArgInfoGUI::updateUIFromFloat()
{
    if (m_type == ArgInfoBinary)
    {
        ui->argCheck->blockSignals(true);
        ui->argCheck->setChecked(m_floatValue == 0.0 ? false : true);
        ui->argCheck->blockSignals(false);
    }
    else if (m_type == ArgInfoContinuous)
    {
        ui->argEdit->blockSignals(true);
        ui->argEdit->setText(QString("%1").arg(m_floatValue));
        ui->argEdit->blockSignals(false);
    }
    else if (m_type == ArgInfoDiscrete)
    {
        for (int i = 0; i < ui->argCombo->count(); i++)
        {
            if (ui->argCombo->itemData(i).type() == QVariant::Double)
            {
                bool ok = false;
                QVariant v = ui->argCombo->itemData(i);

                if (m_floatValue >= v.toDouble(&ok) && ok)
                {
                    ui->argCombo->blockSignals(true);
                    ui->argCombo->setCurrentIndex(i);
                    ui->argCombo->blockSignals(false);
                    break;
                }
            }
        }
    }
}

void ArgInfoGUI::updateUIFromString()
{
    if (m_type == ArgInfoBinary)
    {
        ui->argCheck->blockSignals(true);
        ui->argCheck->setChecked(m_stringValue == "true" ? true : false);
        ui->argCheck->blockSignals(false);
    }
    else if (m_type == ArgInfoContinuous)
    {
        ui->argEdit->blockSignals(true);
        ui->argEdit->setText(m_stringValue);
        ui->argEdit->blockSignals(false);
    }
    else if (m_type == ArgInfoDiscrete)
    {
        for (int i = 0; i < ui->argCombo->count(); i++)
        {
            if (ui->argCombo->itemData(i).type() == QVariant::String)
            {
                QVariant v = ui->argCombo->itemData(i);

                if (m_stringValue == v.toString())
                {
                    ui->argCombo->blockSignals(true);
                    ui->argCombo->setCurrentIndex(i);
                    ui->argCombo->blockSignals(false);
                    break;
                }
            }
        }
    }
}

void ArgInfoGUI::on_argCheck_toggled(bool checked)
{
    setBoolValue(checked);
    emit valueChanged();
}

void ArgInfoGUI::on_argEdit_editingFinished()
{
    setStringValue(ui->argEdit->text());
    emit valueChanged();
}

void ArgInfoGUI::on_argCombo_currentIndexChanged(int index)
{
    (void) index;
    QVariant v = ui->argCombo->currentData();
    bool ok = false;

    if (v.type() == QVariant::Bool)
    {
        setBoolValue(v.toBool());
        emit valueChanged();
    }
    else if (v.type() == QVariant::Int)
    {
        setIntValue(v.toInt(&ok));

        if (ok) {
            emit valueChanged();
        }
    }
    else if (v.type() == QVariant::Double)
    {
        setFloatValue(v.toDouble(&ok));

        if (ok) {
            emit valueChanged();
        }
    }
    else if (v.type() ==QVariant::String)
    {
        setStringValue(v.toString());
        emit valueChanged();
    }
}

