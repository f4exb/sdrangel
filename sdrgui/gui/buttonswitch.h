#ifndef INCLUDE_BUTTONSWITCH_H
#define INCLUDE_BUTTONSWITCH_H

#include <QToolButton>

#include "export.h"

class SDRGUI_API ButtonSwitch : public QToolButton {
	Q_OBJECT

public:
	ButtonSwitch(QWidget* parent = NULL);
	void doToggle(bool checked);
    void setColor(QColor color);
    void resetColor();
};

#endif // INCLUDE_BUTTONSWITCH_H
