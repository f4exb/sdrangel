///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef SDRGUI_GUI_WORKSPACEWINDOW_H_
#define SDRGUI_GUI_WORKSPACEWINDOW_H_

#include <QMdiSubWindow>

#include "export.h"

// A window in a workspace: the device, spectrum, channel and feature windows. What is common
// to them is here, which is mostly telling MainCore where the window is and whether it is
// hidden, so that the Web API, which has no access to the GUI, can answer for it.
class SDRGUI_API WorkspaceWindow : public QMdiSubWindow
{
    Q_OBJECT
public:
    explicit WorkspaceWindow(QWidget *parent = nullptr);
    virtual ~WorkspaceWindow();

    virtual int getWorkspaceIndex() const = 0;
    virtual void setWorkspaceIndex(int index) = 0;
    virtual QString getTitle() const = 0;

    //!< The object the window belongs to (DeviceAPI, SpectrumVis, ChannelAPI or Feature),
    //!< which is the key its state is published under.
    void setWindowOwner(const void *owner);
    //!< Tells MainCore which workspace this window is in, whether it is hidden and what its
    //!< title is. Called on every show and hide, which covers a move between workspaces;
    //!< call it after anything else that changes one of them, as a retitle or a renumbering
    void publishWindowState();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    int workspaceIndexFromParent() const;

    const void *m_windowOwner;
};

#endif // SDRGUI_GUI_WORKSPACEWINDOW_H_
