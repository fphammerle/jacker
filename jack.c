#include <python2.7/Python.h>

#include <jack/jack.h>

typedef struct {
    PyObject_HEAD
    jack_client_t* client;
    PyObject* port_registered_callback;
    PyObject* port_registered_callback_argument;
    PyObject* port_unregistered_callback;
    PyObject* port_unregistered_callback_argument;
    PyObject* port_renamed_callback;
    PyObject* port_renamed_callback_argument;
} Client;

typedef struct {
    PyObject_HEAD
    jack_port_t* port;
} Port;

static PyObject* error;
static PyObject* failure;

static PyTypeObject port_type = {
    PyObject_HEAD_INIT(NULL)
    };

static void jack_registration_callback(jack_port_id_t port_id, int registered, void* arg)
{
    // register: non-zero if the port is being registered, zero if the port is being unregistered
    Client* client = (Client*)arg;

    PyObject* callback;
    PyObject* callback_argument;
    if(registered) {
        callback = client->port_registered_callback;
        callback_argument = client->port_registered_callback_argument;
    } else {
        callback = client->port_unregistered_callback;
        callback_argument = client->port_unregistered_callback_argument;
    }

    if(callback) {
        // Ensure that the current thread is ready to call the Python API.
        // No Python API calls are allowed before this call.
        PyGILState_STATE gil_state = PyGILState_Ensure();

        Port* port = PyObject_New(Port, &port_type);
        port->port = jack_port_by_id(client->client, port_id);

        // 'O' increases reference count
        PyObject* callback_argument_list;
        if(callback_argument) {
            callback_argument_list = Py_BuildValue("(O,O)", (PyObject*)port, callback_argument);
        } else {
            callback_argument_list = Py_BuildValue("(O)", (PyObject*)port);
        }
        PyObject* result = PyObject_CallObject(callback, callback_argument_list);
        Py_DECREF(callback_argument_list);
        if(!result) {
            PyErr_PrintEx(0);
        } else {
            Py_DECREF(result);
        }

        // Release the thread. No Python API calls are allowed beyond this point.
        PyGILState_Release(gil_state);
    }
}

static void jack_port_renamed_callback(jack_port_id_t port_id, const char* old_name, const char* new_name, void* arg)
{
    Client* client = (Client*)arg;

    if(client->port_renamed_callback) {
        // Ensure that the current thread is ready to call the Python API.
        // No Python API calls are allowed before this call.
        PyGILState_STATE gil_state = PyGILState_Ensure();

        Port* port = PyObject_New(Port, &port_type);
        port->port = jack_port_by_id(client->client, port_id);

        // 'O' increases reference count
        PyObject* callback_argument_list = Py_BuildValue(
                "(O,s,s,O)",
                (PyObject*)port,
                old_name,
                new_name,
                client->port_renamed_callback_argument
                );
        PyObject* result = PyObject_CallObject(client->port_renamed_callback, callback_argument_list);
        Py_DECREF(callback_argument_list);
        if(!result) {
            PyErr_PrintEx(0);
        } else {
            Py_DECREF(result);
        }

        // Release the thread. No Python API calls are allowed beyond this point.
        PyGILState_Release(gil_state);
    }
}

static PyObject* client___new__(PyTypeObject* type, PyObject* args, PyObject* kwargs)
{
    Client* self = (Client*)type->tp_alloc(type, 0);

    if(self) {
        const char* name;
        if(!PyArg_ParseTuple(args, "s", &name)) {
            return NULL;
        }

        jack_status_t status;
        self->client = jack_client_open(name, JackNullOption, &status);
        if(!self->client) {
            if(status & JackFailure) {
                PyErr_SetString(failure, "Overall operation failed.");
            }
            return NULL;
        }

        self->port_registered_callback = NULL;
        self->port_unregistered_callback = NULL;
        int error_code = jack_set_port_registration_callback(
            self->client,
            jack_registration_callback,
            (void*)self
            );
        if(error_code) {
            PyErr_SetString(error, "Could not set port registration callback.");
            return NULL;
        }

        self->port_renamed_callback = NULL;
        self->port_renamed_callback_argument = NULL;
        error_code = jack_set_port_rename_callback(
            self->client,
            jack_port_renamed_callback,
            (void*)self
            );
        if(error_code) {
            PyErr_SetString(error, "Could not set port rename callback.");
            return NULL;
        }
    }

    return (PyObject*)self;
}

