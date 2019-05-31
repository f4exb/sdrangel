#ifndef INCLUDE_ROLLUPWIDGET_H
#define INCLUDE_ROLLUPWIDGET_H

#include <QWidget>
#include "export.h"

class SDRGUI_API RollupWidget : public QWidget {
	Q_OBJECT

public:
	RollupWidget(QWidget* parent = NULL);
	void setTitleColor(const QColor& c);
	void setHighlighted(bool highlighted);

signals:
	void widgetRolled(QWidget* widget, bool rollDown);

protected:
	enum {
		VersionMarker = 0xff
	};

    enum ContextMenuType
    {
        ContextMenuNone,
        ContextMenuChannelSettings,
        ContextMenuStreamSettings
    };

	QColor m_titleColor;
	QColor m_titleTextColor;
	bool m_highlighted;
    ContextMenuType m_contextMenuType;
    QString m_streamIndicator;

	int arrangeRollups();

	QByteArray saveState(int version = 0) const;
    bool restoreState(const QByteArray& state, int version = 0);

	void paintEvent(QPaintEvent*);
	int paintRollup(QWidget* rollup, int pos, QPainter* p, bool last, const QColor& frame);

	void resizeEvent(QResizeEvent* size);
	void mousePressEvent(QMouseEvent* event);

	bool event(QEvent* event);
	bool eventFilter(QObject* object, QEvent* event);

    void resetContextMenuType() { m_contextMenuType = ContextMenuNone; }
	void setStreamIndicator(const QString& indicator);
};

#endif // INCLUDE_ROLLUPWIDGET_H
