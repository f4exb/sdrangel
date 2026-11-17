///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#ifndef MODEMRADIOPAGING_RADIOPAGINGCYRILLIC_H
#define MODEMRADIOPAGING_RADIOPAGINGCYRILLIC_H

// The Cyrillic letters of the codepages, UTF-8, by their Latin transliteration: uppercase U+0410-U+042F and
// U+0401, lowercase U+0430-U+044F and U+0451
namespace Cyr {

// Uppercase
constexpr const char *A = "\xD0\x90";    // U+0410
constexpr const char *B = "\xD0\x91";    // U+0411
constexpr const char *V = "\xD0\x92";    // U+0412
constexpr const char *G = "\xD0\x93";    // U+0413
constexpr const char *D = "\xD0\x94";    // U+0414
constexpr const char *E = "\xD0\x95";    // U+0415
constexpr const char *ZH = "\xD0\x96";   // U+0416
constexpr const char *Z = "\xD0\x97";    // U+0417
constexpr const char *I = "\xD0\x98";    // U+0418
constexpr const char *J = "\xD0\x99";    // U+0419 short I
constexpr const char *K = "\xD0\x9A";    // U+041A
constexpr const char *L = "\xD0\x9B";    // U+041B
constexpr const char *M = "\xD0\x9C";    // U+041C
constexpr const char *N = "\xD0\x9D";    // U+041D
constexpr const char *O = "\xD0\x9E";    // U+041E
constexpr const char *P = "\xD0\x9F";    // U+041F
constexpr const char *R = "\xD0\xA0";    // U+0420
constexpr const char *S = "\xD0\xA1";    // U+0421
constexpr const char *T = "\xD0\xA2";    // U+0422
constexpr const char *U = "\xD0\xA3";    // U+0423
constexpr const char *F = "\xD0\xA4";    // U+0424
constexpr const char *KH = "\xD0\xA5";   // U+0425
constexpr const char *TS = "\xD0\xA6";   // U+0426
constexpr const char *CH = "\xD0\xA7";   // U+0427
constexpr const char *SH = "\xD0\xA8";   // U+0428
constexpr const char *SHCH = "\xD0\xA9"; // U+0429
constexpr const char *HARD = "\xD0\xAA"; // U+042A hard sign
constexpr const char *Y = "\xD0\xAB";    // U+042B yeru
constexpr const char *SOFT = "\xD0\xAC"; // U+042C soft sign
constexpr const char *EH = "\xD0\xAD";   // U+042D
constexpr const char *YU = "\xD0\xAE";   // U+042E
constexpr const char *YA = "\xD0\xAF";   // U+042F
constexpr const char *YO = "\xD0\x81";   // U+0401
constexpr const char *DZE = "\xD0\x85";  // U+0405

// Uppercase of the other Slavic alphabets
constexpr const char *DJE = "\xD0\x82";  // U+0402
constexpr const char *GJE = "\xD0\x83";  // U+0403
constexpr const char *UKR_IE = "\xD0\x84";   // U+0404
constexpr const char *UKR_I = "\xD0\x86";    // U+0406
constexpr const char *YI = "\xD0\x87";   // U+0407
constexpr const char *LJE = "\xD0\x89";  // U+0409
constexpr const char *NJE = "\xD0\x8A";  // U+040A
constexpr const char *TSHE = "\xD0\x8B"; // U+040B
constexpr const char *KJE = "\xD0\x8C";  // U+040C
constexpr const char *SHORT_U = "\xD0\x8E";  // U+040E
constexpr const char *DZHE = "\xD0\x8F"; // U+040F
constexpr const char *GHE_UP = "\xD2\x90";   // U+0490 ghe with upturn
constexpr const char *I_GRAVE = "\xD0\x8D";  // U+040D

// Lowercase
constexpr const char *a = "\xD0\xB0";    // U+0430
constexpr const char *b = "\xD0\xB1";    // U+0431
constexpr const char *v = "\xD0\xB2";    // U+0432
constexpr const char *g = "\xD0\xB3";    // U+0433
constexpr const char *d = "\xD0\xB4";    // U+0434
constexpr const char *e = "\xD0\xB5";    // U+0435
constexpr const char *zh = "\xD0\xB6";   // U+0436
constexpr const char *z = "\xD0\xB7";    // U+0437
constexpr const char *i = "\xD0\xB8";    // U+0438
constexpr const char *j = "\xD0\xB9";    // U+0439 short i
constexpr const char *k = "\xD0\xBA";    // U+043A
constexpr const char *l = "\xD0\xBB";    // U+043B
constexpr const char *m = "\xD0\xBC";    // U+043C
constexpr const char *n = "\xD0\xBD";    // U+043D
constexpr const char *o = "\xD0\xBE";    // U+043E
constexpr const char *p = "\xD0\xBF";    // U+043F
constexpr const char *r = "\xD1\x80";    // U+0440
constexpr const char *s = "\xD1\x81";    // U+0441
constexpr const char *t = "\xD1\x82";    // U+0442
constexpr const char *u = "\xD1\x83";    // U+0443
constexpr const char *f = "\xD1\x84";    // U+0444
constexpr const char *kh = "\xD1\x85";   // U+0445
constexpr const char *ts = "\xD1\x86";   // U+0446
constexpr const char *ch = "\xD1\x87";   // U+0447
constexpr const char *sh = "\xD1\x88";   // U+0448
constexpr const char *shch = "\xD1\x89"; // U+0449
constexpr const char *hard = "\xD1\x8A"; // U+044A hard sign
constexpr const char *y = "\xD1\x8B";    // U+044B yeru
constexpr const char *soft = "\xD1\x8C"; // U+044C soft sign
constexpr const char *eh = "\xD1\x8D";   // U+044D
constexpr const char *yu = "\xD1\x8E";   // U+044E
constexpr const char *ya = "\xD1\x8F";   // U+044F
constexpr const char *yo = "\xD1\x91";   // U+0451
constexpr const char *dze = "\xD1\x95";  // U+0455

// Lowercase of the other Slavic alphabets
constexpr const char *dje = "\xD1\x92";  // U+0452
constexpr const char *gje = "\xD1\x93";  // U+0453
constexpr const char *ukr_ie = "\xD1\x94";   // U+0454
constexpr const char *ukr_i = "\xD1\x96";    // U+0456
constexpr const char *yi = "\xD1\x97";   // U+0457
constexpr const char *lje = "\xD1\x99";  // U+0459
constexpr const char *nje = "\xD1\x9A";  // U+045A
constexpr const char *tshe = "\xD1\x9B"; // U+045B
constexpr const char *kje = "\xD1\x9C";  // U+045C
constexpr const char *short_u = "\xD1\x9E";  // U+045E
constexpr const char *dzhe = "\xD1\x9F"; // U+045F
constexpr const char *ghe_up = "\xD2\x91";   // U+0491 ghe with upturn
constexpr const char *i_grave = "\xD1\x9D";  // U+045D

} // namespace Cyr

#endif // MODEMRADIOPAGING_RADIOPAGINGCYRILLIC_H
