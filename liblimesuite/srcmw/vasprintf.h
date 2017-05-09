/*
 * vasprintf.h
 *
 *  Created on: 9 May 2017
 *      Author: egriffiths
 */

#ifndef LIBLIMESUITE_SRCMW_VASPRINTF_H_
#define LIBLIMESUITE_SRCMW_VASPRINTF_H_


#define _GNU_SOURCE
#define __CRT__NO_INLINE

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

int my_vasprintf(char ** __restrict__ ret,
                      const char * __restrict__ format,
                      va_list ap) {
  int len;
  /* Get Length */
  len = _vsnprintf(NULL,0,format,ap);
  if (len < 0) return -1;
  /* +1 for \0 terminator. */
  *ret = (char *) malloc(len + 1);
  /* Check malloc fail*/
  if (!*ret) return -1;
  /* Write String */
  _vsnprintf(*ret,len+1,format,ap);
  /* Terminate explicitly */
  (*ret)[len] = '\0';
  return len;
}


#endif /* LIBLIMESUITE_SRCMW_VASPRINTF_H_ */
