///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef SDRGUI_GUI_FEATURESDOCK_H_
#define SDRGUI_GUI_FEATURESDOCK_H_

#include <QDockWidget>

#include "featureadddialog.h"
#include "featurepresetsdialog.h"

class QHBoxLayout;
class QLabel;
class QPushButton;
class QStringList;

class FeaturesDock : public QDockWidget
{
    Q_OBJECT
public:
    FeaturesDock(QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    ~FeaturesDock();

    void resetAvailableFeatures() { m_featureAddDialog.resetFeatureNames(); }
    void addAvailableFeatures(const QStringList& featureNames) { m_featureAddDialog.addFeatureNames(featureNames); }

    void setPresets(QList<FeatureSetPreset*>* presets) { m_featurePresetsDialog.setPresets(presets); }
    void setFeatureUISet(FeatureUISet *featureUISet) { m_featurePresetsDialog.setFeatureUISet(featureUISet); }
    void setPluginAPI(PluginAPI *pluginAPI) { m_featurePresetsDialog.setPluginAPI(pluginAPI); }
    void setWebAPIAdapter(WebAPIAdapterInterface *apiAdapter) { m_featurePresetsDialog.setWebAPIAdapter(apiAdapter); }

private:
    QPushButton *m_addFeatureButton;
    QPushButton *m_presetsButton;
    QWidget *m_titleBar;
    QHBoxLayout *m_titleBarLayout;
    QLabel *m_titleLabel;
    QPushButton *m_normalButton;
    QPushButton *m_closeButton;
    FeatureAddDialog m_featureAddDialog;
    FeaturePresetsDialog m_featurePresetsDialog;

private slots:
    void toggleFloating();
    void addFeatureDialog();
    void presetsDialog();
    void addFeatureEmitted(int featureIndex);

signals:
    void addFeature(int);
};

#endif // SDRGUI_GUI_FEATURESDOCK_H_
