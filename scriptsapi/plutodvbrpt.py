#!/usr/bin/env python3
"""
Control PlutoDVB2 enabled Pluto Tx switchover watching UDP status of a DATV demodulator channel
This is a way to implement a DATV repeater system
Uses PlutoDVB2 MQTT "mute" command to switch on/off the Tx
"""
from optparse import OptionParser # pylint: disable=deprecated-module

import json
import os
import socket
import signal
import traceback
import sys
import time
import requests
import paho.mqtt.client as mqttclient
import logging

logger = logging.getLogger(__name__)
mqtt_client = None
pluto_state = {}

FEC_TABLE = {
    0: "1/2",
    1: "2/3",
    2: "4/6",
    3: "3/4",
    4: "5/6",
    5: "7/8",
    6: "4/5",
    7: "8/9",
    8: "9/10",
    9: "1/4",
    10: "1/3",
    11: "2/5",
    12: "3/5"
}

CONSTEL_TABLE = {
    0: "BPSK",
    1: "QPSK",
    2: "PSK8",
    3: "APSK16",
    4: "APSK32",
    5: "APSK64E",
    6: "QAM16",
    7: "QAM64",
    8: "QAM256"
}

# ======================================================================
def signal_handler(signum, frame):
    """ Signal handler """
    logger.info("Signal handler called with signal %d", signum)
    # Clean up and exit
    # Close MQTT connection
    if mqtt_client:
        mqtt_client.disconnect()
    sys.exit(0)

signal.signal(signal.SIGINT, signal_handler)
signal.signal(signal.SIGTERM, signal_handler)

# ======================================================================
def get_input_options():
    """ Parse options """
# ----------------------------------------------------------------------
    parser = OptionParser(usage="usage: %%prog [-t]\n")
    parser.add_option("-a", "--sdr_address", dest="sdr_address", help="SDRangel address and port. Default: 127.0.0.1:8091", metavar="ADDRESS:PORT", type="string")
    parser.add_option("-d", "--device", dest="device_index", help="Index of device set. Default 0", metavar="INT", type="int")
    parser.add_option("-c", "--channel", dest="channel_index", help="Index of DATV demod channel. Default 0", metavar="INT", type="int")
    parser.add_option("-A", "--pluto_address", dest="pluto_address", help="Pluto MQTT address and port. Mandatory", metavar="ADDRESS", type="string")
    parser.add_option("-P", "--pluto_port", dest="pluto_port", help="Pluto MQTT port. Default 1883", metavar="INT", type="int")
    parser.add_option("-C", "--callsign", dest="callsign", help="Amateur Radio callsign", metavar="CALLSIGN", type="string")
    parser.add_option("-l", "--clone", dest="clone", help="Clone symbol rate, constellation and fec to Pluto", metavar="BOOL", action="store_true", default=False)

    (options, args) = parser.parse_args()
    if options.sdr_address is None:
        options.sdr_address = "127.0.0.1:8091"
    if options.device_index is None:
        options.device_index = 0
    if options.channel_index is None:
        options.channel_index = 0
    if options.pluto_port is None:
        options.pluto_port = 1883
    if options.pluto_address is None:
        raise RuntimeError("Pluto address (-A or --pluto_address) is mandatory")
    if options.callsign is None:
        raise RuntimeError("Callsign (-C or --callsign) is mandatory")

    return options

# ======================================================================
def pluto_state_change(state, value):
    """ Change Pluto state """
# ----------------------------------------------------------------------
    global pluto_state
    if state not in pluto_state:
        pluto_state[state] = value
    else:
        pluto_state[state] = value
    logger.debug("Pluto state changed: %s = %s", state, value)
    return

# ======================================================================
def connect_mqtt(options):
    """ Connect to Pluto MQTT broker """
# ----------------------------------------------------------------------
    global mqtt_client
    mqtt_client = mqttclient.Client()
    mqtt_client.enable_logger(logger)
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message
    mqtt_client.on_subscribe = on_subscribe

    # Connect to the MQTT broker
    logger.info("Connecting to Pluto MQTT broker at %s:%d", options.pluto_address, options.pluto_port)
    try:
        mqtt_client.connect(options.pluto_address, options.pluto_port, 60)
        mqtt_client.loop_start()
    except Exception as ex:
        raise RuntimeError(f"Failed to connect to Pluto MQTT broker: {ex}")
    logger.info("Connected to Pluto MQTT broker at %s:%d", options.pluto_address, options.pluto_port)
    return mqtt_client

