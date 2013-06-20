#!/usr/bin/python

import time
import sys

import onewire
import socket

CARBON_SERVER = 'sonar.local.sublab.org'
CARBON_PORT = 2003

prefix = "envmon.beaglebone_local_sublab_org."

sock = socket.socket()
sock.connect((CARBON_SERVER, CARBON_PORT))

while True:
    message = ''
    for sensor in onewire.sensors():
        try:
            message += "%s %f %d\n" % (prefix + sensor, onewire.sensor(sensor).temperature, time.time())
        except Exception:
            print >>sys.stderr, "On %s: Could not retrieve temperature for '%s':" % (
                time.strftime('%a, %d %b %Y %T %z'), sensor)
            sys.excepthook(*sys.exc_info())
            print >>sys.stderr, '========================================'
    if message:
	print message
        sock.sendall(message)
    time.sleep(300-len(onewire.sensors()))
