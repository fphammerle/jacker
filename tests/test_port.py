import pytest

import jack

def test_get_short_name():
    client = jack.Client('test')
    port = client.register_port(
            name = 'port name',
            type = jack.DefaultMidiPortType,
            direction = jack.Input,
            )
    assert port.get_short_name() == 'port name'

def test_set_short_name():
    client = jack.Client('test')
    port = client.register_port(
            name = 'port name',
            type = jack.DefaultMidiPortType,
            direction = jack.Input,
            )
    port.set_short_name('new port name')
    assert port.get_short_name() == 'new port name'

def test_get_client_name():
    client = jack.Client('test')
    port = client.register_port(
            name = 'port name',
            type = jack.DefaultMidiPortType,
            direction = jack.Input,
            )
    assert port.get_client_name() == client.get_name()

def test_get_name():
    client = jack.Client('test')
    port = client.register_port(
            name = 'port name',
            type = jack.DefaultMidiPortType,
            direction = jack.Input,
            )
    assert port.get_name() == client.get_name() + ':port name'

def test_is_input():
    client = jack.Client('test')
    port = client.register_port(
            name = 'port name',
            type = jack.DefaultMidiPortType,
            direction = jack.Input,
            )
    assert port.is_input()

def test_is_not_input():
    client = jack.Client('test')
    port = client.register_port(
            name = 'port name',
            type = jack.DefaultMidiPortType,
            direction = jack.Output,
            )
    assert not port.is_input()

def test_is_output():
    client = jack.Client('test')
    port = client.register_port(
            name = 'port name',
            type = jack.DefaultMidiPortType,
            direction = jack.Output,
            )
    assert port.is_output()

def test_is_not_output():
    client = jack.Client('test')
    port = client.register_port(
            name = 'port name',
            type = jack.DefaultMidiPortType,
            direction = jack.Input,
            )
    assert not port.is_output()

def test_equal():
    client = jack.Client('test')
    port = client.register_port(
            name = 'port name',
            type = jack.DefaultMidiPortType,
            direction = jack.Input,
            )
    assert port == [p for p in client.get_ports() if p.get_name() == port.get_name()][0]

def test_unequal():
    client = jack.Client('test')
    port_a = client.register_port(
            name = 'port name a',
            type = jack.DefaultMidiPortType,
            direction = jack.Input,
            )
    port_b = client.register_port(
            name = 'port name b',
            type = jack.DefaultMidiPortType,
            direction = jack.Input,
            )
    assert port_a != port_b
