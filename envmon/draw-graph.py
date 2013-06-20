#!/usr/bin/python
import colorgen
import rrdtool
import time
import onewire
import sys

dir_prefix = '/home/nihilus/envmon/'

datasets = sorted([ (sensor, 'onewire-temp-%s.rrd' % sensor) for sensor in onewire.sensors() ], key = lambda x:x[0])
datasets += [ ('Outside', 'env-outside.rrd') ]

def graph(name, duration):
	print >>sys.stderr, "Graphing %s" % name
	end_struct = time.localtime(time.time() - 450)
	end = time.mktime(end_struct) - end_struct.tm_sec - 60 * (end_struct.tm_min % 5)

	prefix = [
		name,
		'-E',
		'--end', '%d' % end,
		'--start', 'end-%ds' % duration,
		'--width', '800',
		'--height', '300'
	]
	
	body = []
	cg = colorgen.ColorGen()
	for dataset_name, dataset_rrd in datasets:
		body.append('DEF:temperature-%(name)s=%(filename)s:temperature:AVERAGE:start=end-%(duration)d' % {
				'name': dataset_name,
				'filename': dir_prefix + dataset_rrd,
				'duration': duration + 3600
		})
		color = cg.get_next()
		color = (int(color[0] * 255), int(color[1] * 255), int(color[2] * 255))
		body.append('LINE2:temperature-%(name)s#%(r)02x%(g)02x%(b)02x:%(name)s' % {
			'name': dataset_name,
			'r': color[0],
			'g': color[1],
			'b': color[2]
		})
	rrdtool.graph(prefix + body)
	print >>sys.stderr, "Done with %s" % name

graph('env-day.png', 86400)
graph('env-week.png', 604800)
graph('env-month.png', 2592000)
graph('env-year.png', 31557600)
