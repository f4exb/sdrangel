///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
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

#include <QOpenGLContext>

#include "gui/glshadertvarray.h"

const QString GLShaderTVArray::m_strVertexShaderSourceArray2 = QString(
        "uniform highp mat4 uMatrix;\n"
        "attribute highp vec4 vertex;\n"
        "attribute highp vec2 texCoord;\n"
        "varying mediump vec2 texCoordVar;\n"
        "void main() {\n"
        "    gl_Position = uMatrix * vertex;\n"
        "    texCoordVar = texCoord;\n"
        "}\n");

const QString GLShaderTVArray::m_strVertexShaderSourceArray = QString(
        "#version 330\n"
        "uniform highp mat4 uMatrix;\n"
        "in highp vec4 vertex;\n"
        "in highp vec2 texCoord;\n"
        "out mediump vec2 texCoordVar;\n"
        "void main() {\n"
        "    gl_Position = uMatrix * vertex;\n"
        "    texCoordVar = texCoord;\n"
        "}\n");

const QString GLShaderTVArray::m_strFragmentShaderSourceColored2 = QString(
        "uniform lowp sampler2D uTexture;\n"
        "varying mediump vec2 texCoordVar;\n"
        "void main() {\n"
        "    gl_FragColor = texture2D(uTexture, texCoordVar);\n"
        "}\n");

const QString GLShaderTVArray::m_strFragmentShaderSourceColored = QString(
        "#version 330\n"
        "uniform lowp sampler2D uTexture;\n"
        "in mediump vec2 texCoordVar;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    fragColor = texture(uTexture, texCoordVar);\n"
        "}\n");

GLShaderTVArray::GLShaderTVArray(bool blnColor) :
    m_objProgram(nullptr),
    m_vao(nullptr),
    m_verticesBuf(nullptr),
    m_textureCoordsBuf(nullptr),
    m_matrixLoc(0),
    m_textureLoc(0),
    m_objImage(nullptr),
    m_objTexture(nullptr),
    m_intCols(0),
    m_intRows(0),
    m_objCurrentRow(nullptr),
    m_blnInitialized(false),
    m_blnColor(blnColor),
    m_blnAlphaBlend(false),
    m_blnAlphaReset(false)
{
}

GLShaderTVArray::~GLShaderTVArray()
{
    qDebug("GLShaderTVArray::~GLShaderTVArray");
    cleanup();
}

void GLShaderTVArray::initializeGL(int majorVersion, int minorVersion, int intCols, int intRows)
{
    QMatrix4x4 objQMatrix;

    m_blnInitialized = false;

    m_intCols = 0;
    m_intRows = 0;

    m_objCurrentRow = nullptr;

    if (!m_objProgram)
    {
        m_objProgram = new QOpenGLShaderProgram();
        if ((majorVersion > 3) || ((majorVersion == 3) && (minorVersion >= 3)))
        {
            if (!m_objProgram->addShaderFromSourceCode(QOpenGLShader::Vertex,
                    m_strVertexShaderSourceArray))
            {
                qDebug() << "GLShaderArray::initializeGL: error in vertex shader: "
                        << m_objProgram->log();
            }

            if (!m_objProgram->addShaderFromSourceCode(QOpenGLShader::Fragment,
                    m_strFragmentShaderSourceColored))
            {
                qDebug()
                        << "GLShaderArray::initializeGL: error in fragment shader: "
                        << m_objProgram->log();
            }

            m_vao = new QOpenGLVertexArrayObject();
            m_vao->create();
            m_vao->bind();
        }
        else
        {
            if (!m_objProgram->addShaderFromSourceCode(QOpenGLShader::Vertex,
                    m_strVertexShaderSourceArray2))
            {
                qDebug() << "GLShaderArray::initializeGL: error in vertex shader: "
                        << m_objProgram->log();
            }

            if (!m_objProgram->addShaderFromSourceCode(QOpenGLShader::Fragment,
                    m_strFragmentShaderSourceColored2))
            {
                qDebug()
                        << "GLShaderArray::initializeGL: error in fragment shader: "
                        << m_objProgram->log();
            }
        }

        m_objProgram->bindAttributeLocation("vertex", 0);

        if (!m_objProgram->link())
        {
            qDebug() << "GLShaderArray::initializeGL: error linking shader: "
                    << m_objProgram->log();
        }

        m_objProgram->bind();
        m_objProgram->setUniformValue(m_matrixLoc, objQMatrix);
        m_objProgram->setUniformValue(m_textureLoc, 0);
        if (m_vao)
        {
            m_verticesBuf = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
            m_verticesBuf->setUsagePattern(QOpenGLBuffer::DynamicDraw);
            m_verticesBuf->create();
            m_textureCoordsBuf = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
            m_textureCoordsBuf->setUsagePattern(QOpenGLBuffer::DynamicDraw);
            m_textureCoordsBuf->create();
            m_vao->release();
        }
        m_objProgram->release();
    }

    m_matrixLoc = m_objProgram->uniformLocation("uMatrix");
    m_textureLoc = m_objProgram->uniformLocation("uTexture");

    if (m_objTexture)
    {
        delete m_objTexture;
        m_objTexture = nullptr;
    }

    //Image container
    m_objImage = new QImage(intCols, intRows, QImage::Format_RGBA8888);
    m_objImage->fill(QColor(0, 0, 0));

    m_objTexture = new QOpenGLTexture(*m_objImage);
    //m_objTexture->setFormat(QOpenGLTexture::RGBA8_UNorm); avoids OpenGL warning and in fact is useless
    m_objTexture->setMinificationFilter(QOpenGLTexture::Linear);
    m_objTexture->setMagnificationFilter(QOpenGLTexture::Linear);
    m_objTexture->setWrapMode(QOpenGLTexture::ClampToEdge);

    m_intCols = intCols;
    m_intRows = intRows;

    m_blnInitialized = true;

}

