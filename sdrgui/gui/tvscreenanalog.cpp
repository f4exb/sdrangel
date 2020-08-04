///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Vort                                                       //
// Copyright (C) 2018 F4HKW                                                      //
// for F4EXB / SDRAngel                                                          //
//                                                                               //
// OpenGL interface modernization.                                               //
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

#include "tvscreenanalog.h"

static const char* vertexShaderSource =
"attribute highp vec4 vertex;\n"
"attribute highp vec2 texCoord;\n"
"varying highp vec2 texCoordVar;\n"
"void main() {\n"
"    gl_Position = vertex;\n"
"    texCoordVar = texCoord;\n"
"}\n";

static const char* fragmentShaderSource =
"uniform highp sampler2D tex1;\n"
"uniform highp sampler2D tex2;\n"
"uniform highp float imw;\n"
"uniform highp float imh;\n"
"uniform highp float tlw;\n"
"uniform highp float tlh;\n"
"varying highp vec2 texCoordVar;\n"
"void main() {\n"
"    float tlhw = 0.5 * tlw;"
"    float tlhh = 0.5 * tlh;"
"    float tys = (texCoordVar.y + tlhh) * imh;\n"
"    float p1y = floor(tys) * tlh - tlhh;\n"
"    float p3y = p1y + tlh;\n"
"    float tshift1 = texture2D(tex2, vec2(0.0, p1y)).r;\n"
"    float tshift3 = texture2D(tex2, vec2(0.0, p3y)).r;\n"
"    float shift1 = (1.0 - tshift1 * 2.0) * tlw;\n"
"    float shift3 = (1.0 - tshift3 * 2.0) * tlw;\n"
"    float txs1 = (texCoordVar.x + shift1 + tlhw) * imw;\n"
"    float txs3 = (texCoordVar.x + shift3 + tlhw) * imw;\n"
"    float p1x = floor(txs1) * tlw - tlhw;\n"
"    float p3x = floor(txs3) * tlw - tlhw;\n"
"    float p2x = p1x + tlw;\n"
"    float p4x = p3x + tlw;\n"
"    float p1 = texture2D(tex1, vec2(p1x, p1y)).r;\n"
"    float p2 = texture2D(tex1, vec2(p2x, p1y)).r;\n"
"    float p3 = texture2D(tex1, vec2(p3x, p3y)).r;\n"
"    float p4 = texture2D(tex1, vec2(p4x, p3y)).r;\n"
"    float p12 = mix(p1, p2, fract(txs1));\n"
"    float p34 = mix(p3, p4, fract(txs3));\n"
"    float p = mix(p12, p34, fract(tys));\n"
"    gl_FragColor = vec4(p);\n"
"}\n";

TVScreenAnalog::TVScreenAnalog(QWidget *parent)
	: QGLWidget(parent)
{
	m_isDataChanged = false;
	m_data = std::make_shared<TVScreenAnalogData>(5, 1);

	connect(&m_objTimer, SIGNAL(timeout()), this, SLOT(tick()));
	m_objTimer.start(40); // capped at 25 FPS
}

void TVScreenAnalog::cleanup()
{
	m_shader = nullptr;
	m_imageTexture = nullptr;
	m_lineShiftsTexture = nullptr;
}

std::shared_ptr<TVScreenAnalogData> TVScreenAnalog::getData()
{
	return m_data;
}

void TVScreenAnalog::resizeTVScreen(int intCols, int intRows)
{
	qDebug("TVScreen::resizeTVScreen: cols: %d, rows: %d", intCols, intRows);

	int colsAdj = intCols + 4;
	if (m_data->getWidth() != colsAdj || m_data->getHeight() != intRows)
		m_data = std::make_shared<TVScreenAnalogData>(colsAdj, intRows);
}

void TVScreenAnalog::resizeGL(int intWidth, int intHeight)
{
	glViewport(0, 0, intWidth, intHeight);
}

void TVScreenAnalog::initializeGL()
{
	initializeOpenGLFunctions();

	m_shader = std::make_shared<QOpenGLShaderProgram>(this);
	m_shader->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
	m_shader->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
	m_shader->link();

	m_vertexAttribIndex = m_shader->attributeLocation("vertex");
	m_texCoordAttribIndex = m_shader->attributeLocation("texCoord");
	m_textureLoc1 = m_shader->uniformLocation("tex1");
	m_textureLoc2 = m_shader->uniformLocation("tex2");
	m_imageWidthLoc = m_shader->uniformLocation("imw");
	m_imageHeightLoc = m_shader->uniformLocation("imh");
	m_texelWidthLoc = m_shader->uniformLocation("tlw");
	m_texelHeightLoc = m_shader->uniformLocation("tlh");

	connect(QOpenGLContext::currentContext(), &QOpenGLContext::aboutToBeDestroyed,
		this, &TVScreenAnalog::cleanup); // TODO: when migrating to QOpenGLWidget
}

