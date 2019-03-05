/*---------------------------------------------------------------------------*\

  FILE........: machdep.h
  AUTHOR......: David Rowe
  DATE CREATED: May 2 2013

  Machine dependant functions, e.g. profiling that requires access to a clock
  counter register.

\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2013 David Rowe

  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License version 2.1, as
  published by the Free Software Foundation.  This program is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __MACHDEP__
#define __MACHDEP__

namespace FreeDV
{

#ifdef PROFILE
#define PROFILE_VAR(...) unsigned int __VA_ARGS__
#define PROFILE_SAMPLE(timestamp) timestamp = machdep_profile_sample()
#define PROFILE_SAMPLE_AND_LOG(timestamp, prev_timestamp, label) \
    timestamp = machdep_profile_sample_and_log(prev_timestamp, label)
#define PROFILE_SAMPLE_AND_LOG2(prev_timestamp, label) \
    machdep_profile_sample_and_log(prev_timestamp, label)
#else
#define PROFILE_VAR(...)
#define PROFILE_SAMPLE(timestamp)
#define PROFILE_SAMPLE_AND_LOG(timestamp, prev_timestamp, label)
#define PROFILE_SAMPLE_AND_LOG2(prev_timestamp, label)
#endif

void         machdep_profile_init(void);
void         machdep_profile_reset(void);
unsigned int machdep_profile_sample(void);
unsigned int machdep_profile_sample_and_log(unsigned int start, char s[]);
void         machdep_profile_print_logged_samples(void);

} // FreeDV

#endif
