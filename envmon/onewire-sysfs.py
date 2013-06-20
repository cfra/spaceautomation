"""
Onewire interface using w1 sysfs
"""

import os
import re
import sys
import time

class OnewireException(Exception):
	pass

_sensors = {}
with open(os.path.join(os.path.realpath(os.path.dirname(__file__)), 'onewire-sensors.txt'), 'r') as sensors_file:
	for line in sensors_file:
		line = line.strip()
		address, name = line.split(' ', 1)
		_sensors[name] = address

def sensors():
	return list(_sensors.keys())

class SensorFacade(object):
	def __init__(self, addr, name):
		self._addr = addr
		self._name = name

	def get_temperature(self):
		addr = self._addr.replace('.', '-').lower()
		path = '/sys/devices/w1_bus_master1/{0}/w1_slave'.format(addr)

		with open(path, 'r') as w1_file:
			w1_data = w1_file.read()

		match = re.search(r'\st=(\d+)', w1_data)
		temp = float(match.group(1)) / 1000
		return temp

	@property
	def temperature(self):
		for i in range(1,3):
			try:
				rv = self.get_temperature()
			except Exception:
				sys.excepthook(*sys.exc_info())
				time.sleep(0.5)
				continue

			if rv > 65:
				continue
			if rv < -25:
				continue
			return rv
		raise OnewireException("Bus error for %s" % self._name)

def sensor(name):
	if name not in _sensors:
		raise OnewireException('Don\'t know about sensor %s' % name)

	return SensorFacade('%s' % _sensors[name], name)
