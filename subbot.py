#!/usr/bin/env python

import irc.bot
import irc.buffer
import irc.client
import json, time
import urllib2
import sys
import contextlib
import signal
import settings
import select
import errno

svcpw = settings.bot_svcpw

import bot_backend
def reload_backend(signum, frame):
    try:
        reload(bot_backend)
    except Exception:
        print >>sys.stderr, "Error reloading bot backend:"
        sys.excepthook(*sys.exc_info())
    else:
        print >>sys.stderr, "Reloaded backend."
signal.signal(signal.SIGUSR1, reload_backend)

class PermissiveDecodingLineBuffer(irc.buffer.DecodingLineBuffer):
    errors = 'replace'
irc.client.ServerConnection.buffer_class = PermissiveDecodingLineBuffer

class TestBot(irc.bot.SingleServerIRCBot):
    def __init__(self, channel, nickname, server, port=6667):
        self.nickname_base = nickname
        self.status = 'closed'
        self.update_nick(False)

        irc.bot.SingleServerIRCBot.__init__(self, [(server, port)], self.nickname, self.nickname)
        self.channel = channel

    def start(self):
        self._connect()
        while True:
            try:
                self.ircobj.process_forever(30)
            except select.error, e:
                if e.args[0] == errno.EINTR:
                    continue
                raise

    def on_nicknameinuse(self, c, e):
        self.nickname_base += '_'
        self.update_nick()

    def on_welcome(self, c, e):
        c.privmsg('NickServ', 'identify %s %s' % (self.nickname, svcpw))
        self.refresh()
        c.join(self.channel)

    def on_pubmsg(self, c, e):
        try:
            bot_backend.on_pubmsg(self, c, e)
        except Exception:
            print >>sys.stderr, "Error in backend processing message:"
            sys.excepthook(*sys.exc_info())

    def update_nick(self, server_interaction=True):
        """Set new nickname from nickname base and current status"""
        wantnick = self.nickname_base + '|' + self.status

        if not server_interaction:
            self.nickname = wantnick
            return

        curtime = time.time()
        c = self.connection
        havenick = c.get_nickname()

        if wantnick != havenick:
            print int(curtime), 'nick:', havenick, '->', wantnick
            c.privmsg('NickServ', 'ghost %s %s' % (wantnick, svcpw))
            c.privmsg('NickServ', 'release %s %s' % (wantnick, svcpw))
            c.nick(wantnick)
            self.nickname = wantnick

    def refresh(self):
        curtime = time.time()
        try:
            with contextlib.closing(urllib2.urlopen('http://beaglebone.local.sublab.org/')) as json_stream:
                sdata = json.load(json_stream)
            door = sdata[u'door.lock']

            if door[u'ts'] < curtime - 120:
                self.status = 'error'
            elif door[u'text'] == u'closed':
                self.status = 'closed'
            elif door[u'text'] == u'open':
                self.status = 'open'
            else:
                self.status = 'error'
        except Exception, e:
            print >>sys.stderr, "Error in door status retrieval:"
            sys.excepthook(*sys.exc_info())
            self.status = 'error'

        self.update_nick()
        self.ircobj.execute_delayed(5, self.refresh)

def main(nickname, channel):
    bot = TestBot(channel, nickname, 'irc.hackint.org', 6667)
    bot.start()

if __name__ == "__main__":
    if sys.argv[0] == 'subbot.py':
        nickname = 'sublab'
        channel = '#sublab'
    else:
        nickname = 'sublab2'
        channel = '#sublab2'
    main(nickname, channel)
