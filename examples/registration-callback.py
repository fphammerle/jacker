#!/usr/bin/env python
# PYTHON_ARGCOMPLETE_OK

import sys
import jack
import time
import argparse

def port_registered_callback(client, port):
    print(str(port) + ' registered')

def port_unregistered_callback(client, port):
    print(str(port) + ' unregistered')

def run():

    client = jack.Client("registration callback test");
    client.set_port_registered_callback(port_registered_callback)
    client.set_port_unregistered_callback(port_unregistered_callback)
    client.activate();

    print("client name: " + client.get_name())

    while True:
        try:
            time.sleep(1);
        except KeyboardInterrupt:
            break

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

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
