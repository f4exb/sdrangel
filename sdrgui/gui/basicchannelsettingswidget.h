#ifndef INCLUDE_BASICCHANNELSETTINGSWIDGET_H
#define INCLUDE_BASICCHANNELSETTINGSWIDGET_H

#include <QWidget>
#include "util/export.h"

namespace Ui {
	class BasicChannelSettingsWidget;
}

class ChannelMarker;

class SDRANGEL_API BasicChannelSettingsWidget : public QWidget {
	Q_OBJECT

public:
	explicit BasicChannelSettingsWidget(ChannelMarker* marker, QWidget* parent = NULL);
	~BasicChannelSettingsWidget();
	void setUDPDialogVisible(bool visible);
	bool getHasChanged() const { return m_hasChanged; }

private slots:
	void on_title_textChanged(const QString& text);
	void on_colorBtn_clicked();
	void on_address_textEdited(const QString& arg1);
	void on_port_textEdited(const QString& arg1);

private:
	Ui::BasicChannelSettingsWidget* ui;
	ChannelMarker* m_channelMarker;
	bool m_hasChanged;

	void paintColor();
};

#endif // INCLUDE_BASICCHANNELSETTINGSWIDGET_H
