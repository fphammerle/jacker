import pytest

import jack

import mock
import time

def test_create():
    jack.Client('test')

def test_name_conflict():
    client = jack.Client('test')
    with pytest.raises(jack.Error):
        jack.Client(client.get_name(), use_exact_name = True)

def test_register_port():
    client = jack.Client('test')
    port = client.register_port(
            name = 'port name', 
            type = jack.DefaultMidiPortType, 
            direction = jack.Input,
            )
    assert port in client.get_ports()

def test_port_register_callback():
    client = jack.Client('test')
    port_registered = mock.Mock()
    client.set_port_registered_callback(port_registered)
    port_unregistered = mock.Mock()
    client.set_port_unregistered_callback(port_unregistered)
    client.activate()
    port = client.register_port('port', jack.DefaultAudioPortType, jack.Output)
    time.sleep(0.1)
    port_registered.assert_called_with(client, port)
    port_unregistered.assert_not_called()
