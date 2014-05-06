# This program plays a "little" sound when the door
# is opened. This is helpful e.g. when sitting back
# in the smokers lounge but wanting the door to be
# unlocked.

import subprocess
import os
import urllib2
import json
import sys
import time

if __name__ == '__main__':
    workdir = os.path.realpath(os.path.dirname(__file__))
    was_closed = False # Only react to changes
    last_ref = None

    while True:
        time.sleep(1)

        try:
            r = None
            req_url = 'http://beaglebone.local.sublab.org/longpoll'
            if last_ref is not None:
                req_url += '?' + last_ref
            r = urllib2.urlopen(req_url, timeout=120)
            data = json.loads(r.read())
        except Exception:
            sys.excepthook(*sys.exc_info())
            last_ref = None
            continue
        finally:
            if r is not None:
                r.close()

        last_ref = data['ref']
        data = data['data']

        closed = data['door.left']['value']
        if was_closed and not closed:
            print "Door has been opened!"
            subprocess.call(['paplay', '--volume=32661', os.path.join(workdir, 'doorwatch.wav')])
        if not was_closed and closed:
            print "Door has been closed."
        was_closed = closed