# ======================================================================
def on_connect(client, userdata, flags, rc):
    """ Callback for MQTT connection """
# ----------------------------------------------------------------------
    logger.info("Connected to MQTT broker with result code %d", rc)
    return

# ======================================================================
def on_message(client, userdata, msg):
    """ Callback for MQTT message """
# ----------------------------------------------------------------------
    logger.debug("Received message on topic %s: %s", msg.topic, msg.payload)
    # Parse the message
    try:
        if msg.topic.endswith("/mute"):
            logger.debug("Tx mute message received")
            pluto_state_change("muted", msg.payload == b'1')
        elif msg.topic.endswith("/sr"):
            logger.debug("Tx symbol rate message received: %d", int(msg.payload))
            pluto_state_change("symbol_rate", int(msg.payload))
        elif msg.topic.endswith("/constel"):
            logger.debug("Tx constellation message received: %s", msg.payload.decode('utf-8'))
            pluto_state_change("constellation", msg.payload.decode('utf-8'))
        elif msg.topic.endswith("/fec"):
            logger.debug("Tx fec message received: %s", msg.payload.decode('utf-8'))
            pluto_state_change("fec", msg.payload.decode('utf-8'))
        else:
            message = json.loads(msg.payload)
            logger.debug("Message payload is JSON: %s", message)
    except json.JSONDecodeError as ex:
        logger.error("Failed to decode JSON message: %s", ex)
    except Exception as ex:
        logger.error("Failed to handle message: %s", ex)
    return

# ======================================================================
def on_subscribe(client, userdata, mid, granted_qos):
    """ Callback for MQTT subscription """
# ----------------------------------------------------------------------
    logger.info("Subscribed to topic with mid %d and granted QoS %s", mid, granted_qos)
    return

# ======================================================================
def subscribe_to_pluto(options, mqtt_client):
    """ Subscribe to Pluto MQTT broker """
# ----------------------------------------------------------------------
    if mqtt_client is None:
        raise RuntimeError("MQTT client is not connected")
    tx_base_topic = f"dt/pluto/{options.callsign.upper()}/tx"
    mute_dt_topic = f"{tx_base_topic}/mute"
    logger.info("Subscribing to topic %s", mute_dt_topic)
    mqtt_client.subscribe(mute_dt_topic)
    logger.info("Subscribed to topic %s", mute_dt_topic)
    if options.clone:
        # Subscribe to Pluto Tx symbol rate
        symbol_rate_topic = f"{tx_base_topic}/dvbs2/sr"
        logger.info("Subscribing to topic %s", symbol_rate_topic)
        mqtt_client.subscribe(symbol_rate_topic)
        # Subscribe to Pluto Tx constellation
        constellation_topic = f"{tx_base_topic}/dvbs2/constel"
        logger.info("Subscribing to topic %s", constellation_topic)
        mqtt_client.subscribe(constellation_topic)
        # Subscribe to Pluto Tx fec
        fec_topic = f"{tx_base_topic}/dvbs2/fec"
        logger.info("Subscribing to topic %s", fec_topic)
        mqtt_client.subscribe(fec_topic)
    return

# ======================================================================
def mute_pluto_tx(options, mute):
    """ Mute or unmute Pluto Tx """
# ----------------------------------------------------------------------
    global mqtt_client
    if mqtt_client is None:
        raise RuntimeError("MQTT client is not connected")
    topic = f"cmd/pluto/{options.callsign.upper()}/tx/mute"
    message = b'1' if mute else b'0'
    logger.info("Publishing message to topic %s: %s", topic, message)
    mqtt_client.publish(topic, message)
    logger.info("Published message to topic %s", topic)
    # Update Pluto state
    pluto_state_change("muted", mute)
    return

# ======================================================================
def set_pluto_tx_dvbs2(options, symbol_rate, constellation, fec):
    """ Set Pluto Tx DVBS2 parameters """
