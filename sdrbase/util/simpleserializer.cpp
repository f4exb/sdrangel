#include <stdint.h>
#include "util/simpleserializer.h"

#if __WORDSIZE == 64
#define PRINTF_FORMAT_S32 "%d"
#define PRINTF_FORMAT_U32 "%u"
#define PRINTF_FORMAT_S64 "%d"
#define PRINTF_FORMAT_U64 "%u"
#else
#define PRINTF_FORMAT_S32 "%d"
#define PRINTF_FORMAT_U32 "%u"
#define PRINTF_FORMAT_S64 "%lld"
#define PRINTF_FORMAT_U64 "%llu"
#endif

template <bool> struct Assert { };
typedef Assert<(bool(sizeof(float) == 4))> float_must_be_32_bits[bool(sizeof(float) == 4) ? 1 : -1];
typedef Assert<(bool(sizeof(double) == 8))> double_must_be_64_bits[bool(sizeof(double) == 8) ? 1 : -1];

SimpleSerializer::SimpleSerializer(uint version) :
	m_data(),
	m_finalized(false)
{
	m_data.reserve(100);

	// write version information
	int length;
	if(version >= (1 << 24))
		length = 4;
	else if(version >= (1 << 16))
		length = 3;
	else if(version >= (1 << 8))
		length = 2;
	else if(version > 0)
		length = 1;
	else length = 0;
	if(!writeTag(TVersion, 0, length))
		return;
	length--;
	for(int i = length; i >= 0; i--)
		m_data.push_back((char)((version >> (i * 8)) & 0xff));
}

void SimpleSerializer::writeS32(quint32 id, qint32 value)
{
	int length;

	if(id == 0) {
		qCritical("SimpleSerializer: ID 0 is not allowed");
		return;
	}

	if((value < -(1 << 23)) || (value >= (1 << 23)))
		length = 4;
	else if((value < -(1 << 15)) || (value >= (1 << 15)))
		length = 3;
	else if((value < -(1 << 7)) || (value >= (1 << 7)))
		length = 2;
	else if(value != 0)
		length = 1;
	else length = 0;

	if(!writeTag(TSigned32, id, length))
		return;

	length--;
	for(int i = length; i >= 0; i--)
		m_data.push_back((char)((value >> (i * 8)) & 0xff));
}

void SimpleSerializer::writeU32(quint32 id, quint32 value)
{
	int length;

	if(id == 0) {
		qCritical("SimpleSerializer: ID 0 is not allowed");
		return;
	}

	if(value >= (1 << 24))
		length = 4;
	else if(value >= (1 << 16))
		length = 3;
	else if(value >= (1 << 8))
		length = 2;
	else if(value > 0)
		length = 1;
	else length = 0;

	if(!writeTag(TUnsigned32, id, length))
		return;

	length--;
	for(int i = length; i >= 0; i--)
		m_data.push_back((char)((value >> (i * 8)) & 0xff));
}

void SimpleSerializer::writeS64(quint32 id, qint64 value)
{
	int length;

	if(id == 0) {
		qCritical("SimpleSerializer: ID 0 is not allowed");
		return;
	}

	if((value < -(1ll << 55)) || (value >= (1ll << 55)))
		length = 8;
	else if((value < -(1ll << 47)) || (value >= (1ll << 47)))
		length = 7;
	else if((value < -(1ll << 39)) || (value >= (1ll << 39)))
		length = 6;
	else if((value < -(1ll << 31)) || (value >= (1ll << 31)))
		length = 5;
	else if((value < -(1ll << 23)) || (value >= (1ll << 23)))
		length = 4;
	else if((value < -(1ll << 15)) || (value >= (1ll << 15)))
		length = 3;
	else if((value < -(1ll << 7)) || (value >= (1ll << 7)))
		length = 2;
	else if(value != 0)
		length = 1;
	else length = 0;

	if(!writeTag(TSigned64, id, length))
		return;

	length--;
	for(int i = length; i >= 0; i--)
		m_data.push_back((char)((value >> (i * 8)) & 0xff));
}

