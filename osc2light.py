#!/usr/bin/env python

import hashlib
import liblo
import socket
import struct
import sys
import clock
import time
import threading

class TXOp(object):
    def __init__(self, cb, *args, **kwargs):
        self.cb = cb
        self.args = args
        self.kwargs = kwargs

    def perform(self):
        self.cb(*self.args, **self.kwargs)

class TXQueue(threading.Thread):
    def __init__(self, *args, **kwargs):
        # Queue params
        self.wait_iv = 0.10 # 0.07 seems to work, but just play it save
        self.sleep_iv = 0.025
        self.max_wait = 2.0 # This caps the queue len

        self.last_sent = 0
        self.queue = []
        self.queue_sync = threading.Condition()
        threading.Thread.__init__(self, *args, **kwargs)

    def run(self):
        while True:
            try:
                self.fetch_and_send()
            except Exception:
                print >>sys.stderr, "TXQueue Exception:"
                sys.excepthook(*sys.exc_info())

    def fetch_and_send(self):
        with self.queue_sync:
            while not len(self.queue):
                self.queue_sync.wait()
            item = self.queue.pop(0)

        # We can't send too often, this will cause messages to be lost
        while self.last_sent + self.wait_iv > clock.now():
            time.sleep(self.sleep_iv)
        item.perform()
        self.last_sent = clock.now()

    def append(self, txop):
        assert isinstance(txop, TXOp)
        with self.queue_sync:
            if len(self.queue) > (self.max_wait / self.wait_iv):
                print >>sys.stderr, "Queue len == %d. Drop." % len(self.queue)
                return # This limits queue size
            self.queue.append(txop)
            self.queue_sync.notify()


class LightServer(object):
    class Unknown(Exception):
        def __str__(self):
            return 'Unknown message'

    def __init__(self, port, interface):
        self.interface = interface
        self.raw_socket = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(0x88b7))
        self.osc_server = liblo.Server(port)
        self.osc_server.add_method(None, None, self.handle_message)
        self.txqueue = TXQueue()

    def run(self):
        self.txqueue.start()
        while True:
            self.osc_server.recv()

    def handle_message(self, path, args, types, src):
        try:
            prefix = '/dali'
            if path.startswith(prefix):
                self.handle_dali(path[len(prefix):], args, types, src)
                return
            prefix = '/1'
            if path.startswith(prefix):
                self.handle_touch(path[len(prefix):], args, types, src)
                return
            raise self.Unknown()
        except Exception, e:
            sys.excepthook(*sys.exc_info())
            print >>sys.stderr, "Error processing message '%s' from '%s': %s" % (path, src.get_url(), e)

    def handle_touch(self, path, args, types, src):
        prefix = '/fader'
        if not path.startswith(prefix):
            raise self.Unknown()
        lamp_number = int(path[len(prefix):])
        self.handle_lamp_bright(lamp_number, args, types, src)

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

        buf = chr(brightness)
        addr = 0xcc080440 + lamp

        buf = struct.pack('>IB', addr, len(buf)) + buf + '\x00' * (3 + 8 - len(buf))

        src = '\x00\x04\x23\xb6\xde\xe4'
        dst = '\xff\x3a\xf6CAN'
        proto = 0x88b7
        oui = '\x00\x80\x41'
        subp = 0xaaaa
        typ = 3

        buf = struct.pack('<BII', typ, 0, 0) + buf
        key = ''.join([chr(i) for i in [
                0x2f, 0x5d, 0xb5, 0xe4, 0x59, 0x6d, 0xc5, 0xf1, 0xb0, 0xf4,
                0xc3, 0xee, 0x8a, 0xc8, 0xff, 0x06, 0xbc, 0x28, 0x54, 0x08,
                0xa6, 0xc2, 0x96, 0x72, 0xd9, 0x0d, 0x22, 0x76, 0xd1, 0x98,
                0x0a, 0xdb, 0xca, 0xfe, 0xba, 0xbe, 0xde, 0xad, 0xbe, 0xef,
        ]])

        dgst = hashlib.sha256()
        # dgst.update(key)
        dgst.update(buf)

        buf += dgst.digest()
        buf = struct.pack('>6s6sH3sH', dst, src, proto, oui, subp) + buf

        self.txqueue.append(TXOp(self.raw_socket.sendto, buf, (self.interface, 0)))

if __name__ == '__main__':
    server = LightServer(4243, 'bond0.4')
    server.run()
