#!/usr/bin/env python
# vim: set expandtab ts=4:

from gi.repository import GLib, Soup
import json

ml = GLib.MainLoop()

def callback(new, old):
    try:
        import datadiff
        if old is None:
            return
        print datadiff.diff(old, new)
    except ImportError:
        print 'received longpoll update, datadiff not available'

prevjson = None

LPURL = 'http://beaglebone.local.sublab.org/longpoll'
uri = Soup.URI.new(LPURL)

#
# TODO: add timer that aborts/kills previous requests after a timeout
#       for proper error recovery
#
# Note: message.status_code contains HTTP codes _or_ libsoup specific errors
#       (status_code < 100 => libsoup)
#

def cb(session, message, cbarg):
    global prevjson

    def request_next():
        uri.set_query(prevjson['ref'])
        get = Soup.Message.new_from_uri("GET", uri)
        ss.queue_message(get, cb, None)

    if message.status_code != 200:
        print 'cb error:', message.status_code
        if prevjson is None:
            print 'first request failed'
            ml.quit()
        request_next()
        return

    try:
        rdata = message.response_body.flatten().get_data()
        if rdata == '':
            request_next()
            return

        njson = json.loads(rdata)
        print 'CB OK', njson['ref']

        if prevjson is None or njson['ref'] != prevjson['ref']:
            callback(njson['data'],
                prevjson['data'] if prevjson is not None else None)
        prevjson = njson

    finally:
        request_next()

ss = Soup.SessionAsync.new()
get = Soup.Message.new_from_uri("GET", uri)
ss.queue_message(get, cb, None)
del get

ml.run()