void SimpleSerializer::writeU64(quint32 id, quint64 value)
{
	int length;

	if(id == 0) {
		qCritical("SimpleSerializer: ID 0 is not allowed");
		return;
	}

	if(value >= (1ll << 56))
		length = 8;
	else if(value >= (1ll << 48))
		length = 7;
	else if(value >= (1ll << 40))
		length = 6;
	else if(value >= (1ll << 32))
		length = 5;
	else if(value >= (1ll << 24))
		length = 4;
	else if(value >= (1ll << 16))
		length = 3;
	else if(value >= (1ll << 8))
		length = 2;
	else if(value > 0)
		length = 1;
	else length = 0;

	if(!writeTag(TUnsigned64, id, length))
		return;

	length--;
	for(int i = length; i >= 0; i--)
		m_data.push_back((char)((value >> (i * 8)) & 0xff));
}

void SimpleSerializer::writeFloat(quint32 id, float value)
{
	if(id == 0) {
		qCritical("SimpleSerializer: ID 0 is not allowed");
		return;
	}

	if(!writeTag(TFloat, id, 4))
		return;

	quint32 tmp = *((quint32*)&value);
	m_data.push_back((char)((tmp >> 24) & 0xff));
	m_data.push_back((char)((tmp >> 16) & 0xff));
	m_data.push_back((char)((tmp >> 8) & 0xff));
	m_data.push_back((char)(tmp & 0xff));
}

void SimpleSerializer::writeDouble(quint32 id, double value)
{
	if(id == 0) {
		qCritical("SimpleSerializer: ID 0 is not allowed");
		return;
	}

	if(!writeTag(TDouble, id, 8))
		return;

	quint64 tmp = *((quint64*)&value);
	m_data.push_back((char)((tmp >> 56) & 0xff));
	m_data.push_back((char)((tmp >> 48) & 0xff));
	m_data.push_back((char)((tmp >> 40) & 0xff));
	m_data.push_back((char)((tmp >> 32) & 0xff));
	m_data.push_back((char)((tmp >> 24) & 0xff));
	m_data.push_back((char)((tmp >> 16) & 0xff));
	m_data.push_back((char)((tmp >> 8) & 0xff));
	m_data.push_back((char)(tmp & 0xff));
}

void SimpleSerializer::writeBool(quint32 id, bool value)
{
	if(id == 0) {
		qCritical("SimpleSerializer: ID 0 is not allowed");
		return;
	}

	if(!writeTag(TBool, id, 1))
		return;
	if(value)
		m_data.push_back((char)0x01);
	else m_data.push_back((char)0x00);
}

void SimpleSerializer::writeString(quint32 id, const QString& value)
{
	if(id == 0) {
		qCritical("SimpleSerializer: ID 0 is not allowed");
		return;
	}

	QByteArray utf8 = value.toUtf8();
	if(!writeTag(TString, id, utf8.size()))
		return;
	m_data.append(utf8);
}

void SimpleSerializer::writeBlob(quint32 id, const QByteArray& value)
{
	if(id == 0) {
		qCritical("SimpleSerializer: ID 0 is not allowed");
		return;
	}

	if(!writeTag(TBlob, id, value.size()))
		return;
	m_data.append(value);
}

const QByteArray& SimpleSerializer::final()
{
	m_finalized = true;
	return m_data;
}

bool SimpleSerializer::writeTag(Type type, quint32 id, quint32 length)
{
	if(m_finalized) {
		qCritical("SimpleSerializer: config has already been finalized (id %u)", id);
		return false;
	}

	int idLen;
	int lengthLen;

	if(id < (1 << 8))
		idLen = 0;
	else if(id < (1 << 16))
		idLen = 1;
	else if(id < (1 << 24))
		idLen = 2;
	else idLen = 3;

	if(length < (1 << 8))
		lengthLen = 0;
	else if(length < (1 << 16))
		lengthLen = 1;
	else if(length < (1 << 24))
		lengthLen = 2;
	else lengthLen = 3;

	m_data.push_back((char)((type << 4) | (idLen << 2) | lengthLen));
	for(int i = idLen; i >= 0; i--)
		m_data.push_back((char)((id >> (i * 8)) & 0xff));
	for(int i = lengthLen; i >= 0; i--)
		m_data.push_back((char)((length >> (i * 8)) & 0xff));
	return true;
}

