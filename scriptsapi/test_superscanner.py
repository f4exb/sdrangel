import unittest
import mock
import superscanner

# ======================================================================
def print_hex(bytestring):
    print('\\x' + '\\x'.join('{:02x}'.format(x) for x in bytestring))

# ======================================================================
def get_deviceset_info(deviceset_index):
    return {
        "channelcount": 4,
        "channels": [
            {
                "deltaFrequency": 170000,
                "direction": 0,
                "id": "NFMDemod",
                "index": 0,
                "title": "NFM Demodulator",
                "uid": 1590355926650308
            },
            {
                "deltaFrequency": -155000,
                "direction": 0,
                "id": "DSDDemod",
                "index": 1,
                "title": "DSD Demodulator",
                "uid": 1590355926718405
            },
            {
                "deltaFrequency": 170000,
                "direction": 0,
                "id": "NFMDemod",
                "index": 2,
                "title": "NFM Demodulator",
                "uid": 1590355926939766
            },
            {
                "deltaFrequency": -95000,
                "direction": 0,
                "id": "NFMDemod",
                "index": 3,
                "title": "NFM Demodulator",
                "uid": 1590355926945674
            }
        ],
        "samplingDevice": {
            "bandwidth": 768000,
            "centerFrequency": 145480000,
            "deviceNbStreams": 1,
            "deviceStreamIndex": 0,
            "direction": 0,
            "hwType": "AirspyHF",
            "index": 0,
            "sequence": 0,
            "serial": "c852a98040c73f93",
            "state": "running"
        }
    }

# ======================================================================
def set_channel_frequency(channel):
    pass

# ======================================================================
def set_channel_mute(channel):
    pass

# ======================================================================
class TestStringMethods(unittest.TestCase):

    def test_upper(self):
        self.assertEqual('foo'.upper(), 'FOO')

    def test_isupper(self):
        self.assertTrue('FOO'.isupper())
        self.assertFalse('Foo'.isupper())

    def test_split(self):
        s = 'hello world'
        self.assertEqual(s.split(), ['hello', 'world'])
        # check that s.split fails when the separator is not a string
        with self.assertRaises(TypeError):
            s.split(2)

# ======================================================================
class TestSuperScannerOptions(unittest.TestCase):

    def test_options_minimal(self):
        options = superscanner.get_input_options(["-ctoto"])
        self.assertEqual(options.config_file, 'toto')

# ======================================================================
class TestSuperScannerDecode(unittest.TestCase):

    def test_decode_bytes(self):
        msg_bytes = b'\x40\xd9\xab\x08\x00\x00\x00\x00' + \
            b'\xff\x00\x00\x00\x00\x00\x00\x00' + \
            b'\x69\x63\xbb\x55\x72\x01\x00\x00' + \
            b'\x00\x04\x00\x00' + \
            b'\x00\xb8\x0b\x00' + \
            b'\x04\x00\x00\x00'
        for i in range(1024):
            msg_bytes += b'\x00\x00\x00\x00'
        msg_struct = superscanner.decode_message(msg_bytes)
        self.assertEqual(msg_struct['fft_size'], 1024)

# ======================================================================
class TestSuperScannerProcessHotspots(unittest.TestCase):

    @mock.patch('superscanner.get_deviceset_info', side_effect=get_deviceset_info)
    def setUp(self, urandom_function):
        self.options = type('options', (object,), {})()
        self.options.address = '127.0.0.1'
        self.options.passes = 10
        self.options.api_port =  8091
        self.options.ws_port  = 8887
        self.options.config_file = 'toto'
        self.options.psd_input_file = None
        self.options.psd_output_file = None
        self.options.passes = 10
        self.options.margin = 3
        self.options.psd_fixed = None
        self.options.psd_exclude_higher = None
        self.options.psd_exclude_lower = None
        self.options.psd_graph = None
        self.options.group_tolerance = 1
        self.options.freq_round = 1
        self.options.freq_offset = 0
        superscanner.OPTIONS = self.options
        superscanner.CONFIG = {
            "deviceset_index": 0,
            "freqrange_exclusions": [
                [145290000, 145335000],
                [145692500, 145707500]
            ],
            "freqrange_inclusions": [
                [145170000, 145800000]
            ],
            "channel_info": [
                {
                    "index": 0,
                    "fc_pos": "center"
                },
                {
                    "index": 2
                },
                {
                    "index": 3
                }
            ]
        }
        superscanner.make_config()

    def test_make_config(self):
        self.assertEqual(superscanner.CONFIG['device_frequency'], 145480000)

    @mock.patch('superscanner.set_channel_frequency', side_effect=set_channel_frequency)
    @mock.patch('superscanner.set_channel_mute', side_effect=set_channel_mute)
    def test_process_hotspot(self, set_channel_frequency, set_channel_mute):
        hotspots1 = [
            {
                'begin': 145550000,
                'end': 145550000,
                'power': -50
            }
        ]
        superscanner.process_hotspots(hotspots1)
        channel_info = superscanner.CONFIG['channel_info']
        self.assertEqual(channel_info[0]['usage'], 1)
        self.assertEqual(channel_info[1]['usage'], 0)
        self.assertEqual(channel_info[2]['usage'], 0)
        self.assertEqual(channel_info[0]['frequency'], 145550000)
        hotspots2 = [
            {
                'begin': 145200000,
                'end': 145200000,
                'power': -35
            },
            {
                'begin': 145550000,
                'end': 145550000,
                'power': -50
            }
        ]
        superscanner.process_hotspots(hotspots2)
        channel_info = superscanner.CONFIG['channel_info']
        self.assertEqual(channel_info[0]['usage'], 1)
        self.assertEqual(channel_info[1]['usage'], 1)
        self.assertEqual(channel_info[2]['usage'], 0)
        self.assertEqual(channel_info[0]['frequency'], 145550000)
        self.assertEqual(channel_info[1]['frequency'], 145200000)
        hotspots3 = [
            {
                'begin': 145200000,
                'end': 145200000,
                'power': -35
            }
        ]
        superscanner.process_hotspots(hotspots3)
        channel_info = superscanner.CONFIG['channel_info']
        self.assertEqual(channel_info[0]['usage'], 0)
        self.assertEqual(channel_info[1]['usage'], 1)
        self.assertEqual(channel_info[2]['usage'], 0)
        self.assertEqual(channel_info[1]['frequency'], 145200000)


# ======================================================================
if __name__ == '__main__':
    unittest.main()

