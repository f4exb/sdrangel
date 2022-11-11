///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRGUI_MAINSPECTRUM_MAINSPECTRUMGUIGUI_H_
#define SDRGUI_MAINSPECTRUM_MAINSPECTRUMGUIGUI_H_

#include <QMdiSubWindow>
#include <QByteArray>

#include "util/messagequeue.h"
#include "gui/framelesswindowresizer.h"
#include "export.h"

class GLSpectrum;
class GLSpectrumGUI;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;
class QSizeGrip;

class SDRGUI_API MainSpectrumGUI : public QMdiSubWindow
{
    Q_OBJECT
public:
    enum DeviceType
    {
        DeviceRx,
        DeviceTx,
        DeviceMIMO
    };

	MainSpectrumGUI(GLSpectrum *spectrum, GLSpectrumGUI *spectrumGUI, QWidget *parent = nullptr);
	virtual ~MainSpectrumGUI();

    void setDeviceType(DeviceType type);
    DeviceType getDeviceType() const { return m_deviceType; }
    void setTitle(const QString& title);
    QString getTitle() const;
    void setToolTip(const QString& tooltip);
    void setIndex(int index);
    int getIndex() const { return m_deviceSetIndex; }
    void setWorkspaceIndex(int index);
    int getWorkspaceIndex() const { return m_workspaceIndex; }
    void setGeometryBytes(const QByteArray& blob) { m_geometryBytes = blob; }
    const QByteArray& getGeometryBytes() const { return m_geometryBytes; }

private:
    GLSpectrum *m_spectrum;
    GLSpectrumGUI *m_spectrumGUI;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    DeviceType m_deviceType;
    int m_deviceSetIndex;
    QString m_deviceTitle;
    QString m_deviceTooltip;
    QString m_helpURL;

    QLabel *m_indexLabel;
    QLabel *m_spacerLabel;
    QLabel *m_titleLabel;
    QPushButton *m_helpButton;
    QPushButton *m_moveButton;
    QPushButton *m_shrinkButton;
    QPushButton *m_maximizeButton;
    QPushButton *m_hideButton;
    QLabel *m_statusLabel;
    QVBoxLayout *m_layouts;
    QHBoxLayout *m_topLayout;
    QHBoxLayout *m_spectrumLayout;
    QHBoxLayout *m_spectrumGUILayout;
    QHBoxLayout *m_bottomLayout;
    QSizeGrip *m_sizeGripBottomRight;
    bool m_drag;
    QPoint m_DragPosition;
    FramelessWindowResizer m_resizer;
    static const int m_MinimumWidth = 380;
    static const int m_MinimumHeight = 200 + 20 + 10 + 6*22 + 5;

protected:
    void closeEvent(QCloseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    bool isOnMovingPad();
    QString getDeviceTypeColor();
    QString getDeviceTypeTag();

private slots:
    void showHelp();
    void openMoveToWorkspaceDialog();
    void shrinkWindow();
    void maximizeWindow();
    void onRequestCenterFrequency(qint64 frequency);

signals:
    void closing();
    void moveToWorkspace(int workspaceIndex);
    void forceShrink();
    void requestCenterFrequency(int deviceSetIndex, qint64 frequency); // an action from the user to move device center frequency
};


#endif // SDRGUI_MAINSPECTRUM_MAINSPECTRUMGUIGUI_H_
