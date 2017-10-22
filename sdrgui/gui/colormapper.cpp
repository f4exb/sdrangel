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
	case Gold:
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.0, QColor(0x40, 0x36, 0x2b)));
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.1, QColor(0xbf, 0xa3, 0x80)));
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.2, QColor(0xf0, 0xcc, 0xa1)));
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.5, QColor(0xff, 0xd9, 0xab)));
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.8, QColor(0xd1, 0xb2, 0x8c)));
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.9, QColor(0xa1, 0x89, 0x6c)));
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(1.0, QColor(0x40, 0x36, 0x2b)));
		m_foregroundColor = QColor(0x00, 0x00, 0x00);
		m_secondaryForegroundColor = QColor(0x0f, 0x0d, 0x0a);
		m_highlightColor = QColor(0xff, 0xd9, 0xab, 0x80);
		m_boundaryColor = QColor(0x21, 0x1c, 0x16);
		m_boundaryAlphaColor = QColor(0x00, 0x00, 0x00, 0x20);
		m_lightBorderColor = QColor(80, 80, 80);
        m_darkBorderColor = QColor(0, 0, 0);
		break;
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
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.0, QColor(0x97, 0x54, 0x00))); // 59% brightness
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.1, QColor(0x5e, 0x34, 0x00))); // 37% brightness
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.2, QColor(0x2e, 0x19, 0x00))); // 18% brightness
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.5, QColor(0x00, 0x00, 0x00))); //  0% brightness
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.8, QColor(0x40, 0x23, 0x00))); // 25% brightness
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.9, QColor(0x5e, 0x34, 0x00))); // 37% brightness
		m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(1.0, QColor(0x97, 0x54, 0x00))); // 59% brightness
		m_foregroundColor = QColor(0xff, 0x8b, 0x00);           // Base 33 degrees hue
		m_secondaryForegroundColor = QColor(0xff, 0xc5, 0x80);  // 50% saturation
		m_highlightColor = QColor(0xbf, 0x69, 0x00, 0x80);      // 75% saturation
		m_boundaryColor = QColor(0x66, 0x38, 0x20);             // 69% saturation 40% brightness
		m_boundaryAlphaColor = QColor(0xff, 0x8b, 0x00, 0x20);  // Base with alpha
        m_lightBorderColor = QColor(80, 80, 80);
        m_darkBorderColor = QColor(0, 0, 0);
		break;
    case ReverseGreenEmerald:
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.0, QColor(0x00, 0x96, 0x26))); // 59% brightness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.1, QColor(0x00, 0x5e, 0x10))); // 37% brightness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.2, QColor(0x00, 0x2e, 0x0b))); // 18% brightness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.5, QColor(0x00, 0x00, 0x00))); //  0% brightness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.8, QColor(0x00, 0x40, 0x10))); // 25% brightness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.9, QColor(0x00, 0x5e, 0x10))); // 37% brightness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(1.0, QColor(0x00, 0x96, 0x26))); // 59% brightness
        m_foregroundColor = QColor(0x00, 0xff, 0x40);           // Base 135 degrees hue
        m_secondaryForegroundColor = QColor(0x80, 0xff, 0x9f);  // 50% saturation
        m_highlightColor = QColor(0x40, 0xff, 0x70, 0x80);      // 75% saturation
        m_boundaryColor = QColor(0x20, 0x66, 0x31);             // 69% saturation 40% brightness
        m_boundaryAlphaColor = QColor(0x80, 0xff, 0x40, 0x20);  // Base with alpha
        m_lightBorderColor = QColor(80, 80, 80);
        m_darkBorderColor = QColor(0, 0, 0);
        break;
    case ReverseGreen:
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.0, QColor(0x00, 0x96, 0x00))); // 59% brightness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.1, QColor(0x00, 0x5e, 0x00))); // 37% brightness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.2, QColor(0x00, 0x2e, 0x00))); // 18% brightness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.5, QColor(0x00, 0x00, 0x00))); //  0% brightness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.8, QColor(0x00, 0x40, 0x00))); // 25% brightness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.9, QColor(0x00, 0x5e, 0x00))); // 37% brightness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(1.0, QColor(0x00, 0x96, 0x00))); // 59% brightness
        m_foregroundColor = QColor(0x00, 0xff, 0x00);           // Base 120 degrees hue (pure green)
        m_secondaryForegroundColor = QColor(0x80, 0xff, 0x80);  // 50% saturation
        m_highlightColor = QColor(0x40, 0xff, 0x40, 0x80);      // 75% saturation
        m_boundaryColor = QColor(0x20, 0x66, 0x20);             // 69% saturation 40% brightness
        m_boundaryAlphaColor = QColor(0x00, 0xff, 0x00, 0x20);  // Base with alpha
        m_lightBorderColor = QColor(80, 80, 80);
        m_darkBorderColor = QColor(0, 0, 0);
        break;
    case ReverseGreenApple:
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.0, QColor(0x26, 0x96, 0x00))); // 59% brightness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.1, QColor(0x10, 0x5e, 0x00))); // 37% brightness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.2, QColor(0x0b, 0x2e, 0x00))); // 18% brightness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.5, QColor(0x00, 0x00, 0x00))); //  0% brightness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.8, QColor(0x10, 0x40, 0x00))); // 25% brightness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.9, QColor(0x10, 0x5e, 0x00))); // 37% brightness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(1.0, QColor(0x26, 0x96, 0x00))); // 59% brightness
        m_foregroundColor = QColor(0x40, 0xff, 0x00);           // Base 105 degrees hue (opposite towards red of emerald w.r. to pure green)
        m_secondaryForegroundColor = QColor(0x9f, 0xff, 0x80);  // 50% saturation
        m_highlightColor = QColor(0x70, 0xff, 0x40, 0x80);      // 75% saturation
        m_boundaryColor = QColor(0x31, 0x66, 0x20);             // 69% saturation 40% brightness
        m_boundaryAlphaColor = QColor(0x40, 0xff, 0x00, 0x20);  // Base with alpha
        m_lightBorderColor = QColor(80, 80, 80);
        m_darkBorderColor = QColor(0, 0, 0);
        break;
    case ReverseGreenYellow:
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.0, QColor(0x53, 0x96, 0x00))); // 59% brightness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.1, QColor(0x34, 0x5e, 0x00))); // 37% brightness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.2, QColor(0x19, 0x2e, 0x00))); // 18% brightness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.5, QColor(0x00, 0x00, 0x00))); //  0% brightness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.8, QColor(0x23, 0x40, 0x00))); // 25% brightness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.9, QColor(0x34, 0x5e, 0x00))); // 37% brightness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(1.0, QColor(0x53, 0x96, 0x00))); // 59% brightness
        m_foregroundColor = QColor(0x8c, 0xff, 0x00);           // Base 87 (=120-33) degrees hue
        m_secondaryForegroundColor = QColor(0xc6, 0xff, 0x80);  // 50% saturation
        m_highlightColor = QColor(0xa9, 0xff, 0x40, 0x80);      // 75% saturation
        m_boundaryColor = QColor(0x46, 0x66, 0x20);             // 69% saturation 40% brightness
        m_boundaryAlphaColor = QColor(0x8c, 0xff, 0x00, 0x20);  // Base with alpha
        m_lightBorderColor = QColor(80, 80, 80);
        m_darkBorderColor = QColor(0, 0, 0);
        break;
    case GrayGreenYellow: // Base 87 (=120-33) degrees hue 140,255,0 - background 67, 67, 67
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.0, QColor(27, 27, 27))); // 59% darkness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.1, QColor(42, 42, 42))); // 37% darkness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.2, QColor(55, 55, 55))); // 18% darkness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.5, QColor(67, 67, 67))); //  0% darkness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.8, QColor(50, 50, 50))); // 25% darkness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.9, QColor(42, 42, 42))); // 37% darkness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(1.0, QColor(27, 27, 27))); // 59% darkness
        m_foregroundColor = QColor(99, 128, 64);                // 50% saturation 50% brightbess - shadow face color
        m_secondaryForegroundColor = QColor(198, 255, 128);     // 50% saturation - front face color
        m_highlightColor = QColor(169, 255, 64, 80);            // 75% saturation - digit highlight overlay
        m_boundaryColor = QColor(27, 27, 27);                   // background 60% darkness - inter wheel gap
        m_boundaryAlphaColor = QColor(140, 255, 0, 32);         // Base with alpha - wheel edges
        m_lightBorderColor = QColor(80, 80, 80);
        m_darkBorderColor = QColor(0, 0, 0);
        break;
    case GrayGold: // Base 33 degrees hue 255,139,0 - background 67, 67, 67
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.0, QColor(27, 27, 27))); // 59% darkness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.1, QColor(42, 42, 42))); // 37% darkness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.2, QColor(55, 55, 55))); // 18% darkness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.5, QColor(67, 67, 67))); //  0% darkness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.8, QColor(50, 50, 50))); // 25% darkness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.9, QColor(42, 42, 42))); // 37% darkness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(1.0, QColor(27, 27, 27))); // 59% darkness
        m_foregroundColor = QColor(128, 98, 64);                // 50% saturation 50% brightbess - shadow face color
        m_secondaryForegroundColor = QColor(255, 197, 128);     // 50% saturation - front face color
        m_highlightColor = QColor(255, 169, 64, 80);            // 75% saturation - digit highlight overlay
        m_boundaryColor = QColor(27, 27, 27);                   // background 60% darkness - inter wheel gap
        m_boundaryAlphaColor = QColor(255, 139, 0, 32);         // Base with alpha - wheel edges
        m_lightBorderColor = QColor(80, 80, 80);
        m_darkBorderColor = QColor(0, 0, 0);
        break;
    case GrayYellow: // Base 50 degrees hue 255,213,0 - background 67, 67, 67
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.0, QColor(27, 27, 27))); // 59% darkness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.1, QColor(42, 42, 42))); // 37% darkness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.2, QColor(55, 55, 55))); // 18% darkness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.5, QColor(67, 67, 67))); //  0% darkness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.8, QColor(50, 50, 50))); // 25% darkness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(0.9, QColor(42, 42, 42))); // 37% darkness
        m_dialBackgroundcolorMap.push_back(std::pair<float, QColor>(1.0, QColor(27, 27, 27))); // 59% darkness
        m_foregroundColor = QColor(128, 117, 64);               // 50% saturation 50% brightbess - shadow face color
        m_secondaryForegroundColor = QColor(255, 234, 128);     // 50% saturation - front face color
        m_highlightColor = QColor(255, 233, 64, 80);            // 75% saturation - digit highlight overlay
        m_boundaryColor = QColor(27, 27, 27);                   // background 60% darkness - inter wheel gap
        m_boundaryAlphaColor = QColor(255, 213, 0, 32);         // Base with alpha - wheel edges
        m_lightBorderColor = QColor(80, 80, 80);
        m_darkBorderColor = QColor(0, 0, 0);
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
		m_boundaryColor = QColor(0x20, 0x20, 0x20);
		m_boundaryAlphaColor = QColor(0x00, 0x00, 0x00, 0x20);
        m_lightBorderColor = QColor(80, 80, 80);
        m_darkBorderColor = QColor(0, 0, 0);
	}
}

ColorMapper::~ColorMapper()
{
}
