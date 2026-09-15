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

#include <QShowEvent>
#include <QHideEvent>

#include "maincore.h"
#include "workspace.h"
#include "workspacewindow.h"

WorkspaceWindow::WorkspaceWindow(QWidget *parent) :
    QMdiSubWindow(parent),
    m_windowOwner(nullptr)
{
}

WorkspaceWindow::~WorkspaceWindow()
{
    if (m_windowOwner) {
        MainCore::instance()->removeWindowState(m_windowOwner);
    }
}

void WorkspaceWindow::setWindowOwner(const void *owner)
{
    m_windowOwner = owner;
    publishWindowState();
}

void WorkspaceWindow::publishWindowState()
{
    if (!m_windowOwner) {
        return;
    }

    MainCore::WindowState state;
    state.m_workspaceIndex = workspaceIndexFromParent();
    state.m_hidden = isHidden();
    state.m_title = getTitle();
    MainCore::instance()->setWindowState(m_windowOwner, state);
}

// The workspace the window is actually in. Preferred to getWorkspaceIndex(), as channel
// plugins keep that in their settings, where a settings message from the channel, which
// was never told about a move, can overwrite it with the workspace the channel started in
int WorkspaceWindow::workspaceIndexFromParent() const
{
    for (const QWidget *widget = parentWidget(); widget; widget = widget->parentWidget())
    {
        const Workspace *workspace = qobject_cast<const Workspace*>(widget);

        if (workspace) {
            return workspace->getIndex();
        }
    }

    return getWorkspaceIndex();
}

// Spontaneous show and hide events come from the window system, as when the main window is
// minimised, and do not change whether the window itself is hidden
void WorkspaceWindow::showEvent(QShowEvent *event)
{
    QMdiSubWindow::showEvent(event);

    if (!event->spontaneous()) {
        publishWindowState();
    }
}

void WorkspaceWindow::hideEvent(QHideEvent *event)
{
    QMdiSubWindow::hideEvent(event);

    if (!event->spontaneous()) {
        publishWindowState();
    }
}
