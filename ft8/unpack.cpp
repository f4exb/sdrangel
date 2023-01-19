///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// This is the code from ft8mon: https://github.com/rtmrtmrtmrtm/ft8mon          //
// written by Robert Morris, AB1HL                                               //
// reformatted and adapted to Qt and SDRangel context                            //
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
#include <string>
#include <assert.h>

#include "unpack.h"
#include "unpack0.h"
#include "util.h"

namespace FT8 {

int Packing::ihashcall(std::string rawcall, int m)
{
    std::string call = trim(rawcall);
    const char *chars = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/";

    while (call.size() < 11) {
        call += " ";
    }

    unsigned long long x = 0;

    for (int i = 0; i < 11; i++)
    {
        int c = call[i];
        const char *p = strchr(chars, c);
        assert(p);
        int j = p - chars;
        x = 38 * x + j;
    }

    x = x * 47055833459LL;
    x = x >> (64 - m);

    return x;
}

//
// turn 28 bits of packed call into the call
//
std::string Packing::unpackcall(int x)
{
    char tmp[64];

    const char *c1 = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const char *c2 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const char *c3 = "0123456789";
    const char *c4 = " ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    if (x == 0) {
        return "DE";
    }

    if (x == 1) {
        return "QRZ";
    }

    if (x == 2) {
        return "CQ";
    }

    if (x <= 1002)
    {
        sprintf(tmp, "CQ %d", x - 3);
        return std::string(tmp);
    }

    if (x <= 532443)
    {
        x -= 1003;
        int ci1 = x / (27 * 27 * 27);
        x %= 27 * 27 * 27;
        int ci2 = x / (27 * 27);
        x %= 27 * 27;
        int ci3 = x / 27;
        x %= 27;
        int ci4 = x;
        sprintf(tmp, "CQ %c%c%c%c", c4[ci1], c4[ci2], c4[ci3], c4[ci4]);
        return std::string(tmp);
    }

    if (x < NTOKENS) {
        return "<TOKEN>";
    }

    x -= NTOKENS;

    if (x < MAX22)
    {
        // 22-bit hash...
        std::string s;
        hashes_mu.lock();

        if (hashes22.count(x) > 0) {
            s = hashes22[x];
        } else {
            s = "<...22>";
        }

        hashes_mu.unlock();
        return s;
    }

    x -= MAX22;

    char a[7];

    a[5] = c4[x % 27];
    x = x / 27;
    a[4] = c4[x % 27];
    x = x / 27;
    a[3] = c4[x % 27];
    x = x / 27;
    a[2] = c3[x % 10];
    x = x / 10;
    a[1] = c2[x % 36];
    x = x / 36;
    a[0] = c1[x];

    a[6] = '\0';

    return std::string(a);
}

// unpack a 15-bit grid square &c.
// 77-bit version, from inspection of packjt77.f90.
// ir is the bit after the two 28+1-bit callee/caller.
// i3 is the message type, usually 1.
std::string Packing::unpackgrid(int ng, int ir, int i3)
{
    (void) i3;

    if (ng < NGBASE)
    {
        // maidenhead grid system:
        //   latitude from south pole to north pole.
        //   longitude eastward from anti-meridian.
        //   first: 20 degrees longitude.
        //   second: 10 degrees latitude.
        //   third: 2 degrees longitude.
        //   fourth: 1 degree latitude.
        // so there are 18*18*10*10 possibilities.
        int x1 = ng / (18 * 10 * 10);
        ng %= 18 * 10 * 10;
        int x2 = ng / (10 * 10);
        ng %= 10 * 10;
        int x3 = ng / 10;
        ng %= 10;
        int x4 = ng;
        char tmp[5];
        tmp[0] = 'A' + x1;
        tmp[1] = 'A' + x2;
        tmp[2] = '0' + x3;
        tmp[3] = '0' + x4;
        tmp[4] = '\0';
        return tmp;
    }

    ng -= NGBASE;

    if (ng == 1) {
        return "   "; // ???
    }
    if (ng == 2) {
        return "RRR ";
    }
    if (ng == 3) {
        return "RR73";
    }
    if (ng == 4) {
        return "73  ";
    }

    int db = ng - 35;
    char tmp[16];

    if (db >= 0) {
        sprintf(tmp, "%s+%02d", ir ? "R" : "", db);
    } else {
        sprintf(tmp, "%s-%02d", ir ? "R" : "", 0 - db);
    }

    return std::string(tmp);
}

void Packing::remember_call(std::string call)
{
    hashes_mu.lock();

    if (call.size() >= 3 && call[0] != '<')
    {
        hashes22[ihashcall(call, 22)] = call;
        hashes12[ihashcall(call, 12)] = call;
    }

    hashes_mu.unlock();
}

