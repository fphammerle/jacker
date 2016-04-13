#!/usr/bin/env python
# PYTHON_ARGCOMPLETE_OK

import sys
import jack
import time
import random
import argparse

def run():

    client = jack.Client("set port name example");
    
    print("client name: " + client.get_name())

    port = random.choice(client.get_ports())
    print(port)
    # reverse
    port.set_short_name(port.get_short_name()[::-1])
    print(port)

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
