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

static const char* vertexShaderSource2 =
    "attribute highp vec4 vertex;\n"
    "attribute highp vec2 texCoord;\n"
    "varying highp vec2 texCoordVar;\n"
    "void main() {\n"
    "    gl_Position = vertex;\n"
    "    texCoordVar = texCoord;\n"
    "}\n";

static const char* vertexShaderSource =
    "#version 330\n"
    "in highp vec4 vertex;\n"
    "in highp vec2 texCoord;\n"
    "out highp vec2 texCoordVar;\n"
    "void main() {\n"
    "    gl_Position = vertex;\n"
    "    texCoordVar = texCoord;\n"
    "}\n";

static const char* fragmentShaderSource2 =
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

static const char* fragmentShaderSource =
    "#version 330\n"
    "uniform highp sampler2D tex1;\n"
    "uniform highp sampler2D tex2;\n"
    "uniform highp float imw;\n"
    "uniform highp float imh;\n"
    "uniform highp float tlw;\n"
    "uniform highp float tlh;\n"
    "in highp vec2 texCoordVar;\n"
    "out vec4 fragColor;\n"
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
    "    float p1 = texture(tex1, vec2(p1x, p1y)).r;\n"
    "    float p2 = texture(tex1, vec2(p2x, p1y)).r;\n"
    "    float p3 = texture(tex1, vec2(p3x, p3y)).r;\n"
    "    float p4 = texture(tex1, vec2(p4x, p3y)).r;\n"
    "    float p12 = mix(p1, p2, fract(txs1));\n"
    "    float p34 = mix(p3, p4, fract(txs3));\n"
    "    float p = mix(p12, p34, fract(tys));\n"
    "    fragColor = vec4(p);\n"
    "}\n";

TVScreenAnalog::TVScreenAnalog(QWidget *parent)	:
	QOpenGLWidget(parent),
	m_shader(nullptr),
    m_vao(nullptr),
    m_verticesBuf(nullptr),
    m_textureCoordsBuf(nullptr),
	m_imageTexture(nullptr),
	m_lineShiftsTexture(nullptr)
{
	m_isDataChanged = false;
	m_frontBuffer = new TVScreenAnalogBuffer(5, 1);
	m_backBuffer = new TVScreenAnalogBuffer(5, 1);

	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(tick()));
	m_updateTimer.start(40); // capped at 25 FPS
}

TVScreenAnalog::~TVScreenAnalog()
{
	delete m_backBuffer;
	delete m_frontBuffer;
}

void TVScreenAnalog::cleanup()
{
	if (!QOpenGLContext::currentContext()) {
		return;
	}

	if (m_shader)
	{
		delete m_shader;
		m_shader = nullptr;
	}

	if (m_imageTexture)
	{
		delete m_imageTexture;
		m_imageTexture = nullptr;
	}

	if (m_lineShiftsTexture)
	{
		delete m_lineShiftsTexture;
		m_lineShiftsTexture = nullptr;
	}

    delete m_verticesBuf;
    m_verticesBuf = nullptr;
    delete m_textureCoordsBuf;
    m_textureCoordsBuf = nullptr;
    delete m_vao;
    m_vao = nullptr;
 }

TVScreenAnalogBuffer *TVScreenAnalog::getBackBuffer()
{
	return m_backBuffer;
}

void TVScreenAnalog::resizeTVScreen(int intCols, int intRows)
{
	qDebug("TVScreenAnalog::resizeTVScreen: cols: %d, rows: %d", intCols, intRows);

	int colsAdj = intCols + 4;
	QMutexLocker lock(&m_buffersMutex);
	if (m_frontBuffer->getWidth() != colsAdj || m_frontBuffer->getHeight() != intRows)
	{
		delete m_backBuffer;
		delete m_frontBuffer;
		m_frontBuffer = new TVScreenAnalogBuffer(colsAdj, intRows);
		m_backBuffer = new TVScreenAnalogBuffer(colsAdj, intRows);
	}
}

void TVScreenAnalog::resizeGL(int intWidth, int intHeight)
{
	glViewport(0, 0, intWidth, intHeight);
}

