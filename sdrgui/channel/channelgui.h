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

#ifndef SDRGUI_CHANNEL_CHANNELGUI_H_
#define SDRGUI_CHANNEL_CHANNELGUI_H_

#include <QMdiSubWindow>
#include <QMap>

#include "gui/qtcompatibility.h"
#include "gui/framelesswindowresizer.h"
#include "settings/serializableinterface.h"
#include "export.h"

class QCloseEvent;
class MessageQueue;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;
class QSizeGrip;
class RollupContents;
class ChannelMarker;

class SDRGUI_API ChannelGUI : public QMdiSubWindow, public SerializableInterface
{
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
        ContextMenuChannelSettings
    };

	ChannelGUI(QWidget *parent = nullptr);
	virtual ~ChannelGUI();
	virtual void destroy() = 0;

	virtual void resetToDefaults() = 0;
    // Data saved in the derived settings
    virtual void setWorkspaceIndex(int index)= 0;
    virtual int getWorkspaceIndex() const = 0;
    virtual void setGeometryBytes(const QByteArray& blob) = 0;
    virtual QByteArray getGeometryBytes() const = 0;
    virtual QString getTitle() const = 0;
    virtual QColor getTitleColor() const  = 0;
    virtual void zetHidden(bool hidden) = 0;
    virtual bool getHidden() const = 0;
    virtual ChannelMarker& getChannelMarker() = 0;
    virtual int getStreamIndex() const = 0;
    virtual void setStreamIndex(int streamIndex) = 0;

	virtual MessageQueue* getInputMessageQueue() = 0;

    RollupContents *getRollupContents() { return m_rollupContents; }
    void sizeToContents();
    void setTitle(const QString& title);
    void setTitleColor(const QColor& c);
    void setDeviceType(DeviceType type);
    void setDisplayedame(const QString& name);
    DeviceType getDeviceType() const { return m_deviceType; }
    void setIndexToolTip(const QString& tooltip);
    void setIndex(int index);
    int getIndex() const { return m_channelIndex; }
    void setDeviceSetIndex(int index);
    int getDeviceSetIndex() const { return m_deviceSetIndex; }
    void setStatusFrequency(qint64 frequency);
    void setStatusText(const QString& text);

protected:
    void closeEvent(QCloseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void resetContextMenuType() { m_contextMenuType = ContextMenuNone; }
    void updateIndexLabel();
    int getAdditionalHeight() const { return 22 + 22; }  // height of top and bottom bars
    void setHighlighted(bool highlighted);
    int gripSize() { return m_resizer.m_gripSize; } // size in pixels of resize grip around the window

    DeviceType m_deviceType;
    int m_deviceSetIndex;
    int m_channelIndex;
    QString m_helpURL;
    RollupContents* m_rollupContents;
    ContextMenuType m_contextMenuType;
    QString m_displayedName;

protected slots:
    void shrinkWindow();
    void maximizeWindow();

private:
    bool isOnMovingPad();
    QString getDeviceTypeTag();
    static QColor getTitleColor(const QColor& backgroundColor);

    QLabel *m_indexLabel;
    QPushButton *m_settingsButton;
    QLabel *m_titleLabel;
    QPushButton *m_helpButton;
    QPushButton *m_moveButton;
    QPushButton *m_shrinkButton;
    QPushButton *m_maximizeButton;
    QPushButton *m_hideButton;
    QPushButton *m_closeButton;
    QPushButton *m_duplicateButton;
    QPushButton *m_moveToDeviceButton;
    QLabel *m_statusFrequency;
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
    void duplicateChannel();
    void openMoveToDeviceSetDialog();

signals:
    void closing();
    void moveToWorkspace(int workspaceIndex);
    void forceShrink();
    void duplicateChannelEmitted();
    void moveToDeviceSet(int deviceSetIndex);
};

#endif // SDRGUI_CHANNEL_CHANNELGUI_H_
