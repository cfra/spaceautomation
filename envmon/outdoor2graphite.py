#!/usr/bin/python

import time
import sys

import socket

import weather

location = '20065491' # Leipzig

CARBON_SERVER = 'sonar.local.sublab.org'
CARBON_PORT = 2003

prefix = "envmon.beaglebone_local_sublab_org."

sock = socket.socket()
sock.connect((CARBON_SERVER, CARBON_PORT))

while True:
    message = ''
    try:
        data = weather.weather(location)
        message += '%s %s %d\n' % (prefix + 'outside',
                                   data.temperature,
                                   time.time())
    except Exception:
        sys.stderr.write('Exception occured at %s:\n' % time.strftime('%a, %d %b %Y %T %z'))
        sys.excepthook(*sys.exc_info())
        sys.stderr.write('========================================\n')
        time.sleep(60)
    else:
        sock.sendall(message)
        time.sleep(300)

