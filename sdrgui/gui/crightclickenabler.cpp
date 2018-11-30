/*
 * crightclickenabler.cpp
 *
 *  Created on: Mar 26, 2018
 *      Author: f4exb
 */

#include "crightclickenabler.h"

CRightClickEnabler::CRightClickEnabler(QAbstractButton *button): QObject(button), _button(button) {
        button->installEventFilter(this);
};

