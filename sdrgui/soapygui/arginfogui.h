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

#ifndef SDRGUI_SOAPYGUI_ARGINFOGUI_H_
#define SDRGUI_SOAPYGUI_ARGINFOGUI_H_

#include <QWidget>
#include <QString>

#include "export.h"

namespace Ui {
    class ArgInfoGUI;
}

class SDRGUI_API ArgInfoGUI : public QWidget
{
    Q_OBJECT
public:
    enum ArgInfoType
    {
        ArgInfoBinary,
        ArgInfoContinuous,
        ArgInfoDiscrete
    };

    enum ArgInfoValueType
    {
        ArgInfoValueBool,
        ArgInfoValueInt,
        ArgInfoValueFloat,
        ArgInfoValueString
    };

    explicit ArgInfoGUI(ArgInfoType type, ArgInfoValueType valueType, QWidget *parent = 0);
    ~ArgInfoGUI();

    void setLabel(const QString& text);
    void setToolTip(const QString& text);
    void setUnits(const QString& units);

    ArgInfoType getType() const { return m_type; }
    ArgInfoValueType getValueType() const { return m_valueType; }
    void setRange(double min, double max);

    bool getBoolValue() const { return m_boolValue; }
    void setBoolValue(bool value);
    void addBoolValue(const QString& text, bool value);

    int getIntValue() const { return m_intValue; }
    void setIntValue(int value);
    void addIntValue(const QString& text, int value);

    double getFloatValue() const { return m_floatValue; }
    void setFloatValue(double value);
    void addFloatValue(const QString& text, double value);

    const QString& getStringValue() const { return m_stringValue; }
    void setStringValue(const QString& value);
    void addStringValue(const QString& text, const QString& value);

signals:
    void valueChanged();

private slots:
    void on_argCheck_toggled(bool checked);
    void on_argEdit_editingFinished();
    void on_argCombo_currentIndexChanged(int index);

private:
    int setIntegerValue(int value);
    double setDoubleValue(double value);
    void updateUIFromBool();
    void updateUIFromInt();
    void updateUIFromFloat();
    void updateUIFromString();

    Ui::ArgInfoGUI* ui;
    ArgInfoType m_type;
    ArgInfoValueType m_valueType;
    bool m_boolValue;
    int m_intValue;
    double m_floatValue;
    QString m_stringValue;
    bool m_hasRange;
    double m_minValue;
    double m_maxValue;
};

#endif /* SDRGUI_SOAPYGUI_ARGINFOGUI_H_ */
