#ifndef INCLUDE_CHANNELWINDOW_H
#define INCLUDE_CHANNELWINDOW_H

#include <QScrollArea>

class QBoxLayout;
class QSpacerItem;
class RollupWidget;

class ChannelWindow : public QScrollArea {
	Q_OBJECT

public:
	ChannelWindow(QWidget* parent = NULL);

	void addRollupWidget(QWidget* rollupWidget);

protected:
	QWidget* m_container;
	QBoxLayout* m_layout;

	void resizeEvent(QResizeEvent* event);
};

#endif // INCLUDE_CHANNELWINDOW_H