SimpleDeserializer::SimpleDeserializer(const QByteArray& data) :
	m_data(data)
{
	m_valid = parseAll();

	// read version information
	uint readOfs;
	Elements::const_iterator it = m_elements.constFind(0);
	if(it == m_elements.constEnd())
		goto setInvalid;
	if(it->type != TVersion)
		goto setInvalid;
	if(it->length > 4)
		goto setInvalid;

	readOfs = it->ofs;
	m_version = 0;
	for(uint i = 0; i < it->length; i++)
		m_version = (m_version << 8) | readByte(&readOfs);
	return;

setInvalid:
	m_valid = false;
}

bool SimpleDeserializer::readS32(quint32 id, qint32* result, qint32 def) const
{
	uint readOfs;
	qint32 tmp;
	Elements::const_iterator it = m_elements.constFind(id);
	if(it == m_elements.constEnd())
		goto returnDefault;
	if(it->type != TSigned32)
		goto returnDefault;
	if(it->length > 4)
		goto returnDefault;

	readOfs = it->ofs;
	tmp = 0;
	for(uint i = 0; i < it->length; i++) {
		quint8 byte = readByte(&readOfs);
		if((i == 0) && (byte & 0x80))
			tmp = -1;
		tmp = (tmp << 8) | byte;
	}
	*result = tmp;
	return true;

returnDefault:
	*result = def;
	return false;
}

bool SimpleDeserializer::readU32(quint32 id, quint32* result, quint32 def) const
{
	uint readOfs;
	quint32 tmp;
	Elements::const_iterator it = m_elements.constFind(id);
	if(it == m_elements.constEnd())
		goto returnDefault;
	if(it->type != TUnsigned32)
		goto returnDefault;
	if(it->length > 4)
		goto returnDefault;

	readOfs = it->ofs;
	tmp = 0;
	for(uint i = 0; i < it->length; i++)
		tmp = (tmp << 8) | readByte(&readOfs);
	*result = tmp;
	return true;

returnDefault:
	*result = def;
	return false;
}

bool SimpleDeserializer::readS64(quint32 id, qint64* result, qint64 def) const
{
	uint readOfs;
	qint64 tmp;
	Elements::const_iterator it = m_elements.constFind(id);
	if(it == m_elements.constEnd())
		goto returnDefault;
	if(it->type != TSigned64)
		goto returnDefault;
	if(it->length > 8)
		goto returnDefault;

	readOfs = it->ofs;
	tmp = 0;
	for(uint i = 0; i < it->length; i++) {
		quint8 byte = readByte(&readOfs);
		if((i == 0) && (byte & 0x80))
			tmp = -1;
		tmp = (tmp << 8) | byte;
	}
	*result = tmp;
	return true;

returnDefault:
	*result = def;
	return false;
}

bool SimpleDeserializer::readU64(quint32 id, quint64* result, quint64 def) const
{
	uint readOfs;
	quint64 tmp;
	Elements::const_iterator it = m_elements.constFind(id);
	if(it == m_elements.constEnd())
		goto returnDefault;
	if(it->type != TUnsigned64)
		goto returnDefault;
	if(it->length > 8)
		goto returnDefault;

	readOfs = it->ofs;
	tmp = 0;
	for(uint i = 0; i < it->length; i++)
		tmp = (tmp << 8) | readByte(&readOfs);
	*result = tmp;
	return true;

returnDefault:
	*result = def;
	return false;
}