QRgb *GLShaderTVArray::GetRowBuffer(int intRow)
{
    if (!m_blnInitialized) {
        return nullptr;
    }

    if (!m_objImage) {
        return nullptr;
    }

    if (intRow > m_intRows) {
        return nullptr;
    }

    return (QRgb *) m_objImage->scanLine(intRow);
}

void GLShaderTVArray::RenderPixels(unsigned char *chrData)
{
    QOpenGLFunctions *ptrF;
    int intI;
    int intJ;
    int intNbVertices = 6;

    QMatrix4x4 objQMatrix;

    GLfloat arrVertices[] =
    // 2 3
    // 1 4
    //1             2            3           3           4            1
    { -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -1.0f, -1.0f, -1.0f };

    GLfloat arrTextureCoords[] =
    // 1 4
    // 2 3
    //1           2           3           3           4           1
    { 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };

    QRgb *ptrLine;
    int intVal;

    if (!m_blnInitialized) {
        return;
    }

    if (!m_objImage) {
        return;
    }

    if (chrData != 0)
    {
        for (intJ = 0; intJ < m_intRows; intJ++)
        {
            ptrLine = (QRgb *) m_objImage->scanLine(intJ);

            for (intI = 0; intI < m_intCols; intI++)
            {
                if (m_blnColor)
                {
                    *ptrLine = qRgb((int) (*(chrData+2)), (int) (*(chrData+1)), (int) (*chrData));
                    chrData+=3;
                }
                else
                {
                    intVal = (int) (*chrData);
                    *ptrLine = qRgb(intVal, intVal, intVal);
                    chrData++;
                }

                ptrLine++;
            }
        }
    }

    //Affichage
    ptrF = QOpenGLContext::currentContext()->functions();

    m_objProgram->bind();

    m_objProgram->setUniformValue(m_matrixLoc, objQMatrix);
    m_objProgram->setUniformValue(m_textureLoc, 0);

    if (m_blnAlphaReset)
    {
        ptrF->glClear(GL_COLOR_BUFFER_BIT);
        m_blnAlphaReset = false;
    }

    if (m_blnAlphaBlend)
    {
        ptrF->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        ptrF->glEnable(GL_BLEND);
    }
    else
    {
    	ptrF->glDisable(GL_BLEND);
    }

    m_objTexture->bind();

    ptrF->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_intCols, m_intRows, GL_RGBA,
            GL_UNSIGNED_BYTE, m_objImage->bits());

    if (m_vao)
    {
        m_vao->bind();

        m_verticesBuf->bind();
        m_verticesBuf->allocate(arrVertices, intNbVertices * 2 * sizeof(GL_FLOAT));
        m_objProgram->enableAttributeArray(0);
        m_objProgram->setAttributeBuffer(0, GL_FLOAT, 0, 2);

        m_textureCoordsBuf->bind();
        m_textureCoordsBuf->allocate(arrTextureCoords, intNbVertices * 2 * sizeof(GL_FLOAT));
        m_objProgram->enableAttributeArray(1);
        m_objProgram->setAttributeBuffer(1, GL_FLOAT, 0, 2);
    }
    else
    {
        ptrF->glEnableVertexAttribArray(0); // vertex
        ptrF->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, arrVertices);

        ptrF->glEnableVertexAttribArray(1); // texture coordinates
        ptrF->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, arrTextureCoords);
    }

    ptrF->glDrawArrays(GL_TRIANGLES, 0, intNbVertices);

    //cleanup
    if (m_vao)
    {
        m_vao->release();
    }
    else
    {
        ptrF->glDisableVertexAttribArray(0);
        ptrF->glDisableVertexAttribArray(1);
    }

    //*********************//

    m_objTexture->release();
    m_objProgram->release();
}

void GLShaderTVArray::ResetPixels()
{
    if (m_objImage) {
        m_objImage->fill(0);
    }
}

void GLShaderTVArray::ResetPixels(int alpha)
{
    if (m_objImage) {
        m_objImage->fill(qRgba(0, 0, 0, alpha));
    }
}

void GLShaderTVArray::cleanup()
{
    m_blnInitialized = false;

    m_intCols = 0;
    m_intRows = 0;

    m_objCurrentRow = nullptr;

	if (!QOpenGLContext::currentContext()) {
		return;
	}

    if (m_objProgram)
    {
        delete m_objProgram;
        m_objProgram = nullptr;
    }

    if (m_objTexture)
    {
        delete m_objTexture;
        m_objTexture = nullptr;
    }

    if (m_objImage)
    {
        delete m_objImage;
        m_objImage = nullptr;
    }

    delete m_verticesBuf;
    m_verticesBuf = nullptr;
    delete m_textureCoordsBuf;
    m_textureCoordsBuf = nullptr;
    delete m_vao;
    m_vao = nullptr;
}

bool GLShaderTVArray::SelectRow(int intLine)
{
    bool blnRslt = false;

    if (m_blnInitialized)
    {
        if ((intLine < m_intRows) && (intLine >= 0))
        {
            m_objCurrentRow = (QRgb *) m_objImage->scanLine(intLine);
            blnRslt = true;
        }
        else
        {
            m_objCurrentRow = nullptr;
        }
    }

    return blnRslt;
}

bool GLShaderTVArray::SetDataColor(int intCol, QRgb objColor)
{
    bool blnRslt = false;

    if (m_blnInitialized)
    {
        if ((intCol < m_intCols) && (intCol >= 0) && m_objCurrentRow)
        {
            m_objCurrentRow[intCol] = objColor;
            blnRslt = true;
        }
    }

    return blnRslt;
}

