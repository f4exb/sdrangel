#include <stdio.h>
#include <string.h>
#include <vector>
#include <iostream>

#if !(defined(max)) && _MSC_VER
	// VC fix
	#define max __max
#endif

#define MYMAX(a, b) (a) > (b) ? (a) : (b)

using namespace std;

class MemBlock
{
public:
	unsigned long m_startAddress;
	vector<unsigned char> m_bytes;
};

class MCU_File
{
public:
	explicit MCU_File(const char *fileName, const char *mode)
	{
		m_file = fopen(fileName, mode);
		if (m_file != NULL)
		{
			return;
		}

		cout << "Error opening";
		//string errorStr = "Error opening ";
		//errorStr += fileName;
		//errorStr += "\n";
		//throw errorStr;
	}

	~MCU_File()
	{
        if (m_file)
		    fclose(m_file);
	}

    bool FileOpened()
    {
        return m_file != NULL;
    }

	// Read binary file
	void ReadBin(unsigned long limit)
	{
		m_top = 0;

		m_chunks.push_back(MemBlock());
		m_chunks.back().m_startAddress = 0;

		cout << "Reading binary file\n";

		int tmp = fgetc(m_file);

		while (!feof(m_file))
		{
			m_chunks.back().m_bytes.push_back(tmp);

			if (m_chunks.back().m_bytes.size() > limit + 1)
			{
				m_chunks.back().m_bytes.pop_back();
				m_top = m_chunks.back().m_bytes.size() - 1;
				cout << "Ignoring data above address space!\n";
				cout << " Limit: " << limit << "\n";
				return;
			}

			tmp = fgetc(m_file);
		}

		m_top = m_chunks.back().m_bytes.size() - 1;

		if (!m_chunks.back().m_bytes.size())
		{
			cout << "No data!\n";

			m_chunks.pop_back();
		}
	}

