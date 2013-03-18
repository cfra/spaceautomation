import socket, select
from socket import AF_PACKET, SOCK_RAW, htons, ntohs
from struct import pack, unpack
from time import time, sleep, strftime
import json
import os, traceback
import hub_ctrl, usb
import contextlib
import linux
import sys
import settings


def ts():
	return strftime('%Y-%m-%d %H:%M:%S')

class SubCANDevice(object):
	def __init__(s, addr, name, dsize = 1):
		s.addr = addr
		s.name = name
		s.dsize = dsize
		s.lastval = None
		s.lastupd = None
		s.lastchg = None
		s.actorval = None
		s.actorupd = None
		s.actorchg = None
	def dict(s):
		rv = {}
		rv['addr'] = s.addr
		return rv

class SubCANBool(SubCANDevice):
	def __init__(s, addr, name, falseval, trueval):
		SubCANDevice.__init__(s, addr, name, 1)
		s.vals = [falseval, trueval]
	def __str__(s):
		if s.lastval is None:
			return 'None'
		return s.vals[s.lastval & 1]
	def dict(s):
		rv = SubCANDevice.dict(s)
		rv['klass'] = 'beancounter'
		if s.lastval is not None:
			rv['raw'] = s.lastval
			rv['value'] = bool(s.lastval & 1)
			rv['text'] = s.vals[s.lastval & 1]
			rv['ts'] = int(s.lastupd)
			rv['tschg'] = int(s.lastchg)
		return rv

class SubCANDALI(SubCANDevice):
	def __str__(s):
		if s.lastval is None or s.actorval is None:
			return ''
		return 'set: %02x actual: %02x' % (s.actorval, s.lastval)
	def dict(s):
		rv = SubCANDevice.dict(s)
		rv['klass'] = 'light'
		if s.lastval is not None:
			rv['actual'] = s.lastval
			rv['actual_ts'] = int(s.lastupd)
			rv['actual_tschg'] = int(s.lastchg)
		if s.actorval is not None:
			rv['set'] = s.actorval
			rv['set_ts'] = int(s.actorupd)
			rv['set_tschg'] = int(s.actorchg)
		return rv

devices = [
	SubCANBool(0x100, 'door.right',	'open', 'closed'),
	SubCANBool(0x101, 'door.left',	'open', 'closed'),
	SubCANBool(0x102, 'door.light',	'triggered', 'normal'),
	SubCANBool(0x103, 'door.lock',	'open', 'closed'),
	SubCANDALI(0x441, 'dali.lounge_buehne'),
	SubCANDALI(0x442, 'dali.lounge_buero'),
	SubCANDALI(0x443, 'dali.lounge_bar'),
	SubCANDALI(0x444, 'dali.lounge_durchreiche'),
#	SubCANDALI(0x47f, 'dali.lswitch'),
]
def find_dev(addr):
	for dev in devices:
		if addr in range(dev.addr, dev.addr + dev.dsize):
			return dev
	return None

class SubCANFrame(object):
	def __init__(s, frame):
		s.frame = frame
		s.dstaddr = frame.eid & 0xfff

	def process(s):
		data = s.frame.payload
		addr = s.dstaddr
		while len(data) > 0:
			dev = find_dev(addr)
			if dev is None:
				addr += 1
				data = data[1:]
			else:
				val = None
				devdata = data[:dev.dsize]
				if dev.dsize == 1:
					(val, ) = unpack('>B', devdata)
				elif dev.dsize == 2:
					(val, ) = unpack('>H', devdata)

				s.do_process(dev, devdata, val)
				addr += dev.dsize
				data = data[dev.dsize:]

	@classmethod
	def create(s, frame):
		for fclass in s.frametypes:
			if frame.sid == fclass.matchsid:
				return fclass(frame)
		return None

class SensorFrame(SubCANFrame):
	matchsid = 0xe60
	def do_process(s, dev, data, val):
		dev.lastupd = s.frame.ts
		if val != dev.lastval:
			dev.lastchg = s.frame.ts
		dev.lastval = val

class ActorFrame(SubCANFrame):
	matchsid = 0xcc0
	def do_process(s, dev, data, val):
		dev.actorupd = s.frame.ts
		if val != dev.actorval:
			dev.actorchg = s.frame.ts
		dev.actorval = val

SubCANFrame.frametypes = [SensorFrame, ActorFrame]

class MacAddr(object):
	def __init__(s, mac):
		s.mac = mac
	def __str__(s):
		return ':'.join(['%02x' % (ord(c),) for c in s.mac])

class NotACANFrame(Exception):
	pass
class InvalidCANFrame(Exception):
	pass

from collections import namedtuple
StatsTuple = namedtuple('StatsTuple', 'ethstat_tx_overrun, ethstat_tx_ok, ethstat_tx_error, ethstat_tx_fnord, ' +
	'ethstat_rx_overrun, ethstat_rx_ok, ethstat_rx_error, ethstat_lastrxerr, ' +
	'ethstat_again, ethstat_hasherr, ' +
	'mcp2515_errors, mcp2515_rx_ok, mcp2515_tx')

