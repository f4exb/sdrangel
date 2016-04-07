/*
 * Copyright (C) 2010 DSD Author
 * GPG Key ID: 0x3F1D7FD0 (74EF 430D F7F2 0A48 FCE6  F630 FAA2 635D 3F1D 7FD0)
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _MAIN

extern const int iW[72];
extern const int iX[72];
extern const int iY[72];
extern const int iZ[72];

#else
/*
 * P25 Phase1 IMBE interleave schedule
 */

const int iW[72] = {
  0, 2, 4, 1, 3, 5,
  0, 2, 4, 1, 3, 6,
  0, 2, 4, 1, 3, 6,
  0, 2, 4, 1, 3, 6,
  0, 2, 4, 1, 3, 6,
  0, 2, 4, 1, 3, 6,
  0, 2, 5, 1, 3, 6,
  0, 2, 5, 1, 3, 6,
  0, 2, 5, 1, 3, 7,
  0, 2, 5, 1, 3, 7,
  0, 2, 5, 1, 4, 7,
  0, 3, 5, 2, 4, 7
};

const int iX[72] = {
  22, 20, 10, 20, 18, 0,
  20, 18, 8, 18, 16, 13,
  18, 16, 6, 16, 14, 11,
  16, 14, 4, 14, 12, 9,
  14, 12, 2, 12, 10, 7,
  12, 10, 0, 10, 8, 5,
  10, 8, 13, 8, 6, 3,
  8, 6, 11, 6, 4, 1,
  6, 4, 9, 4, 2, 6,
  4, 2, 7, 2, 0, 4,
  2, 0, 5, 0, 13, 2,
  0, 21, 3, 21, 11, 0
};

const int iY[72] = {
  1, 3, 5, 0, 2, 4,
  1, 3, 6, 0, 2, 4,
  1, 3, 6, 0, 2, 4,
  1, 3, 6, 0, 2, 4,
  1, 3, 6, 0, 2, 4,
  1, 3, 6, 0, 2, 5,
  1, 3, 6, 0, 2, 5,
  1, 3, 6, 0, 2, 5,
  1, 3, 6, 0, 2, 5,
  1, 3, 7, 0, 2, 5,
  1, 4, 7, 0, 3, 5,
  2, 4, 7, 1, 3, 5
};

const int iZ[72] = {
  21, 19, 1, 21, 19, 9,
  19, 17, 14, 19, 17, 7,
  17, 15, 12, 17, 15, 5,
  15, 13, 10, 15, 13, 3,
  13, 11, 8, 13, 11, 1,
  11, 9, 6, 11, 9, 14,
  9, 7, 4, 9, 7, 12,
  7, 5, 2, 7, 5, 10,
  5, 3, 0, 5, 3, 8,
  3, 1, 5, 3, 1, 6,
  1, 14, 3, 1, 22, 4,
  22, 12, 1, 22, 20, 2
};
#endif
