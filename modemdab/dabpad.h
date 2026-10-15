///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef MODEMDAB_DABPAD_H_
#define MODEMDAB_DABPAD_H_

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "export.h"

namespace modemdab
{

// A DL Plus object (ETSI TS 102 980): a content type and the part of the label it tags.
struct MODEMDAB_API DABDLPlusObject
{
    int contentType = 0;        // 1 ITEM.TITLE, 4 ITEM.ARTIST...
    std::string text;

    static std::string contentTypeName(int contentType);
};

struct MODEMDAB_API DABDynamicLabel
{
    std::string text;           // UTF-8, with line breaks for the line break and end of headline codes
    int toggle = -1;
    bool dlPlus = false;
    bool itemToggle = false;
    bool itemRunning = false;
    std::vector<DABDLPlusObject> objects;
};

// Programme Associated Data (EN 300 401 clause 7.4): F-PAD, X-PAD, the dynamic label (with
// DL Plus) and MSC data groups in X-PAD for MOT applications.
class MODEMDAB_API DABPADDecoder
{
public:
    DABPADDecoder();

    void reset();
    // X-PAD application type of the MOT application (SlideShow), from FIG 0/13. 12 by default.
    void setMOTApplicationType(int xpadAppType);
    // X-PAD application type of Journaline, from FIG 0/13. -1 (the default) for none.
    void setJournalineApplicationType(int xpadAppType);

    // The PAD of one audio frame or AU. fpad points to the two F-PAD bytes (byte L-1, byte L).
    // The X-PAD ends at xpadEnd (exclusive) and is in transmitted (reversed) order; at most
    // xpadAvailable bytes before xpadEnd may belong to it.
    void process(const uint8_t *xpadEnd, int xpadAvailable, const uint8_t *fpad);

    // True once after each change of the label
    bool labelUpdated();
    const DABDynamicLabel& label() const { return m_label; }
    // MSC data groups for the MOT application completed so far; the caller clears it.
    std::vector<std::vector<uint8_t>>& motDataGroups() { return m_motDataGroups; }
    // Likewise for Journaline
    std::vector<std::vector<uint8_t>>& journalineDataGroups() { return m_journalineDataGroups; }

    int crcErrors() const { return m_crcErrors; }

private:
    enum Group
    {
        GroupNone,
        GroupLengthIndicator,
        GroupDynamicLabel,
        GroupMOT,
        GroupJournaline
    };

    struct Segments
    {
        int toggle = -1;
        int last = -1;
        int charset = 0;
        std::array<std::vector<uint8_t>, 8> data;
        std::array<bool, 8> received{};

        void clear();
        bool complete() const;
        std::vector<uint8_t> join() const;
    };

    int m_motAppType;
    int m_lastAppType;
    int m_lastXPadLength;      // Of an X-PAD continuing without contents indicators: the last data sub-field's
    // Data groups of different applications may interleave, so each has its own buffer
    std::array<std::vector<uint8_t>, 5> m_groupData;
    std::array<bool, 5> m_groupActive;
    int m_nextDataGroupLength;
    int m_motGroupLength;
    int m_journalineAppType;
    int m_journalineGroupLength;

    Segments m_labelSegments;
    Segments m_commandSegments;
    std::vector<uint8_t> m_labelBytes;
    int m_labelCharset;
    DABDynamicLabel m_label;
    bool m_labelUpdated;

    std::vector<std::vector<uint8_t>> m_motDataGroups;
    std::vector<std::vector<uint8_t>> m_journalineDataGroups;
    int m_crcErrors;

    int continuationOf(int appType) const;
    void subfield(int appType, const uint8_t *data, int length);
    void startGroup(Group group);
    void appendGroup(Group group, const uint8_t *data, int length);
    void dynamicLabelSegment(const std::vector<uint8_t>& segment);
    void dlPlusCommand(const std::vector<uint8_t>& command, int link);
};

} // namespace modemdab

#endif // MODEMDAB_DABPAD_H_
