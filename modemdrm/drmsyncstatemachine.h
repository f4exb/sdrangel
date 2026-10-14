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

#ifndef MODEMDRM_DRMSYNCSTATEMACHINE_H_
#define MODEMDRM_DRMSYNCSTATEMACHINE_H_

#include <cstdint>

#include "export.h"

namespace modemdrm
{

enum class SyncState
{
    Stopped,
    Searching,
    Acquiring,
    Locked,
    Reacquiring
};

class MODEMDRM_API SyncStateMachine
{
public:
    static constexpr int RequiredValidFACBlocks = 3;
    static constexpr int AllowedConsecutiveFACFailures = 5;

    SyncState state() const { return m_state; }
    std::uint64_t epoch() const { return m_epoch; }
    int consecutiveValidFACBlocks() const { return m_validFACBlocks; }
    int consecutiveFailedFACBlocks() const { return m_failedFACBlocks; }

    void start();
    void stop();
    void resetOrRetune();
    void candidateDetected();
    void candidateRejected();
    void facBlock(bool crcValid, bool coherentFrameProgression);
    void hardInputDiscontinuity();

    static const char* name(SyncState state);

private:
    void enterSearch(bool reacquiring, bool incrementEpoch);

    SyncState m_state = SyncState::Stopped;
    SyncState m_searchOrigin = SyncState::Searching;
    std::uint64_t m_epoch = 0;
    int m_validFACBlocks = 0;
    int m_failedFACBlocks = 0;
};

} // namespace modemdrm

#endif // MODEMDRM_DRMSYNCSTATEMACHINE_H_