bool SimpleDeserializer::readFloat(quint32 id, float* result, float def) const
{
	uint readOfs;
	quint32 tmp;
	Elements::const_iterator it = m_elements.constFind(id);
	if(it == m_elements.constEnd())
		goto returnDefault;
	if(it->type != TFloat)
		goto returnDefault;
	if(it->length != 4)
		goto returnDefault;

	readOfs = it->ofs;
	tmp = 0;
	for(int i = 0; i < 4; i++)
		tmp = (tmp << 8) | readByte(&readOfs);
	*result = *((float*)&tmp);
	return true;

returnDefault:
	*result = def;
	return false;
}

bool SimpleDeserializer::readDouble(quint32 id, double* result, double def) const
{
	uint readOfs;
	quint64 tmp;
	Elements::const_iterator it = m_elements.constFind(id);
	if(it == m_elements.constEnd())
		goto returnDefault;
	if(it->type != TDouble)
		goto returnDefault;
	if(it->length != 8)
		goto returnDefault;

	readOfs = it->ofs;
	tmp = 0;
	for(int i = 0; i < 8; i++)
		tmp = (tmp << 8) | readByte(&readOfs);
	*result = *((double*)&tmp);
	return true;

returnDefault:
	*result = def;
	return false;
}

bool SimpleDeserializer::readReal(quint32 id, Real* result, Real def) const
{
	if(sizeof(Real) == 4) {
		uint readOfs;
		quint32 tmp;
		Elements::const_iterator it = m_elements.constFind(id);
		if(it == m_elements.constEnd())
			goto returnDefault32;
		if(it->type != TFloat)
			goto returnDefault32;
		if(it->length != 4)
			goto returnDefault32;

		readOfs = it->ofs;
		tmp = 0;
		for(int i = 0; i < 4; i++)
			tmp = (tmp << 8) | readByte(&readOfs);
		*result = *((Real*)&tmp);
		return true;

	returnDefault32:
		*result = def;
		return false;
	} else {
		uint readOfs;
		quint64 tmp;
		Elements::const_iterator it = m_elements.constFind(id);
		if(it == m_elements.constEnd())
			goto returnDefault64;
		if(it->type != TDouble)
			goto returnDefault64;
		if(it->length != 8)
			goto returnDefault64;

		readOfs = it->ofs;
		tmp = 0;
		for(int i = 0; i < 8; i++)
			tmp = (tmp << 8) | readByte(&readOfs);
		*result = *((Real*)&tmp);
		return true;

	returnDefault64:
		*result = def;
		return false;
	}
}

bool SimpleDeserializer::readBool(quint32 id, bool* result, bool def) const
{
	uint readOfs;
	quint8 tmp;
	Elements::const_iterator it = m_elements.constFind(id);
	if(it == m_elements.constEnd())
		goto returnDefault;
	if(it->type != TBool)
		goto returnDefault;
	if(it->length != 1)
		goto returnDefault;

	readOfs = it->ofs;
	tmp = readByte(&readOfs);
	if(tmp == 0x00)
		*result = false;
	else *result = true;
	return true;

returnDefault:
	*result = def;
	return false;
}

bool SimpleDeserializer::readString(quint32 id, QString* result, const QString& def) const
{
	Elements::const_iterator it = m_elements.constFind(id);
	if(it == m_elements.constEnd())
		goto returnDefault;
	if(it->type != TString)
		goto returnDefault;

	*result = QString::fromUtf8(m_data.data() + it->ofs, it->length);
	return true;

returnDefault:
	*result = def;
	return false;
}

bool SimpleDeserializer::readBlob(quint32 id, QByteArray* result, const QByteArray& def) const
{
	Elements::const_iterator it = m_elements.constFind(id);
	if(it == m_elements.constEnd())
		goto returnDefault;
	if(it->type != TBlob)
		goto returnDefault;

	*result = QByteArray(m_data.data() + it->ofs, it->length);
	return true;

returnDefault:
	*result = def;
	return false;
}

