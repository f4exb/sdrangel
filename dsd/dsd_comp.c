/*
 * dsd_comp.c
 *
 *  Created on: Apr 8, 2016
 *      Author: f4exb
 */

#include "dsd_comp.h"

int comp(const void *a, const void *b)
{
    if (*((const int *) a) == *((const int *) b))
        return 0;
    else if (*((const int *) a) < *((const int *) b))
        return -1;
    else
        return 1;
}

