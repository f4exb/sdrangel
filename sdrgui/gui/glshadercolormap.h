///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_GUI_GLSHADERCOLORMAP_H_
#define INCLUDE_GUI_GLSHADERCOLORMAP_H_

#include <QString>
#include <QOpenGLTexture>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>

#include "export.h"

class QOpenGLShaderProgram;
class QMatrix4x4;
class QVector4D;

// Shader to fill with a gradient from a color map
// Used for filling the area under the spectrum
class SDRGUI_API GLShaderColorMap : protected QOpenGLFunctions
{
public:
    GLShaderColorMap();
    ~GLShaderColorMap();

    void initializeGL(int majorVersion, int minorVersion);
    void initColorMapTexture(const QString &colorMapName);
    void drawSurfaceStrip(const QMatrix4x4& transformMatrix, GLfloat *vertices, int nbVertices, float scale, float alpha);
    void cleanup();

private:
    void initColorMapTextureMutable(const QString &colorMapName);
    void initColorMapTextureImmutable(const QString &colorMapName);
    bool useImmutableStorage();

    QOpenGLShaderProgram *m_program;
    QOpenGLVertexArrayObject *m_vao;
    QOpenGLBuffer *m_verticesBuf;
    QOpenGLTexture *m_colorMapTexture;
    unsigned int m_colorMapTextureId;
    int m_vertexLoc;
    int m_matrixLoc;
    int m_colorMapLoc;
    int m_scaleLoc;
    int m_alphaLoc;
    bool m_useImmutableStorage;
    static const QString m_vertexShaderSourceColorMap2;
    static const QString m_vertexShaderSourceColorMap;
    static const QString m_fragmentShaderSourceColorMap2;
    static const QString m_fragmentShaderSourceColorMap;
};

#endif /* INCLUDE_GUI_GLSHADERCOLORMAP_H_ */
