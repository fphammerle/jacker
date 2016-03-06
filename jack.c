#include <python2.7/Python.h>

#include <jack/jack.h>

typedef struct {
    PyObject_HEAD
    jack_client_t* client;
    PyObject* registration_callback;
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

    if(client->registration_callback) {
        // Ensure that the current thread is ready to call the Python API.
        PyGILState_STATE gil_state = PyGILState_Ensure();

        Port* port = PyObject_New(Port, &port_type);
        port->port = jack_port_by_id(client->client, port_id);

        PyObject* callback_argument_list = Py_BuildValue("(O,i)", (PyObject*)port, registered);
        PyObject* result = PyObject_CallObject(client->registration_callback, callback_argument_list);
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

        self->registration_callback = NULL;
        int error_code = jack_set_port_registration_callback(
            self->client,
            jack_registration_callback,
            (void*)self
            );
        if(error_code) {
            PyErr_SetString(error, "Could not set port registration callback.");
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

static PyObject* client_set_port_registration_callback(Client* self, PyObject* args)
{
    PyObject* callback;
    if(!PyArg_ParseTuple(args, "O", &callback)) {
        return NULL;
    }
    if(!PyCallable_Check(callback)) {
        PyErr_SetString(PyExc_TypeError, "Parameter must be callable.");
        return NULL;
    }

    Py_XINCREF(callback);
    Py_XDECREF(self->registration_callback);
    self->registration_callback = callback;

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
        "set_port_registration_callback",
        (PyCFunction)client_set_port_registration_callback,
        METH_VARARGS,
        "Tell the JACK server to call a function whenever a port is registered or unregistered.",
        },
    {NULL},
    };

static PyObject* port_get_name(Port* self)
{
    return (PyObject*)PyString_FromString(jack_port_name(self->port));
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
