///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2015 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2022-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#ifndef SDRGUI_GUI_WORKSPACE_H_
#define SDRGUI_GUI_WORKSPACE_H_

#include <QDockWidget>
#include <QStringList>

#include "export.h"
#include "featureadddialog.h"
#include "device/deviceapi.h"

class QHBoxLayout;
class QLabel;
class QToolButton;
class QPushButton;
class QMdiArea;
class QMdiSubWindow;
class QFrame;
class ButtonSwitch;
class ChannelGUI;
class FeatureGUI;
class DeviceGUI;
class MainSpectrumGUI;
class SDRGUI_API Workspace : public QDockWidget
{
    Q_OBJECT
public:
    Workspace(int index, QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    ~Workspace();

    int getIndex() const { return m_index; }
    void setIndex(int index);
    void resetAvailableFeatures() { m_featureAddDialog.resetFeatureNames(); }
    void addAvailableFeatures(const QStringList& featureNames) { m_featureAddDialog.addFeatureNames(featureNames); }
    void addToMdiArea(QMdiSubWindow *sub);
    void removeFromMdiArea(QMdiSubWindow *sub);
    int getNumberOfSubWindows() const;
    QByteArray saveMdiGeometry();
    void restoreMdiGeometry(const QByteArray& blob);
    bool getAutoStackOption() const;
    void setAutoStackOption(bool autoStack);
    bool getTabSubWindowsOption() const;
    void setTabSubWindowsOption(bool tab);
    QList<QMdiSubWindow *> getSubWindowList() const;
    void orderByIndex(QList<ChannelGUI *> &list);
    void orderByIndex(QList<FeatureGUI *> &list);
    void orderByIndex(QList<DeviceGUI *> &list);
    void orderByIndex(QList<MainSpectrumGUI *> &list);
    void adjustSubWindowsAfterRestore();
    void updateStartStopButton(bool checked);
    QToolButton *getMenuButton() const { return m_menuButton; }

private:
    int m_index;
    QToolButton *m_menuButton;
    QPushButton *m_configurationPresetsButton;
    ButtonSwitch *m_startStopButton;
    QFrame *m_vline1;
    QPushButton *m_addRxDeviceButton;
    QPushButton *m_addTxDeviceButton;
    QPushButton *m_addMIMODeviceButton;
    QFrame *m_vline2;
    QPushButton *m_addFeatureButton;
    QPushButton *m_featurePresetsButton;
    QFrame *m_vline3;
    QPushButton *m_cascadeSubWindows;
    QPushButton *m_tileSubWindows;
    QPushButton *m_stackVerticalSubWindows;
    QPushButton *m_stackSubWindows;
    ButtonSwitch *m_tabSubWindows;
    QWidget *m_titleBar;
    QHBoxLayout *m_titleBarLayout;
    QLabel *m_titleLabel;
    QPushButton *m_normalButton;
    QPushButton *m_closeButton;
    FeatureAddDialog m_featureAddDialog;
    QMdiArea *m_mdi;
    bool m_stacking;                // Set when stackSubWindows() is running
    bool m_autoStack;               // Automatically stack
    int m_userChannelMinWidth;      // Minimum width of channels column for stackSubWindows(), set by user resizing a channel window
    int m_autoStackChannelMinWidth; // Width of channel requested by stackSubWindows()

    void unmaximizeSubWindows();

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void addRxDeviceClicked();
    void addTxDeviceClicked();
    void addMIMODeviceClicked();
    void addFeatureDialog();
    void featurePresetsDialog();
    void configurationPresetsDialog();
    void cascadeSubWindows();
    void tileSubWindows();
    void stackVerticalSubWindows();
    void stackSubWindows();
    void autoStackSubWindows(const QPoint&);
    void tabSubWindows();
    void startStopClicked(bool checked = false);
    void addFeatureEmitted(int featureIndex);
    void toggleFloating();
    void deviceStateChanged(int, DeviceAPI *deviceAPI);
    void subWindowActivated(QMdiSubWindow *window);
public slots:
    void layoutSubWindows();

signals:
    void addRxDevice(Workspace *inWorkspace, int deviceIndex);
    void addTxDevice(Workspace *inWorkspace, int deviceIndex);
    void addMIMODevice(Workspace *inWorkspace, int deviceIndex);
    void addFeature(Workspace*, int);
    void featurePresetsDialogRequested(QPoint, Workspace*);
    void configurationPresetsDialogRequested();
    void startAllDevices(Workspace *inWorkspace);
    void stopAllDevices(Workspace *inWorkspace);
};


#endif // SDRGUI_GUI_WORKSPACE_H_