void TVScreenAnalog::initializeGL()
{
	initializeOpenGLFunctions();

	connect(
		QOpenGLContext::currentContext(),
		&QOpenGLContext::aboutToBeDestroyed,
		this,
		&TVScreenAnalog::cleanup
	);

	m_shader = new QOpenGLShaderProgram(this);

    int majorVersion = 0, minorVersion = 0;
    if (QOpenGLContext::currentContext())
    {
        majorVersion = QOpenGLContext::currentContext()->format().majorVersion();
        minorVersion = QOpenGLContext::currentContext()->format().minorVersion();
    }
    if ((majorVersion > 3) || ((majorVersion == 3) && (minorVersion >= 3)))
    {
        if (!m_shader->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource))
        {
            qWarning()
                << "TVScreenAnalog::initializeGL: error in vertex shader:"
                << m_shader->log();
            return;
        }

        if (!m_shader->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource))
        {
            qWarning()
                << "TVScreenAnalog::initializeGL: error in fragment shader:"
                << m_shader->log();
            return;
        }

        m_vao = new QOpenGLVertexArrayObject();
        m_vao->create();
        m_vao->bind();
    }
    else
    {
        if (!m_shader->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource2))
        {
            qWarning()
                << "TVScreenAnalog::initializeGL: error in vertex shader:"
                << m_shader->log();
            return;
        }

        if (!m_shader->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource2))
        {
            qWarning()
                << "TVScreenAnalog::initializeGL: error in fragment shader:"
                << m_shader->log();
            return;
        }
    }

	if (!m_shader->link())
	{
		qWarning()
			<< "TVScreenAnalog::initializeGL: error linking shader:"
			<< m_shader->log();
		return;
	}

	m_vertexAttribIndex = m_shader->attributeLocation("vertex");
	m_texCoordAttribIndex = m_shader->attributeLocation("texCoord");
	m_textureLoc1 = m_shader->uniformLocation("tex1");
	m_textureLoc2 = m_shader->uniformLocation("tex2");
	m_imageWidthLoc = m_shader->uniformLocation("imw");
	m_imageHeightLoc = m_shader->uniformLocation("imh");
	m_texelWidthLoc = m_shader->uniformLocation("tlw");
	m_texelHeightLoc = m_shader->uniformLocation("tlh");
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
}

void TVScreenAnalog::initializeTextures(TVScreenAnalogBuffer *buffer)
{
	m_imageTexture = new QOpenGLTexture(QOpenGLTexture::Target2D);
	m_lineShiftsTexture = new QOpenGLTexture(QOpenGLTexture::Target2D);
	m_imageTexture->setSize(buffer->getWidth(), buffer->getHeight());
	m_lineShiftsTexture->setSize(1, buffer->getHeight());
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

TVScreenAnalogBuffer *TVScreenAnalog::swapBuffers()
{
	QMutexLocker lock(&m_buffersMutex);
	std::swap(m_frontBuffer, m_backBuffer);
	m_isDataChanged = true;
	return m_backBuffer;
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

	if (!m_shader)
	{
		glClearColor(0.2f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		return;
	}

	TVScreenAnalogBuffer *buffer = m_frontBuffer;

	if (!m_imageTexture ||
		m_imageTexture->width() != buffer->getWidth() ||
		m_imageTexture->height() != buffer->getHeight())
	{
		initializeTextures(buffer);
	}

	float imageWidth = buffer->getWidth();
	float imageHeight = buffer->getHeight();
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
		buffer->getWidth(), buffer->getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, buffer->getImageData());

	glActiveTexture(GL_TEXTURE1);
	m_lineShiftsTexture->bind();
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
		1, buffer->getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, buffer->getLineShiftData());

	float rectHalfWidth = 1.0f + 4.0f / (imageWidth - 4.0f);
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

    if (m_vao)
    {
        m_vao->bind();

        m_verticesBuf->bind();
        m_verticesBuf->allocate(vertices, 4 * 2 * sizeof(GL_FLOAT));
        m_shader->enableAttributeArray(m_vertexAttribIndex);
        m_shader->setAttributeBuffer(m_vertexAttribIndex, GL_FLOAT, 0, 2);

        // As these coords are constant, this could be moved into the init method
        m_textureCoordsBuf->bind();
        m_textureCoordsBuf->allocate(arrTextureCoords, 4 * 2 * sizeof(GL_FLOAT));
        m_shader->enableAttributeArray(m_texCoordAttribIndex);
        m_shader->setAttributeBuffer(m_texCoordAttribIndex, GL_FLOAT, 0, 2);
    }
    else
    {
        glVertexAttribPointer(m_vertexAttribIndex, 2, GL_FLOAT, GL_FALSE, 0, vertices);
        glEnableVertexAttribArray(m_vertexAttribIndex);
        glVertexAttribPointer(m_texCoordAttribIndex, 2, GL_FLOAT, GL_FALSE, 0, arrTextureCoords);
        glEnableVertexAttribArray(m_texCoordAttribIndex);
    }

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    if (m_vao)
    {
        m_vao->release();
    }
    else
    {
	   glDisableVertexAttribArray(m_vertexAttribIndex);
	   glDisableVertexAttribArray(m_texCoordAttribIndex);
    }

	m_shader->release();
}
