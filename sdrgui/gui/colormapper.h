///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2018 Edouard Griffiths, F4EXB <f4exb06@gmail.com>              //
//                                                                                   //
// This program is free software; you can redistribute it and/or modify              //
// it under the terms of the GNU General Public License as published by              //
// the Free Software Foundation as version 3 of the License, or                      //
// (at your option) any later version.                                               //
//                                                                                   //
// This program is distributed in the hope that it will be useful,                   //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                    //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                      //
// GNU General Public License V3 for more details.                                   //
//                                                                                   //
// You should have received a copy of the GNU General Public License                 //
// along with this program. If not, see <http://www.gnu.org/licenses/>.              //
///////////////////////////////////////////////////////////////////////////////////////
#ifndef INCLUDE_GPL_GUI_COLORMAPPER_H_
#define INCLUDE_GPL_GUI_COLORMAPPER_H_

#include <QColor>
#include <vector>
#include <utility>
#include "export.h"

class SDRGUI_API ColorMapper
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