class Frame(object):
	def __init__(s, data):
		(dst, src, proto, oui, subp, typ) = unpack('>6s6sH3sHB', data[:20])
		(tsv, ) = unpack('<I', data[20:24])
		s.data = data[24:]
		s.dst = MacAddr(dst)
		s.src = MacAddr(src)
		s.proto = proto
		s.ts = time()
		s.tsr = tsv / 100.

		if s.proto != 0x88b7 or oui != '\x00\x80\x41' or subp != 0xaaaa or len(data) < 5:
			raise NotACANFrame('invalid protocol/OUI/subp or too short')
		if typ == 0x01:
			stats = []
			data = data[24:]
			while len(data) >= 4:
				stats.append(unpack('<I', data[:4])[0])
				data = data[4:]
			spretty = StatsTuple._make(stats[:len(StatsTuple._fields)])
			print >>debug_log, '%s stats: %s' % (ts(), repr(spretty))
			raise NotACANFrame('stats frame')

		if typ != 0x03:
			raise NotACANFrame('invalid type %02x' % (typ))

		(ts2, ) = unpack('<I', s.data[:4])
		(addr, dlc) = unpack('>IB', s.data[4:9])
		s.tsr2 = ts2 / 100.
		
		s.dlc = dlc & 0x0f
		s.is_eid = addr & 0x00080000
		s.is_rtr = bool(dlc & 0x40 if s.is_eid else addr & 0x00100000)
		s.addr = addr & 0xffe3ffff
		s.sid = addr >> 20
		s.eid = addr & 0x3ffff

		if s.is_eid:
			s.addrstr = '%03x-%05x' % (s.sid, s.eid)
		else:
			s.addrstr = '%03x-XXXXX' % (s.sid)

		if len(data) < 9 + s.dlc:
			raise InvalidCANFrame('truncated frame')
		s.payload = s.data[9:9+s.dlc]

	def __str__(s):
		return '%s <- %s  %8.2f d=%.2f %s %s: %s' % (
			s.dst, s.src, s.tsr, s.tsr - s.tsr2,
			s.addrstr, 'RTR:' if s.is_rtr else 'norm',
			' '.join(['%02x' % (ord(c), ) for c in s.payload]))

@contextlib.contextmanager
def USBHandle(dev):
	uh = dev.open()
	try:
		yield uh
	finally:
		del uh

def repower_r0ket():
	hubs = hub_ctrl.find_hubs(False, False)
	for h in hubs:
		if h['dev'].idVendor == 0x050f and h['dev'].idProduct == 0x0003:
			break
	else:
		print >>sys.stderr, 'could not find USB hub!'
		return

	with USBHandle(h['dev']) as uh:
		feat = hub_ctrl.USB_PORT_FEAT_POWER
		index = 1	# port no

		req = usb.REQ_CLEAR_FEATURE
		uh.controlMsg(requestType = hub_ctrl.USB_RT_PORT, request = req, value = feat, index = index, buffer = None, timeout = 1000)
		print >>debug_log, '%s: port powered off.' % (ts(), )
		sleep(2)	
		req = usb.REQ_SET_FEATURE
		uh.controlMsg(requestType = hub_ctrl.USB_RT_PORT, request = req, value = feat, index = index, buffer = None, timeout = 1000)
		print >>debug_log, '%s: port powered on.' % (ts(), )

	print >>sys.stderr, 'r0ket powercycled'

def main(interface):
	poller = select.poll()
	poller.register(s, select.POLLIN)
	timeout = 10
	while True:
		ev = poller.poll(timeout * 1000)
		if len(ev) == 0:
			# no stats frame for 10s ... r0ket stuck.
			try:	repower_r0ket()
			except:	traceback.print_exc()
			timeout = 20
			continue
		else:
			timeout = 10
	
		(data, addr) = s.recvfrom(65536)
		if addr[0] != interface:
			continue
		try:
			pkt = Frame(data)
		except NotACANFrame, e:
			if str(e) != 'stats frame':
				traceback.print_exc()
			continue
		except InvalidCANFrame, e:
			traceback.print_exc()
			continue
	
		scf = SubCANFrame.create(pkt)
		if scf is not None:
			scf.process()
	
		print >>debug_log, ts(), addr[0], pkt
	
		rd = {}
		for d in devices:
			rd[d.name] = d.dict()
		output_file = 'subcan.json'
		with open(output_file + '.new', 'w') as output:
			output.write(json.dumps(rd, sort_keys=True, indent=4))
		os.rename(output_file + '.new', output_file)

if __name__ == '__main__':
	interface = settings.ethcan_if

	ifindex = linux.if_nametoindex(interface)
	s = socket.socket(AF_PACKET, SOCK_RAW, htons(0x88b7))
	mreq = pack('@iHH8s', ifindex, 0, 6, '\xff\x3a\xf6CAN\x00\x00')
	s.setsockopt(263, 1, mreq)

	debug_log = open(settings.ethcan_log, 'a', buffering=1)
	main(interface)
