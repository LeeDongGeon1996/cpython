/* microprotocols.c - minimalist and non-validating protocols implementation
 *
 * Copyright (C) 2003-2004 Federico Di Gregorio <fog@debian.org>
 *
 * This file is part of psycopg and was adapted for pysqlite. Federico Di
 * Gregorio gave the permission to use it within pysqlite under the following
 * license:
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#define NEEDS_PY_IDENTIFIER

#include <Python.h>

#include "cursor.h"
#include "microprotocols.h"
#include "prepare_protocol.h"


/* pysqlite_microprotocols_init - initialize the adapters dictionary */

int
pysqlite_microprotocols_init(PyObject *module)
{
    /* create adapters dictionary and put it in module namespace */
    pysqlite_state *state = pysqlite_get_state(module);
    state->psyco_adapters = PyDict_New();
    if (state->psyco_adapters == NULL) {
        return -1;
    }

    return PyModule_AddObjectRef(module, "adapters", state->psyco_adapters);
}


/* pysqlite_microprotocols_add - add a reverse type-caster to the dictionary */

int
pysqlite_microprotocols_add(pysqlite_state *state, PyTypeObject *type,
                            PyObject *proto, PyObject *cast)
{
    PyObject* key;
    int rc;

    assert(type != NULL);
    assert(proto != NULL);
    key = Py_BuildValue("(OO)", (PyObject*)type, proto);
    if (!key) {
        return -1;
    }

    rc = PyDict_SetItem(state->psyco_adapters, key, cast);
    Py_DECREF(key);

    return rc;
}

/* pysqlite_microprotocols_adapt - adapt an object to the built-in protocol */

PyObject *
pysqlite_microprotocols_adapt(pysqlite_state *state, PyObject *obj,
                              PyObject *proto, PyObject *alt)
{
    _Py_IDENTIFIER(__adapt__);
    _Py_IDENTIFIER(__conform__);
    PyObject *adapter, *key, *adapted;

    /* we don't check for exact type conformance as specified in PEP 246
       because the PrepareProtocolType type is abstract and there is no
       way to get a quotable object to be its instance */

    /* look for an adapter in the registry */
    key = Py_BuildValue("(OO)", (PyObject*)Py_TYPE(obj), proto);
    if (!key) {
        return NULL;
    }
    adapter = PyDict_GetItemWithError(state->psyco_adapters, key);
    Py_DECREF(key);
    if (adapter) {
        Py_INCREF(adapter);
        adapted = PyObject_CallOneArg(adapter, obj);
        Py_DECREF(adapter);
        return adapted;
    }
    if (PyErr_Occurred()) {
        return NULL;
    }

    /* try to have the protocol adapt this object */
    if (_PyObject_LookupAttrId(proto, &PyId___adapt__, &adapter) < 0) {
        return NULL;
    }
    if (adapter) {
        adapted = PyObject_CallOneArg(adapter, obj);
        Py_DECREF(adapter);

        if (adapted == Py_None) {
            Py_DECREF(adapted);
        }
        else if (adapted || !PyErr_ExceptionMatches(PyExc_TypeError)) {
            return adapted;
        }
        else {
            PyErr_Clear();
        }
    }

    /* and finally try to have the object adapt itself */
    if (_PyObject_LookupAttrId(obj, &PyId___conform__, &adapter) < 0) {
        return NULL;
    }
    if (adapter) {
        adapted = PyObject_CallOneArg(adapter, proto);
        Py_DECREF(adapter);

        if (adapted == Py_None) {
            Py_DECREF(adapted);
        }
        else if (adapted || !PyErr_ExceptionMatches(PyExc_TypeError)) {
            return adapted;
        }
        else {
            PyErr_Clear();
        }
    }

    if (alt) {
        return Py_NewRef(alt);
    }
    /* else set the right exception and return NULL */
    PyErr_SetString(state->ProgrammingError, "can't adapt");
    return NULL;
}
