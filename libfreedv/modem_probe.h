/*---------------------------------------------------------------------------*\

  FILE........: modem_probe.h
  AUTHOR......: Brady O'Brien
  DATE CREATED: 9 January 2016

  Library to easily extract debug traces from modems during development

\*---------------------------------------------------------------------------*/

/*
  Copyright (C) 2016 David Rowe

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

#ifndef __MODEMPROBE_H
#define __MODEMPROBE_H
#include <stdint.h>
#include <stdlib.h>
#include <complex.h>
#include "comp.h"

namespace FreeDV
{

#ifdef MODEMPROBE_ENABLE

/* Internal functions */
void modem_probe_init_int(char *modname, char *runname);
void modem_probe_close_int();

void modem_probe_samp_i_int(char * tracename,int samp[],size_t cnt);
void modem_probe_samp_f_int(char * tracename,float samp[],size_t cnt);
void modem_probe_samp_c_int(char * tracename,COMP samp[],size_t cnt);

/*
 * Init the probe library.
 * char *modname - Name of the modem under test
 * char *runname - Name/path of the file data is dumped to
 */
static inline void modem_probe_init(char *modname,char *runname){
        modem_probe_init_int(modname,runname);
}

/*
 * Dump traces to a file and clean up
 */
static inline void modem_probe_close(){
        modem_probe_close_int();
}

/*
 * Save some number of int samples to a named trace
 * char *tracename - name of trace being saved to
 * int samp[] - int samples
 * size_t cnt - how many samples to save
 */
static inline void modem_probe_samp_i(char *tracename,int samp[],size_t cnt){
        modem_probe_samp_i_int(tracename,samp,cnt);
}

/*
 * Save some number of float samples to a named trace
 * char *tracename - name of trace being saved to
 * float samp[] - int samples
 * size_t cnt - how many samples to save
 */
static inline void modem_probe_samp_f(char *tracename,float samp[],size_t cnt){
        modem_probe_samp_f_int(tracename,samp,cnt);
}

/*
 * Save some number of complex samples to a named trace
 * char *tracename - name of trace being saved to
 * COMP samp[] - int samples
 * size_t cnt - how many samples to save
 */
static inline void modem_probe_samp_c(char *tracename,COMP samp[],size_t cnt){
        modem_probe_samp_c_int(tracename,samp,cnt);
}

/*
 * Save some number of complex samples to a named trace
 * char *tracename - name of trace being saved to
 * float complex samp[] - int samples
 * size_t cnt - how many samples to save
 */
static inline void modem_probe_samp_cft(char *tracename,complex float samp[],size_t cnt){
        modem_probe_samp_c_int(tracename,(COMP*)samp,cnt);
}

#else

static inline void modem_probe_init(char *modname,char *runname){
        return;
}

static inline void modem_probe_close(){
        return;
}

static inline void modem_probe_samp_i(char *name,int samp[],size_t sampcnt){
        return;
}

static inline void modem_probe_samp_f(char *name,float samp[],size_t cnt){
        return;
}

static inline void modem_probe_samp_c(char *name,COMP samp[],size_t cnt){
        return;
}

static inline void modem_probe_samp_cft(char *name,complex float samp[],size_t cnt){
        return;
}

#endif

} // FreeDV

#endif
