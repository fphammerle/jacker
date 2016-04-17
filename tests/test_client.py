import pytest

import jack

def test_create():
    jack.Client('test')

def test_name_conflict():
    client = jack.Client('test')
    with pytest.raises(jack.Error):
        jack.Client(client.get_name(), use_exact_name = True)
