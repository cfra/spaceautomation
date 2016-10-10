#!/usr/bin/python

import time
import sys

import onewire
import socket

CARBON_SERVER = '2a02:238:f02a:8e2f:1:67:7261:7068' # 'sonar.local.sublab.org'
CARBON_PORT = 2003

prefix = "sublab.beaglebone.env."

sock = socket.socket(socket.AF_INET6)
sock.connect((CARBON_SERVER, CARBON_PORT))

sensors = onewire.sensors()
sensobj = dict([(sensor, onewire.sensor(sensor)) for sensor in sensors])
vals = {}

while True:
    nextslot = time.time() + 65
    nextslot = nextslot - (nextslot % 60)

    time.sleep(max(nextslot - len(sensors) * 0.8 - time.time(), 0))

    for sensor in sensors:
        try:
            vals.setdefault(sensor, None)
            vals[sensor] = sensobj[sensor].temperature
        except onewire.SensorNotPresent, e:
            print >>sys.stderr, sensor, 'SensorNotPresent:', str(e)
        except onewire.OutOfRange, e:
            # print >>sys.stderr, sensor, 'OutOfRange:', str(e)
            pass
        except Exception:
            print >>sys.stderr, "On %s: Could not retrieve temperature for '%s':" % (
                time.strftime('%a, %d %b %Y %T %z'), sensor)
            sys.excepthook(*sys.exc_info())
            print >>sys.stderr, '========================================'

    time.sleep(max(nextslot - time.time(), 0))
    goodvals = [sensor for sensor in sensors if vals.get(sensor, None) is not None]
    print '%d of %d sensors have data' % (len(goodvals), len(sensors))
    message = ''.join(["%s %f %d\n" % (prefix + sensor, vals[sensor], nextslot) for sensor in goodvals])
    if message != '':
        # print message
        sock.sendall(message)
