#!/usr/bin/env python

import irc.bot
import json, time
import traceback

import settings

svcpw = settings.bot_svcpw

class TestBot(irc.bot.SingleServerIRCBot):
    def __init__(self, channel, nickname, server, port=6667):
        irc.bot.SingleServerIRCBot.__init__(self, [(server, port)], nickname, nickname)
        self.channel = channel

    def on_nicknameinuse(self, c, e):
        c.nick(c.get_nickname() + "_")

    def on_welcome(self, c, e):
        c.privmsg('NickServ', 'identify sublab|open %s' % (svcpw, ))
        self.refresh()
        c.join(self.channel)

    def refresh(self):
        c = self.connection
        havenick = c.get_nickname()
        curtime = time.time()

        try:
            sdata = json.load(file('/home/services/http/subcan.json', 'r'))
            door = sdata[u'door.lock']

            if door[u'ts'] < curtime - 120:
                wantnick = 'sublab|error'
            elif door[u'text'] == u'closed':
                wantnick = 'sublab|closed'
            elif door[u'text'] == u'open':
                wantnick = 'sublab|open'
            else:
                wantnick = 'sublab|error'
        except Exception, e:
            traceback.print_exc()
            wantnick = 'sublab|error'

        if wantnick != havenick:
            print int(curtime), 'nick:', havenick, '->', wantnick
            c.privmsg('NickServ', 'ghost %s %s' % (wantnick, svcpw))
            c.privmsg('NickServ', 'release %s %s' % (wantnick, svcpw))
            c.nick(wantnick)
        self.ircobj.execute_delayed(5, self.refresh)

def main():
    bot = TestBot('#sublab', 'sublab|closed', '172.22.24.1', 6667)
    bot.start()

if __name__ == "__main__":
    main()
