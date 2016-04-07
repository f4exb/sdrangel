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

extern const int pW[142];
extern const int pX[142];

#else
/*
 * ProVoice IMBE interleave schedule
 */

const int pW[142] = {
  0, 1, 2, 3, 4, 6,
  0, 1, 2, 3, 4, 6,
  0, 1, 2, 3, 4, 6,
  0, 1, 2, 3, 5, 6,
  0, 1, 2, 3, 5, 6,
  0, 1, 2, 3, 5, 6,
  0, 1, 3, 4, 5, 6,
  1, 2, 3, 4, 5, 6,
  0, 1, 2, 3, 4, 6,
  0, 1, 2, 3, 4, 6,
  0, 1, 2, 3, 4, 6,
  0, 1, 2, 3, 5, 6,
  0, 1, 2, 3, 5, 6,
  0, 1, 2, 3, 5, 6,
  1, 2, 3, 4, 5, 6,
  1, 2, 3, 4, 5,
  0, 1, 2, 3, 4, 6,
  0, 1, 2, 3, 4, 6,
  0, 1, 2, 3, 5, 6,
  0, 1, 2, 3, 5, 6,
  0, 1, 2, 3, 5, 6,
  0, 1, 2, 4, 5, 6,
  1, 2, 3, 4, 5, 6,
  1, 2, 3, 4, 6
};

const int pX[142] = {
  18, 18, 17, 16, 7, 21,
  15, 15, 14, 13, 4, 18,
  12, 12, 11, 10, 1, 15,
  9, 9, 8, 7, 13, 12,
  6, 6, 5, 4, 10, 9,
  3, 3, 2, 1, 7, 6,
  0, 0, 22, 13, 4, 3,
  21, 20, 19, 10, 1, 0,
  17, 17, 16, 15, 6, 20,
  14, 14, 13, 12, 3, 17,
  11, 11, 10, 9, 0, 14,
  8, 8, 7, 6, 12, 11,
  5, 5, 4, 3, 9, 8,
  2, 2, 1, 0, 6, 5,
  23, 22, 21, 12, 3, 2,
  20, 19, 18, 9, 0,
  16, 16, 15, 14, 5, 19,
  13, 13, 12, 11, 2, 16,
  10, 10, 9, 8, 14, 13,
  7, 7, 6, 5, 11, 10,
  4, 4, 3, 2, 8, 7,
  1, 1, 0, 14, 5, 4,
  22, 21, 20, 11, 2, 1,
  19, 18, 17, 8, 22
};
#endif