//
// i3 == 4
// a call that doesn't fit in 28 bits.
// 12 bits: hash of a previous call
// 58 bits: 11 characters
// 1 bit: swap
// 2 bits: 1 RRR, 2 RR73, 3 73
// 1 bit: 1 means CQ
std::string Packing::unpack_4(int a77[], std::string& call1str, std::string& call2str, std::string& locstr)
{
    (void) locstr;
    // 38 possible characters:
    const char *chars = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ/";
    long long n58 = un64(a77, 12, 58);
    char call[16];

    for (int i = 0; i < 11; i++)
    {
        call[10 - i] = chars[n58 % 38];
        n58 = n58 / 38;
    }

    call[11] = '\0';
    std::string callstr(call);

    remember_call(callstr);

    if (un64(a77, 73, 1) == 1)
    {
        call1str = std::string("CQ ") + callstr;
        return call1str;
    }

    int x12 = un64(a77, 0, 12);
    // 12-bit hash
    hashes_mu.lock();
    std::string ocall;

    if (hashes12.count(x12) > 0) {
        ocall = hashes12[x12];
    } else {
        ocall = "<...12>";
    }

    hashes_mu.unlock();
    int swap = un64(a77, 70, 1);
    std::string msg;

    if (swap)
    {
        msg = std::string(call) + " " + ocall;
        call1str = call;
        call2str = ocall;
    }
    else
    {
        msg = std::string(ocall) + " " + call;
        call1str = ocall;
        call2str = call;
    }

    int suffix = un64(a77, 71, 2);

    if (suffix == 1)
    {
        locstr = " RRR";
    } else if (suffix == 2) {
        locstr = " RR73";
    } else if (suffix == 3) {
        locstr = " 73";
    }

    msg += locstr;
    return msg;
}

//
// i3=1
//
std::string Packing::unpack_1(int a77[], std::string& call1str, std::string& call2str, std::string& locstr)
{
    // type 1:
    // 28 call1
    // 1 P/R
    // 28 call2
    // 1 P/R
    // 1 ???
    // 15 grid
    // 3 type

    int i = 0;
    int call1 = un64(a77, i, 28);
    i += 28;
    int rover1 = a77[i];
    i += 1;
    int call2 = un64(a77, i, 28);
    i += 28;
    int rover2 = a77[i];
    i += 1;
    int ir = a77[i];
    i += 1;
    int grid = un64(a77, i, 15);
    i += 15;
    int i3 = un64(a77, i, 3);
    i += 3;
    assert((i3 == 1 || i3 == 2) && i == 77);

    call1str = trim(unpackcall(call1));
    call2str = trim(unpackcall(call2));
    locstr = unpackgrid(grid, ir, i3);

    remember_call(call1str);
    remember_call(call2str);

    const std::string pr = (i3 == 1 ? "/R" : "/P");

    return call1str + (rover1 ? pr : "") + " " + call2str + (rover2 ? pr : "") + " " + locstr;
}

// free text
// 71 bits, 13 characters, each one of 42 choices.
// reversed.
// details from wsjt-x's packjt77.f90
std::string Packing::unpack_0_0(int a77[], std::string& call1str, std::string& call2str, std::string& locstr)
{
    (void) call2str;
    (void) locstr;
    // the 42 possible characters.
    const char *cc = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ+-./?";
    boost::multiprecision::int128_t x = un128(a77, 0, 71);
    std::string msg = "0123456789123";

    for (int i = 0; i < 13; i++)
    {
        msg[13 - 1 - i] = cc[(int) (x % 42)];
        x = x / 42;
    }

    call1str = msg;
    return msg;
}

// ARRL RTTY Round-Up states/provinces
const char *Packing::ru_states[] = {
    "AL", "AK", "AZ", "AR", "CA", "CO", "CT", "DE", "FL", "GA",
    "HI", "ID", "IL", "IN", "IA", "KS", "KY", "LA", "ME", "MD",
    "MA", "MI", "MN", "MS", "MO", "MT", "NE", "NV", "NH", "NJ",
    "NM", "NY", "NC", "ND", "OH", "OK", "OR", "PA", "RI", "SC",
    "SD", "TN", "TX", "UT", "VT", "VA", "WA", "WV", "WI", "WY",
    "NB", "NS", "QC", "ON", "MB", "SK", "AB", "BC", "NWT", "NF",
    "LB", "NU", "YT", "PEI", "DC"};