static PyObject* client_activate(Client* self) {
    int error_code = jack_activate(self->client);
    if(error_code) {
        PyErr_SetString(error, "");
        return NULL;
    } else {
        Py_INCREF(Py_None);
        return Py_None;
    }
}

static PyObject* client_get_name(Client* self)
{
    return (PyObject*)PyString_FromString(jack_get_client_name(self->client));
}

static PyObject* client_get_ports(Client* self)
{
    PyObject* ports = PyList_New(0);
    if(!ports) {
        return NULL;
    }

    const char** port_names = jack_get_ports(self->client, NULL, NULL, 0);
    int port_index;
    for(port_index = 0; port_names[port_index] != NULL; port_index++) {
        Port* port = PyObject_New(Port, &port_type);
        port->port = jack_port_by_name(self->client, port_names[port_index]);
        if(PyList_Append(ports, (PyObject*)port)) {
            return NULL;
        }
    }
    jack_free(port_names);

    return ports;
}

static PyObject* client_set_port_registered_callback(Client* self, PyObject* args)
{
    PyObject* callback = 0;
    PyObject* callback_argument = 0;
    if(!PyArg_ParseTuple(args, "O|O", &callback, &callback_argument)) {
        return NULL;
    }
    if(!PyCallable_Check(callback)) {
        PyErr_SetString(PyExc_TypeError, "Parameter must be callable.");
        return NULL;
    }

    Py_XINCREF(callback);
    Py_XDECREF(self->port_registered_callback);
    self->port_registered_callback = callback;

    Py_XINCREF(callback_argument);
    Py_XDECREF(self->port_registered_callback_argument);
    self->port_registered_callback_argument = callback_argument;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject* client_set_port_unregistered_callback(Client* self, PyObject* args)
{
    PyObject* callback = 0;
    PyObject* callback_argument = 0;
    if(!PyArg_ParseTuple(args, "O|O", &callback, &callback_argument)) {
        return NULL;
    }
    if(!PyCallable_Check(callback)) {
        PyErr_SetString(PyExc_TypeError, "Parameter must be callable.");
        return NULL;
    }

    Py_XINCREF(callback);
    Py_XDECREF(self->port_unregistered_callback);
    self->port_unregistered_callback = callback;

    Py_XINCREF(callback_argument);
    Py_XDECREF(self->port_unregistered_callback_argument);
    self->port_unregistered_callback_argument = callback_argument;

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject* client_set_port_renamed_callback(Client* self, PyObject* args)
{
    PyObject* callback = 0;
    PyObject* callback_argument = 0;
    if(!PyArg_ParseTuple(args, "O|O", &callback, &callback_argument)) {
        return NULL;
    }
    if(!PyCallable_Check(callback)) {
        PyErr_SetString(PyExc_TypeError, "Parameter must be callable.");
        return NULL;
    }

    Py_XINCREF(callback);
    Py_XDECREF(self->port_renamed_callback);
    self->port_renamed_callback = callback;

    Py_XINCREF(callback_argument);
    Py_XDECREF(self->port_renamed_callback_argument);
    self->port_renamed_callback_argument = callback_argument;

    Py_INCREF(Py_None);
    return Py_None;
}

static void client_dealloc(Client* self)
{
    jack_client_close(self->client);
    self->ob_type->tp_free((PyObject*)self);
}

static PyMethodDef client_methods[] = {
    {
        "activate",
        (PyCFunction)client_activate,
        METH_NOARGS,
        "Tell the Jack server that the program is ready to start processing audio.",
        },
    {
        "get_name",
        (PyCFunction)client_get_name,
        METH_NOARGS,
        "Return client's actual name.",
        },
    {
        "get_ports",
        (PyCFunction)client_get_ports,
        METH_NOARGS,
        "Return list of ports.",
        },
    {
        "set_port_registered_callback",
        (PyCFunction)client_set_port_registered_callback,
        METH_VARARGS,
        "Tell the JACK server to call a function whenever a port is registered.",
        },
    {
        "set_port_unregistered_callback",
        (PyCFunction)client_set_port_unregistered_callback,
        METH_VARARGS,
        "Tell the JACK server to call a function whenever a port is unregistered.",
        },
    {
        "set_port_renamed_callback",
        (PyCFunction)client_set_port_renamed_callback,
        METH_VARARGS,
        "Tell the JACK server to call a function whenever a port is renamed.",
        },
    {NULL},
    };

static PyObject* port_get_name(Port* self)
{
    return (PyObject*)PyString_FromString(jack_port_name(self->port));
}

static PyObject* port_get_short_name(Port* self)
{
    return (PyObject*)PyString_FromString(jack_port_short_name(self->port));
}

static PyObject* port_set_short_name(Port* self, PyObject* args)
{
    const char* name;
    if(!PyArg_ParseTuple(args, "s", &name)) {
        return NULL;
    }

    // 0 on success, otherwise a non-zero error code.
    if(jack_port_set_name(self->port, name)) {
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject* port_get_client_name(Port* self)
{
    // e.g. "PulseAudio JACK Sink:front-left"
    const char* port_name = jack_port_name(self->port);

    int client_name_length = strlen(port_name) - strlen(jack_port_short_name(self->port)) - 1;

    // e.g. "PulseAudio JACK Sink"
    char* client_name = (char*) malloc(jack_port_name_size());
    strncpy(client_name, port_name, client_name_length);
    client_name[client_name_length] = '\0';
    PyObject* client_name_python = PyString_FromString(client_name);
    free(client_name);

    return client_name_python;
}

static PyObject* port_get_aliases(Port* self)
{
    PyObject* aliases_list = PyList_New(0);
    if(!aliases_list) {
        return NULL;
    }

    char* aliases[2];
    aliases[0] = (char*) malloc(jack_port_name_size());
    aliases[1] = (char*) malloc(jack_port_name_size());
    int alias_count = jack_port_get_aliases(self->port, aliases);
    int alias_index;
    for(alias_index = 0; alias_index < alias_count; alias_index++) {
        PyList_Append(aliases_list, PyString_FromString(aliases[alias_index]));
    }
    free(aliases[0]);
    free(aliases[1]);

    return aliases_list;
}

static PyObject* port___repr__(Port* self)
{
    static PyObject* format;
    if(!format) {
        format = PyString_FromString("jack.Port(name = '%s')");
    }
    PyObject* name = PyString_FromString(jack_port_name(self->port));
    PyObject* repr = PyString_Format(format, name);
    Py_DECREF(name);
    return repr;
}

static PyMethodDef port_methods[] = {
    {
        "get_name",
        (PyCFunction)port_get_name,
        METH_NOARGS,
        "Return port's name.",
        },
    {
        "get_short_name",
        (PyCFunction)port_get_short_name,
        METH_NOARGS,
        "Return port's name without the preceding name of the associated client.",
        },
    {
        "get_client_name",
        (PyCFunction)port_get_client_name,
        METH_NOARGS,
        "Return the name of the associated client.",
        },
    {
        "set_short_name",
        (PyCFunction)port_set_short_name,
        METH_VARARGS,
        "Modify a port's short name. May be called at any time.",
        },
    {
        "get_aliases",
        (PyCFunction)port_get_aliases,
        METH_NOARGS,
        "Return list of assigned aliases.",
        },
    {NULL},
    };

PyMODINIT_FUNC initjack()
{
    // Initialize and acquire the global interpreter lock.
    // This must be done in the main thread before creating engaging in any thread operations.
    PyEval_InitThreads();

    PyObject* module = Py_InitModule("jack", NULL);
    if(!module) {
        return;
    }

    error = PyErr_NewException((char*)"jack.Error", NULL, NULL);
    Py_INCREF(error);
    PyModule_AddObject(module, "Error", error);

    failure = PyErr_NewException((char*)"jack.Failure", error, NULL);
    Py_INCREF(failure);
    PyModule_AddObject(module, "Failure", failure);

    static PyTypeObject client_type = {
        PyObject_HEAD_INIT(NULL)
        };
    client_type.tp_name = "jack.Client";
    client_type.tp_basicsize = sizeof(Client);
    client_type.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    client_type.tp_new = client___new__;
    client_type.tp_dealloc = (destructor)client_dealloc;
    client_type.tp_methods = client_methods;
    if(PyType_Ready(&client_type) < 0) {
        return;
    }
    Py_INCREF(&client_type);
    PyModule_AddObject(module, "Client", (PyObject*)&client_type);

    port_type.tp_name = "jack.Port";
    port_type.tp_basicsize = sizeof(Port);
    port_type.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    // Forbid direct instantiation.
    port_type.tp_new = NULL;
    port_type.tp_repr = (reprfunc)port___repr__;
    port_type.tp_methods = port_methods;
    if(PyType_Ready(&port_type) < 0) {
        return;
    }
    Py_INCREF(&port_type);
    PyModule_AddObject(module, "Port", (PyObject*)&port_type);
}
