"""
Onewire interface using w1 sysfs
"""

import os
import errno
import re
import sys
import time
import random

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
		self._backoff = None

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
		if self._backoff > time.time():
			return None

		try:
			rv = self.get_temperature()
			if rv > 65 or rv < -25:
				raise OutOfRange, (self._path(), rv)
			# print '\033[32m', self._name, 'read', rv, '\033[m'
			return rv
		except SensorNotPresent:
			self._backoff = time.time() + 2 + random.expovariate(1./3.) * 2
			# print '\033[31m', self._name, 'SensorNotPresent back-off', self._backoff - time.time(), '\033[m'
			raise
		except OutOfRange:
			self._backoff = time.time() + 15 + random.expovariate(1./4.) * 10
			# print '\033[33m', self._name, 'OutOfRange back-off', self._backoff - time.time(), '\033[m'
			raise

def sensor(name):
	if name not in _sensors:
		raise OnewireException('Don\'t know about sensor %s' % name)

	return SensorFacade('%s' % _sensors[name], name)
