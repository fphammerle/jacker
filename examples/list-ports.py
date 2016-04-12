#!/usr/bin/env python
# PYTHON_ARGCOMPLETE_OK

import sys
import jack
import time
import pprint
import argparse

def run():

    client = jack.Client("list ports example");

    for port in client.get_ports():
        print port.get_name()
        print '\tclient: ' + port.get_client_name()
        print '\tshort: ' + port.get_short_name()
        for alias in port.get_aliases():
            print '\talias: ' + alias
        print '\ttype: %s %s' % (
            port.get_type(),
            'input' if port.is_input() else ('output' if port.is_output() else ''),
            )

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
