# This program plays a "little" sound when the door
# is opened. This is helpful e.g. when sitting back
# in the smokers lounge but wanting the door to be
# unlocked.

import subprocess
import os
import urllib
import json
import sys
import time

if __name__ == '__main__':
    workdir = os.path.realpath(os.path.dirname(__file__))
    was_closed = False # Don't ding at startup

    while True:
        time.sleep(1)

        try:
            r = None
            r = urllib.urlopen('http://172.22.83.5/subcan.json')
            data = json.loads(r.read())
        except Exception:
            sys.excepthook(*sys.exc_info())
            continue
        finally:
            if r is not None:
                r.close()

        closed = data['door.left']['value']
        if was_closed and not closed:
            print "Door has been opened!"
            subprocess.call(['paplay', os.path.join(workdir, 'doorwatch.wav')])
        if not was_closed and closed:
            print "Door has been closed."
        was_closed = closed
