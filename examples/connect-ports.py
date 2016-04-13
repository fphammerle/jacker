#!/usr/bin/env python
# PYTHON_ARGCOMPLETE_OK

import sys
import jack
import random
import argparse

def run():

    client = jack.Client("connect test");
    client.activate();

    print("client name: " + client.get_name())

    midi_ports = [p for p in client.get_ports() if 'midi' in p.get_type()]
    source_ports = [p for p in midi_ports if p.is_output()]
    target_ports = [p for p in midi_ports if p.is_input()]

    source_port = random.choice(source_ports)
    target_port = random.choice(target_ports)

    print(source_port)
    print(target_port)

    try:
        client.connect(source_port, target_port)
    except jack.ConnectionExists:
        print('ports already connected')

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
