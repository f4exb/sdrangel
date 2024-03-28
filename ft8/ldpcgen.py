#!/usr/bin/env python

# Generates (prints out) C structure of FT8 LDPC generator matrix
# Derived from WSJT-X ldpc_174_91_c_generator.f90

DATA_G = [
    "8329ce11bf31eaf509f27fc",
    "761c264e25c259335493132",
    "dc265902fb277c6410a1bdc",
    "1b3f417858cd2dd33ec7f62",
    "09fda4fee04195fd034783a",
    "077cccc11b8873ed5c3d48a",
    "29b62afe3ca036f4fe1a9da",
    "6054faf5f35d96d3b0c8c3e",
    "e20798e4310eed27884ae90",
    "775c9c08e80e26ddae56318",
    "b0b811028c2bf997213487c",
    "18a0c9231fc60adf5c5ea32",
    "76471e8302a0721e01b12b8",
    "ffbccb80ca8341fafb47b2e",
    "66a72a158f9325a2bf67170",
    "c4243689fe85b1c51363a18",
    "0dff739414d1a1b34b1c270",
    "15b48830636c8b99894972e",
    "29a89c0d3de81d665489b0e",
    "4f126f37fa51cbe61bd6b94",
    "99c47239d0d97d3c84e0940",
    "1919b75119765621bb4f1e8",
    "09db12d731faee0b86df6b8",
    "488fc33df43fbdeea4eafb4",
    "827423ee40b675f756eb5fe",
    "abe197c484cb74757144a9a",
    "2b500e4bc0ec5a6d2bdbdd0",
    "c474aa53d70218761669360",
    "8eba1a13db3390bd6718cec",
    "753844673a27782cc42012e",
    "06ff83a145c37035a5c1268",
    "3b37417858cc2dd33ec3f62",
    "9a4a5a28ee17ca9c324842c",
    "bc29f465309c977e89610a4",
    "2663ae6ddf8b5ce2bb29488",
    "46f231efe457034c1814418",
    "3fb2ce85abe9b0c72e06fbe",
    "de87481f282c153971a0a2e",
    "fcd7ccf23c69fa99bba1412",
    "f0261447e9490ca8e474cec",
    "4410115818196f95cdd7012",
    "088fc31df4bfbde2a4eafb4",
    "b8fef1b6307729fb0a078c0",
    "5afea7acccb77bbc9d99a90",
    "49a7016ac653f65ecdc9076",
    "1944d085be4e7da8d6cc7d0",
    "251f62adc4032f0ee714002",
    "56471f8702a0721e00b12b8",
    "2b8e4923f2dd51e2d537fa0",
    "6b550a40a66f4755de95c26",
    "a18ad28d4e27fe92a4f6c84",
    "10c2e586388cb82a3d80758",
    "ef34a41817ee02133db2eb0",
    "7e9c0c54325a9c15836e000",
    "3693e572d1fde4cdf079e86",
    "bfb2cec5abe1b0c72e07fbe",
    "7ee18230c583cccc57d4b08",
    "a066cb2fedafc9f52664126",
    "bb23725abc47cc5f4cc4cd2",
    "ded9dba3bee40c59b5609b4",
    "d9a7016ac653e6decdc9036",
    "9ad46aed5f707f280ab5fc4",
    "e5921c77822587316d7d3c2",
    "4f14da8242a8b86dca73352",
    "8b8b507ad467d4441df770e",
    "22831c9cf1169467ad04b68",
    "213b838fe2ae54c38ee7180",
    "5d926b6dd71f085181a4e12",
    "66ab79d4b29ee6e69509e56",
    "958148682d748a38dd68baa",
    "b8ce020cf069c32a723ab14",
    "f4331d6d461607e95752746",
    "6da23ba424b9596133cf9c8",
    "a636bcbc7b30c5fbeae67fe",
    "5cb0d86a07df654a9089a20",
    "f11f106848780fc9ecdd80a",
    "1fbb5364fb8d2c9d730d5ba",
    "fcb86bc70a50c9d02a5d034",
    "a534433029eac15f322e34c",
    "c989d9c7c3d3b8c55d75130",
    "7bb38b2f0186d46643ae962",
    "2644ebadeb44b9467d1f42c",
    "608cc857594bfbb55d69600"
]

for data_g in DATA_G:
    rest = data_g
    rowstr = '{ '
    while len(rest) > 0:
        byte_hex = rest[:2]
        if len(byte_hex) == 1:
            rowstr += f'0x{byte_hex}0, '
        else:
            rowstr += f'0x{byte_hex}, '
        rest = rest[2:]
    rowstr = rowstr[:-2] + ' },'
    print(rowstr)

