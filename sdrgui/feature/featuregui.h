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
#include <QMap>

#include "gui/framelesswindowresizer.h"
#include "gui/rollupcontents.h"
#include "export.h"

class QCloseEvent;
class MessageQueue;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;
class QSizeGrip;
class Feature;

class SDRGUI_API FeatureGUI : public QMdiSubWindow
{
    Q_OBJECT
public:
    enum ContextMenuType
    {
        ContextMenuNone,
        ContextMenuChannelSettings
    };

	FeatureGUI(QWidget *parent = nullptr);
	virtual ~FeatureGUI();
	virtual void destroy() = 0;

	virtual void resetToDefaults() = 0;
	virtual QByteArray serialize() const = 0;
	virtual bool deserialize(const QByteArray& data) = 0;
    // Data saved in the derived settings
    virtual void setWorkspaceIndex(int index)= 0;
    virtual int getWorkspaceIndex() const = 0;
    virtual void setGeometryBytes(const QByteArray& blob) = 0;
    virtual QByteArray getGeometryBytes() const = 0;

	virtual MessageQueue* getInputMessageQueue() = 0;

    RollupContents *getRollupContents() { return &m_rollupContents; }
    void sizeToContents();
    void setTitleColor(const QColor&) {} // not implemented for a feature
    void setTitle(const QString& title);
    void setIndex(int index);
    int getIndex() const { return m_featureIndex; }
    void setDisplayedame(const QString& name);

protected:
    void closeEvent(QCloseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void resetContextMenuType() { m_contextMenuType = ContextMenuNone; }
    int getAdditionalHeight() const { return 22 + 22; } // height of top and bottom bars
    int gripSize() { return m_resizer.m_gripSize; } // size in pixels of resize grip around the window

    Feature *m_feature;
    int m_featureIndex;
    QString m_helpURL;
    RollupContents m_rollupContents;
    ContextMenuType m_contextMenuType;
    QString m_displayedName;

protected slots:
    void shrinkWindow();
    void maximizeWindow();

private:
    bool isOnMovingPad();

    QLabel *m_indexLabel;
    QPushButton *m_settingsButton;
    QLabel *m_titleLabel;
    QPushButton *m_helpButton;
    QPushButton *m_moveButton;
    QPushButton *m_shrinkButton;
    QPushButton *m_maximizeButton;
    QPushButton *m_closeButton;
    QLabel *m_statusLabel;
    QVBoxLayout *m_layouts;
    QHBoxLayout *m_topLayout;
    QHBoxLayout *m_centerLayout;
    QHBoxLayout *m_bottomLayout;
    QSizeGrip *m_sizeGripBottomRight;
    bool m_drag;
    QPoint m_DragPosition;
    QMap<QWidget*, int> m_heightsMap;
    FramelessWindowResizer m_resizer;
    bool m_disableResize;
    QMdiArea *m_mdi;                    // Saved pointer to MDI when in full screen mode

private slots:
    void activateSettingsDialog();
    void showHelp();
    void openMoveToWorkspaceDialog();
    void onWidgetRolled(QWidget *widget, bool show);

signals:
    void closing();
    void moveToWorkspace(int workspaceIndex);
    void forceShrink();
};

#endif // SDRGUI_FEATURE_FEATUREGUI_H_
