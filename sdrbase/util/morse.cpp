///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include <QStringList>
#include <QDebug>

#include "util/morse.h"

// ITU encoding with a few extras
const struct Morse::ASCIIToMorse Morse::m_asciiToMorse[] = {
    {' ', " "},
    {'!', "-.-.--"}, // Non-ITU
    {'\"', ".-..-."},
    {'#', ""},
    {'$', "...-..-"}, // Non-ITU
    {'%', ""},
    {'&', ".-..."},
    {'\'', ".----."},
    {'(', "-.--."},
    {')', "-.--.-"},
    {'*', "_.._"},
    {'+', ".-.-."},
    {',', "--..--"},
    {'-', "-....-"},
    {'.', ".-.-.-"},
    {'/', "-..-."},
    {'0', "-----"},
    {'1', ".----"},
    {'2', "..---"},
    {'3', "...--"},
    {'4', "....-"},
    {'5', "....."},
    {'6', "-...."},
    {'7', "--..."},
    {'8', "---.."},
    {'9', "----."},
    {':', "---..."},
    {';', "-.-.-."}, // Non-ITU
    {'<', ""},
    {'=', "-...-"},
    {'>', ""},
    {'?', "..--.."},
    {'@', ".--.-."},
    {'A', ".-"},
    {'B', "-..."},
    {'C', "-.-."},
    {'D', "-.."},
    {'E', "."},
    {'F', "..-."},
    {'G', "--."},
    {'H', "...."},
    {'I', ".."},
    {'J', ".---"},
    {'K', "-.-"},
    {'L', ".-.."},
    {'M', "--"},
    {'N', "-."},
    {'O', "---"},
    {'P', ".--."},
    {'Q', "--.-"},
    {'R', ".-."},
    {'S', "..."},
    {'T', "-"},
    {'U', "..-"},
    {'V', "...-"},
    {'W', ".--"},
    {'X', "-..-"},
    {'Y', "-.--"},
    {'Z', "--.."},
    {'[', ""},
    {'\\', ""},
    {']', ""},
    {'^', ""},
    {'_', "..--.-"}, // Non-ITU
    {'`', ""},
    {'a', ".-"},
    {'b', "-..."},
    {'c', "-.-."},
    {'d', "-.."},
    {'e', "."},
    {'f', "..-."},
    {'g', "--."},
    {'h', "...."},
    {'i', ".."},
    {'j', ".---"},
    {'k', "-.-"},
    {'l', ".-.."},
    {'m', "--"},
    {'n', "-."},
    {'o', "---"},
    {'p', ".--."},
    {'q', "--.-"},
    {'r', ".-."},
    {'s', "..."},
    {'t', "-"},
    {'u', "..-"},
    {'v', "...-"},
    {'w', ".--"},
    {'x', "-..-"},
    {'y', "-.--"},
    {'z', "--.."},
    {'{', ""},
    {'|', ""},
    {'}', ""},
    {'~', ""}
};

// Convert ASCII character to Morse code sequence consisting of . and - characters
QString Morse::toMorse(char ascii)
{
    char firstChar = m_asciiToMorse[0].ascii;
    if ((ascii >= firstChar) && (ascii < 127))
        return QString(m_asciiToMorse[ascii - firstChar].morse);
    else
        return QString();
}

// Convert string to Morse code sequence consisting of . and - characters separated by spaces
QString Morse::toMorse(QString &string)
{
    QStringList list;
    for (int i = 0; i < string.size(); i++)
    {
        if (i != 0)
            list.append(" ");
        list.append(toMorse(string.at(i).toLatin1()));
    }
    return list.join("");
}

// Converts Morse code sequence using ASCII . and - to Unicode bullet and minus sign
// which are horizontally aligned, so look nicer in GUIs
QString Morse::toUnicode(QString &morse)
{
    return morse.replace(QChar('.'), QChar(0x2022)).replace(QChar('-'), QChar(0x2212));
}

// Converts a string to a unicode Morse sequence with extra space characters between
// dots and dashes to improve readability in GUIs
QString Morse::toSpacedUnicode(QString &morse)
{
    QString temp = toUnicode(morse);
    for (int i = 0; i < temp.size(); i+=2)
        temp.insert(i, ' ');
    return temp;
}

// Converts a string to a unicode Morse sequence
QString Morse::toUnicodeMorse(QString &string)
{
    QString ascii = toMorse(string);
    return ascii.replace(QChar('.'), QChar(0x2022)).replace(QChar('-'), QChar(0x2212));
}

// Converts a string to a unicode Morse sequence with spacing between dots and dashes
QString Morse::toSpacedUnicodeMorse(QString &string)
{
    QString temp = toUnicodeMorse(string);
    for (int i = 0; i < temp.size(); i+=2)
        temp.insert(i, ' ');
    return temp;
}

#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

// Converts a Morse sequence to an ASCII character. -1 if no mapping found.
int Morse::toASCII(QString &morse)
{
    for (unsigned int i = 0; i < COUNT_OF(m_asciiToMorse); i++)
    {
        if (morse == m_asciiToMorse[i].morse)
            return m_asciiToMorse[i].ascii;
    }
    return -1;
}

// Converts a sequence of Morse code to a string. Unknown Morse codes are ignored.
QString Morse::toString(QString &morse)
{
    QString string("");
    QStringList groups = morse.split(" ");
    for (int i = 0; i < groups.size(); i++)
    {
        int c = Morse::toASCII(groups[i]);
        if ((c != -1) && (groups[i] != ""))
           string.append(QChar(c));
    }
    return string;
}
