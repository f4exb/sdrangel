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

#include "gui/channeladddialog.h"
#include "gui/framelesswindowresizer.h"
#include "export.h"

class QCloseEvent;
class Message;
class MessageQueue;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;
class QSizeGrip;
class DeviceUISet;

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
        ContextMenuDeviceSettings
    };

	DeviceGUI(QWidget *parent = nullptr);
	virtual ~DeviceGUI();
	virtual void destroy() = 0;

	virtual void resetToDefaults() = 0;
	virtual QByteArray serialize() const = 0;
	virtual bool deserialize(const QByteArray& data) = 0;
    void setWorkspaceIndex(int index);
    int getWorkspaceIndex() const { return m_workspaceIndex; }

	virtual MessageQueue* getInputMessageQueue() = 0;

    QWidget *getContents() { return m_contents; }
    void sizeToContents();
    void setDeviceType(DeviceType type);
    DeviceType getDeviceType() const { return m_deviceType; }
    void setTitle(const QString& title);
    QString getTitle() const;
    void setToolTip(const QString& tooltip);
    void setIndex(int index);
    int getIndex() const { return m_deviceSetIndex; }
    void setCurrentDeviceIndex(int index) { m_currentDeviceIndex = index; } //!< index in plugins list
    void setChannelNames(const QStringList& channelNames) { m_channelAddDialog.addChannelNames(channelNames); }

protected:
    void closeEvent(QCloseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void resetContextMenuType() { m_contextMenuType = ContextMenuNone; }
    int getAdditionalHeight() const { return 22 + 22; }

    DeviceUISet* m_deviceUISet;
    DeviceType m_deviceType;
    int m_deviceSetIndex;
    int m_workspaceIndex;
    QString m_helpURL;
    QWidget *m_contents;
    ContextMenuType m_contextMenuType;

protected slots:
    void shrinkWindow();
    void maximizeWindow();

private:
    bool isOnMovingPad();
    QString getDeviceTypeColor();
    QString getDeviceTypeTag();

    QLabel *m_indexLabel;
    QPushButton *m_settingsButton;
    QPushButton *m_changeDeviceButton;
    QPushButton *m_reloadDeviceButton;
    QPushButton *m_addChannelsButton;
    QPushButton *m_deviceSetPresetsButton;
    QLabel *m_titleLabel;
    QPushButton *m_helpButton;
    QPushButton *m_moveButton;
    QPushButton *m_shrinkButton;
    QPushButton *m_maximizeButton;
    QPushButton *m_closeButton;
    QPushButton *m_showSpectrumButton;
    QPushButton *m_showAllChannelsButton;
    QLabel *m_statusLabel;
    QVBoxLayout *m_layouts;
    QHBoxLayout *m_topLayout;
    QHBoxLayout *m_centerLayout;
    QHBoxLayout *m_bottomLayout;
    QSizeGrip *m_sizeGripBottomRight;
    bool m_drag;
    QPoint m_DragPosition;
    int m_currentDeviceIndex; //!< Index in device plugins registrations
    ChannelAddDialog m_channelAddDialog;
    FramelessWindowResizer m_resizer;

private slots:
    void activateSettingsDialog();
    void openChangeDeviceDialog();
    void openAddChannelsDialog();
    void deviceReload();
    void showHelp();
    void openMoveToWorkspaceDialog();
    void showSpectrumHandler();
    void showAllChannelsHandler();
    void deviceSetPresetsDialog();

signals:
    void closing();
    void moveToWorkspace(int workspaceIndex);
    void forceShrink();
    void deviceAdd(int deviceType, int deviceIndex);
    void deviceChange(int newDeviceIndex);
    void showSpectrum(int deviceSetIndex);
    void showAllChannels(int deviceSetIndex);
    void addChannelEmitted(int channelPluginIndex);
    void deviceSetPresetsDialogRequested(QPoint, DeviceGUI*);
};

#endif // INCLUDE_DEVICEGUI_H
