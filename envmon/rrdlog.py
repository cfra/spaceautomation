import rrdtool
import os

class TempLog(object):
	def __init__(self, name):
		self.name = name
		if not os.path.exists(self.name):
			rrdtool.create(self.name, 'DS:temperature:GAUGE:600:U:U',
				'RRA:AVERAGE:0.5:1:17280', # Keep 5min snapshots for the last two months
				'RRA:AVERAGE:0.5:12:87660', # Keep 1h averages for 10 years (as if...)
			)
	def update(self, temperature):
		rrdtool.update(self.name, 'N:%f' % temperature)
