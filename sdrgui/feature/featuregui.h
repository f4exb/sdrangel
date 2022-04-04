///////////////////////////////////////////////////////////////////////////////////
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

#ifndef SDRGUI_FEATURE_FEATUREGUI_H_
#define SDRGUI_FEATURE_FEATUREGUI_H_

#include <QMdiSubWindow>

#include "gui/rollupcontents.h"
#include "export.h"

class QCloseEvent;
class MessageQueue;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;
class QSizeGrip;

class SDRGUI_API FeatureGUI : public QMdiSubWindow
{
    Q_OBJECT
public:
    enum ContextMenuType
    {
        ContextMenuNone,
        ContextMenuChannelSettings,
        ContextMenuStreamSettings
    };

	FeatureGUI(QWidget *parent = nullptr);
	virtual ~FeatureGUI();
	virtual void destroy() = 0;

	virtual void resetToDefaults() = 0;
	virtual QByteArray serialize() const = 0;
	virtual bool deserialize(const QByteArray& data) = 0;

	virtual MessageQueue* getInputMessageQueue() = 0;

    RollupContents *getRollupContents() { return &m_rollupContents; }
    void setTitleColor(const QColor&) {} // not implemented for a feature
    void setTitle(const QString& title);
    void setIndex(int index);
    void setWorkspaceIndex(int index) { m_workspaceIndex = index; }
    int getWorkspaceIndex() const { return m_workspaceIndex; }

protected:
    void closeEvent(QCloseEvent *event);
    void mousePressEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void resetContextMenuType() { m_contextMenuType = ContextMenuNone; }

    int m_featureIndex;
    int m_workspaceIndex;
    QString m_helpURL;
    RollupContents m_rollupContents;
    ContextMenuType m_contextMenuType;

protected slots:
    void shrinkWindow();

private:
    bool isOnMovingPad();

    QLabel *m_indexLabel;
    QPushButton *m_settingsButton;
    QLabel *m_titleLabel;
    QPushButton *m_helpButton;
    QPushButton *m_moveButton;
    QPushButton *m_shrinkButton;
    QPushButton *m_closeButton;
    QLabel *m_statusLabel;
    QVBoxLayout *m_layouts;
    QHBoxLayout *m_topLayout;
    QHBoxLayout *m_centerLayout;
    QHBoxLayout *m_bottomLayout;
    QSizeGrip *m_sizeGripTopRight;
    QSizeGrip *m_sizeGripBottomRight;
    bool m_drag;
    QPoint m_DragPosition;

private slots:
    void activateSettingsDialog();
    void showHelp();
    void openMoveToWorkspaceDialog();

signals:
    void closing();
    void moveToWorkspace(int workspaceIndex);
    void forceShrink();
};

#endif // SDRGUI_FEATURE_FEATUREGUI_H_
