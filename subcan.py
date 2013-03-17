import json
import sys
from lxml import etree
from time import time, sleep

def run():
	doc = etree.parse('subcan.svg')
	data = json.loads(file('/home/services/http/subcan.json').read())

	for i in ['door.left', 'door.right', 'door.lock']:
		cond = '%s=%s' % (i, data[i]['text'])
		print cond
		elems = doc.xpath('//svg:g[@inkscape:label="%s"]' % (cond,),
			namespaces = {
				'svg': 'http://www.w3.org/2000/svg',
				'inkscape': 'http://www.inkscape.org/namespaces/inkscape',
			})
		for e in elems:
			e.set('style', 'display:inline')

	for i in data.keys():
		if not i.startswith('dali.'):
			continue
		for j in ['set', 'actual']:
			#for i in [('dali.lounge', 'set'), ('dali.lounge', 'actual')]:
			elems = doc.xpath('//svg:text[@id="%s_%s"]/svg:tspan' % (i, j),
				namespaces = {
					'svg': 'http://www.w3.org/2000/svg',
				})
			print 'elems for %s_%s: %d' % (i, j, len(elems))
			for e in elems:
				try:
					text = data[i][j]
					if data[i][j + '_ts'] > time() - 300.:
						text = u'%d%%' % (text / 254. * 100.)
					else:
						text = u'<?>'
					e.text = text
				except KeyError:
					print 'error processing \'%s_%s\'' % (i, j)
					e.text = u'<?>'

	file('/home/services/http/subcan.svg', 'w').write(etree.tostring(doc))

while True:
	try:
		run()
	except Exception:
		sys.excepthook(*sys.exc_info())
	sleep(10.)
