// Frames of a reference transmitter for every message type, with the messages sent and the messages
// a reference receiver decodes from them (cap TAG|text, tags ALN SEC NUM SNUM NNUM NSNUM HEX SMSG TON).
// Frames of one scenario are decoded in order by one decoder. mode: 0 1600/2, 1 3200/2, 2 3200/4, 3 6400/4.
// expected: capcode, type name, text, message number (-2 any), complete, fragments (0 any).
{
	"types", "idle", 0, 0, 0, 0,
		"aaaaaaaa78f359395555870ca6c6f0000283",
		"aed845127baaaa2aaa2a2a2a2a2a2aaa2a2a2a2a2a2a2a2a2a2a2aaa2aaa2aaa"
		"aa2aaaaa2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	0, {},
	0, {}
},
{
	"types", "alpha", 0, 0, 1, 10,
		"aaaaaaaa78f359395555870ca6c62850065a",
		"aed845127bd8d9e434311f3d6222574d92551f6e166f46001f4f47ae3d8d0221"
		"82c613224eaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1234567ULL, "alphanumeric", "Hello World", 5, 1, 0}},
	1, {"0001234567 ALN|Hello World"}
},
{
	"types", "alpha one char", 0, 0, 1, 11,
		"aaaaaaaa78f359395555870ca6c6c8d004e5",
		"aed845127bb28aca2a2a4a622a22120292120a1a6a020202120262ca1ae2424a"
		"8af25a3a6aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{100ULL, "alphanumeric", "A", 2, 1, 0}},
	1, {"0000000100 ALN|A"}
},
{
	"types", "alpha two chars", 0, 0, 1, 12,
		"aaaaaaaa78f359395555870ca6c648300606",
		"aed845127be282da3a2a4a7a2a32020292121a127a121212120a229252ea0a1a"
		"82b2423222aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{101ULL, "alphanumeric", "AB", 63, 1, 0}},
	1, {"0000000101 ALN|AB"}
},
{
	"types", "alpha max", 0, 0, 1, 13,
		"aaaaaaaa78f359395555870ca6c688b00562",
		"aed845127ba2b49a213f1738353602019f1f100b0236406e5f6100a831a16523"
		"94b9227d0e55994b3807ffffaacc5a3907ffff5566d2310fffff2ee8212cb950"
		"5deeb28fc92acc259c8380ff55662d1c03807faa3369180700ff9774ef9623a8"
		"d188d9c71badc99b40f83fc712241289b87f87ad361289f87f877607495b093f"
		"2ace2aff36569849c43cfcffa9ce59c43cfcff5263d1c838fcfbdfdbdf1d4f9b"
		"62e0b98b0caacca4621f01fe55662ce21f01fea83168e41d03fc6eee128f5acf"
		"cd8e5ec6f9b5d94b27e0ff1f6a4cda06c1ff3f9526924ec1ff3fd9e7df236f01"
		"1bc63a6eb04a8c4523e0f0efb5c64d23e0e0ff4a73c927e0e0ff3c1320c1a8b0"
		"b2fcad8727a2c4a291770ff055662211ff0ff0ab326610fe0ff1988069e92dd3"
		"2c895a4de655994b3807ffffaacc5a3907ffff5566d2310fffff2ee8212cb950"
		"5deeb28fc92acc249c8280fe54662c1c02807eaa3268180600fe9674ee9622a8"
		"d088d8c61a",
	1, {{1933312ULL, "alphanumeric", "0123456789abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRS", 9, 1, 0}},
	1, {"0001933312 ALN|0123456789abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRS"}
},
{
	"types", "alpha mail drop", 0, 0, 1, 14,
		"aaaaaaaa78f359395555870ca6c60870066f",
		"aed845127b9488f25471177778721c4c921f1e3b623303001f1b25f34ddd6d1d"
		"f6be614151ea2aaa2a2aeaeaeaea6a2aaaeaeaeaaa6a2aaaeaeaaaaaaa6a2aea"
		"6a6a2a6a6aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1500ULL, "alphanumeric", "mail drop message", 11, 1, 0}},
	1, {"0000001500 ALN|mail drop message"}
},
{
	"types", "numeric", 0, 0, 2, 20,
		"aaaaaaaa78f359395555870ca6c614280445",
		"aed845127b92baa24a22625a6a624a0a9a020232520a0a02223a4282729a726a"
		"dab26a3a7aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1000ULL, "numeric", "0123456789", -2, 1, 0}},
	1, {"0000001000 NUM|0123456789"}
},
{
	"types", "numeric all chars", 0, 0, 2, 21,
		"aaaaaaaa78f359395555870ca6c6e4a8017e",
		"aed845127bfe929e6e2e6e427e624a0e8e1a121e760a3a16061a02ea168a2a0e"
		"c6ee6a1616aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1001ULL, "numeric", "12 U-]5[.9-U", -2, 1, 0}},
	1, {"0000001001 NUM|12 U-]5[.9-U   "}
},
{
	"types", "numeric one digit", 0, 0, 2, 22,
		"aaaaaaaa78f359395555870ca6c664680273",
		"aed845127b8afa9a7a3a6a4a6a7a5a0a8a1a1a0a4a1a1a0a2a2a5afa0afa7a0a"
		"8a9a3a1a1aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1002ULL, "numeric", "7", -2, 1, 0}},
	1, {"0000001002 NUM|7   "}
},
{
	"types", "numeric max", 0, 0, 2, 23,
		"aaaaaaaa78f359395555870ca6c6a4e80117",
		"aed845127be6e2b86b226e497964441292090827603d203403310ba369ef227a"
		"a8ec2010542aea8a8a4a4a2a2a8a8a6a0aca0a4a2a0aca4a0a4aea8a6a0a4aea"
		"4a2a6aea8aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1003ULL, "numeric", "12345678901234567890123456789012345678901", -2, 1, 0}},
	1, {"0000001003 NUM|12345678901234567890123456789012345678901"}
},
{
	"types", "special numeric", 0, 0, 2, 24,
		"aaaaaaaa78f359395555870ca6c62418046b",
		"aed845127b92aafa421242726a7242128a1a02324a0a02023a0a42922aea126a"
		"aaca5a7a12aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1004ULL, "special_numeric", "5551234", -2, 1, 0}},
	1, {"0000001004 SNUM|5551234   "}
},
{
	"types", "numbered numeric", 0, 0, 2, 25,
		"aaaaaaaa78f359395555870ca6c6c49806d4",
		"aed845127bf2b2fa5a326a626a7242128a021a2a420212321a1a22fa4aca5212"
		"82b2527242aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1005ULL, "numbered_numeric", "9876543", 7, 1, 0}},
	1, {"0000001005 NNUM|9876543 "}
},
{
	"types", "numbered special numeric", 0, 0, 2, 26,
		"aaaaaaaa78f359395555870ca6c6445805d9",
		"aed845127ba2f2fa62226a6a72725a1a820202324a0a02223a0a4aea4ab23a2a"
		"e2ea1a4a5aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1006ULL, "numbered_special_numeric", "112233", 33, 1, 0}},
	1, {"0000001006 NSNUM|112233  "}
},
{
	"types", "tone", 0, 0, 3, 30,
		"aaaaaaaa78f359395555870ca6c6bc780576",
		"aed845127baaeaaa2a6a2a2a6a2a2a2aaa6a2a2a6a2a2a2a2a2a6aea6aea2a2a"
		"aaea2a2a2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{4242ULL, "tone", "", -2, 1, 0}},
	1, {"0000004242 TON|"}
},
{
	"types", "secure", 0, 0, 3, 31,
		"aaaaaaaa78f359395555870ca6c63cf805a7",
		"aed845127b8d99b4001d0f4d2b255a42951f0f0f480620000f0f24cf5ceb3600"
		"89f9402d7a8a8aea0a6aeaeaea2aca0a0acaca2a2a0a4a4aca4a6a2a0a2aea4a"
		"ca8aca8acaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1600ULL, "secure", "secure message text", -2, 1, 0}},
	1, {"0000001600 SEC|secure message text"}
},
{
	"types", "secure registration ack", 0, 0, 3, 32,
		"aaaaaaaa78f359395555870ca6c65c040441",
		"aed845127bed848f180a0f4726285344981f0f2d472800060e0e57b91eea5451"
		"a584462839aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1601ULL, "secure", "=registered", -2, 1, 0}},
	1, {"0000001601 SEC|=registered"}
},
{
	"binary", "hex 8 digits", 0, 0, 4, 1,
		"aaaaaaaa78f359395555870ca6c6528000cc",
		"aed845127baea2f6240276367e2256429404181c5c3c04000c0076e23a9c3442"
		"dccc181470aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1700ULL, "binary", "DEADBEEF", 3, 1, 0}},
	1, {"0000001700 HEX|DEADBEEF00"}
},
{
	"binary", "hex 1 digit", 0, 0, 4, 2,
		"aaaaaaaa78f359395555870ca6c692400074",
		"aed845127bca86f20612622a7222525292021a326a021a0a0a0a368a7a9a520e"
		"d68a0a3a1eaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1701ULL, "binary", "A", 4, 1, 0}},
	1, {"0000001701 HEX|A0"}
},
{
	"binary", "hex 21 digits", 0, 0, 4, 3,
		"aaaaaaaa78f359395555870ca6c612c000a5",
		"aed845127ba8e3e9090e6b286233574190051d1273211107010f4ab145dd1924"
		"ade7742031aa2a2a2a2aaa2a2a2a2aaa2a2aaaaa2a2a2a2aaa2a2aaaaaaa2aaa"
		"aa2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1702ULL, "binary", "0123456789ABCDEF01234", 5, 1, 0}},
	1, {"0000001702 HEX|0123456789ABCDEF01234F"}
},
{
	"binary", "hex 16 bit blocks", 0, 0, 4, 4,
		"aaaaaaaa78f359395555870ca6c6e22007ac",
		"aed845127bf8f0c0321362337327574797051d354d3b1901030b04f93bd77979"
		"81b27f3449aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1703ULL, "binary", "00FF00FF1234", 6, 1, 0}},
	1, {"0000001703 HEX|00FF00FF1234FF"}
},
{
	"binary", "hex 0000", 0, 0, 4, 5,
		"aaaaaaaa78f359395555870ca6c662a0077d",
		"aed845127b9a82b2520a72327232425292021a3a7a1e1e0e0e0e2696329a4e52"
		"969a46761eaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1704ULL, "binary", "0000", 7, 1, 0}},
	1, {"0000001704 HEX|0000FF"}
},
{
	"short", "short digits", 0, 0, 5, 1,
		"aaaaaaaa78f359395555870ca6c69a8007d2",
		"aed845127baa8a8a4a0a2a0a0a4a6a4a8a0a0a2a4a0a2a2a0a0a0aaa0aca4a6a"
		"caaa6a2a2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1800ULL, "short_message", "123", -2, 1, 0}},
	1, {"0000001800 SMSG|123"}
},
{
	"short", "short digits partial", 0, 0, 5, 2,
		"aaaaaaaa78f359395555870ca6c61a4004df",
		"aed845127bcaaaaa6a0a2a0a0a4a6a4a8a0a0a2a4a0a0a0a2a2a4aca4aea2a2a"
		"caea6a2a4aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1801ULL, "short_message", "12 ", -2, 1, 0}},
	1, {"0000001801 SMSG|12 "}
},
{
	"short", "short source", 0, 0, 5, 3,
		"aaaaaaaa78f359395555870ca6c6eac001e4",
		"aed845127baaea8a6a0a2a0a2a4a6a4aaa0a0a0a4a0a0a0a0a0a0aca4a8a4a2a"
		"aa8a0a0a6aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1802ULL, "short_message", "", -2, 1, 0}},
	0, {}
},
{
	"short", "short numbered", 0, 0, 5, 4,
		"aaaaaaaa78f359395555870ca6c66a200307",
		"aed845127bcaca8a4a0a2a0a0a6a6a6a8a2a0a0a4a2a0a2a0a0a6aaa2a8a2a4a"
		"aaea2a0a0aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1803ULL, "short_message", "", 17, 1, 0}},
	0, {}
},
{
	"short", "short long address", 0, 0, 5, 5,
		"aaaaaaaa78f359395555870ca6c6aaa00063",
		"aed845127b8af22a2222322a222a2aa2a2323a32322a2232322212da42ea02da"
		"7a8afa22daaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{2101250ULL, "short_message", "87654321", -2, 1, 0}},
	1, {"0002101250 SMSG|87654321"}
},
{
	"short", "instruction event", 0, 0, 5, 6,
		"aaaaaaaa78f359395555870ca6c62a60036e",
		"aed845127bcaeaca6a6a0a0a2a0a0a2aea4a4a4a0a4a4a4a4a4a4a8a0aca4a6a"
		"8a8a0a0a2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{2029599ULL, "short_instruction", "", -2, 1, 0}},
	0, {}
},
{
	"long", "alpha set 0", 0, 0, 4, 20,
		"aaaaaaaa78f359395555870ca6c662280463",
		"aed845127bc9be3e2e342b3b3a2237a4a82f2f24342534202f2542cf5ba851d4"
		"2dcab136d05ec22a22c2fefe8e0e86022afeaede8a16328afefe4afa3a06126e"
		"a6b6f2a666aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{2101249ULL, "alphanumeric", "long address alpha message", 1, 1, 0}},
	1, {"0002101249 ALN|long address alpha message"}
},
{
	"long", "alpha one char set 0", 0, 0, 4, 30,
		"aaaaaaaa78f359395555870ca6c632780411",
		"aed845127b86e62e3a3a2a323a223aa6ae2e2e26362222222a221ed256f606c6"
		"6a8afe3aceaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{2101250ULL, "alphanumeric", "x", 1, 1, 0}},
	1, {"0002101250 ALN|x"}
},
{
	"long", "numeric set 0", 0, 0, 4, 40,
		"aaaaaaaa78f359395555870ca6c682140688",
		"aed845127bd2b02c243036243c2232a8a8242422322c303a223846d246a440dc"
		"26c4b038c8aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{2101249ULL, "numeric", "1234567890123", -2, 1, 0}},
	1, {"0002101249 NUM|1234567890123  "}
},
{
	"long", "numeric one word set 0", 0, 0, 4, 50,
		"aaaaaaaa78f359395555870ca6c6624c036a",
		"aed845127bd2aa2a223232222a2232aaaa222222222a2222323252ca4aaa52ca"
		"32daa232daaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{2101249ULL, "numeric", "1234", -2, 1, 0}},
	1, {"0002101249 NUM|1234"}
},
{
	"long", "tone set 0", 0, 0, 4, 60,
		"aaaaaaaa78f359395555870ca6c6323c02f6",
		"aed845127bcaaa2a2a2a2a2a2a2a2aaaaa2a2a2a2a2a2a2a2a2a4aca4aaa4aca"
		"2acaaa2acaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{2101249ULL, "tone", "", -2, 1, 0}},
	1, {"0002101249 TON|"}
},
{
	"long", "special numeric set 0", 0, 0, 4, 70,
		"aaaaaaaa78f359395555870ca6c6826203ea",
		"aed845127bcaa22a26262a3236263aaaae2e22322e2e223a363e5ad25aba4ace"
		"22c6b636d2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{2101249ULL, "special_numeric", "98765", -2, 1, 0}},
	1, {"0002101249 SNUM|98765     "}
},
{
	"long", "numbered numeric set 0", 0, 0, 4, 80,
		"aaaaaaaa78f359395555870ca6c6620a0079",
		"aed845127bc2a632223a3a36322a36aea22a223a262e323a262e5ac656b64ada"
		"32d6aa22daaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{2101249ULL, "numbered_numeric", "555123", 12, 1, 0}},
	1, {"0002101249 NNUM|555123  "}
},
{
	"long", "hex set 0", 0, 0, 4, 90,
		"aaaaaaaa78f359395555870ca6c6325a000b",
		"aed845127bcdb82b3b29363532233ba3aa202a3a27362a202a254ac24aac50da"
		"2fc6a231d62a2aaaaa2a2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa2a2aaaaa"
		"2aaa2a2a2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{2101249ULL, "binary", "CAFEBABE0123", 20, 1, 0}},
	1, {"0002101249 HEX|CAFEBABE0123FF"}
},
{
	"long", "short message set 0", 0, 0, 4, 100,
		"aaaaaaaa78f359395555870ca6c68226050d",
		"aed845127bdaba2a32223a22222232aaa2223a3a222232223a2252d24aba4ad2"
		"3adab232daaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{2101249ULL, "short_message", "13572468", -2, 1, 0}},
	1, {"0002101249 SMSG|13572468"}
},
{
	"long", "secure set 0", 0, 0, 4, 110,
		"aaaaaaaa78f359395555870ca6c6e2760320",
		"aed845127bcea422202e272e3c2a35a1aa2f273725332120272742cf56bd4ac0"
		"35dcb837c52aaaaaaa2aaaaaaaaaaa2a2aaaaaaaaa2a2a2a2a2a2aaaaa2aaaaa"
		"aa2a2aaa2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{2101249ULL, "secure", "secure long", -2, 1, 0}},
	1, {"0002101249 SEC|secure long"}
},
{
	"long", "alpha set 1", 0, 0, 4, 21,
		"aaaaaaaa78f359395555870ca6c6a2a80707",
		"aed845127be99e1e0e140b1b1a021784880f0f04140534202f2562ef7b8851f4"
		"0dea9136f05ec22a22c2fefe8e0e86022afeaede8a16328afefe4afa3a06126e"
		"a6b6f2a666aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1075843073ULL, "alphanumeric", "long address alpha message", 1, 1, 0}},
	1, {"1075843073 ALN|long address alpha message"}
},
{
	"long", "alpha one char set 1", 0, 0, 4, 31,
		"aaaaaaaa78f359395555870ca6c6d2f806ae",
		"aed845127ba6c60e1a1a0a121a021a868e0e0e06160222222a223ef276d606e6"
		"4aaade3aeeaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1075843074ULL, "alphanumeric", "x", 1, 1, 0}},
	1, {"1075843074 ALN|x"}
},
{
	"long", "numeric set 1", 0, 0, 4, 41,
		"aaaaaaaa78f359395555870ca6c602940659",
		"aed845127bf2900c041016041c02128888040402120c303a223866f2668440fc"
		"06e49038e8aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1075843073ULL, "numeric", "1234567890123", -2, 1, 0}},
	1, {"1075843073 NUM|1234567890123  "}
},
{
	"long", "numeric one word set 1", 0, 0, 4, 51,
		"aaaaaaaa78f359395555870ca6c6a2cc000e",
		"aed845127bf28a0a021212020a02128a8a020202020a2222323272ea6a8a52ea"
		"12fa8232faaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1075843073ULL, "numeric", "1234", -2, 1, 0}},
	1, {"1075843073 NUM|1234"}
},
{
	"long", "tone set 1", 0, 0, 4, 61,
		"aaaaaaaa78f359395555870ca6c6d2bc0049",
		"aed845127bea8a0a0a0a0a0a0a0a0a8a8a0a0a0a0a0a2a2a2a2a6aea6a8a4aea"
		"0aea8a2aeaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1075843073ULL, "tone", "", -2, 1, 0}},
	1, {"1075843073 TON|"}
},
{
	"long", "special numeric set 1", 0, 0, 4, 71,
		"aaaaaaaa78f359395555870ca6c602e2033b",
		"aed845127bea820a06060a1216061a8a8e0e02120e0e223a363e7af27a9a4aee"
		"02e69636f2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1075843073ULL, "special_numeric", "98765", -2, 1, 0}},
	1, {"1075843073 SNUM|98765     "}
},
{
	"long", "numbered numeric set 1", 0, 0, 4, 81,
		"aaaaaaaa78f359395555870ca6c6a28a031d",
		"aed845127bfa9e0a121a1a16120a168e820a021a060e323a363e62f66e964aea"
		"12f6923af2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1075843073ULL, "numbered_numeric", "555123", 13, 1, 0}},
	1, {"1075843073 NNUM|555123  "}
},
{
	"long", "hex set 1", 0, 0, 4, 91,
		"aaaaaaaa78f359395555870ca6c6d2da02b4",
		"aed845127bed980b1b0916151a0b13838a000a1a0f162a202a2562ea628c50f2"
		"07ee8a31fe2a2aaaaa2a2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa2a2aaaaa"
		"2aaa2a2a2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1075843073ULL, "binary", "CAFEBABE0123", 21, 1, 0}},
	1, {"1075843073 HEX|CAFEBABE0123FF"}
},
{
	"long", "short message set 1", 0, 0, 4, 101,
		"aaaaaaaa78f359395555870ca6c602a605dc",
		"aed845127bfa9a0a12021a020202128a82021a1a020232223a2272f26a9a4af2"
		"1afa9232faaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1075843073ULL, "short_message", "13572468", -2, 1, 0}},
	1, {"1075843073 SMSG|13572468"}
},
{
	"long", "secure set 1", 0, 0, 4, 111,
		"aaaaaaaa78f359395555870ca6c662f603f1",
		"aed845127bee8402000e070e1c0a15818a0f071705132120272762ef769d4ae0"
		"15fc9837e52aaaaaaa2aaaaaaaaaaa2a2aaaaaaaaa2a2a2a2a2a2aaaaa2aaaaa"
		"aa2a2aaa2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{1075843073ULL, "secure", "secure long", -2, 1, 0}},
	1, {"1075843073 SEC|secure long"}
},
{
	"long", "alpha set 2", 0, 0, 4, 22,
		"aaaaaaaa78f359395555870ca6c62268040a",
		"aed845127be99e1e0e140b1b1a021784880f0f04544574606f6522ef7bc851b4"
		"0daad136b05ec22a22c2fefe8e0e86022afeaede8a16328afefe4afa3a06126e"
		"a6b6f2a666aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{3223326721ULL, "alphanumeric", "long address alpha message", 1, 1, 0}},
	1, {"3223326721 ALN|long address alpha message"}
},
{
	"long", "alpha one char set 2", 0, 0, 4, 32,
		"aaaaaaaa78f359395555870ca6c692040693",
		"aed845127ba6c60e1a1a0a121a021a868e0e0e06564262626a627ef2769606a6"
		"4aea9e3aaeaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{3223326722ULL, "alphanumeric", "x", 1, 1, 0}},
	1, {"3223326722 ALN|x"}
},
{
	"long", "numeric set 2", 0, 0, 4, 42,
		"aaaaaaaa78f359395555870ca6c6f25400be",
		"aed845127bf2900c041016041c02128888040402524c707a627826f266c440bc"
		"06a4d038a8aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{3223326721ULL, "numeric", "1234567890123", -2, 1, 0}},
	1, {"3223326721 NUM|1234567890123  "}
},
{
	"long", "numeric one word set 2", 0, 0, 4, 52,
		"aaaaaaaa78f359395555870ca6c6222c02ed",
		"aed845127bf28a0a021212020a02128a8a020202424a6262727232ea6aca52aa"
		"12bac232baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{3223326721ULL, "numeric", "1234", -2, 1, 0}},
	1, {"3223326721 NUM|1234"}
},
{
	"long", "tone set 2", 0, 0, 4, 62,
		"aaaaaaaa78f359395555870ca6c6527c0344",
		"aed845127bea8a0a0a0a0a0a0a0a0a8a8a0a0a0a4a4a6a6a6a6a2aea6aca4aaa"
		"0aaaca2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{3223326721ULL, "tone", "", -2, 1, 0}},
	1, {"3223326721 TON|"}
},
{
	"long", "special numeric set 2", 0, 0, 4, 72,
		"aaaaaaaa78f359395555870ca6c6f21203ad",
		"aed845127bea820a06060a1216061a8a8e0e02124e4e627a767e3af27ada4aae"
		"02a6d636b2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{3223326721ULL, "special_numeric", "98765", -2, 1, 0}},
	1, {"3223326721 SNUM|98765     "}
},
{
	"long", "numbered numeric set 2", 0, 0, 4, 82,
		"aaaaaaaa78f359395555870ca6c6224a0010",
		"aed845127bfa8e120a1a1a16120a168e820a021a464e627a667e3afe7ec642aa"
		"0abeca22aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{3223326721ULL, "numbered_numeric", "555123", 14, 1, 0}},
	1, {"3223326721 NNUM|555123  "}
},
{
	"long", "hex set 2", 0, 0, 4, 92,
		"aaaaaaaa78f359395555870ca6c6523a0057",
		"aed845127be5980b1b09161512031b838a000a1a475e6a606a652ae262cc58b2"
		"07aeca31b62a2aaaaa2a2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa2a2aaaaa"
		"2aaa2a2a2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{3223326721ULL, "binary", "CAFEBABE0123", 22, 1, 0}},
	1, {"3223326721 HEX|CAFEBABE0123FF"}
},
{
	"long", "short message set 2", 0, 0, 4, 102,
		"aaaaaaaa78f359395555870ca6c6f266033b",
		"aed845127bfa9a0a12021a020202128a82021a1a424272627a6232f26ada4ab2"
		"1abad232baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{3223326721ULL, "short_message", "13572468", -2, 1, 0}},
	1, {"3223326721 SMSG|13572468"}
},
{
	"long", "secure set 2", 0, 0, 4, 112,
		"aaaaaaaa78f359395555870ca6c6220e06f7",
		"aed845127bee8402000e070e1c0a15818a0f071745536160676722ef76dd4aa0"
		"15bcd837a52aaaaaaa2aaaaaaaaaaa2a2aaaaaaaaa2a2a2a2a2a2aaaaa2aaaaa"
		"aa2a2aaa2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{3223326721ULL, "secure", "secure long", -2, 1, 0}},
	1, {"3223326721 SEC|secure long"}
},
{
	"long", "alpha set 3", 0, 0, 4, 23,
		"aaaaaaaa78f359395555870ca6c6c2e806b5",
		"aed845127b89de5e4e544b5b5a4257c4c84f4f44744574606f6502af1be811b4"
		"6daad156905ec22a22c2fefe8e0e86022afeaede8a16328afefe4afa3a06126e"
		"a6b6f2a666aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{4297068542ULL, "alphanumeric", "long address alpha message", 1, 1, 0}},
	1, {"4297068542 ALN|long address alpha message"}
},
{
	"long", "alpha one char set 3", 0, 0, 4, 33,
		"aaaaaaaa78f359395555870ca6c612840642",
		"aed845127bc6864e5a5a4a525a425ac6ce4e4e46764262626a625eb216b646a6"
		"2aea9e5a8eaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{4297068541ULL, "alphanumeric", "x", 1, 1, 0}},
	1, {"4297068541 ALN|x"}
},
{
	"long", "numeric set 3", 0, 0, 4, 43,
		"aaaaaaaa78f359395555870ca6c672d4006f",
		"aed845127b92d04c445056445c4252c8c8444442724c707a627806b206e400bc"
		"66a4d05888aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{4297068542ULL, "numeric", "1234567890123", -2, 1, 0}},
	1, {"4297068542 NUM|1234567890123  "}
},
{
	"long", "numeric one word set 3", 0, 0, 4, 53,
		"aaaaaaaa78f359395555870ca6c6c2ac0052",
		"aed845127b92ca4a425252424a4252caca424242624a6262727212aa0aea12aa"
		"72bac2529aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{4297068542ULL, "numeric", "1234", -2, 1, 0}},
	1, {"4297068542 NUM|1234"}
},
{
	"long", "tone set 3", 0, 0, 4, 63,
		"aaaaaaaa78f359395555870ca6c692fc0020",
		"aed845127b8aca4a4a4a4a4a4a4a4acaca4a4a4a6a4a6a6a6a6a0aaa0aea0aaa"
		"6aaaca4a8aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{4297068542ULL, "tone", "", -2, 1, 0}},
	1, {"4297068542 TON|"}
},
{
	"long", "special numeric set 3", 0, 0, 4, 73,
		"aaaaaaaa78f359395555870ca6c67292037c",
		"aed845127b8ac24a46464a5256465acace4e42526e4e627a767e1ab21afa0aae"
		"62a6d65692aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{4297068542ULL, "special_numeric", "98765", -2, 1, 0}},
	1, {"4297068542 SNUM|98765     "}
},
{
	"long", "numbered numeric set 3", 0, 0, 4, 83,
		"aaaaaaaa78f359395555870ca6c6c2ca02af",
		"aed845127b8ade5a5a5a5a56524a56cec24a425a664e627a766e0aae16fe1aba"
		"72a6da4a92aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{4297068542ULL, "numbered_numeric", "555123", 15, 1, 0}},
	1, {"4297068542 NNUM|555123  "}
},
{
	"long", "hex set 3", 0, 0, 4, 93,
		"aaaaaaaa78f359395555870ca6c692ba0333",
		"aed845127b85d84b5b4956555a4b53c3ca404a5a6f5e6a606a6502aa0aec18ba"
		"6fa6c2519e2a2aaaaa2a2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa2a2aaaaa"
		"2aaa2a2a2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{4297068542ULL, "binary", "CAFEBABE0123", 23, 1, 0}},
	1, {"4297068542 HEX|CAFEBABE0123FF"}
},
{
	"long", "short message set 3", 0, 0, 4, 103,
		"aaaaaaaa78f359395555870ca6c672e603ea",
		"aed845127b9ada4a52425a42424252cac2425a5a624272627a6212b20afa0ab2"
		"7abad2529aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{4297068542ULL, "short_message", "13572468", -2, 1, 0}},
	1, {"4297068542 SMSG|13572468"}
},
{
	"long", "secure set 3", 0, 0, 4, 113,
		"aaaaaaaa78f359395555870ca6c6c28e0448",
		"aed845127b8ec442404e474e5c4a55c1ca4f475765536160676702af16fd0aa0"
		"75bcd857852aaaaaaa2aaaaaaaaaaa2a2aaaaaaaaa2a2a2a2a2a2aaaaa2aaaaa"
		"aa2a2aaa2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{4297068542ULL, "secure", "secure long", -2, 1, 0}},
	1, {"4297068542 SEC|secure long"}
},
{
	"mixed", "mixed priority", 0, 0, 6, 40,
		"aaaaaaaa78f359395555870ca6c6f61405eb",
		"aed845127ba2e40d047f8c777777777704048404770404048404d931436d5042"
		"9ea3170543bbf26a01fe51afc55da199760f031ca3a000431f4370dea55018a7"
		"5270fd4d8e8812118926eba75a0c863698efeea70454d480efe7eb678f128337"
		"00e8671a8c4a2aaa8a0acaca6a4a0a0aaaaa8a4a6a2a8a0aaaaa0acaea6a0aea"
		"eaaaaaaa8aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	6, {{2000ULL, "alphanumeric", "normal alpha", 1, 1, 0}, {2001ULL, "tone", "", -2, 1, 0}, {2002ULL, "alphanumeric", "priority alpha", 2, 1, 0}, {2101300ULL, "numeric", "5551234", -2, 1, 0}, {2003ULL, "numeric", "123", -2, 1, 0}, {2004ULL, "tone", "", -2, 1, 0}},
	6, {"0000002002 ALN|priority alpha", "0000002003 NUM|123 ", "0000002000 ALN|normal alpha", "0002101300 NUM|5551234   ", "0000002001 TON|", "0000002004 TON|"}
},
{
	"mixed", "every type", 0, 0, 6, 41,
		"aaaaaaaa78f359395555870ca6c67694053a",
		"aed845127baa1978877f7f00000000807f0080007f0000000000ad344c6652cc"
		"e1ad61074b9cf16081e2ba5624251c0bf700005ad1000c2402242ff38d22d6ca"
		"f27abaa11acf9280c3f46a7a5291023a8be36a3e8e414d00e060ea60190cc97d"
		"11aee45d121a3a0aeada3a3a7aea1a8a6a7a1a9ada4a0a8a5adaaa1a9a3a2aca"
		"3a7a6a7abaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	8, {{2100ULL, "alphanumeric", "alpha", 4, 1, 0}, {2101ULL, "numeric", "4711", -2, 1, 0}, {2102ULL, "binary", "BEEF", 5, 1, 0}, {2103ULL, "short_message", "321", -2, 1, 0}, {2104ULL, "special_numeric", "246", -2, 1, 0}, {2105ULL, "numbered_numeric", "135", 6, 1, 0}, {2106ULL, "secure", "sec", -2, 1, 0}, {2107ULL, "tone", "", -2, 1, 0}},
	8, {"0000002100 ALN|alpha", "0000002101 NUM|4711", "0000002102 HEX|BEEF00", "0000002103 SMSG|321", "0000002104 SNUM|246 ", "0000002105 NNUM|135     ", "0000002106 SEC|sec", "0000002107 TON|"}
},
{
	"mixed", "nine tones", 0, 0, 6, 42,
		"aaaaaaaa78f359395555870ca6c6b6540582",
		"aed845127baa19f8077f7f7f7f7f0000800080007f0000000000ad4b4c192d4c"
		"e12de18734aaaa6aeaeaeaeaeaea2a2a2a2a2a2aea2a2a2a2a2a6a2aaaea2aea"
		"ea2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	9, {{500ULL, "tone", "", -2, 1, 0}, {501ULL, "tone", "", -2, 1, 0}, {502ULL, "tone", "", -2, 1, 0}, {503ULL, "tone", "", -2, 1, 0}, {504ULL, "tone", "", -2, 1, 0}, {505ULL, "tone", "", -2, 1, 0}, {506ULL, "tone", "", -2, 1, 0}, {507ULL, "tone", "", -2, 1, 0}, {508ULL, "tone", "", -2, 1, 0}},
	9, {"0000000500 TON|", "0000000501 TON|", "0000000502 TON|", "0000000503 TON|", "0000000504 TON|", "0000000505 TON|", "0000000506 TON|", "0000000507 TON|", "0000000508 TON|"}
},
{
	"mode 3200/2", "alpha phase 0", 1, 0, 5, 60,
		"aaaaaaaa84e7593955557b18a6c6da3c0433",
		"aaaaaaed84555555127bc6e6c446a6446cc40c4626ee0ce60c6e2e66044c444c"
		"86e606ee04ee046e2c440c6c04c4044406ee04ee2ee4c64e2ec4eeee0e6c4c64"
		"ec6686e66ec44c460ccccccccccccccc4ccc4ccccccccccccccc4ccccccc4ccc"
		"4ccccccccccccccccccc4ccc4ccc4ccc4ccc4ccccccc4ccc4ccc4ccc4ccccccc"
		"4ccc4ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccc",
	1, {{300ULL, "alphanumeric", "phase message", 0, 1, 0}},
	1, {"0000000300 ALN|phase message"}
},
{
	"mode 3200/2", "numeric and tone phase 0", 1, 0, 5, 70,
		"aaaaaaaa84e7593955557b18a6c60a620741",
		"aaaaaaed84555555127bccecc64c2ecc6e6c066c2e4c04cc04ec2c6c064cc4cc"
		"846c04ec044c06cc2cec04ec044c04cc066c046c26ecccec266ceecc0eeccecc"
		"6e4c8eccee6c46ec8eeccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccc",
	2, {{300ULL, "numeric", "13579", -2, 1, 0}, {301ULL, "tone", "", -2, 1, 0}},
	2, {"0000000300 NUM|13579     ", "0000000301 TON|"}
},
{
	"mode 3200/2", "long hex phase 0", 1, 0, 5, 80,
		"aaaaaaaa84e7593955557b18a6c6aa0a0767",
		"aaaaaaed84555555127bcc66c6c606c646c404cc0ecc0eec06cc0cc426c4ec44"
		"a4c4044c0cec2cec0cec06640c440ce40c440c4c0eeecc6c04cecc6c26ecc4ec"
		"6444a64ccece4446a4cecccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccc",
	1, {{3000000000ULL, "binary", "0F0F0F", 9, 1, 0}},
	1, {"3000000000 HEX|0F0F0F00"}
},
{
	"mode 3200/2", "alpha phase 1", 1, 0, 5, 61,
		"aaaaaaaa84e7593955557b18a6c65abc04e2",
		"aaaaaaed84555555127bc9d9c88949888cc81c8918dd0cd90c9d1d99088c888c"
		"49d909dd09dd089d1c880c9c08c8088809dd08dd0dd8c88d0cc8ccdd0d9c8d98"
		"cd9959d98dc88d891ccccccccccccccc8ccc8ccccccccccccccc8ccccccc8ccc"
		"8ccccccccccccccccccc8ccc8ccc8ccc8ccc8ccccccc8ccc8ccc8ccc8ccccccc"
		"8ccc8ccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccc",
	1, {{304ULL, "alphanumeric", "phase message", 1, 1, 0}},
	1, {"0000000304 ALN|phase message"}
},
{
	"mode 3200/2", "numeric and tone phase 1", 1, 0, 5, 71,
		"aaaaaaaa84e7593955557b18a6c6fae2027a",
		"aaaaaaed84555555127bccecc64c06cc466c2e6c2e4c04cc04ec2c6c064cc4cc"
		"846c04ec044c06cc2cec04ec044c04cc066c046c0eecccec0e6cc6cc0eeccecc"
		"464ca6ccc66c46eca6eccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccc",
	2, {{304ULL, "numeric", "13579", -2, 1, 0}, {305ULL, "tone", "", -2, 1, 0}},
	2, {"0000000304 NUM|13579     ", "0000000305 TON|"}
},
{
	"mode 3200/2", "long hex phase 1", 1, 0, 5, 81,
		"aaaaaaaa84e7593955557b18a6c62a8a07b6",
		"aaaaaaed84555555127bdc99c9c909c989c808cc0dcc0ddc09cc0cc819c8dc88"
		"58c8088c0cdc1cdc0cdc09980c880cd80c880c8c1ddddc9c18cdcc9c09dcd8dc"
		"9888498ccdcd888948cdcccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccc",
	1, {{3000000001ULL, "binary", "0F0F0F", 9, 1, 0}},
	1, {"3000000001 HEX|0F0F0F00"}
},
{
	"mode 3200/2", "all phases", 1, 0, 5, 90,
		"aaaaaaaa84e7593955557b18a6c6da5a06ce",
		"aaaaaaed84555555127bc444c4cc0e4e44c62ec4046606ee2e6e2c460644c4c4"
		"8464046c044c06e62ec6046c044c0444046c044c2e64ece6246ccc6e0ce4ee4c"
		"4cc48e4cc6ce64cea66444cc64cc6ccc4ccc44cc4ccceccceccc4ccc44cc44cc"
		"c4cce4cce4cccccc4ccce4cc64cc44cce4cc64ccc4cc4ccc64cc6cccc4cce4cc"
		"c4cc64cc6cccecccc4cccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccc",
	2, {{400ULL, "alphanumeric", "all", 10, 1, 0}, {404ULL, "alphanumeric", "all", 11, 1, 0}},
	2, {"0000000400 ALN|all", "0000000404 ALN|all"}
},
{
	"mode 3200/2", "inverted", 1, 0, 5, 91,
		"aaaaaaaa84e7593955557b18a6c65ada061f",
		"aaaaaaed84555555127bcec4cee4a4ec4cc42cec246e2cec2eec2c44046c44c4"
		"864606ee06ee0e4624e604ce0cc6046406ee04ee066eec6e2ecec6cc06446e66"
		"eee68e4666ce4e4e0eec66cc44ccc4cce4cc44cceecce6ccc6cc4ccc6ecc46cc"
		"64cceecceecc66cccccc46cc64cceccceecceecc66cccecc64cc4ecc6ecc46cc"
		"c4cce6ccc6cccccc6ecccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
		"cccccccccccccccccccc",
	1, {{500ULL, "alphanumeric", "inverted polarity frame", 3, 1, 0}},
	1, {"0000000500 ALN|inverted polarity frame"}
},
{
	"mode 3200/4", "alpha phase 0", 2, 0, 5, 60,
		"aaaaaaaa4f9759395555b068a6c6da3c0433",
		"888888ed84222222127bc2a2c002a2006880080222aa08a2082a2a2200084008"
		"82a202aa00aa002a280008280080000002aa00aa2aa0c20a2a80eaaa0a284820"
		"e82282a26a804802088888888888888808880888888888888888088888880888"
		"0888888888888888888808880888088808880888888808880888088808888888"
		"0888088888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"88888888888888888888",
	1, {{300ULL, "alphanumeric", "phase message", 0, 1, 0}},
	1, {"0000000300 ALN|phase message"}
},
{
	"mode 3200/4", "numeric and tone phase 0", 2, 0, 5, 70,
		"aaaaaaaa4f9759395555b068a6c60a620741",
		"888888ed84222222127bc8a8c2082a886a2802282a08008800a828280208c088"
		"802800a80008028828a800a8000800880228002822a8c8a82228ea880aa8ca88"
		"6a088a88ea2842a88aa888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"88888888888888888888",
	2, {{300ULL, "numeric", "13579", -2, 1, 0}, {301ULL, "tone", "", -2, 1, 0}},
	2, {"0000000300 NUM|13579     ", "0000000301 TON|"}
},
{
	"mode 3200/4", "long hex phase 0", 2, 0, 5, 80,
		"aaaaaaaa4f9759395555b068a6c6aa0a0767",
		"888888ed84222222127bc822c2820282428000880a880aa8028808802280e800"
		"a080000808a828a808a80220080008a0080008080aaac828008ac82822a8c0a8"
		"6000a208ca8a4002a08a88888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"88888888888888888888",
	1, {{3000000000ULL, "binary", "0F0F0F", 9, 1, 0}},
	1, {"3000000000 HEX|0F0F0F00"}
},
{
	"mode 3200/4", "alpha phase 1", 2, 0, 5, 61,
		"aaaaaaaa4f9759395555b068a6c65abc04e2",
		"888888ed84222222127bc9d9c88949888cc81c8918dd0cd90c9d1d99088c888c"
		"49d909dd09dd089d1c880c9c08c8088809dd08dd0dd8c88d0cc8ccdd0d9c8d98"
		"cd9959d98dc88d891cccc888c888c88888888888c888c888c8888888c8888888"
		"8888c888c888c888c88888888888888888888888c8888888888888888888c888"
		"88888888c888c888c88888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"88888888888888888888",
	1, {{304ULL, "alphanumeric", "phase message", 1, 1, 0}},
	1, {"0000000304 ALN|phase message"}
},
{
	"mode 3200/4", "numeric and tone phase 1", 2, 0, 5, 71,
		"aaaaaaaa4f9759395555b068a6c6fae2027a",
		"888888ed84222222127bc8a8c208028842282a282a08008800a828280208c088"
		"802800a80008028828a800a800080088022800280aa8c8a80a28c2880aa8ca88"
		"4208a288c22842a8a2a888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"88888888888888888888",
	2, {{304ULL, "numeric", "13579", -2, 1, 0}, {305ULL, "tone", "", -2, 1, 0}},
	2, {"0000000304 NUM|13579     ", "0000000305 TON|"}
},
{
	"mode 3200/4", "long hex phase 1", 2, 0, 5, 81,
		"aaaaaaaa4f9759395555b068a6c62a8a07b6",
		"888888ed84222222127bdc99c9c909c989c808cc0dcc0ddc09cc0cc819c8dc88"
		"58c8088c0cdc1cdc0cdc09980c880cd80c880c8c1ddddc9c18cdcc9c09dcd8dc"
		"9888498ccdcd888948cd88888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"88888888888888888888",
	1, {{3000000001ULL, "binary", "0F0F0F", 9, 1, 0}},
	1, {"3000000001 HEX|0F0F0F00"}
},
{
	"mode 3200/4", "all phases", 2, 0, 5, 90,
		"aaaaaaaa4f9759395555b068a6c6da5a06ce",
		"888888ed84222222127bc000c0880a0a40822a80002202aa2a2a28020200c080"
		"80200028000802a22a82002800080000002800082a20e8a22028c82a08a0ea08"
		"48808a08c28a608aa220008820882888088800880888a888a888088800880088"
		"8088a088a08888880888a08820880088a088208880880888208828888088a088"
		"808820882888a888808888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"88888888888888888888",
	2, {{400ULL, "alphanumeric", "all", 10, 1, 0}, {404ULL, "alphanumeric", "all", 11, 1, 0}},
	2, {"0000000400 ALN|all", "0000000404 ALN|all"}
},
{
	"mode 3200/4", "inverted", 2, 0, 5, 91,
		"aaaaaaaa4f9759395555b068a6c65ada061f",
		"888888ed84222222127bca80caa0a0a8488028a8202a28a82aa8280000284080"
		"820202aa02aa0a0220a2008a0882002002aa00aa022ae82a2a8ac28802006a22"
		"eaa28a02628a4a0a0aa8228800888088a0880088aa88a288828808882a880288"
		"2088aa88aa882288888802882088a888aa88aa8822888a8820880a882a880288"
		"8088a288828888882a8888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"8888888888888888888888888888888888888888888888888888888888888888"
		"88888888888888888888",
	1, {{500ULL, "alphanumeric", "inverted polarity frame", 3, 1, 0}},
	1, {"0000000500 ALN|inverted polarity frame"}
},
{
	"mode 6400/4", "alpha phase 0", 3, 0, 5, 60,
		"aaaaaaaa215f59395555dea0a6c6da3c0433",
		"8888888888888888ed842222222222222222127bf028a828f020202888282020"
		"78a0a02000a020280828a8a800a0a82800a028a808a82828002020a0702020a0"
		"8028a8280028a8a80020a8a8002028a808a0202000a028a00020a02000202020"
		"0028a8a80020a8a808a8a820f02820a808a8a020f8a8a8a800a828a070a02820"
		"f8a028288028a82878a8a02070a0202800a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"20a0a0a020a0a0a0a0a0a0a0a0a0a0a0a0a0a0a020a0a0a0a0a0a0a020a0a0a0"
		"20a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a020a0a0a020a0a0a020a0a0a0"
		"20a0a0a020a0a0a0a0a0a0a020a0a0a020a0a0a020a0a0a020a0a0a0a0a0a0a0"
		"20a0a0a020a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0",
	1, {{300ULL, "alphanumeric", "phase message", 0, 1, 0}},
	1, {"0000000300 ALN|phase message"}
},
{
	"mode 6400/4", "numeric and tone phase 0", 3, 0, 5, 70,
		"aaaaaaaa215f59395555dea0a6c60a620741",
		"8888888888888888ed842222222222222222127bf0a0a8a0f02820a008a8a0a0"
		"78a828a0002828a008a820a00020a0a00020a8a008a028a0002820a0f020a0a0"
		"802028a00020a8a0002020a00028a0a008a0a8a00020a8a0002020a00020a0a0"
		"002828a0002028a00828a8a0f0a0a8a0082828a0f8a8a0a000a8a8a0f0a8a0a0"
		"78a820a080a8a0a0f8a828a07028a8a080a8a8a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0",
	2, {{300ULL, "numeric", "13579", -2, 1, 0}, {301ULL, "tone", "", -2, 1, 0}},
	2, {"0000000300 NUM|13579     ", "0000000301 TON|"}
},
{
	"mode 6400/4", "long hex phase 0", 3, 0, 5, 80,
		"aaaaaaaa215f59395555dea0a6c6aa0a0767",
		"8888888888888888ed842222222222222222127bf0a02828f028a0280028a028"
		"7028a0200020a0a000a8a0a000a8a8a00028a0a000a0a0200828a020f8a02020"
		"8820a020002020a000a0a8a008a0a8a000a0a8a00028282000a0202000a0a820"
		"00a0202000a020a000a8a8a8f0a028a00020a0a8f0a028a00828a8a0f020a8a0"
		"78202020882820a0f0a8a0a8702020288820a0a8a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0",
	1, {{3000000000ULL, "binary", "0F0F0F", 9, 1, 0}},
	1, {"3000000000 HEX|0F0F0F00"}
},
{
	"mode 6400/4", "alpha phase 1", 3, 0, 5, 61,
		"aaaaaaaa215f59395555dea0a6c65abc04e2",
		"8888888888888888ed842222222222222222127bf0a4e4a4f0a0a0a440a4a0a0"
		"b0e0e0a004e0a0a404a0e4e400e0e4a400e0a4e404e4a4a400a0a0e0b0a0a0e0"
		"40a4e4a400a4e4e400a4e4e400a0a4e404e0a0a000e0a4e000a0e0a000a0a0a0"
		"00a4e4e400a0e4e400e4e4a0f0a0a0e400e0e0a0f0e0e4e400e4a4e0b0e4a4a0"
		"f0e4a4a444a4e4a4b0e4e0a0b0e4a0a404e0e0e0e0a0a0a0e0a0a0a0e0a0a0a0"
		"a0a0a0a0a0a0a0a0e0a0a0a0e0a0a0a0e0a0a0a0a0a0a0a0e0a0a0a0a0a0a0a0"
		"a0a0a0a0e0a0a0a0e0a0a0a0e0a0a0a0e0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0e0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0e0a0a0a0"
		"a0a0a0a0a0a0a0a0e0a0a0a0e0a0a0a0e0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0",
	1, {{304ULL, "alphanumeric", "phase message", 1, 1, 0}},
	1, {"0000000304 ALN|phase message"}
},
{
	"mode 6400/4", "numeric and tone phase 1", 3, 0, 5, 71,
		"aaaaaaaa215f59395555dea0a6c6fae2027a",
		"8888888888888888ed842222222222222222127bf0a0a8a0f02820a00028a0a0"
		"702828a008a828a008a820a00020a0a00020a8a008a028a0002820a0f020a0a0"
		"802028a00020a8a0002020a00028a0a008a0a8a00020a8a0002020a00020a0a0"
		"002828a0002028a000a8a8a0f0a0a8a000a828a0f028a0a000a8a8a0f0a8a0a0"
		"702820a08828a0a0f02828a07028a8a08828a8a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0",
	2, {{304ULL, "numeric", "13579", -2, 1, 0}, {305ULL, "tone", "", -2, 1, 0}},
	2, {"0000000304 NUM|13579     ", "0000000305 TON|"}
},
{
	"mode 6400/4", "long hex phase 1", 3, 0, 5, 81,
		"aaaaaaaa215f59395555dea0a6c62a8a07b6",
		"8888888888888888ed842222222222222222127bf4e0a4a4f0a4e0a400a4e0a4"
		"b0a4e0a000a0e0e000e4e0e000e4e4e000a4e0e000e0e0a004a4e0a0f4e0a0a0"
		"44a0e0a000a0a0e000e0e4e004e0e4e000e0e4e000a4a4a000e0a0a000e0e4a0"
		"00e0a0a000e0a0e004e4e4e4f4e0a4e004a0e0e4f0e0a4e000a4e4e0f4a0e4e0"
		"b4a0a0a040a4a0e0f0e4e0e4b0a0a0a440a0e0e4a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0",
	1, {{3000000001ULL, "binary", "0F0F0F", 9, 1, 0}},
	1, {"3000000001 HEX|0F0F0F00"}
},
{
	"mode 6400/4", "alpha phase 2", 3, 0, 5, 62,
		"aaaaaaaa215f59395555dea0a6c69a7c045a",
		"8888888888888888ed842222222222222222127bf082a282f080808222828080"
		"d0a0a08002a080820282a2a200a2a28200a282a202a08282008080a0d08080a0"
		"2082a2820082a2a20080a2a2008282a202a0808000a082a00080a08000808080"
		"0082a2a20080a2a200a2a280f08280a202a2a080f2a0a2a202a282a0d0a28280"
		"f2a082822080a282d0a2a080d2a2808200a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"80a0a0a080a0a0a0a0a0a0a0a0a0a0a0a0a0a0a080a0a0a0a0a0a0a080a0a0a0"
		"80a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a080a0a0a080a0a0a080a0a0a0"
		"80a0a0a080a0a0a0a0a0a0a080a0a0a080a0a0a080a0a0a080a0a0a0a0a0a0a0"
		"80a0a0a080a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0",
	1, {{308ULL, "alphanumeric", "phase message", 2, 1, 0}},
	1, {"0000000308 ALN|phase message"}
},
{
	"mode 6400/4", "numeric and tone phase 2", 3, 0, 5, 72,
		"aaaaaaaa215f59395555dea0a6c67a120706",
		"8888888888888888ed842222222222222222127bf0a0a8a0f02820a008a8a0a0"
		"702828a008a828a008a820a00020a0a00020a8a008a028a0002820a0f020a0a0"
		"802028a00020a8a0002020a00028a0a008a0a8a00020a8a0002020a00020a0a0"
		"002828a0002028a000a8a8a0f0a0a8a0082828a0f8a8a0a00828a8a0f0a8a0a0"
		"78a820a080a8a0a0f02828a078a8a8a080a8a8a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0",
	2, {{308ULL, "numeric", "13579", -2, 1, 0}, {309ULL, "tone", "", -2, 1, 0}},
	2, {"0000000308 NUM|13579     ", "0000000309 TON|"}
},
{
	"mode 6400/4", "long hex phase 2", 3, 0, 5, 82,
		"aaaaaaaa215f59395555dea0a6c6ca4a06d5",
		"8888888888888888ed842222222222222222127bf0a08282f282a0820082a082"
		"d082a0800080a0a000a2a0a000a2a2a00082a0a000a0a0800282a080f2a08080"
		"2280a080008080a000a0a2a002a0a2a000a0a2a00082828000a0808000a0a280"
		"00a0808000a080a000a2a2a2f2a082a00280a0a2f2a082a00282a2a0f280a2a0"
		"d0808080228280a0f2a2a0a2d08080822080a0a2a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0",
	1, {{3000000002ULL, "binary", "0F0F0F", 9, 1, 0}},
	1, {"3000000002 HEX|0F0F0F00"}
},
{
	"mode 6400/4", "alpha phase 3", 3, 0, 5, 63,
		"aaaaaaaa215f59395555dea0a6c61afc048b",
		"8888888888888888ed842222222222222222127bf0a1b1a1f0a0a0a110a1a0a0"
		"e1b0b0a001b0a0a101a0b1b100b1b1a100b1a1b101b0a1a100a0a0b0e0a0a0b0"
		"10a1b1a100a1b1b100a1b1b100a1a1b101b0a0a000b0a1b000a0b0a000a0a0a0"
		"00a1b1b100a0b1b101b1b1a0f1a0a0b101b0b0a0f1b1b1b100b1a1b0e0b0a1a0"
		"f0b1a1a111a0b1a1e1b1b0a0e0b0a0a101b0b0b0b0a0a0a0b0a0a0a0b0a0a0a0"
		"a0a0a0a0a0a0a0a0b0a0a0a0b0a0a0a0b0a0a0a0a0a0a0a0b0a0a0a0a0a0a0a0"
		"a0a0a0a0b0a0a0a0b0a0a0a0b0a0a0a0b0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0b0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0b0a0a0a0"
		"a0a0a0a0a0a0a0a0b0a0a0a0b0a0a0a0b0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0",
	1, {{312ULL, "alphanumeric", "phase message", 3, 1, 0}},
	1, {"0000000312 ALN|phase message"}
},
{
	"mode 6400/4", "numeric and tone phase 3", 3, 0, 5, 73,
		"aaaaaaaa215f59395555dea0a6c6ba920462",
		"8888888888888888ed842222222222222222127bf0a0a8a0f02820a00028a0a0"
		"78a828a008a828a008a820a00020a0a00020a8a008a028a0002820a0f020a0a0"
		"802028a00020a8a0002020a00028a0a008a0a8a00020a8a0002020a00020a0a0"
		"002828a0002028a00828a8a0f820a8a0082828a0f8a8a0a000a8a8a0f0a8a0a0"
		"702820a08828a0a0f8a828a07028a8a08828a8a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0",
	2, {{312ULL, "numeric", "13579", -2, 1, 0}, {313ULL, "tone", "", -2, 1, 0}},
	2, {"0000000312 NUM|13579     ", "0000000313 TON|"}
},
{
	"mode 6400/4", "long hex phase 3", 3, 0, 5, 83,
		"aaaaaaaa215f59395555dea0a6c64aca0604",
		"8888888888888888ed842222222222222222127bf1b0a1a1f1a1b0a100a1b0a1"
		"e0a1b0a000a0b0b000b1b0b000b1b1b000a1b0b000b0b0a001a1b0a0f1b0a0a0"
		"11a0b0a000a0a0b000b0b1b001b0b1b000b0b1b000a1a1a000b0a0a000b0b1a0"
		"00b0a0a000b0a0b001b1b1b1f0b0a1b000a0b0b1f1b0a1b000a1b1b0f0a0b1b0"
		"e0a0a0a010a1a0b0f1b1b0b1e0a0a0a111a0b0b1a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0",
	1, {{3000000003ULL, "binary", "0F0F0F", 9, 1, 0}},
	1, {"3000000003 HEX|0F0F0F00"}
},
{
	"mode 6400/4", "all phases", 3, 0, 5, 90,
		"aaaaaaaa215f59395555dea0a6c6da5a06ce",
		"8888888888888888ed842222222222222222127b70202028f020282800a0a028"
		"f028a0a808a8a8a800202020002028a808a8a82808a8a028002020a8f02028a8"
		"002020208020202000202020002028a808a8a8a8002020200020202000202020"
		"002020200020202008a0282078a020a08820a02870a8202080a0a820f8a8a8a8"
		"f0a0a8a080a0a0a07028a0a0782820a8882828a8a020202020a02820a0a828a0"
		"202820a0a0202020282820a8a8a8a8a028a8a8a8a02820a02020202020202020"
		"a820a02828a0a82820a0a820a828a0a0a02820a828a0a82820a0282020202020"
		"28a0a82820a028202820a020282820a8a8a0282828a828a82820a02020a0a820"
		"a020a020a0a02828a0a828a020a8a8a82820a020202020a0a02820a0a828a0a0"
		"2820a0a0202020a02820a0a0a828a0a0a8a8a0a02820a0a0202020a0202020a0"
		"20a020a0a0a820a0a0a820a02820a0a028a0a0a0a0a820a0a02820a0202020a0"
		"a0a820a0a02820a0202020a02820a0a0a02820a0a828a0a0202020a0a0a820a0"
		"20a020a0a0a820a0a828a0a0a828a0a0202020a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0",
	4, {{400ULL, "alphanumeric", "all", 10, 1, 0}, {404ULL, "alphanumeric", "all", 11, 1, 0}, {408ULL, "alphanumeric", "all", 12, 1, 0}, {412ULL, "alphanumeric", "all", 13, 1, 0}},
	4, {"0000000400 ALN|all", "0000000404 ALN|all", "0000000408 ALN|all", "0000000412 ALN|all"}
},
{
	"mode 6400/4", "inverted", 3, 0, 5, 91,
		"aaaaaaaa215f59395555dea0a6c65ada061f",
		"8888888888888888ed842222222222222222127bf0a8a020f0a8a8208820a8a0"
		"70a0a02008a0a8a0082028a808a0a8a008a8a8a008a02020002028a07020a020"
		"802820280028a8a80028a8a800a820280820a8280020a0a800a0a02800202820"
		"0028a8a80020a8a8002828a8f8a028a808a8a0a8f028a0a00028202078a82828"
		"f8a8a82880a820287828a0a870a820a800a8a8a02828a0a02020a0a0a020a0a0"
		"a820a0a02020a0a0a8a8a0a0a828a0a0a028a0a020a0a0a028a8a0a02028a0a0"
		"2820a0a0a8a8a0a0a8a8a0a02828a0a0a0a0a0a02028a0a02820a0a0a8a0a0a0"
		"a8a8a0a0a8a8a0a02828a0a0a0a8a0a02820a0a020a8a0a028a8a0a02028a0a0"
		"a020a0a0a828a0a0a028a0a0a0a0a0a028a8a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0"
		"a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0a0",
	1, {{500ULL, "alphanumeric", "inverted polarity frame", 3, 1, 0}},
	1, {"0000000500 ALN|inverted polarity frame"}
},
{
	"fragments", "fragment 0", 0, 0, 7, 8,
		"aaaaaaaa78f359395555870ca6c60e100191",
		"aed845127b909df018350f6d746c1910d7171f0a603408241f0e208979de6e5c"
		"9f8e240711ecaca7a00bffef1ca8b43208ffbe3b20704100ff7b627aa2fab663"
		"4e4d262e17c388410088ffcb2a475f6110ff7f8aa46b32c5ffff30c7321614d3"
		"fb384984ddaaaa2a2aaaaaaa2aaaaaaa2aaa2a2a2a2a2a2aaa2a2a2a2aaa2a2a"
		"2aaa2a2a2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	0, {},
	0, {}
},
{
	"fragments", "fragment 1", 0, 0, 7, 12,
		"aaaaaaaa78f359395555870ca6c63e300620",
		"aed845127b9282d90028077b7666021ac40f1e2c623e02210f0f1a9b69d76f6b"
		"95a916282a9810342915ffbd10da6b2e91ffff6660e66080ffe6e3a92566de15"
		"02225097e060a0f46412fff64bab641482ffff9f081a0901ff9fc9883039f65d"
		"0bcd445a87aaea6a6a2aeaaa2a2aaaaa2aeaaaea6aaa2a2aaaaaaa2a2aeaeaaa"
		"aaea2aeaeaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	0, {},
	0, {}
},
{
	"fragments", "fragment 2", 0, 0, 7, 16,
		"aaaaaaaa78f359395555870ca6c6ee0807af",
		"aed845127b8ba0d23030177b71791c08d50f1d20403b26010f0f07ab5fcd6757"
		"848c303a3e3752250c52ff7f0e216d21c0ffee8707320014feb6235e55a8af10"
		"efbd0dc664aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{2500ULL, "alphanumeric", "First part of a long message that is sent in three fragments. The second part follows in the next frame of the collapse cycle. And the last part ends the message.", 9, 1, 3}},
	1, {"0000002500 ALN|First part of a long message that is sent in three fragments. The second part follows in the next frame of the collapse cycle. And the last part ends the message."}
},
{
	"fragments", "long fragment 0", 0, 0, 7, 40,
		"aaaaaaaa78f359395555870ca6c67e140140",
		"aed845127ba5964a0e18637f122a57ecc80b2f743c053c202f257aa313a45184"
		"01dae14a8c50e41c14e8fefcb602940000fcb4aafa7c1c80fcf85ec2760e1864"
		"f2f6cee830aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	0, {},
	0, {}
},
{
	"fragments", "long fragment 1", 0, 0, 7, 44,
		"aaaaaaaa78f359395555870ca6c65e340175",
		"aed845127bba9f5f0110677f1a2453e2c5072f6638122820272667bd18a34488"
		"10cde14b8e2a6aea6aaaeaaaeaeaaa2aaaaaaaeaeaaaaa2aaaaa2aeaaaaa6aea"
		"6a2aea6aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{3000000100ULL, "alphanumeric", "Long address fragment one, fragment two.", 21, 1, 2}},
	1, {"3000000100 ALN|Long address fragment one, fragment two."}
},
{
	"fragments", "open stream", 0, 0, 7, 60,
		"aaaaaaaa78f359395555870ca6c69e3c010f",
		"aed845127b8a9483722d47272d384410d81f0d167338100b1f0f67c568e82978"
		"f39f682a222a2aaa2aaaaaaa2a2aaaaa2aaa2a2a2a2a2a2aaa2a2aaa2a2a2aaa"
		"aa2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	0, {},
	0, {}
},
{
	"fragments", "complete during stream", 0, 0, 7, 64,
		"aaaaaaaa78f359395555870ca6c62e020221",
		"aed845127b9e90a654204f2629385301d61f1f3b783f1c001f0f60e548cb3f42"
		"f8b0620c0deaea6a2aaaeaeaeaaa6a2aaaeaeaea6a2a2a2aaaaa6aaa2aaa2a6a"
		"aaaa6aea6aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{2600ULL, "alphanumeric", "complete message", 31, 1, 0}},
	1, {"0000002600 ALN|complete message"}
},
{
	"group", "setup", 0, 0, 8, 10,
		"aaaaaaaa78f359395555870ca6c6b15004d6",
		"aed845127b27938f7f7f70007070700cfc8300037c00030003002444b810d747"
		"9c57ef8fc48aeaea8aea0a0a0a0a0a0a6a8a0aea0a0a8a6aea0a6a6a0a0aea8a"
		"6aeaea8a6aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	5, {{3000ULL, "short_instruction", "", -2, 1, 0}, {3001ULL, "short_instruction", "", -2, 1, 0}, {3002ULL, "short_instruction", "", -2, 1, 0}, {3100ULL, "short_instruction", "", -2, 1, 0}, {3101ULL, "short_instruction", "", -2, 1, 0}},
	0, {}
},
{
	"group", "delivery alpha", 0, 0, 8, 20,
		"aaaaaaaa78f359395555870ca6c64128075e",
		"aed845127bf5a5d414391f3d3f390e02d55f4f632852604c5f4f30fb588b3e1c"
		"d595284d7e9682f6026afefeda46d26206fefe0e2e0a0226fe2e263e521e9212"
		"e29aeadee6aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{2029573ULL, "alphanumeric", "group message to three pagers", 8, 1, 0}},
	1, {"0002029573 ALN|group message to three pagers"}
},
{
	"group", "delivery numeric", 0, 0, 8, 22,
		"aaaaaaaa78f359395555870ca6c601680737",
		"aed845127b92e2ea223222023a2a0202ca5a42621a4a42424a7a7ac27afa420a"
		"aae2725a7aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{2029574ULL, "numeric", "424242", -2, 1, 0}},
	1, {"0002029574 NUM|424242    "}
},
{
	"group", "delivery tone", 0, 0, 8, 24,
		"aaaaaaaa78f359395555870ca6c671180770",
		"aed845127beaeaea2a2a2a2a2a2a2a2aea6a6a6a2a6a6a6a6a6a2aaa2aea2a6a"
		"aaaa6a6a2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{2029575ULL, "tone", "", -2, 1, 0}},
	1, {"0002029575 TON|"}
},
{
	"duplicates", "original", 0, 0, 9, 10,
		"aaaaaaaa78f359395555870ca6c63950007d",
		"aed845127b2891f7880d777f790a08707781010f720008000500253ecb14ad42"
		"6ad6e27b411f22ed34c2ffff27e11c00f0fefee707b22014fef6724f34102ff1"
		"66cd44d79d320a422aaa0a124ae2022a6a4ab22aead2221252c24ab2ea72a2c2"
		"f2b28242c2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	3, {{3300ULL, "alphanumeric", "retransmitted message", 3, 1, 0}, {3301ULL, "numbered_numeric", "8642", 4, 1, 0}, {3302ULL, "binary", "ABCD", 5, 1, 0}},
	3, {"0000003300 ALN|retransmitted message", "0000003301 NNUM|8642    ", "0000003302 HEX|ABCD00"}
},
{
	"duplicates", "retransmission", 0, 0, 9, 14,
		"aaaaaaaa78f359395555870ca6c619700048",
		"aed845127b2895f3890d777f790a08707781010f72000c000400243ecf15a847"
		"6bd6e67a451f22ed34c2ffff27e11c00f0fefee707b22014fef6724f34102ff1"
		"66cd44d79d022a422aaa0a124a62022a6a4ab22aead2221252c2fa227ad29252"
		"5202a242d2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	0, {},
	0, {}
},
{
	"duplicates", "same number other message", 0, 0, 9, 18,
		"aaaaaaaa78f359395555870ca6c6c94801c7",
		"aed845127bb182df183847777e310345d01f1f30660500230f071e89428e1a43"
		"fbd47d626f3f247a2904ff7f8c30292011ffbd2eaee6c10affefa72d3ab2adb0"
		"9d4aa8fe096a064a0a06fe6e261eb23282faba7e463a8252fafaae72c2eea2b6"
		"daee765eeaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{3300ULL, "alphanumeric", "a different and longer message with the same number", 3, 1, 0}},
	1, {"0000003300 ALN|a different and longer message with the same number"}
},
{
	"duplicates", "same message again with R", 0, 0, 9, 22,
		"aaaaaaaa78f359395555870ca6c6f9680676",
		"aed845127bb182df083847777e310345d01f1f30660500231f070e89429e0a53"
		"ebd47d726f3f247a2904ff7f8c30292011ffbd2eaee6c10affefa72d3ab2adb0"
		"9d4aa8fe096a064a0a06fe6e261eb23282faba7e463a8252fafaae72c2eea2b6"
		"daee765eeaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	0, {},
	0, {}
},
{
	"special", "operator alpha", 0, 0, 10, 10,
		"aaaaaaaa78f359395555870ca6c6d5500031",
		"aed845127b8f949b326c172f29390400dd5f4d4f21664a495f4f73c92fb86461"
		"ba8c306a62aa2a2a2a2aaaaaaaaaaa2a2aaaaaaa2aaa2a2aaaaa2a2a2aaa2aaa"
		"aaaa2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{2029584ULL, "alphanumeric", "system message", 0, 1, 0}},
	1, {"0002029584 ALN|system message"}
},
{
	"special", "info service alpha", 0, 0, 10, 11,
		"aaaaaaaa78f359395555870ca6c655d000e0",
		"aed845127b8498aa04390f2f28220c0cd21f5e2b236242415f4b7dab14c5654d"
		"eeaf295159ea2aaa6a2aeaea6aea2a2aaaeaea6aaaea2aaaeaeaea2aaa6aeaea"
		"2a2a2aea6aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{2009088ULL, "alphanumeric", "mail drop service", 1, 1, 0}},
	1, {"0002009088 ALN|mail drop service"}
},
{
	"special", "network alpha", 0, 0, 10, 12,
		"aaaaaaaa78f359395555870ca6c6953001b6",
		"aed845127b908295302f0737372e0d0ad41f4f5c246c45415f4d31946cae3137"
		"a99067707f2a2aaa2aaaaaaaaaaa2a2a2a2a2aaaaa2a2a2a2a2aaaaa2aaaaa2a"
		"2a2a2a2a2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{2025472ULL, "alphanumeric", "network text", 2, 1, 0}},
	1, {"0002025472 ALN|network text"}
},
{
	"special", "reserved short", 0, 0, 10, 13,
		"aaaaaaaa78f359395555870ca6c615b00167",
		"aed845127bda9c8e042816363c24160c901e0e1a0e6840445c4c169a48b8104e"
		"d2e850482eaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	0, {},
	0, {}
},
{
	"biw", "biw 0", 0, 0, 3, 8,
		"aaaaaaaa78f359395555870ca6c62c10030f",
		"aed845127baaca4aca4a2a0a1aaaaa2a1aea0a3a7a0a5a4a2a5a3acacaba5a3a"
		"aa9a2a9a0aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	0, {},
	0, {}
},
{
	"biw", "biw 1", 0, 0, 3, 9,
		"aaaaaaaa78f359395555870ca6c6cc9001b0",
		"aed845127baada4ada5a2a1a1aaaba2a0afa0a2a6a1a4a5a2a4a2adacaba4a2a"
		"ba8a3a9a0aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	0, {},
	0, {}
},
{
	"biw", "biw 2", 0, 0, 3, 10,
		"aaaaaaaa78f359395555870ca6c64c5002bd",
		"aed845127ba2d7c65e57221e11adb5a80bf303236c1441502341ae584c3d4dad"
		"3883369c820a5a8a0a9afada6a2a3aca2afafa7a7a6a2a5afa7aca3a5afaea7a"
		"fa7a1a4a7aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	1, {{3400ULL, "alphanumeric", "with biw words", 3, 1, 0}},
	1, {"0000003400 ALN|with biw words"}
},
{
	"biw", "biw 3", 0, 0, 3, 11,
		"aaaaaaaa78f359395555870ca6c68cd001d9",
		"aed845127bcaea0aaa6a4a6a7a8aea0a1aea4a1a1a2a1a2a4a1a1aea8aba5a1a"
		"aa9a6afa0aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	0, {},
	0, {}
},
{
	"biw", "biw system message type", 0, 0, 3, 12,
		"aaaaaaaa78f359395555870ca6c60c30033a",
		"aed845127b6aeaea2a6a2a6a2aea6a2aaa2a2a2a2a2a2a2a2a2a2aaa6a6aea6a"
		"2aaa6a2aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
		"aaaaaaaaaa",
	0, {},
	0, {}
},
