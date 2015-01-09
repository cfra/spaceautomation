import contextlib
import urllib2

# Known lights, terminal symbols
lights_terminal = {}

for i in range(1,12):
    ident = '%02d' % i
    lights_terminal[ident] = 'http://beaglebone.local.sublab.org/set?%s=' % (i + 0x440)

# Production Rules/Aliases
lights_production = {
    u'lounge-stage':        [u'01'],
    u'lounge-office':       [u'02'],
    u'lounge-bar':          [u'03'],
    u'lounge-clock':        [u'04'],
    u'hacklab-east-outer':  [u'05'],
    u'hacklab-east-middle': [u'06'],
    u'hacklab-west-inner':  [u'07'],
    # hacklab 08
    # hacklab 09
    # hacklab 10
    u'hallway':             [u'11'],
    u'lounge-west':         [u'lounge-bar', u'lounge-clock'],
    u'lounge-east':         [u'lounge-stage', u'lounge-office'],
    u'lounge':              [u'lounge-east', u'lounge-west'],
    u'hacklab':             [u'hacklab-east-outer', u'hacklab-east-middle', u'hacklab-west-inner'],
}

def tmp_set(url, value):
    with contextlib.closing(urllib2.urlopen(url + str(value))):
        pass

def on_pubmsg(self, c, e):
    message = e.arguments[0]

    nick = e.source.nick
    commands = {
        '!light' : on_light_command,
    }

    if message.startswith('!help'):
        self.connection.privmsg(self.channel, "The following commands are currently known: %s" % ', '.join(sorted(commands.keys())))
    else:
        for key in commands:
            if message.startswith(key):
                commands[key](self, nick, message[len(key) + 1:])
		break

def on_light_command(self, nick, commandline):
    tokens = commandline.split(' ')
    if len(tokens) < 1:
        self.light_usage()
        return

    if tokens[0].lower() == 'set':
        on_light_set(self, tokens[1:])
    elif tokens[0].lower() == 'list':
        on_light_list(self, tokens[1:])
    else:
        light_usage(self)

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
        tmp_set(lights_terminal[light], status)
    self.connection.privmsg(self.channel, "Light command processed.")

def light_usage(self):
    self.connection.privmsg(self.channel, "Usage: !light list")
    self.connection.privmsg(self.channel, "Usage: !light set <id>[,...] <on|off|0..255>")
