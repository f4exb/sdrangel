///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
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

#include "glshaderarray.h"

const QString GLShaderArray::m_strVertexShaderSourceArray = QString(
        "uniform highp mat4 uMatrix;\n"
        "attribute highp vec4 vertex;\n"
        "attribute highp vec2 texCoord;\n"
        "varying mediump vec2 texCoordVar;\n"
        "void main() {\n"
        "    gl_Position = uMatrix * vertex;\n"
        "    texCoordVar = texCoord;\n"
        "}\n"
        );

const QString GLShaderArray::m_strFragmentShaderSourceColored = QString(
        "uniform lowp sampler2D uTexture;\n"
        "varying mediump vec2 texCoordVar;\n"
        "void main() {\n"
        "    gl_FragColor = texture2D(uTexture, texCoordVar);\n"
        "}\n"
        );

GLShaderArray::GLShaderArray()
{
    m_objProgram=NULL;
    m_objImage = NULL;
    m_objTexture = NULL;
    m_intCols=0;
    m_intRows=0;
    m_blnInitialized=false;
    m_objCurrentRow=NULL;
}

GLShaderArray::~GLShaderArray()
{
    Cleanup();
}

void GLShaderArray::InitializeGL(int intCols, int intRows)
{
    QMatrix4x4 objQMatrix;

    m_blnInitialized=false;

    m_intCols=0;
    m_intRows=0;

    m_objCurrentRow=NULL;

    if(m_objProgram==NULL)
    {
        m_objProgram = new QOpenGLShaderProgram();

        if (!m_objProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, m_strVertexShaderSourceArray))
        {
            qDebug() << "GLShaderArray::initializeGL: error in vertex shader: " << m_objProgram->log();
        }

        if (!m_objProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, m_strFragmentShaderSourceColored))
        {
            qDebug() << "GLShaderArray::initializeGL: error in fragment shader: " << m_objProgram->log();
        }

        m_objProgram->bindAttributeLocation("vertex", 0);

        if (!m_objProgram->link())
        {
            qDebug() << "GLShaderArray::initializeGL: error linking shader: " << m_objProgram->log();
        }

        m_objProgram->bind();
        m_objProgram->setUniformValue(m_objMatrixLoc, objQMatrix);
        m_objProgram->setUniformValue(m_objTextureLoc, 0);
        m_objProgram->release();
    }

    m_objMatrixLoc = m_objProgram->uniformLocation("uMatrix");
    m_objTextureLoc = m_objProgram->uniformLocation("uTexture");
    m_objColorLoc = m_objProgram->uniformLocation("uColour");

    if(m_objTexture!=NULL)
    {
        delete m_objTexture;
        m_objTexture=NULL;
    }

    //Image container
    m_objImage = new QImage(intCols,intRows,QImage::Format_RGBA8888);
    m_objImage->fill(QColor(0,0,0));

    m_objTexture = new QOpenGLTexture(*m_objImage);
    m_objTexture->setMinificationFilter(QOpenGLTexture::Linear);
    m_objTexture->setMagnificationFilter(QOpenGLTexture::Linear);
    m_objTexture->setWrapMode(QOpenGLTexture::ClampToEdge);

    m_intCols = intCols;
    m_intRows = intRows;

    m_blnInitialized=true;

}

QRgb * GLShaderArray::GetRowBuffer(int intRow)
{
    if(m_blnInitialized==false)
    {
        return NULL;
    }

    if(m_objImage==NULL)
    {
        return NULL;
    }

    if(intRow>m_intRows)
    {
        return NULL;
    }

    return (QRgb *)m_objImage->scanLine(intRow);
}


void GLShaderArray::RenderPixels(unsigned char *chrData)
{   
    QOpenGLFunctions *ptrF;
    int intI;
    int intJ;
    int intNbVertices=4;

    QMatrix4x4 objQMatrix;

    GLfloat arrVertices[] =
    {
        -1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f,
         1.0f, -1.0f
    };

    GLfloat arrTextureCoords[] =
    {
        0.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,
        1.0f, 1.0f
    };

    QRgb *ptrLine;
    int intVal;

    if(m_blnInitialized==false)
    {
        return;
    }

    if(m_objImage==NULL)
    {
        return ;
    }

    if(chrData!=NULL)
    {
        for(intJ=0; intJ<m_intRows; intJ++)
        {
            ptrLine = (QRgb *)m_objImage->scanLine(intJ);

            for(intI=0; intI<m_intCols; intI ++)
            {

                intVal = (int)(*chrData);
                *ptrLine = qRgb(intVal,intVal,intVal);
                ptrLine ++;

                chrData ++;
            }
        }
    }

    //Affichage
    ptrF = QOpenGLContext::currentContext()->functions();

    m_objProgram->bind();

    m_objProgram->setUniformValue(m_objMatrixLoc, objQMatrix);
    m_objProgram->setUniformValue(m_objTextureLoc, 0);

    m_objTexture->bind();

    ptrF->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_intCols, m_intRows, GL_RGBA, GL_UNSIGNED_BYTE, m_objImage->bits());

    ptrF->glEnableVertexAttribArray(0); // vertex
    ptrF->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, arrVertices);

    ptrF->glEnableVertexAttribArray(1); // texture coordinates
    ptrF->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, arrTextureCoords);

    ptrF->glDrawArrays(GL_POLYGON, 0, intNbVertices);

    //cleanup
    ptrF->glDisableVertexAttribArray(0);
    ptrF->glDisableVertexAttribArray(1);

    //*********************//

    m_objTexture->release();
    m_objProgram->release();
}

void GLShaderArray::ResetPixels()
{
    if(m_objImage!=NULL)
    {
        m_objImage->fill(0);
    }
}

void GLShaderArray::Cleanup()
{
    m_blnInitialized=false;

    m_intCols=0;
    m_intRows=0;

    m_objCurrentRow=NULL;

    if (m_objProgram)
    {
        delete m_objProgram;
        m_objProgram = NULL;
    }

    if(m_objTexture!=NULL)
    {
        delete m_objTexture;
        m_objTexture=NULL;
    }

    if (m_objImage!=NULL)
    {
        delete m_objImage;
        m_objImage = NULL;
    }
}


bool GLShaderArray::SelectRow(int intLine)
{
    bool blnRslt=false;

    if(m_blnInitialized)
    {
        if((intLine<m_intRows)
            &&(intLine>=0))
        {
            m_objCurrentRow=(QRgb *)m_objImage->scanLine(intLine);
            blnRslt=true;
        }
        else
        {
            m_objCurrentRow=NULL;
        }
    }

    return blnRslt;
}

bool GLShaderArray::SetDataColor(int intCol,QRgb objColor)
{
    bool blnRslt=false;

    if(m_blnInitialized)
    {
        if((intCol<m_intCols)
            &&(intCol>=0)
            &&(m_objCurrentRow!=NULL))
        {
            m_objCurrentRow[intCol]=objColor;
            blnRslt=true;
        }
    }

    return blnRslt;

}


