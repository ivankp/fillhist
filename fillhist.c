#include <inttypes.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#define STR1(x) #x
#define STR(x) STR1(x)

#define CAT1(a,b) a##b
#define CAT(a,b) CAT1(a,b)

#define MODULE_NAME fillhist

enum a_flag {
  a_obj   = 1 << 0,
  a_under = 1 << 1,
  a_over  = 1 << 2,
};

enum h_flag {
  h_obj_bins = 1 << 0,
  h_int_bins = 1 << 1,
};

typedef struct {
  PyObject_HEAD
  void *bins; // either array of `double`s, `int64_t`s or `PyObject`s
  void *dim; // array of pointers to arrays of axes
  uint32_t nbins; // number of bins
  uint8_t ndim; // number of dimensions
  uint8_t flags;
} hist;

// static
// PyObject* hist_new(PyTypeObject* type, PyObject* args, PyObject* kwargs) {
//   hist* self = (hist*) type->tp_alloc(type, 0);
//   return (PyObject*) self;
// }

static
int hist_init(hist* self, PyObject* args, PyObject* kwargs) {
  /* printf(STR(__LINE__)": %s\n",__PRETTY_FUNCTION__); */
  const Py_ssize_t nargs = PyTuple_GET_SIZE(args);
  if (nargs==3) {
    PyObject* arg0 = PyTuple_GET_ITEM(args,0);
    if (PyLong_Check(arg0)) { // single uniform axis
      const long n = PyLong_AsLong(arg0);
      if (n < 0) {
        PyErr_SetString(PyExc_ValueError,"negative number of bins");
        goto error;
      }
      const double a = PyFloat_AsDouble(PyTuple_GET_ITEM(args,1));
      if (a==-1. && PyErr_Occurred()) goto error;
      const double b = PyFloat_AsDouble(PyTuple_GET_ITEM(args,2));
      if (b==-1. && PyErr_Occurred()) goto error;

      self->nbins = n;
      PyObject* u = kwargs ? PyDict_GetItemString(kwargs,"u") : NULL;
      if (u) {
        int x = PyObject_IsTrue(u);
        if (x==-1) {
          goto error;
        } else if (x) {
          ++self->nbins;
          // TODO: set underflow flag
        } else {
          // TODO: unset underflow flag
        }
      } else {
        ++self->nbins;
        // TODO: set underflow flag
      }
      PyObject* o = kwargs ? PyDict_GetItemString(kwargs,"o") : NULL;
      if (o) {
        int x = PyObject_IsTrue(o);
        if (x==-1) {
          goto error;
        } else if (x) {
          ++self->nbins;
          // TODO: set overflow flag
        } else {
          // TODO: unset overflow flag
        }
      } else {
        ++self->nbins;
        // TODO: set overflow flag
      }
      printf("%ld %f %f %"PRIu32"\n",n,a,b,self->nbins);
    } else { // first argument is not int
      goto invalid_args;
    }
  } else { // not 3 args
    goto invalid_args;
  }

  return 0;

invalid_args:
  PyErr_SetString(PyExc_ValueError,"invalid arguments");

error:
  return -1;
}

static
PyObject* hist_fill(hist* self, PyObject* args, PyObject* kwargs) {
  PyObject* iter = PyObject_GetIter(args); // args is always a tuple
  unsigned b = 0; // bin index
  unsigned d = 0; // dimension index
  for (PyObject* item; (item = PyIter_Next(iter)); ) {
    if (d == self->ndim) {
      // TODO: too many arguments, throw
      Py_DECREF(item);
      goto ret;
    }

    ++d;
    Py_DECREF(item);
  }

  PyObject* w = PyDict_GetItemString(kwargs,"w"); // weight
  PyObject* f = PyDict_GetItemString(kwargs,"f"); // increment function

  // fill the bin
  if (self->flags & h_obj_bins) { // object bins
    PyObject** bin = ((PyObject**)self->bins) + b;
    PyObject* sum;
    if (f) {
      PyObject* args = PyTuple_Pack(2,*bin,w);
      sum = PyObject_CallObject(f,args);
      Py_DECREF(args);
    } else {
      sum = PyNumber_InPlaceAdd(*bin,w);
    }
    if (!sum) {
      // TODO: throw if can't add
    }
    Py_DECREF(*bin);
    *bin = sum;
  } else if (self->flags & h_int_bins) { // integer bins
    ((int64_t*)self->bins)[b] += w ? PyLong_AsLongLong(w) : 1;
  } else { // float bins
    ((double*)self->bins)[b] += w ? PyFloat_AsDouble(w) : 1.;
  }

ret:
  Py_DECREF(iter);
  Py_RETURN_NONE;
}

static
PyMethodDef hist_methods[] = {
  { "fill", (PyCFunction) hist_fill, METH_VARARGS,
    "Fill histogram bin corresponding to the provided point"
  },
  { NULL }
};

static
PyTypeObject hist_type = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name = STR(MODULE_NAME) ".hist",
  .tp_doc = "Histogram object type",
  .tp_basicsize = sizeof(hist),
  .tp_itemsize = 0,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_new = PyType_GenericNew,
  .tp_init = (initproc) hist_init,
  /* .tp_dealloc = (destructor) hist_dealloc, */
  // .tp_members = hist_members,
  .tp_call = (ternaryfunc) hist_fill,
  .tp_methods = hist_methods,
};

/* static PyMethodDef CAT(MODULE_NAME,_methods)[] = { */
/*   { NULL } */
/* }; */

static
PyModuleDef CAT(MODULE_NAME,_module) = {
  PyModuleDef_HEAD_INIT,
  .m_name = STR(MODULE_NAME),
  .m_doc = "Multi-dimensional histograms that can be filled in a loop",
  .m_size = -1,
  // .m_methods = CAT(MODULE_NAME,_methods),
};

PyMODINIT_FUNC CAT(PyInit_,MODULE_NAME)(void) {
  if (PyType_Ready(&hist_type) < 0) return NULL;

  PyObject *m = PyModule_Create(&CAT(MODULE_NAME,_module));
  if (m == NULL) return NULL;

  Py_INCREF(&hist_type);
  if (PyModule_AddObject(m,"hist",(PyObject*)&hist_type) < 0) {
    Py_DECREF(&hist_type);
    Py_DECREF(m);
    return NULL;
  }

  return m;
}
