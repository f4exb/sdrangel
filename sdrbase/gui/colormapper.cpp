/*
 * colormap.cpp
 *
 *  Created on: Jul 19, 2015
 *      Author: f4exb
 */

#include <gui/colormapper.h>

ColorMapper::ColorMapper(Theme theme) :
	m_theme(theme)
{
	switch (m_theme)
	{
	case ReverseGold:
		/*
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.0, QColor(0x97, 0x54, 0x00)));
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.1, QColor(0x5e, 0x34, 0x00)));
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.2, QColor(0x2e, 0x19, 0x00)));
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.5, QColor(0x00, 0x00, 0x00)));
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.8, QColor(0x0f, 0x08, 0x00)));
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.9, QColor(0x40, 0x23, 0x00)));
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(1.0, QColor(0x97, 0x54, 0x00)));
		*/
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.0, QColor(0x97, 0x54, 0x00)));
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.1, QColor(0x5e, 0x34, 0x00)));
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.2, QColor(0x5e, 0x34, 0x00)));
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.5, QColor(0x00, 0x00, 0x00)));
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.8, QColor(0x5e, 0x34, 0x00)));
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.9, QColor(0x40, 0x23, 0x00)));
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(1.0, QColor(0x97, 0x54, 0x00)));
		m_foregroundColor = QColor(0xff, 0x8b, 0x00);
		m_secondaryForegroundColor = QColor(0xff, 0xc5, 0x80);
		m_highlightColor = QColor(0xbf, 0x69, 0x00, 0x80);
		break;
	case Normal:
	default:
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.0, QColor(0x40, 0x40, 0x40)));
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.1, QColor(0xc0, 0xc0, 0xc0)));
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.2, QColor(0xf0, 0xf0, 0xf0)));
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.5, QColor(0xff, 0xff, 0xff)));
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.8, QColor(0xd0, 0xd0, 0xd0)));
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.9, QColor(0xa0, 0xa0, 0xa0)));
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(1.0, QColor(0x40, 0x40, 0x40)));
		m_foregroundColor = QColor(0x00, 0x00, 0x00);
		m_secondaryForegroundColor = QColor(0x10, 0x10, 0x10);
		m_highlightColor = QColor(0xff, 0x00, 0x00, 0x20);
	}
}

ColorMapper::~ColorMapper()
{
}