void TVScreenAnalog::initializeTextures()
{
	m_imageTexture = std::make_shared<QOpenGLTexture>(QOpenGLTexture::Target2D);
	m_lineShiftsTexture = std::make_shared<QOpenGLTexture>(QOpenGLTexture::Target2D);
	m_imageTexture->setSize(m_data->getWidth(), m_data->getHeight());
	m_lineShiftsTexture->setSize(1, m_data->getHeight());
	m_imageTexture->setFormat(QOpenGLTexture::RGBA8_UNorm);
	m_lineShiftsTexture->setFormat(QOpenGLTexture::RGBA8_UNorm);
	m_imageTexture->setAutoMipMapGenerationEnabled(false);
	m_lineShiftsTexture->setAutoMipMapGenerationEnabled(false);
	m_imageTexture->allocateStorage(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8);
	m_lineShiftsTexture->allocateStorage(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8);

	m_imageTexture->setMinificationFilter(QOpenGLTexture::Nearest);
	m_imageTexture->setMagnificationFilter(QOpenGLTexture::Nearest);
	m_lineShiftsTexture->setMinificationFilter(QOpenGLTexture::Nearest);
	m_lineShiftsTexture->setMagnificationFilter(QOpenGLTexture::Nearest);
	m_imageTexture->setWrapMode(QOpenGLTexture::DirectionS, QOpenGLTexture::ClampToBorder);
	m_imageTexture->setWrapMode(QOpenGLTexture::DirectionT, QOpenGLTexture::ClampToEdge);
	m_lineShiftsTexture->setWrapMode(QOpenGLTexture::DirectionS, QOpenGLTexture::Repeat);
	m_lineShiftsTexture->setWrapMode(QOpenGLTexture::DirectionT, QOpenGLTexture::ClampToEdge);
}

void TVScreenAnalog::renderImage()
{
	m_isDataChanged = true;
}

void TVScreenAnalog::tick()
{
	if (m_isDataChanged)
	{
		update();
	}
}

void TVScreenAnalog::paintGL()
{
	m_isDataChanged = false;

	if (!m_imageTexture ||
		m_imageTexture->width() != m_data->getWidth() ||
		m_imageTexture->height() != m_data->getHeight())
	{
		initializeTextures();
	}

	float imageWidth = m_data->getWidth();
	float imageHeight = m_data->getHeight();
	float texelWidth = 1.0f / imageWidth;
	float texelHeight = 1.0f / imageHeight;

	m_shader->bind();
	m_shader->setUniformValue(m_textureLoc1, 0);
	m_shader->setUniformValue(m_textureLoc2, 1);
	m_shader->setUniformValue(m_imageWidthLoc, imageWidth);
	m_shader->setUniformValue(m_imageHeightLoc, imageHeight);
	m_shader->setUniformValue(m_texelWidthLoc, texelWidth);
	m_shader->setUniformValue(m_texelHeightLoc, texelHeight);

	glActiveTexture(GL_TEXTURE0);
	m_imageTexture->bind();
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
		m_data->getWidth(), m_data->getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, m_data->getImageData());

	glActiveTexture(GL_TEXTURE1);
	m_lineShiftsTexture->bind();
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
		1, m_data->getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, m_data->getLineShiftData());

	float rectHalfWidth = 1.0f + 4 * texelWidth;
	GLfloat vertices[] =
	{
		-rectHalfWidth, -1.0f,
		-rectHalfWidth,  1.0f,
		 rectHalfWidth,  1.0f,
		 rectHalfWidth, -1.0f
	};

	static const GLfloat arrTextureCoords[] =
	{
		0.0f, 1.0f,
		0.0f, 0.0f,
		1.0f, 0.0f,
		1.0f, 1.0f
	};

	glVertexAttribPointer(m_vertexAttribIndex, 2, GL_FLOAT, GL_FALSE, 0, vertices);
	glEnableVertexAttribArray(m_vertexAttribIndex);
	glVertexAttribPointer(m_texCoordAttribIndex, 2, GL_FLOAT, GL_FALSE, 0, arrTextureCoords);
	glEnableVertexAttribArray(m_texCoordAttribIndex);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDisableVertexAttribArray(m_vertexAttribIndex);
	glDisableVertexAttribArray(m_texCoordAttribIndex);

	m_shader->release();
}