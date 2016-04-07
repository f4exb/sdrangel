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

/*
 *  dstar interleave experiments 
 */

#ifndef _MAIN

extern const int dW[72];
extern const int dX[72];

#else
const int dW[72] = {

		// 0-11
		0, 0,
		3, 2,
		1, 1,
		0, 0,
		1, 1,
		0, 0,

		// 12-23
		3, 2,
		1, 1,
		3, 2,
		1, 1,
		0, 0,
		3, 2,

		// 24-35
		0, 0,
		3, 2,
		1, 1,
		0, 0,
		1, 1,
		0, 0,

		// 36-47
		3, 2,
		1, 1,
		3, 2,
		1, 1,
		0, 0,
		3, 2,

		// 48-59
		0, 0,
		3, 2,
		1, 1,
		0, 0,
		1, 1,
		0, 0,

		// 60-71
		3, 2,
		1, 1,
		3, 3,
		2, 1,
		0, 0,
		3, 3,
};
const int dX[72] = {

		// 0-11
		10, 22,
		11, 9,
		10, 22,
		11, 23,
		8, 20,
		9, 21,

		// 12-23
		10, 8,
		9, 21,
		8, 6,
		7, 19,
		8, 20,
		9, 7,

		// 24-35
		6, 18,
		7, 5,
		6, 18,
		7, 19,
		4, 16,
		5, 17,

		// 36-47
		6, 4,
		5, 17,
		4, 2,
		3, 15,
		4, 16,
		5, 3,

		// 48-59
		2, 14,
		3, 1,
		2, 14,
		3, 15,
		0, 12,
		1, 13,

		// 60-71
		2, 0,
		1, 13,
		0, 12,
		10, 11,
		0, 12,
		1, 13,
};

#endif
