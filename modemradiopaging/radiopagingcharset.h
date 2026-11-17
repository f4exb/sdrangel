///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#ifndef MODEMRADIOPAGING_RADIOPAGINGCHARSET_H
#define MODEMRADIOPAGING_RADIOPAGINGCHARSET_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "export.h"

// Codepages of the alphanumeric characters of the pagers.
//
// The codes of POCSAG and FLEX are 7 bits (128), those of GSC 6 bits (64). A pager shows a code with the glyph of
// its codepage, which depends on the pager: the transmission does not tell the codepage. A codepage belongs to a
// protocol, then a vendor, a model and the codepage of that model (protocol, vendor, model, codepage); within the
// protocol the key is the vendor, the model and the codepage. Several of them (pager models) may share one table.
//
// An entry of a table is the glyph of the code (UTF-8, null: no glyph, a control or unused code), then the other
// characters taken for that code when text is encoded (lowercase letters of an uppercase pager, the Latin letters
// that look like a Cyrillic one, ...). Decoding gives the glyph of each code, <0xNN> for a code without one;
// encoding takes the glyph or an alias of each character (a glyph of several characters, the Arabic lam-alef,
// first), and <0xNN> for any code.
class MODEMRADIOPAGING_API RadioPagingCharset
{
public:
    static const int m_maxGlyphs = 8;           //!< Glyph and aliases of an entry
    typedef const char *Entry[m_maxGlyphs];

    struct Table
    {
        int m_codeBits;                         //!< 7 (128 entries) or 6 (64)
        const Entry *m_entries;
    };

    //!< A special format of the messages of a pager on top of its table (a command language of the pager, a
    //!< transform): the text the user writes and reads is not the text sent
    struct Transcoder
    {
        const char *m_hint;                     //!< For the user: what the text is, and an example
        //!< The text to send of the text of the user. False and an error if it is not valid; a warning if it was changed
        bool (*m_encode)(const std::string& text, std::string& out, std::string& error, std::string& warning);
        //!< The text of the user of a text received
        std::string (*m_decode)(const std::string& text);
        //!< The characters of HEX/binary messages (FLEX Kanji, Shift JIS, RadioPagingKanji), not of the alphanumeric
        //!< messages: those keep the table, encode and decode do not apply it to them
        bool m_binary = false;
    };

    //!< A codepage within its protocol. No vendor: none chosen (the default of the user of the key)
    struct Key
    {
        std::string m_vendor;
        std::string m_model;
        std::string m_codepage;

        bool empty() const { return m_vendor.empty(); }
        bool operator==(const Key& other) const {
            return (m_vendor == other.m_vendor) && (m_model == other.m_model) && (m_codepage == other.m_codepage);
        }
        bool operator!=(const Key& other) const { return !(*this == other); }
    };

    // Exported on its own: the export of the class does not cover its nested classes
    struct MODEMRADIOPAGING_API Codepage
    {
        const char *m_protocol;                 //!< pocsag, flex or gsc
        const char *m_vendor;
        const char *m_model;
        const char *m_codepage;
        const char *m_label;                    //!< For the user
        const Table *m_table;
        const Transcoder *m_transcoder = nullptr;   //!< A special format on top of the table, nullptr for none
        bool m_rtl = false;                     //!< Right-to-left: the text is shown in the reverse order of the codes

        Key key() const { return Key{m_vendor, m_model, m_codepage}; }
        int codes() const { return 1 << m_table->m_codeBits; }
        //!< The glyph of a code, nullptr if it has none
        const char *glyph(unsigned code) const;
        //!< The glyph and the aliases of a code
        std::vector<std::string> glyphs(unsigned code) const;
    };

    //!< Every codepage, the default of each protocol first
    static const std::vector<Codepage>& codepages();
    //!< The codepages of a protocol (golay is taken for gsc)
    static std::vector<const Codepage*> codepages(const std::string& protocol);
    //!< The codepage of a key in a protocol (not case sensitive), nullptr if none
    static const Codepage *find(const std::string& protocol, const Key& key);
    //!< The default codepage of a protocol (generic ASCII), nullptr for a protocol without codepages
    static const Codepage *defaultFor(const std::string& protocol);

    //!< The text of codes: glyphs, <0xNN> for the codes without one; with a transcoder, its text of them
    static std::string decode(const Codepage& codepage, const std::vector<uint8_t>& codes);
    //!< The codes of a text: a glyph or an alias for each character, or <0xNN> (a code of the codepage); with a
    //!< transcoder, of its text to send. False and an error naming the first character that has no code (or why the
    //!< transcoder refuses the text); a warning if the transcoder changed the text
    static bool encode(const Codepage& codepage, const std::string& text, std::vector<uint8_t>& codes, std::string *error = nullptr,
        std::string *warning = nullptr);
    //!< The code of the character at text[pos] (advanced past it), -1 if it has none
    static int encodeOne(const Codepage& codepage, const std::string& text, size_t& pos);
};

// The custom codepage of a protocol (generic/generic/custom): the default table of the protocol with codes mapped to
// other characters by the user, and the direction of its text. As text: "60=05D0 61=05D1", hex code = hex Unicode code
// point, separated by spaces or commas, with rtl for a right-to-left text. The codepage points into the object: it is
// not copied
class MODEMRADIOPAGING_API RadioPagingCustomCodepage
{
public:
    typedef std::vector<std::pair<int, uint32_t>> Codes;

    static RadioPagingCharset::Key key() { return RadioPagingCharset::Key{"generic", "generic", "custom"}; }
    static bool isCustom(const RadioPagingCharset::Key& key) { return key == RadioPagingCustomCodepage::key(); }
    //!< The mapping and the direction of a text; false and an error if it is not valid (a code out of the protocol, not
    //!< a character)
    static bool parse(const std::string& protocol, const std::string& text, Codes& codes, bool& rtl, std::string& error);
    static std::string text(const Codes& codes, bool rtl);

    //!< The codepage of a protocol with the mapping of a text (the codes not valid are left out)
    RadioPagingCustomCodepage(const std::string& protocol, const std::string& text);
    RadioPagingCustomCodepage(const RadioPagingCustomCodepage&) = delete;
    RadioPagingCustomCodepage& operator=(const RadioPagingCustomCodepage&) = delete;
    const RadioPagingCharset::Codepage& codepage() const { return m_codepage; }

private:
    std::string m_protocol;
    std::vector<std::string> m_glyphs;      //!< The UTF-8 of each code mapped
    RadioPagingCharset::Entry m_entries[128];
    RadioPagingCharset::Table m_table;
    RadioPagingCharset::Codepage m_codepage;
};

#endif // MODEMRADIOPAGING_RADIOPAGINGCHARSET_H
