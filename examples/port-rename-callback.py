#!/usr/bin/env python
# PYTHON_ARGCOMPLETE_OK

import sys
import jack
import time
import argparse

def port_renamed(client, port, old_name, new_name, arg):
    assert(arg == 1234)
    print("renamed port %s\n\tfrom '%s'\n\tto '%s'\n" % (repr(port), old_name, new_name))

def run():

    client = jack.Client('port rename callback example');
    client.set_port_renamed_callback(port_renamed, 1234)
    client.activate();

    print('client name: ' + client.get_name())

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

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
