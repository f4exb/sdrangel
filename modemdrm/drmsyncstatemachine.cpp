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

#include "drmsyncstatemachine.h"

namespace modemdrm
{

void SyncStateMachine::start()
{
    if (m_state == SyncState::Stopped) {
        enterSearch(false, true);
    }
}

void SyncStateMachine::stop()
{
    m_state = SyncState::Stopped;
    m_validFACBlocks = 0;
    m_failedFACBlocks = 0;
}

void SyncStateMachine::resetOrRetune()
{
    if (m_state != SyncState::Stopped) {
        enterSearch(false, true);
    }
}

void SyncStateMachine::candidateDetected()
{
    if (m_state == SyncState::Searching || m_state == SyncState::Reacquiring)
    {
        m_searchOrigin = m_state;
        m_state = SyncState::Acquiring;
        m_validFACBlocks = 0;
        m_failedFACBlocks = 0;
    }
}

void SyncStateMachine::candidateRejected()
{
    if (m_state == SyncState::Acquiring)
    {
        m_state = m_searchOrigin;
        m_validFACBlocks = 0;
        m_failedFACBlocks = 0;
    }
}

void SyncStateMachine::facBlock(bool crcValid, bool coherentFrameProgression)
{
    const bool valid = crcValid && coherentFrameProgression;

    if (m_state == SyncState::Acquiring)
    {
        if (!valid)
        {
            candidateRejected();
            return;
        }

        ++m_validFACBlocks;
        if (m_validFACBlocks >= RequiredValidFACBlocks)
        {
            m_state = SyncState::Locked;
            m_failedFACBlocks = 0;
        }
    }
    else if (m_state == SyncState::Locked)
    {
        if (valid) {
            m_failedFACBlocks = 0;
        } else if (++m_failedFACBlocks >= AllowedConsecutiveFACFailures) {
            enterSearch(true, true);
        }
    }
}

void SyncStateMachine::hardInputDiscontinuity()
{
    // Only losing an established lock starts a new epoch. A candidate under
    // validation returns to the search it came from; a search restarts in the
    // same epoch, since no signalling was valid to invalidate.
    switch (m_state)
    {
    case SyncState::Locked:
        enterSearch(true, true);
        break;
    case SyncState::Acquiring:
        candidateRejected();
        break;
    case SyncState::Searching:
    case SyncState::Reacquiring:
        m_validFACBlocks = 0;
        m_failedFACBlocks = 0;
        break;
    case SyncState::Stopped:
        break;
    }
}

const char* SyncStateMachine::name(SyncState state)
{
    switch (state)
    {
    case SyncState::Stopped:
        return "stopped";
    case SyncState::Searching:
        return "searching";
    case SyncState::Acquiring:
        return "acquiring";
    case SyncState::Locked:
        return "locked";
    case SyncState::Reacquiring:
        return "reacquiring";
    }

    return "stopped";
}

void SyncStateMachine::enterSearch(bool reacquiring, bool incrementEpoch)
{
    m_state = reacquiring ? SyncState::Reacquiring : SyncState::Searching;
    m_searchOrigin = m_state;
    m_validFACBlocks = 0;
    m_failedFACBlocks = 0;
    if (incrementEpoch) {
        ++m_epoch;
    }
}

} // namespace modemdrm
