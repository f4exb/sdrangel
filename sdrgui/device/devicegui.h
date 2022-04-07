///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020-2022 Edouard Griffiths, F4EXB                              //
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

#ifndef INCLUDE_DEVICEGUI_H
#define INCLUDE_DEVICEGUI_H

#include <QMdiSubWindow>

#include <QtGlobal>
#include <QString>
#include <QByteArray>
#include <QWidget>

#include "export.h"

class QCloseEvent;
class Message;
class MessageQueue;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;
class QSizeGrip;
class SDRGUI_API DeviceGUI : public QMdiSubWindow {
    Q_OBJECT
public:
    enum DeviceType
    {
        DeviceRx,
        DeviceTx,
        DeviceMIMO
    };

    enum ContextMenuType
    {
        ContextMenuNone,
        ContextMenuChannelSettings,
        ContextMenuStreamSettings
    };

	DeviceGUI(QWidget *parent = nullptr);
	virtual ~DeviceGUI();
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

    QWidget *getContents() { return m_contents; }
    void setDeviceType(DeviceType type);
    DeviceType getDeviceType() const { return m_deviceType; }
    void setTitle(const QString& title);
    QString getTitle() const;
    void setToolTip(const QString& tooltip);
    void setIndex(int index);
    int getIndex() const { return m_deviceSetIndex; }
    void setCurrentDeviceIndex(int index) { m_currentDeviceIndex = index; } //!< index in plugins list

protected:
    void closeEvent(QCloseEvent *event);
    void mousePressEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void resetContextMenuType() { m_contextMenuType = ContextMenuNone; }

    DeviceType m_deviceType;
    int m_deviceSetIndex;
    QString m_helpURL;
    QWidget *m_contents;
    ContextMenuType m_contextMenuType;

protected slots:
    void shrinkWindow();

private:
    bool isOnMovingPad();
    QString getDeviceTypeColor();
    QString getDeviceTypeTag();

    QLabel *m_indexLabel;
    QPushButton *m_changeDeviceButton;
    QPushButton *m_reloadDeviceButton;
    QPushButton *m_addChannelsButton;
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
    int m_currentDeviceIndex; //!< Index in device plugins registrations

private slots:
    void openChangeDeviceDialog();
    void deviceReload();
    void showHelp();
    void openMoveToWorkspaceDialog();

signals:
    void forceClose();
    void closing();
    void moveToWorkspace(int workspaceIndex);
    void forceShrink();
    void deviceAdd(int deviceType, int deviceIndex);
    void deviceChange(int newDeviceIndex);
};

#endif // INCLUDE_DEVICEGUI_H
