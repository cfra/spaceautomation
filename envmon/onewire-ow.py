"""
Onewire interface using owserver
"""

import os
import ow

class OnewireException(Exception):
	pass

ow.init('127.0.0.1:4304')

_sensors = {}
with open(os.path.join(os.path.realpath(os.path.dirname(__file__)), 'onewire-sensors.txt'), 'r') as sensors_file:
	for line in sensors_file:
		line = line.strip()
		address, name = line.split(' ', 1)
		_sensors[name] = address

def sensors():
	return list(_sensors.keys())

class SensorFacade(object):
	def __init__(self, sensor, name):
		self._sensor = sensor
		self._name = name

	def __getattr__(self, name):
		return getattr(self._sensor, name)

	@property
	def temperature(self):
		for i in range(1,3):
			rv = float(self._sensor.temperature)
			if rv > 65:
				continue
			if rv < -25:
				continue
			return rv
		raise OnewireException("Bus error for %s" % self._name)

def sensor(name):
	if name not in _sensors:
		raise OnewireException('Don\'t know about sensor %s' % name)

	return SensorFacade(ow.Sensor('/%s' % _sensors[name]), name)