// i3=3
// 3     TU; W9XYZ K1ABC R 579 MA           1 28 28 1 3 13   74   ARRL RTTY Roundup
//  1 TU
// 28 call1
// 28 call2
//  1 R
//  3 RST 529 to 599
// 13 state/province/serialnumber
std::string Packing::unpack_3(int a77[], std::string& call1str, std::string& call2str, std::string& locstr)
{
    (void) locstr;
    int i = 0;
    int tu = a77[i];
    i += 1;
    int call1 = un64(a77, i, 28);
    i += 28;
    int call2 = un64(a77, i, 28);
    i += 28;
    int r = a77[i];
    i += 1;
    int rst = un64(a77, i, 3);
    i += 3;
    int serial = un64(a77, i, 13);
    i += 13;

    call1str = trim(unpackcall(call1));
    call2str = trim(unpackcall(call2));

    rst = 529 + 10 * rst;

    int statei = serial - 8001;
    std::string serialstr;
    int nstates = sizeof(ru_states) / sizeof(ru_states[0]);

    if (serial > 8000 && statei < nstates)
    {
        serialstr = ru_states[statei];
    }
    else
    {
        char tmp[32];
        sprintf(tmp, "%04d", serial);
        serialstr = std::string(tmp);
    }

    std::string msg;

    if (tu) {
        msg += "TU; ";
    }

    msg += call1str + " " + call2str + " ";

    if (r)
    {
        msg += "R ";
    }
    {
        char tmp[16];
        sprintf(tmp, "%d ", rst);
        msg += std::string(tmp);
    }

    msg += serialstr;

    remember_call(call1str);
    remember_call(call2str);

    return msg;
}

// ARRL Field Day sections
const char *Packing::sections[] = {
    "AB ", "AK ", "AL ", "AR ", "AZ ", "BC ", "CO ", "CT ", "DE ", "EB ",
    "EMA", "ENY", "EPA", "EWA", "GA ", "GTA", "IA ", "ID ", "IL ", "IN ",
    "KS ", "KY ", "LA ", "LAX", "MAR", "MB ", "MDC", "ME ", "MI ", "MN ",
    "MO ", "MS ", "MT ", "NC ", "ND ", "NE ", "NFL", "NH ", "NL ", "NLI",
    "NM ", "NNJ", "NNY", "NT ", "NTX", "NV ", "OH ", "OK ", "ONE", "ONN",
    "ONS", "OR ", "ORG", "PAC", "PR ", "QC ", "RI ", "SB ", "SC ", "SCV",
    "SD ", "SDG", "SF ", "SFL", "SJV", "SK ", "SNJ", "STX", "SV ", "TN ",
    "UT ", "VA ", "VI ", "VT ", "WCF", "WI ", "WMA", "WNY", "WPA", "WTX",
    "WV ", "WWA", "WY ", "DX "};

// i3 = 0, n3 = 3 or 4: ARRL Field Day
// 0.3   WA9XYZ KA1ABC R 16A EMA            28 28 1 4 3 7    71   ARRL Field Day
// 0.4   WA9XYZ KA1ABC R 32A EMA            28 28 1 4 3 7    71   ARRL Field Day
std::string Packing::unpack_0_3(int a77[], int n3, std::string& call1str, std::string& call2str, std::string& locstr)
{
    (void) locstr;
    int i = 0;
    int call1 = un64(a77, i, 28);
    i += 28;
    int call2 = un64(a77, i, 28);
    i += 28;
    int R = un64(a77, i, 1);
    i += 1;
    int n_transmitters = un64(a77, i, 4);

    if (n3 == 4) {
        n_transmitters += 16;
    }

    i += 4;
    int clss = un64(a77, i, 3); // class
    i += 3;
    int section = un64(a77, i, 7); // ARRL section
    i += 7;

    std::string msg;
    call1str = trim(unpackcall(call1));
    msg += call1str;
    msg += " ";
    call2str = trim(unpackcall(call2));
    msg += call2str;
    msg += " ";

    if (R) {
        msg += "R ";
    }

    {
        char tmp[16];
        sprintf(tmp, "%d%c ", n_transmitters + 1, clss + 'A');
        msg += std::string(tmp);
    }

    if (section - 1 >= 0 && section - 1 < (int)(sizeof(sections) / sizeof(sections[0]))) {
        msg += sections[section - 1];
    }

    return msg;
}

//
// unpack an FT8 message.
// a77 is 91 bits -- 77 plus the 14-bit CRC.
// CRC and LDPC have already been checked.
// details from wsjt-x's packjt77.f90 and 77bit.txt.
//
std::string Packing::unpack(int a77[], std::string& call1, std::string& call2, std::string& loc)
{
    int i3 = un64(a77, 74, 3);
    int n3 = un64(a77, 71, 3);

    if (i3 == 0 && n3 == 0)
    {
        // free text
        return unpack_0_0(a77, call1, call2, loc);
    }

    if (i3 == 0 && (n3 == 3 || n3 == 4))
    {
        // ARRL Field Day
        return unpack_0_3(a77, n3, call1, call2, loc);
    }

    if (i3 == 1 || i3 == 2)
    {
        // ordinary message
        return unpack_1(a77, call1, call2, loc);
    }

    if (i3 == 3)
    {
        // RTTY Round-Up
        return unpack_3(a77, call1, call2, loc);
    }

    if (i3 == 4)
    {
        // call that doesn't fit in 28 bits
        return unpack_4(a77, call1, call2, loc);
    }

    char tmp[64];
    call1 = "UNK";
    sprintf(tmp, "UNK i3=%d n3=%d", i3, n3);
    return std::string(tmp);
}

} // namespace FT8
