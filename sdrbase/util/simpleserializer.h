///////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany     //
// written by Christian Daniel                                                       //
// Copyright (C) 2015-2016, 2018-2019 Edouard Griffiths, F4EXB <f4exb06@gmail.com>   //
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                         //
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
#ifndef INCLUDE_SIMPLESERIALIZER_H
#define INCLUDE_SIMPLESERIALIZER_H

#include <QString>
#include <QMap>
#include <QDataStream>
#include <QIODevice>
#include "dsp/dsptypes.h"
#include "export.h"

class SDRBASE_API SimpleSerializer {
public:
	SimpleSerializer(quint32 version);

	void writeS32(quint32 id, qint32 value);
	void writeU32(quint32 id, quint32 value);
	void writeS64(quint32 id, qint64 value);
	void writeU64(quint32 id, quint64 value);
	void writeFloat(quint32 id, float value);
	void writeDouble(quint32 id, double value);
	void writeReal(quint32 id, Real value)
	{
		if(sizeof(Real) == 4)
			writeFloat(id, value);
		else writeDouble(id, value);
	}
	void writeBool(quint32 id, bool value);
	void writeString(quint32 id, const QString& value);
	void writeBlob(quint32 id, const QByteArray& value);

	template<typename T>
	void writeList(quint32 id, const QList<T>& value)
	{
		QByteArray data;
		QDataStream *stream = new QDataStream(&data, QIODevice::WriteOnly);
		(*stream) << value;
		delete stream;
		writeBlob(id, data);
	}
	template<typename TK, typename TV>
	void writeHash(quint32 id, const QHash<TK,TV>& value)
	{
		QByteArray data;
		QDataStream *stream = new QDataStream(&data, QIODevice::WriteOnly);
		(*stream) << value;
		delete stream;
		writeBlob(id, data);
	}

	const QByteArray& final();

protected:
    // Lists and hashes are written as TBlob
	enum Type {
		TSigned32 = 0,
		TUnsigned32 = 1,
		TSigned64 = 2,
		TUnsigned64 = 3,
		TFloat = 4,
		TDouble = 5,
		TBool = 6,
		TString = 7,
		TBlob = 8,
		TVersion = 9
	};

	QByteArray m_data;
	bool m_finalized;

	bool writeTag(Type type, quint32 id, quint32 length);
};

class SDRBASE_API SimpleDeserializer {
public:
	SimpleDeserializer(const QByteArray& data);

	bool readS32(quint32 id, qint32* result, qint32 def = 0) const;
	bool readU32(quint32 id, quint32* result, quint32 def = 0) const;
	bool readS64(quint32 id, qint64* result, qint64 def = 0) const;
	bool readU64(quint32 id, quint64* result, quint64 def = 0) const;
	bool readFloat(quint32 id, float* result, float def = 0) const;
	bool readDouble(quint32 id, double* result, double def = 0) const;
	bool readReal(quint32 id, Real* result, Real def = 0) const;
	bool readBool(quint32 id, bool* result, bool def = false) const;
	bool readString(quint32 id, QString* result, const QString& def = QString()) const;
	bool readBlob(quint32 id, QByteArray* result, const QByteArray& def = QByteArray()) const;

	template<typename T>
	bool readList(quint32 id, QList<T>* result, const QList<T>& def = {})
	{
		QByteArray data;
		bool ok = readBlob(id, &data);
		if (ok)
		{
			QDataStream *stream = new QDataStream(data);
			(*stream) >> *result;
			delete stream;
		}
		else
		{
			*result = def;
		}
		return ok;
	}
	template<typename TK, typename TV>
	bool readHash(quint32 id, QHash<TK,TV>* result, const QHash<TK,TV>& def = {})
	{
		QByteArray data;
		bool ok = readBlob(id, &data);
		if (ok)
		{
			QDataStream *stream = new QDataStream(data);
			(*stream) >> *result;
			delete stream;
		}
		else
		{
			*result = def;
		}
		return ok;
	}

	bool isValid() const { return m_valid; }
	quint32 getVersion() const { return m_version; }
	void dump() const;

private:
	enum Type {
		TSigned32 = 0,
		TUnsigned32 = 1,
		TSigned64 = 2,
		TUnsigned64 = 3,
		TFloat = 4,
		TDouble = 5,
		TBool = 6,
		TString = 7,
		TBlob = 8,
		TVersion = 9
	};

	struct Element {
		Type type;
		quint32 ofs;
		quint32 length;

		Element(Type _type, quint32 _ofs, quint32 _length) :
			type(_type),
			ofs(_ofs),
			length(_length)
		{ }
	};
	typedef QMap<quint32, Element> Elements;

	QByteArray m_data;
	bool m_valid;
	Elements m_elements;
	quint32 m_version;

	bool parseAll();
	bool readTag(uint* readOfs, uint readEnd, Type* type, quint32* id, quint32* length) const;
	quint8 readByte(uint* readOfs) const
	{
		quint8 res = m_data[*readOfs];
		(*readOfs)++;
		return res;
	}
};

#endif // INCLUDE_SIMPLESERIALIZER_H
