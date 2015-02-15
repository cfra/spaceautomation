"""
Onewire interface using w1 sysfs
"""

import os
import errno
import re
import sys
import time

class OnewireException(Exception):
	pass
class SensorNotPresent(OnewireException):
	pass
class ReadFailed(OnewireException):
	pass
class OutOfRange(OnewireException):
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

	def _path(self):
		addr = self._addr.replace('.', '-').lower()
		return '/sys/devices/w1_bus_master1/{0}/w1_slave'.format(addr)

	def get_temperature(self):
		path = self._path()

		try:	os.stat(path)
		except OSError, ose:
			if ose.errno == errno.ENOENT:
				raise SensorNotPresent, path
			else:
				raise

		with open(path, 'r') as w1_file:
			w1_data = w1_file.read()

		if w1_data.strip() == '':
			raise ReadFailed, path

		match = re.search(r'\st=(\d+)', w1_data)
		temp = float(match.group(1)) / 1000
		return temp

	@property
	def temperature(self):
		for i in range(2, 0,-1):
			try:
				rv = self.get_temperature()
				if rv > 65 or rv < -25:
					raise OutOfRange, (self._path(), rv)
				return rv
			except SensorNotPresent:
				raise
			except Exception, e:
				if i == 1:
					raise
				sys.excepthook(*sys.exc_info())
				time.sleep(0.5)

def sensor(name):
	if name not in _sensors:
		raise OnewireException('Don\'t know about sensor %s' % name)

	return SensorFacade('%s' % _sensors[name], name)