void SimpleDeserializer::dump() const
{
	if(!m_valid) {
		qDebug("SimpleDeserializer dump: data invalid");
		return;
	} else {
		qDebug("SimpleDeserializer dump: version %u", m_version);
	}

	for(Elements::const_iterator it = m_elements.constBegin(); it != m_elements.constEnd(); ++it) {
		switch(it->type) {
			case TSigned32: {
				qint32 tmp;
				readS32(it.key(), &tmp);
				qDebug("id %d, S32, len %d: " PRINTF_FORMAT_S32, it.key(), it->length, tmp);
				break;
			}
			case TUnsigned32: {
				quint32 tmp;
				readU32(it.key(), &tmp);
				qDebug("id %d, U32, len %d: " PRINTF_FORMAT_U32, it.key(), it->length, tmp);
				break;
			}
			case TSigned64: {
				qint64 tmp;
				readS64(it.key(), &tmp);
				qDebug("id %d, S64, len %d: " PRINTF_FORMAT_S64, it.key(), it->length, (int)tmp);
				break;
			}
			case TUnsigned64: {
				quint64 tmp;
				readU64(it.key(), &tmp);
				qDebug("id %d, U64, len %d: " PRINTF_FORMAT_U64, it.key(), it->length, (uint)tmp);
				break;
			}
			case TFloat: {
				float tmp;
				readFloat(it.key(), &tmp);
				qDebug("id %d, FLOAT, len %d: %f", it.key(), it->length, tmp);
				break;
			}
			case TDouble: {
				double tmp;
				readDouble(it.key(), &tmp);
				qDebug("id %d, DOUBLE, len %d: %f", it.key(), it->length, tmp);
				break;
			}
			case TBool: {
				bool tmp;
				readBool(it.key(), &tmp);
				qDebug("id %d, BOOL, len %d: %s", it.key(), it->length, tmp ? "true" : "false");
				break;
			}
			case TString: {
				QString tmp;
				readString(it.key(), &tmp);
				qDebug("id %d, STRING, len %d: \"%s\"", it.key(), it->length, qPrintable(tmp));
				break;
			}
			case TBlob: {
				QByteArray tmp;
				readBlob(it.key(), &tmp);
				qDebug("id %d, BLOB, len %d", it.key(), it->length);
				break;
			}
			case TVersion: {
				qDebug("id %d, VERSION, len %d", it.key(), it->length);
				break;
			}
			default: {
				qDebug("id %d, UNKNOWN TYPE 0x%02x, len %d", it.key(), it->type, it->length);
				break;
			}
		}
	}
}

bool SimpleDeserializer::parseAll()
{
	uint readOfs = 0;
	Type type;
	quint32 id;
	quint32 length;

	/*
	QString hex;
	for(int i = 0; i < m_data.size(); i++)
		hex.append(QString("%1 ").arg((quint8)m_data[i], 2, 16, QChar('0')));
	qDebug("%s", qPrintable(hex));
	qDebug("==");
	*/

	while(readOfs < (uint)m_data.size()) {
		if(!readTag(&readOfs, m_data.size(), &type, &id, &length))
			return false;

		//qDebug("-- id %d, TYPE 0x%02x, len %d", id, type, length);

		if(m_elements.contains(id)) {
			qDebug("SimpleDeserializer: same ID found twice (id %u)", id);
			return false;
		}

		m_elements.insert(id, Element(type, readOfs, length));

		readOfs += length;

		if(readOfs == m_data.size())
			return true;
	}
	return false;
}

bool SimpleDeserializer::readTag(uint* readOfs, uint readEnd, Type* type, quint32* id, quint32* length) const
{
	quint8 tag = readByte(readOfs);

	*type = (Type)(tag >> 4);
	int idLen = ((tag >> 2) & 0x03) + 1;
	int lengthLen = (tag & 0x03) + 1;

	// make sure we have enough header bytes left
	if(((*readOfs) + idLen + lengthLen) > readEnd)
		return false;

	quint32 tmp = 0;
	for(int i = 0; i < idLen; i++)
		tmp = (tmp << 8) | readByte(readOfs);
	*id = tmp;
	tmp = 0;
	for(int i = 0; i < lengthLen; i++)
		tmp = (tmp << 8) | readByte(readOfs);
	*length = tmp;

	// check if payload fits the buffer
	if(((*readOfs) + (*length)) > readEnd)
		return false;
	else return true;
}

