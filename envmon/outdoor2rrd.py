#!/usr/bin/python

import time
import sys

import rrdlog
import weather

location = '20065491' # Leipzig

while True:
	try:
		data = weather.weather(location)
		rrdlog.TempLog('env-outside.rrd').update(data.temperature)
	except Exception:
		sys.stderr.write('Exception occured at %s:\n' % time.strftime('%a, %d %b %Y %T %z'))
		sys.excepthook(*sys.exc_info())
		sys.stderr.write('========================================\n')
		time.sleep(60)
	else:
		time.sleep(300)
