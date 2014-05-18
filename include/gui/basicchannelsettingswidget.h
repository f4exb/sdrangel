#ifndef INCLUDE_BASICCHANNELSETTINGSWIDGET_H
#define INCLUDE_BASICCHANNELSETTINGSWIDGET_H

#include <QWidget>
#include "util/export.h"

namespace Ui {
	class BasicChannelSettingsWidget;
}

class ChannelMarker;

class SDRANGELOVE_API BasicChannelSettingsWidget : public QWidget {
	Q_OBJECT

public:
	explicit BasicChannelSettingsWidget(ChannelMarker* marker, QWidget* parent = NULL);
	~BasicChannelSettingsWidget();

private slots:
	void on_title_textChanged(const QString& text);
	void on_colorBtn_clicked();
	void on_red_valueChanged(int value);
	void on_green_valueChanged(int value);
	void on_blue_valueChanged(int value);

private:
	Ui::BasicChannelSettingsWidget* ui;
	ChannelMarker* m_channelMarker;

	void paintColor();
};

#endif // INCLUDE_BASICCHANNELSETTINGSWIDGET_H