# ----------------------------------------------------------------------
    global mqtt_client
    if mqtt_client is None:
        raise RuntimeError("MQTT client is not connected")
    topic_dvbs2 = f"cmd/pluto/{options.callsign.upper()}/tx/dvbs2"
    topic_sr = f"{topic_dvbs2}/sr"
    topic_constel = f"{topic_dvbs2}/constel"
    topic_fec = f"{topic_dvbs2}/fec"
    logger.info("Publishing message to topic %s: %d", topic_sr, symbol_rate)
    mqtt_client.publish(topic_sr, symbol_rate)
    logger.info("Published message to topic %s", topic_sr)
    logger.info("Publishing message to topic %s: %s", topic_constel, constellation)
    mqtt_client.publish(topic_constel, constellation)
    logger.info("Published message to topic %s", topic_constel)
    logger.info("Publishing message to topic %s: %s", topic_fec, fec)
    mqtt_client.publish(topic_fec, fec)
    logger.info("Published message to topic %s", topic_fec)
    # Update Pluto state
    pluto_state_change("symbol_rate", symbol_rate)
    pluto_state_change("constellation", constellation)
    pluto_state_change("fec", fec)
    return

# ======================================================================
def monitor_datv_demod(options):
    """ Monitor DATV demodulator channel and control Pluto Tx """
# ----------------------------------------------------------------------
    # Check DATV demodulator channel status
    sdrangel_url = f"http://{options.sdr_address}/sdrangel"
    report_url = f"{sdrangel_url}/deviceset/{options.device_index}/channel/{options.channel_index}/report"
    response = requests.get(report_url)
    if response.status_code != 200:
        raise RuntimeError(f"Failed to read report at {report_url}")
    datv_channel_report = response.json().get("DATVDemodReport", None)
    if not datv_channel_report:
        raise RuntimeError(f"Failed to read DATV demodulator report at {report_url}")
    udp_running = datv_channel_report.get("udpRunning", None)
    if udp_running is None:
        raise RuntimeError(f"Failed to read udpRunning in {datv_channel_report}")
    logger.debug("DATV UDP: %d", udp_running)
    if "muted" in pluto_state and pluto_state["muted"] == udp_running or "muted" not in pluto_state:
        logger.info("Pluto Tx %s", "muted" if not udp_running else "unmuted")
        mute_pluto_tx(options, not udp_running)
        logger.info("Pluto state: %s", pluto_state)
    if options.clone and datv_channel_report.get("setByModcod", None):
        mod = datv_channel_report.get("modcodModulation", -1)
        logger.debug("DATV Modulation: %s", CONSTEL_TABLE.get(mod, "Unknown"))
        fec = datv_channel_report.get("modcodCodeRate", -1)
        logger.debug("DATV FEC: %s", FEC_TABLE.get(fec, "Unknown"))
        symbol_rate = datv_channel_report.get("symbolRate", 0)
        logger.debug("DATV Symbol Rate: %d", symbol_rate)
        if "symbol_rate" in pluto_state and pluto_state["symbol_rate"] == symbol_rate and \
           "constellation" in pluto_state and pluto_state["constellation"].upper() == CONSTEL_TABLE.get(mod, "Unknown") and \
           "fec" in pluto_state and pluto_state["fec"] == FEC_TABLE.get(fec, "Unknown"):
            logger.debug("Pluto Tx parameters unchanged")
        else:
            logger.info("Pluto Tx parameters changed")
            set_pluto_tx_dvbs2(options, symbol_rate, CONSTEL_TABLE.get(mod, "Unknown").lower(), FEC_TABLE.get(fec, "Unknown"))
            logger.info("Pluto state: %s", pluto_state)
    return

# ======================================================================
def main():
    """ Main program """
# ----------------------------------------------------------------------
    try:
        FORMAT = '%(asctime)s %(levelname)s %(message)s'
        logging.basicConfig(format=FORMAT, level=logging.INFO)
        options = get_input_options()

        connect_mqtt(options)
        subscribe_to_pluto(options, mqtt_client)

        # Run forever
        logger.info("Start monitoring SDRangel channel %d:%d at %s", options.device_index, options.channel_index, options.sdr_address)
        while True:
            # Monitor DATV demodulator channel
            monitor_datv_demod(options)

            # Sleep for a while before checking again
            time.sleep(1)

    except Exception as ex: # pylint: disable=broad-except
        tb = traceback.format_exc()
        print(f"Exception caught {ex}")
        print(tb, file=sys.stderr)
        sys.exit(1)

# ======================================================================
if __name__ == "__main__":
    main()
