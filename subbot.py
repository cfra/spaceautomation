#!/usr/bin/env python

import irc.bot
import json, time
import traceback
import liblo
import urllib2

import settings

svcpw = settings.bot_svcpw

# Known lights, terminal symbols
lights_terminal = {}

for i in range(1,5):
    ident = '%02d' % i
    lights_terminal[ident] = 'osc.udp://172.22.83.5:4243/dali/lamps/%s/bright' % ident

# Production Rules/Aliases
lights_production = {
    u'lounge-stage':    [u'01'],
    u'lounge-office':   [u'02'],
    u'lounge-bar':      [u'03'],
    u'lounge-clock':    [u'04'],
    u'lounge-west':     [u'lounge-bar', u'lounge-clock'],
    u'lounge-east':     [u'lounge-stage', u'lounge-office'],
    u'lounge':          [u'lounge-east', u'lounge-west'],
}

class OSCMessage(object):
    def __init__(self, url):
        parsed = urllib2.urlparse.urlparse(url)
        self.path = parsed.path
        self.url = urllib2.urlparse.urlunparse((parsed.scheme, parsed.netloc, '', '', '', ''))

        self.msg = liblo.Message(self.path)

    def __getattr__(self, name):
        return getattr(self.msg, name)

    def send(self):
        liblo.send(self.url, self.msg)

class TestBot(irc.bot.SingleServerIRCBot):
    def __init__(self, channel, nickname, server, port=6667):
        self.nickname_base = nickname
        self.status = 'closed'
        self.update_nick(False)

        irc.bot.SingleServerIRCBot.__init__(self, [(server, port)], self.nickname, self.nickname)
        self.channel = channel

    def on_nicknameinuse(self, c, e):
        self.nickname_base += '_'
        self.update_nick()

    def on_welcome(self, c, e):
        c.privmsg('NickServ', 'identify %s %s' % (self.nickname, svcpw))
        self.refresh()
        c.join(self.channel)

    def on_pubmsg(self, c, e):
        message = e.arguments()[0]

        light_command_prefix = '!light'
        if message.startswith(light_command_prefix):
            nick = e.source().nick
            self.on_light_command(nick, message[len(light_command_prefix) + 1:])

    def on_light_command(self, nick, commandline):
        tokens = commandline.split(' ')
        if len(tokens) < 1:
            self.light_usage()
            return

        if tokens[0].lower() == 'set':
            self.on_light_set(tokens[1:])
        elif tokens[0].lower() == 'list':
            self.on_light_list(tokens[1:])
        else:
            self.light_usage()

    def on_light_list(self, tokens):
        self.connection.privmsg(self.channel,
            "The following ids are currently known: %s" % ', '.join(sorted(lights_production.keys())))

    def on_light_set(self, tokens):
        if len(tokens) != 2:
            return False

        lights = tokens[0].split(',')
        for i in range(32):
            for i in range(len(lights)):
                light = lights[i]
                if light in lights_terminal:
                    continue
                if light not in lights_production:
                    self.connection.privmsg(self.channel, "Unknown Alias: '%s'" % light)
                    return
                lights = lights[:i] + lights_production[light] + lights[i+1:]
                break

        status = tokens[1]
        if status.lower() == 'on':
            status = 255
        elif status.lower() == 'off':
            status = 0
        else:
            try:
                status = int(status, 0)
                if status not in range(256):
                    raise ValueError
            except ValueError:
                self.connection.privmsg(self.channel, "Invalid light value: '%s'" % status)
                return

        for light in lights:
            msg = OSCMessage(lights_terminal[light])
            msg.add(status)
            msg.send()

    def light_usage(self):
        self.connection.privmsg(self.channel, "Usage: !light list")
        self.connection.privmsg(self.channel, "Usage: !light set <id>[,...] <on|off|0..255>")

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
            sdata = json.load(file('/home/services/http/subcan.json', 'r'))
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
            traceback.print_exc()
            self.status = 'error'

        self.update_nick()
        self.ircobj.execute_delayed(5, self.refresh)

def main():
    bot = TestBot('#sublab', 'sublab', '172.22.24.1', 6667)
    bot.start()

if __name__ == "__main__":
    main()
