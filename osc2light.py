#!/usr/bin/env python

import liblo
import socket
import struct
import sys

class LightServer(object):
    class Unknown(Exception):
        def __str__(self):
            return 'Unknown message'

    def __init__(self, port, interface):
        self.interface = interface
        self.raw_socket = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(0x88b7))
        self.osc_server = liblo.Server(port)
        self.osc_server.add_method(None, None, self.handle_message)

    def run(self):
        while True:
            self.osc_server.recv()

    def handle_message(self, path, args, types, src):
        try:
            prefix = '/dali'
            if path.startswith(prefix):
                self.handle_dali(path[len(prefix):], args, types, src)
            else:
                raise self.Unknown()
        except Exception, e:
            print >>sys.stderr, "Error processing message '%s' from '%s': %s" % (path, src.get_url(), e)

    def handle_dali(self, path, args, types, src):
        prefix = '/lamps'
        if path.startswith(prefix):
            self.handle_dali_lamp(path[len(prefix):], args, types, src)
        else:
            raise self.Unknown()

    def handle_dali_lamp(self, path, args, types, src):
        path_parts = path.split('/')
        while len(path_parts) and not path_parts[0]:
            path_parts = path_parts[1:]
        lamp_number = int(path_parts[0])
        self.handle_lamp(lamp_number, '/'.join(path_parts[1:]), args, types, src)

    def handle_lamp(self, lamp, path, args, types, src):
        if path == 'bright':
            self.handle_lamp_bright(lamp, args, types, src)
        else:
            raise self.Unknown()

    def handle_lamp_bright(self, lamp, args, types, src):
        arguments = zip(args, types)
        if not arguments:
            raise ValueError("Brightness expects a value")
        if arguments[0][1] in 'ih':
            brightness = arguments[0][0]
        elif arguments[0][1] in 'fd':
            brightness = int(255 * arguments[0][0])
        else:
            raise ValueError("Unexpected Brightness value")

        if brightness < 5:
            brightness = 0
        else:
            brightness = 70 + float(brightness) * ((255.0 - 70) / 255)
            brightness = int(brightness)

        if brightness >= 255:
            brightness = 254

        src = '\x00\x04\x23\xb6\xde\xe4'
        dst = '\xff\x3a\xf6CAN'
        proto = 0x88b7
        oui = '\x00\x80\x41'
        subp = 0xaaaa
        typ = 2
        buf = struct.pack('>6s6sH3sHB', dst, src, proto, oui, subp, typ)

        addr = 0xcc080440 + lamp
        buf += struct.pack('>IBB', addr, 1, brightness)

        self.raw_socket.sendto(buf, (self.interface, 0))

if __name__ == '__main__':
    server = LightServer(4243, 'bond0.4')
    server.run()
