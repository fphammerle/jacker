#!/usr/bin/env python
# PYTHON_ARGCOMPLETE_OK

import sys
import jack
import time
import argparse
from gi.repository import GLib

def shutdown(client, reason, loop):
    print(reason)
    loop.quit()

def run():

    loop = GLib.MainLoop()

    client = jack.Client('client shutdown callback example');
    client.set_shutdown_callback(shutdown, loop)
    client.activate();

    print('client name: ' + client.get_name())

    try:
        loop.run()
    except KeyboardInterrupt:
        print('keyboard interrupt')

def _init_argparser():

    argparser = argparse.ArgumentParser(description = None)
    return argparser

def main(argv):

    argparser = _init_argparser()
    try:
        import argcomplete
        argcomplete.autocomplete(argparser)
    except ImportError:
        pass
    args = argparser.parse_args(argv)

    run(**vars(args))

    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
