///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "gui/glshadersources.h"

const QString GLShaderSources::m_vertexShaderSourceSimple = QString(
	    "#version 150\n"
		"uniform mat4 uMatrix;\n"
		"in vec4 vertex\n"
		"void main() {\n"
		"    gl_Position = uMatrix * vertex;\n"
		"}\n"
		);

const QString GLShaderSources::m_fragmentShaderSourceColored = QString(
	    "#version 150\n"
		"uniform mediump vec4 uColour;\n"
		"void main() {\n"
		"    gl_FragColor = uColour;\n"
		"}\n"
		);
