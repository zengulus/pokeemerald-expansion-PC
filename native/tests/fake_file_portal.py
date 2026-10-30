#!/usr/bin/env python3
"""Minimal XDG file-chooser portal for integration tests."""
import sys
import dbus
import dbus.service
from dbus.mainloop.glib import DBusGMainLoop
from gi.repository import GLib

DBusGMainLoop(set_as_default=True)
bus = dbus.SessionBus()
name = dbus.service.BusName('org.freedesktop.portal.Desktop', bus)
requests = []


class Request(dbus.service.Object):
    def __init__(self, path):
        super().__init__(bus, path, bus_name=name)

    @dbus.service.signal('org.freedesktop.portal.Request', signature='ua{sv}')
    def Response(self, response, results):
        pass

    @dbus.service.method('org.freedesktop.portal.Request', in_signature='', out_signature='')
    def Close(self):
        print('CLOSE', flush=True)


class Portal(dbus.service.Object):
    def __init__(self):
        super().__init__(bus, '/org/freedesktop/portal/desktop', bus_name=name)
        self.index = 0

    @dbus.service.method('org.freedesktop.portal.FileChooser', in_signature='ssa{sv}', out_signature='o')
    def OpenFile(self, parent, title, options):
        self.index += 1
        expected = 'Choose a SoundFont file' if self.index == 2 else 'Choose a MIDI file'
        if str(title) != expected or 'filters' not in options:
            raise dbus.DBusException('Unexpected chooser title or filter')
        path = '/org/freedesktop/portal/desktop/request/test/request' + str(self.index)
        request = Request(path)
        requests.append(request)
        if self.index <= 3:
            uri = sys.argv[self.index] if self.index <= 2 else 'file://other-host/remote.mid'
            def respond():
                request.Response(dbus.UInt32(0), {'uris': dbus.Array([uri], signature='s')})
                return False
            GLib.timeout_add(50, respond)
        elif self.index == 5:
            GLib.timeout_add(50, lambda: (request.Response(dbus.UInt32(1), {}), False)[1])
        return dbus.ObjectPath(path)


portal = Portal()
print('READY', flush=True)
GLib.MainLoop().run()
