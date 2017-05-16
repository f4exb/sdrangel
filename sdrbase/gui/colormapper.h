/*
 * colormap.h
 *
 *  Created on: Jul 19, 2015
 *      Author: f4exb
 */
#ifndef INCLUDE_GPL_GUI_COLORMAPPER_H_
#define INCLUDE_GPL_GUI_COLORMAPPER_H_

#include <QColor>
#include <vector>
#include <utility>
#include "util/export.h"

class SDRANGEL_API ColorMapper
{
public:
	enum Theme {
		Normal,
		Gold,
		ReverseGold,
		ReverseGreenEmerald,
		ReverseGreen,
		ReverseGreenApple,
		ReverseGreenYellow,
        GrayGreenYellow,
        GrayGold,
        GrayYellow
	};

	typedef std::vector<std::pair<float, QColor> > colormap;

	ColorMapper(Theme theme = Normal);
	~ColorMapper();

	const colormap& getDialBackgroundColorMap() const { return m_dialBackgroundcolorMap; };
	const QColor& getForegroundColor() const { return m_foregroundColor; };
	const QColor& getSecondaryForegroundColor() const { return m_secondaryForegroundColor; };
	const QColor& getHighlightColor() const { return m_highlightColor; };
	const QColor& getBoundaryColor() const { return m_boundaryColor; };
	const QColor& getBoundaryAlphaColor() const { return m_boundaryAlphaColor; };
    const QColor& getLightBorderColor() const { return m_lightBorderColor; };
    const QColor& getDarkBorderColor() const { return m_darkBorderColor; };

private:
	Theme m_theme;
	std::vector<std::pair<float, QColor> > m_dialBackgroundcolorMap;
	QColor m_foregroundColor;
	QColor m_secondaryForegroundColor;
	QColor m_highlightColor;
	QColor m_boundaryColor;
	QColor m_boundaryAlphaColor;
	QColor m_lightBorderColor;
    QColor m_darkBorderColor;
};

#endif /* INCLUDE_GPL_GUI_COLORMAPPER_H_ */
