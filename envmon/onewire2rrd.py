#!/usr/bin/python

import time
import sys

import onewire
import rrdlog

while True:
	for sensor in onewire.sensors():
		try:
			rrdlog.TempLog('onewire-temp-%s.rrd' % sensor).update(onewire.sensor(sensor).temperature)
		except Exception:
			print >>sys.stderr, "On %s: Could not retrieve temperature for '%s':" % (
				time.strftime('%a, %d %b %Y %T %z'), sensor)
			sys.excepthook(*sys.exc_info())
			print >>sys.stderr, '========================================'
	time.sleep(300-len(onewire.sensors()))