	// Read hex file
	void ReadHex(unsigned long limit)
	{
		char szLine[1024];
		bool formatDetected = false;
		bool intel = false;
		bool endSeen = false;
		bool linear = true;				// Only used for intel hex
		unsigned long addressBase = 0;	// Only used for intel hex
		unsigned long dataRecords = 0;	// Only used for s-record
		while (!feof(m_file))
		{
			if (fgets(szLine, 1024, m_file) == 0)
			{
				if (ferror(m_file))
				{
					throw "Error reading input!\n";
				}
				continue;
			}

			if (szLine[strlen(szLine) - 1] == 0xA || szLine[strlen(szLine) - 1] == 0xD)
			{
				szLine[strlen(szLine) - 1] = 0;
			}

			if (szLine[strlen(szLine) - 1] == 0xA || szLine[strlen(szLine) - 1] == 0xD)
			{
				szLine[strlen(szLine) - 1] = 0;
			}

			if (strlen(szLine) == 1023)
			{
				throw "Hex file lines to long!\n";
			}
			// Ignore blank lines
			if (szLine[0] == '\n')
			{
				continue;
			}
			// Detect format and warn if garbage lines are found
			if (!formatDetected)
			{
				if (szLine[0] != ':' && szLine[0] != 'S')
				{
					cout << "Ignoring garbage line!\n";
					continue;
				}
				if (szLine[0] == 'S')
				{
					intel = false;
					cout << "Detected S-Record\n";
				}
				else
				{
					intel = true;
					cout << "Detected intel hex file\n";
				}
				formatDetected = true;
			}
			else if ((intel && szLine[0] != ':') ||
					(!intel && szLine[0] != 'S'))
			{
				cout << "Ignoring garbage line!\n";
				continue;
			}

			if (endSeen)
			{
				throw "Hex line after end of file record!\n";
			}

			if (intel)
			{
				unsigned long	dataBytes;
				unsigned long	startAddress;
				unsigned long	type;
				if (sscanf(&szLine[1], "%2lx%4lx%2lx", &dataBytes, &startAddress, &type) != 3)
				{
					throw "Hex line beginning corrupt!\n";
				}
				// Check line length
				if (szLine[11 + dataBytes * 2] != '\n' && szLine[11 + dataBytes * 2] != 0)
				{
					throw "Hex line length incorrect!\n";
				}
				// Check line checksum
				unsigned char	checkSum = 0;
				unsigned long	tmp;
				for (unsigned int i = 0; i <= dataBytes + 4; ++i)
				{
					if (sscanf(&szLine[1 + i * 2], "%2lx", &tmp) != 1)
					{
						throw "Hex line data corrupt!\n";
					}
					checkSum += tmp;
				}
				if (checkSum != 0)
				{
					throw "Hex line checksum error!\n";
				}

				switch (type)
				{
				case 0:
					// Data record
					if (!linear)
					{
						// Segmented
						unsigned long test = startAddress;
						test += dataBytes;
						if (test > 0xffff)
						{
							throw "Can't handle wrapped segments!\n";
						}
					}
					if (!m_chunks.size() ||
						m_chunks.back().m_startAddress + m_chunks.back().m_bytes.size() !=
						addressBase + startAddress)
					{
						m_chunks.push_back(MemBlock());
						m_chunks.back().m_startAddress = addressBase + startAddress;
					}
					{
						unsigned char i = 0;
						for (i = 0; i < dataBytes; ++i)
						{
							sscanf(&szLine[9 + i * 2], "%2lx", &tmp);
							if (addressBase + startAddress + i > limit)
							{
								cout << "Ignoring data above address space!\n";
								cout << "Data address: " << addressBase + startAddress + i;
								cout << " Limit: " << limit << "\n";
								if (!m_chunks.back().m_bytes.size())
								{
									m_chunks.pop_back();
								}
								continue;
							}
							m_chunks.back().m_bytes.push_back(tmp);
						}
					}
					break;

				case 1:
					// End-of-file record
					if (dataBytes != 0)
					{
						cout << "Warning: End of file record not zero length!\n";
					}
					if (startAddress != 0)
					{
						cout << "Warning: End of file record address not zero!\n";
					}
					endSeen = true;
					break;

				case 2:
					// Extended segment address record
					if (dataBytes != 2)
					{
						throw "Length field must be 2 in extended segment address record!\n";
					}
					if (startAddress != 0)
					{
						throw "Address field must be zero in extended segment address record!\n";
					}
					sscanf(&szLine[9], "%4lx", &startAddress);
					addressBase = startAddress << 4;
					linear = false;
					break;

				case 3:
					// Start segment address record
					if (dataBytes != 4)
					{
						cout << "Warning: Length field must be 4 in start segment address record!\n";
					}
					if (startAddress != 0)
					{
						cout << "Warning: Address field must be zero in start segment address record!\n";
					}
					if (dataBytes == 4)
					{
						unsigned long ssa;
						char	ssaStr[16];
						sscanf(&szLine[9], "%8lx", &ssa);
						sprintf(ssaStr, "%08lX\n", ssa);
						cout << "Segment start address (CS/IP): ";
						cout << ssaStr;
					}
					break;

				case 4:
					// Extended linear address record
					if (dataBytes != 2)
					{
						throw "Length field must be 2 in extended linear address record!\n";
					}
					if (startAddress != 0)
					{
						throw "Address field must be zero in extended linear address record!\n";
					}
					sscanf(&szLine[9], "%4lx", &startAddress);
					addressBase = ((unsigned long)startAddress) << 16;
					linear = true;
					break;

				case 5:
					// Start linear address record
					if (dataBytes != 4)
					{
						cout << "Warning: Length field must be 4 in start linear address record!\n";
					}
					if (startAddress != 0)
					{
						cout << "Warning: Address field must be zero in start linear address record!\n";
					}
					if (dataBytes == 4)
					{
						unsigned long lsa;
						char	lsaStr[16];
						sscanf(&szLine[9], "%8lx", &lsa);
						sprintf(lsaStr, "%08lX\n", lsa);
						cout << "Linear start address: ";
						cout << lsaStr;
					}
					break;

				default:
					cout << "Waring: Unknown record found!\n";
				}
			}
			else
			{
				// S-record
				unsigned long count;
				char			type;
				if (sscanf(&szLine[1], "%c%2lx", &type, &count) != 2)
				{
					throw "Hex line beginning corrupt!\n";
				}
				// Check line length
				if (szLine[4 + count * 2] != '\n' && szLine[4 + count * 2] != 0)
				{
					throw "Hex line length incorrect!\n";
				}
				// Check line checksum
				unsigned char	checkSum = 0;
				unsigned long	tmp;
				for (unsigned int i = 0; i < count + 1; ++i)
				{
					if (sscanf(&szLine[2 + i * 2], "%2lx", &tmp) != 1)
					{
						throw "Hex line data corrupt!\n";
					}
					checkSum += tmp;
				}
				if (checkSum != 255)
				{
					throw "Hex line checksum error!\n";
				}

				switch (type)
				{
				case '0':
					// Header record
					{
						char header[256];
						uint8_t i = 0;
						for (i = 0; uint8_t(i + 3) < count; ++i)
						{
							sscanf(&szLine[8 + i * 2], "%2lx", &tmp);
							header[i] = tmp;
						}
						header[i] = 0;
						if (i > 0)
						{
							cout << "Module name: " << header << "\n";
						}
					}
					break;

				case '1':
				case '2':
				case '3':
					// Data record
					{
						dataRecords++;
						unsigned long	startAddress;
						if (type == '1')
						{
							sscanf(&szLine[4], "%4lx", &startAddress);
						}
						else if (type == '2')
						{
							sscanf(&szLine[4], "%6lx", &startAddress);
						}
						else
						{
							sscanf(&szLine[4], "%8lx", &startAddress);
						}

						if (!m_chunks.size() ||
							m_chunks.back().m_startAddress + m_chunks.back().m_bytes.size() !=
							startAddress)
						{
							m_chunks.push_back(MemBlock());
							m_chunks.back().m_startAddress = startAddress;
						}
						unsigned char i = 0;
						for (i = uint8_t(type - '1'); uint8_t(i + 3) < count; ++i)
						{
							sscanf(&szLine[8 + i * 2], "%2lx", &tmp);
							if (startAddress + i > limit)
							{
								cout << "Ignoring data above address space!\n";
								cout << "Data address: " << startAddress + i;
								cout << " Limit: " << limit << "\n";
								if (!m_chunks.back().m_bytes.size())
								{
									m_chunks.pop_back();
								}
								continue;
							}
							m_chunks.back().m_bytes.push_back(tmp);
						}
					}
					break;

				case '5':
					// Count record
					{
						unsigned long	address;
						sscanf(&szLine[4], "%4lx", &address);
						if (address != dataRecords)
						{
							throw "Wrong number of data records!\n";
						}
					}
					break;

				case '7':
				case '8':
				case '9':
					// Start address record
					cout << "Ignoring start address record!\n";
					break;

				default:
					cout << "Unknown record found!\n";
				}
			}
		}
		if (intel && !endSeen)
		{
			cout << "No end of file record!\n";
		}
		if (!m_chunks.size())
		{
			throw "No data in file!\n";
		}
		vector<MemBlock>::iterator	vi;
		m_top = 0;
		for (vi = m_chunks.begin(); vi < m_chunks.end(); vi++)
		{
            //m_top = m_top > vi->m_startAddress + vi->m_bytes.size() - 1 ? m_top : vi->m_startAddress + vi->m_bytes.size() - 1;
            m_top = MYMAX(m_top, vi->m_startAddress + vi->m_bytes.size() - 1);
		}
	}

