///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FEATURE_STARTRACKERGUI_H_
#define INCLUDE_FEATURE_STARTRACKERGUI_H_

#include <QTimer>
#include <QtCharts>

#include "feature/featuregui.h"
#include "util/messagequeue.h"
#include "startrackersettings.h"

class PluginAPI;
class FeatureUISet;
class StarTracker;

namespace Ui {
    class StarTrackerGUI;
}

using namespace QtCharts;

class StarTrackerGUI : public FeatureGUI {
    Q_OBJECT
public:
    static StarTrackerGUI* create(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature);
    virtual void destroy();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    virtual MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

private:
    Ui::StarTrackerGUI* ui;
    PluginAPI* m_pluginAPI;
    FeatureUISet* m_featureUISet;
    StarTrackerSettings m_settings;
    bool m_doApplySettings;

    StarTracker* m_starTracker;
    MessageQueue m_inputMessageQueue;
    QTimer m_statusTimer;
    int m_lastFeatureState;

    QChart m_chart;
    QDateTimeAxis m_chartXAxis;
    QValueAxis m_chartYAxis;

    explicit StarTrackerGUI(PluginAPI* pluginAPI, FeatureUISet *featureUISet, Feature *feature, QWidget* parent = nullptr);
    virtual ~StarTrackerGUI();

    void blockApplySettings(bool block);
    void applySettings(bool force = false);
    void displaySettings();
    void updateForTarget();
    QString convertDegreesToText(double degrees);
    bool handleMessage(const Message& message);
    void updateLST();
    void plotChart();

    void leaveEvent(QEvent*);
    void enterEvent(QEvent*);

private slots:
    void onMenuDialogCalled(const QPoint &p);
    void onWidgetRolled(QWidget* widget, bool rollDown);
    void handleInputMessages();
    void on_startStop_toggled(bool checked);
    void on_useMyPosition_clicked(bool checked=false);
    void on_latitude_valueChanged(double value);
    void on_longitude_valueChanged(double value);
    void on_rightAscension_editingFinished();
    void on_declination_editingFinished();
    void on_target_currentTextChanged(const QString &text);
    void on_displaySettings_clicked();
    void on_dateTimeSelect_currentTextChanged(const QString &text);
    void on_dateTime_dateTimeChanged(const QDateTime &datetime);
    void updateStatus();
    void on_viewOnMap_clicked();
};


#endif // INCLUDE_FEATURE_STARTRACKERGUI_H_
