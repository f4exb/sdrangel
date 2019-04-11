///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef INCLUDE_MESSAGE_H
#define INCLUDE_MESSAGE_H

#include <stdlib.h>
#include "export.h"

class SDRBASE_API Message {
public:
	Message();
	virtual ~Message();

	virtual const char* getIdentifier() const;
	virtual bool matchIdentifier(const char* identifier) const;
	static bool match(const Message* message);

	void* getDestination() const { return m_destination; }
	void setDestination(void *destination) { m_destination = destination; }

protected:
	// addressing
	static const char* m_identifier;
	void* m_destination;
};

#define MESSAGE_CLASS_DECLARATION \
	public: \
		const char* getIdentifier() const; \
		bool matchIdentifier(const char* identifier) const; \
		static bool match(const Message& message); \
	protected: \
		static const char* m_identifier; \
	private:

#define MESSAGE_CLASS_DEFINITION(Name, BaseClass) \
	const char* Name::m_identifier = #Name; \
	const char* Name::getIdentifier() const { return m_identifier; } \
	bool Name::matchIdentifier(const char* identifier) const {\
		return (m_identifier == identifier) ? true : BaseClass::matchIdentifier(identifier); \
	} \
	bool Name::match(const Message& message) { return message.matchIdentifier(m_identifier); }

#endif // INCLUDE_MESSAGE_H
