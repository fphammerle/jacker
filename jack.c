#include <python2.7/Python.h>

#include <string.h>
#include <jack/jack.h>

const int port_input = 1;
const int port_output = 2;

typedef struct {
    PyObject_HEAD
    jack_client_t* client;
    PyObject* port_registered_callback;
    PyObject* port_registered_callback_argument;
    PyObject* port_renamed_callback;
    PyObject* port_renamed_callback_argument;
    PyObject* port_unregistered_callback;
    PyObject* port_unregistered_callback_argument;
    PyObject* shutdown_callback;
    PyObject* shutdown_callback_argument;
} Client;

typedef struct {
    PyObject_HEAD
    jack_port_t* port;
} Port;

static PyObject* error;
static PyObject* failure;
static PyObject* connection_exists;

static PyTypeObject port_type = {
    PyObject_HEAD_INIT(NULL)
    };

static PyObject* python_import(const char* name)
{
    PyObject* python_name = PyString_FromString(name);
    PyObject* object = PyImport_Import(python_name);
    Py_DECREF(python_name);
    return object;
}

long get_reference_count(PyObject* object)
{
    PyObject* sys = python_import("sys");
    PyObject* getrefcount = PyObject_GetAttrString(sys, "getrefcount");
    // Return the reference count of the object.
    // The count returned is generally one higher than you might expect,
    // because it includes the (temporary) reference as an argument to getrefcount().
    PyObject* argument_list = PyTuple_Pack(1, object);
    PyObject* result = PyObject_CallObject(getrefcount, argument_list);
    Py_DECREF(argument_list);
    long count;
    if(!result) {
        PyErr_PrintEx(0);
        count = -2;
    } else {
        count = PyInt_AsLong(result);
        Py_DECREF(result);
    }
    Py_DECREF(getrefcount);
    Py_DECREF(sys);
    return count;
}

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
            callback_argument_list = Py_BuildValue("(O,O,O)", (PyObject*)client, (PyObject*)port, callback_argument);
        } else {
            callback_argument_list = Py_BuildValue("(O,O)", (PyObject*)client, (PyObject*)port);
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

// typedef int (*JackPortRenameCallback)(jack_port_id_t port, const char* old_name, const char* new_name, void *arg);
static int jack_port_renamed_callback(jack_port_id_t port_id, const char* old_name, const char* new_name, void* arg)
{
    int return_code = 0;
    Client* client = (Client*)arg;

    if(client->port_renamed_callback) {
        // Ensure that the current thread is ready to call the Python API.
        // No Python API calls are allowed before this call.
        PyGILState_STATE gil_state = PyGILState_Ensure();

        Port* port = PyObject_New(Port, &port_type);
        port->port = jack_port_by_id(client->client, port_id);

        // 'O' increases reference count
        PyObject* callback_argument_list = Py_BuildValue(
                "(O,O,s,s,O)",
                (PyObject*)client,
                (PyObject*)port,
                old_name,
                new_name,
                client->port_renamed_callback_argument
                );
        PyObject* result = PyObject_CallObject(client->port_renamed_callback, callback_argument_list);
        Py_DECREF(callback_argument_list);
        if(!result) {
            PyErr_PrintEx(0);
            return_code = -1;
        } else {
            Py_DECREF(result);
            return_code = 0;
        }

        // Release the thread. No Python API calls are allowed beyond this point.
        PyGILState_Release(gil_state);
    }

    return return_code;
}

// typedef void(* JackInfoShutdownCallback)(jack_status_t code, const char *reason, void *arg)
static void jack_shutdown_callback(jack_status_t code, const char* reason, void* arg)
{
    Client* client = (Client*)arg;

    if(client->shutdown_callback) {
        // Ensure that the current thread is ready to call the Python API.
        // No Python API calls are allowed before this call.
        PyGILState_STATE gil_state = PyGILState_Ensure();

        // 'O' increases reference count
        PyObject* callback_argument_list = NULL;
        if(client->shutdown_callback_argument) {
            callback_argument_list = Py_BuildValue(
                "(O,s,O)",
                (PyObject*)client,
                reason,
                client->shutdown_callback_argument
                );
        } else {
            callback_argument_list = Py_BuildValue(
                "(O,s)",
                (PyObject*)client,
                reason
                );
        }
        PyObject* result = PyObject_CallObject(client->shutdown_callback, callback_argument_list);
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
        char* client_name;
        unsigned char use_exact_name = 0;
        char* server_name = NULL;
        static char* kwlist[] = {"client_name", "use_exact_name", "server_name", NULL};
        if(!PyArg_ParseTupleAndKeywords(
                    args, kwargs, "s|bs", kwlist,
                    &client_name, &use_exact_name, &server_name
                    )) {
            return NULL;
        }

        jack_options_t open_options = JackNullOption;
        if(server_name) {
            open_options |= JackServerName;
        }
        if(use_exact_name) {
            open_options |= JackUseExactName;
        }

        jack_status_t status;
        self->client = jack_client_open(client_name, open_options, &status, server_name);
        if(!self->client) {
            if(status & JackNameNotUnique) {
                PyErr_SetString(error, "The desired client name was not unique.");
            } else if(status & JackServerError) {
                PyErr_SetString(error, "Communication error with the JACK server.");
            } else if(status & JackFailure) {
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

        self->shutdown_callback = NULL;
        self->shutdown_callback_argument = NULL;
        jack_on_info_shutdown(
            self->client,
            jack_shutdown_callback,
            (void*)self
            );
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

static PyObject* client_deactivate(Client* self) {
    int error_code = jack_deactivate(self->client);
    if(error_code) {
        PyErr_SetString(error, "");
        return NULL;
    } else {
        Py_INCREF(Py_None);
        return Py_None;
    }
}
#include <stdio.h>
static PyObject* client_connect(Client* self, PyObject* args)
{
    PyObject *source_port_python, *target_port_python;
    // The object’s reference count is not increased.
    if(!PyArg_ParseTuple(args, "O!O!", &port_type, &source_port_python, &port_type, &target_port_python)) {
        return NULL;
    }

    Port* source_port = (Port*) source_port_python;
    Port* target_port = (Port*) target_port_python;

    int return_code = jack_connect(
            self->client,
            jack_port_name(source_port->port),
            jack_port_name(target_port->port)
            );

    if(return_code) {
        if(return_code == EEXIST) {
            PyErr_SetString(connection_exists, "The specified ports are already connected.");
        }
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

static PyObject* client_register_port(Client* self, PyObject* args, PyObject* kwargs)
{
    char* name;
    char* type;
    int direction;
    unsigned char physical = 0;
    unsigned char terminal = 0;
    unsigned long buffer_size = 0;
    static char* kwlist[] = {
        "name", "type", "direction", "physical",
        "terminal", "buffer_size", NULL
        };
    if(!PyArg_ParseTupleAndKeywords(
                args, kwargs, "ssi|bbk", kwlist,
                &name, &type, &direction,
                &physical, &terminal, &buffer_size
                )) {
        return NULL;
    }

    unsigned long flags = 0;
    if(direction == port_input) {
        flags |= JackPortIsInput;
    } else if(direction == port_output) {
        flags |= JackPortIsOutput;
    } else {
        PyErr_SetString(PyExc_ValueError, "Invalid port direction given.");
        return NULL;
    }
    if(physical) {
        flags |= JackPortIsPhysical;
    }
    if(terminal) {
        flags |= JackPortIsTerminal;
    }

    Port* port = PyObject_New(Port, &port_type);
    port->port = jack_port_register(
            self->client,
            name,
            type,
            flags,
            64
            );
    if(!port->port) {
        PyErr_SetString(error, "Could not register port.");
        return NULL;
    }
    return (PyObject*)port;
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

static PyObject* client_set_shutdown_callback(Client* self, PyObject* args)
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
    Py_XDECREF(self->shutdown_callback);
    self->shutdown_callback = callback;

    Py_XINCREF(callback_argument);
    Py_XDECREF(self->shutdown_callback_argument);
    self->shutdown_callback_argument = callback_argument;

    Py_INCREF(Py_None);
    return Py_None;
}

static void client_dealloc(Client* self)
{
    jack_client_close(self->client);

    Py_XDECREF(self->port_registered_callback);
    Py_XDECREF(self->port_registered_callback_argument);
    Py_XDECREF(self->port_renamed_callback);
    Py_XDECREF(self->port_renamed_callback_argument);
    Py_XDECREF(self->port_unregistered_callback);
    Py_XDECREF(self->port_unregistered_callback_argument);
    Py_XDECREF(self->shutdown_callback);
    Py_XDECREF(self->shutdown_callback_argument);

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
        "connect",
        (PyCFunction)client_connect,
        METH_VARARGS,
        "Establish a connection between two ports.",
        },
    {
        "deactivate",
        (PyCFunction)client_deactivate,
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
        "register_port",
        (PyCFunction)client_register_port,
        METH_VARARGS | METH_KEYWORDS,
        "Register a new port for the client.",
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
    {
        "set_shutdown_callback",
        (PyCFunction)client_set_shutdown_callback,
        METH_VARARGS,
        "Tell the JACK server to call a function when the server shuts down the client.",
        },
    {NULL},
    };

unsigned char port_is_input(const Port* port)
{
    return jack_port_flags(port->port) & JackPortIsInput;
}

static PyObject* python_port_is_input(Port* self)
{
    // The flags "JackPortIsInput" and "JackPortIsOutput" are mutually exclusive.
    return (PyObject*)PyBool_FromLong(port_is_input(self));
}

unsigned char port_is_output(const Port* port)
{
    return jack_port_flags(port->port) & JackPortIsOutput;
}

static PyObject* python_port_is_output(Port* self)
{
    // The flags "JackPortIsInput" and "JackPortIsOutput" are mutually exclusive.
    return (PyObject*)PyBool_FromLong(port_is_output(self));
}

static PyObject* port_get_name(Port* self)
{
    return (PyObject*)PyString_FromString(jack_port_name(self->port));
}

static PyObject* port_get_short_name(Port* self)
{
    return (PyObject*)PyString_FromString(jack_port_short_name(self->port));
}

static PyObject* port_get_type(Port* self)
{
    return (PyObject*)PyString_FromString(jack_port_type(self->port));
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

unsigned char port_is_physical(const Port* port)
{
    return jack_port_flags(port->port) & JackPortIsPhysical;
}

unsigned char port_is_terminal(const Port* port)
{
    return jack_port_flags(port->port) & JackPortIsTerminal;
}

static PyObject* port___repr__(Port* self)
{
    static PyObject* format;
    if(!format) {
        format = PyString_FromString(
                "jack.Port(name = '%s', type = '%s', direction = %s, physical = %s, terminal = %s)"
                );
    }
    PyObject* args = Py_BuildValue(
            "(s,s,s,s,s)",
            jack_port_name(self->port),
            jack_port_type(self->port),
            port_is_input(self) ? "jack.Input" : (port_is_output(self) ? "jack.Output" : "?"),
            port_is_physical(self) ? "True" : "False",
            port_is_terminal(self) ? "True" : "False"
            );
    PyObject* repr = PyString_Format(format, args);
    Py_DECREF(args);
    return repr;
}

static unsigned char port_equal(const Port* port_a, const Port* port_b) 
{
    // return jack_port_uuid(port_a->port) == jack_port_uuid(port_b->port);
    return strcmp(jack_port_name(port_a->port), jack_port_name(port_b->port)) == 0;
}

static PyObject* port_richcompare(Port* port_a, Port* port_b, int operation)
{
    PyObject* result; 

    switch(operation) {
        case Py_EQ:
            result = port_equal(port_a, port_b) ? Py_True : Py_False;
            break;
        case Py_NE:
            result = !port_equal(port_a, port_b) ? Py_True : Py_False;
            break;
        default:
            PyErr_SetString(PyExc_ValueError, "This operation is not available for jack ports.");
            return NULL;
    }

    Py_INCREF(result);
    return result;
}

static void port_dealloc(Port* self)
{
    self->ob_type->tp_free((PyObject*)self);
}

static PyMethodDef port_methods[] = {
    {
        "is_input",
        (PyCFunction)python_port_is_input,
        METH_NOARGS,
        "Return true if the port can receive data.",
        },
    {
        "is_output",
        (PyCFunction)python_port_is_output,
        METH_NOARGS,
        "Return true if data can be read from the port.",
        },
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
        "get_type",
        (PyCFunction)port_get_type,
        METH_NOARGS,
        "Return port's type.",
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

#ifndef PyMODINIT_FUNC	// declarations for DLL import/export
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC initjack(void)
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

    connection_exists = PyErr_NewException((char*)"jack.ConnectionExists", error, NULL);
    Py_INCREF(connection_exists);
    PyModule_AddObject(module, "ConnectionExists", connection_exists);

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
    port_type.tp_dealloc = (destructor)port_dealloc;
    port_type.tp_repr = (reprfunc)port___repr__;
    port_type.tp_methods = port_methods;
    port_type.tp_richcompare = (richcmpfunc)port_richcompare;
    if(PyType_Ready(&port_type) < 0) {
        return;
    }
    Py_INCREF(&port_type);
    PyModule_AddObject(module, "Port", (PyObject*)&port_type);

    PyModule_AddIntConstant(module, "Input", port_input);
    PyModule_AddIntConstant(module, "Output", port_output);

    PyModule_AddStringConstant(module, "DefaultAudioPortType", JACK_DEFAULT_AUDIO_TYPE);
    PyModule_AddStringConstant(module, "DefaultMidiPortType", JACK_DEFAULT_MIDI_TYPE);
}
