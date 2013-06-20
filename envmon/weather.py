import contextlib
import urllib2
import xml.etree.ElementTree

class WeatherData(object):
	def __init__(self):
		pass

def weather(woeid):
	"""
	Takes a woeid as argument (see yahoo api docs) and returns a dictionary
	containing weather data.
	"""

	with contextlib.closing(urllib2.urlopen('http://weather.yahooapis.com/forecastrss?w=%s&u=c' % woeid, timeout=2.0)) as request:
		tree = xml.etree.ElementTree.fromstring(request.read())
	
	yweather = 'http://xml.weather.yahoo.com/ns/rss/1.0'
	results = WeatherData()
	
	results.temperature = float(tree.find('.//{%s}condition' % yweather).get('temp'))
	results.humidity = float(tree.find('.//{%s}atmosphere' % yweather).get('humidity'))

	return results