	// Rather inefficient this one, fix sometime
	bool GetByte(const unsigned long address, unsigned char &chr)
	{
		vector<MemBlock>::iterator	vi;

		for (vi = m_chunks.begin(); vi < m_chunks.end(); vi++)
		{
			if (vi->m_startAddress + vi->m_bytes.size() > address && vi->m_startAddress <= address)
			{
				break;
			}
		}
		if (vi == m_chunks.end())
		{
			return false;
		}
		chr = vi->m_bytes[address - vi->m_startAddress];
		return true;
	}

	bool BitString(const unsigned long address, const unsigned char bits, const bool lEndian, string &str)
	{
		bool			ok = false;
		long			i;
		unsigned char	chr;
		unsigned long	data = 0;
		unsigned long	tmp;

		if (lEndian)
		{
			for (i = 0; i < (bits + 7) / 8; ++i)
			{
				ok |= GetByte(address + i, chr);
				tmp = chr;
				data |= tmp << (8 * i);
			}
		}
		else
		{
			for (i = 0; i < (bits + 7) / 8; ++i)
			{
				ok |= GetByte(address + i, chr);
				tmp = chr;
				data |= tmp << (8 * ((bits + 7) / 8 - i - 1));
			}
		}

		if (!ok)
		{
			return false;
		}

		unsigned long mask = 1;

		str = "";
		for (i = 0; i < bits; i++)
		{
			if (data & mask)
			{
				str.insert(0,"1");
			}
			else
			{
				str.insert(0,"0");
			}
			mask <<= 1;
		}
		return true;
	}

	FILE *Handle() { return m_file; };
	vector<MemBlock>	m_chunks;
	unsigned long		m_top;
private:
	FILE				*m_file;
};
