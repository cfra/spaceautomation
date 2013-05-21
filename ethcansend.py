import socket
from socket import AF_PACKET, SOCK_RAW, htons, ntohs
from struct import pack, unpack
from time import time, sleep
import json
import hashlib

ifindex = 3

s = socket.socket(AF_PACKET, SOCK_RAW, htons(0x88b7))
# mreq = pack('@iHH8s', ifindex, 0, 6, '\xff\x3a\xf6CAN\x00\x00')
# s.setsockopt(263, 1, mreq)

#		(addr, dlc) = unpack('>IB', s.data[:5])
import sys

addr = int(sys.argv[1])
data = ''
for d in sys.argv[2:]:
	data += chr(int(d))
dlc = len(data)
addr = 0xcc080440 + addr

data = pack('>IB', addr, dlc) + data + '\x00' * (3 + 8 - len(data))
	
src = '\x00\x04\x23\xb6\xde\xe4'
dst = '\xff\x3a\xf6CAN' 
# dst = '\xd2r0ket'
proto = 0x88b7
oui = '\x00\x80\x41'
subp = 0xaaaa
typ = 3

data = pack('<BII', typ, 0, 0) + data

key = ''.join([chr(i) for i in [
	0x2f, 0x5d, 0xb5, 0xe4, 0x59, 0x6d, 0xc5, 0xf1, 0xb0, 0xf4,
	0xc3, 0xee, 0x8a, 0xc8, 0xff, 0x06, 0xbc, 0x28, 0x54, 0x08,
	0xa6, 0xc2, 0x96, 0x72, 0xd9, 0x0d, 0x22, 0x76, 0xd1, 0x98,
	0x0a, 0xdb, 0xca, 0xfe, 0xba, 0xbe, 0xde, 0xad, 0xbe, 0xef,
]])

dgst = hashlib.sha256()
# dgst.update(key)
dgst.update(data)

data += dgst.digest()

data = pack('>6s6sH3sH', dst, src, proto, oui, subp) + data

s.sendto(data, ('eth0', 0))
#sleep(0.1)
#s.sendto(data, ('bond0.4', 0))
#sleep(0.1)
#s.sendto(data, ('bond0.4', 0))

