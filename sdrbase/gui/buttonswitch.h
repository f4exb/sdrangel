#ifndef INCLUDE_BUTTONSWITCH_H
#define INCLUDE_BUTTONSWITCH_H

#include <QToolButton>

class ButtonSwitch : public QToolButton {
	Q_OBJECT

public:
	ButtonSwitch(QWidget* parent = NULL);
	void doToggle(bool checked);

private slots:
	void onToggled(bool checked);

private:
	QPalette m_originalPalette;
};

#endif // INCLUDE_BUTTONSWITCH_H
