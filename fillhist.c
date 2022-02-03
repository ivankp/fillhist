#include <inttypes.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#define STR1(x) #x
#define STR(x) STR1(x)

#define CAT1(a,b) a##b
#define CAT(a,b) CAT1(a,b)

#define MODULE_NAME fillhist

#define LINEFCN printf(STR(__LINE__)": %s\n",__PRETTY_FUNCTION__);

enum a_flag {
  a_obj   = 1 << 0,
  a_under = 1 << 1,
  a_over  = 1 << 2,
  a_eq    = 1 << 3,
};

enum h_flag {
  h_obj_bins = 1 << 0,
  h_int_bins = 1 << 1,
  h_sub_axes = 1 << 2,
};

typedef struct {
  PyObject_HEAD
  void* axes[2]; // if h_sub_axes - second ptr to sizes
  void* bins; // either array of `double`s, `int64_t`s or `PyObject*`s
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
int parse_uniform_axis(PyObject** args, PyObject* kwargs) {
  const long n = PyLong_AsLong(args[0]);
  if (n < 0) {
    PyErr_SetString(PyExc_ValueError,"negative number of bins");
    return -1;
  }
  const double a = PyFloat_AsDouble(args[1]);
  if (a==-1. && PyErr_Occurred()) return -1;
  const double b = PyFloat_AsDouble(args[2]);
  if (b==-1. && PyErr_Occurred()) return -1;

  uint32_t nbins = n;
  PyObject* u = kwargs ? PyDict_GetItemString(kwargs,"u") : NULL;
  if (u) {
    int x = PyObject_IsTrue(u);
    if (x==-1) {
      return -1;
    } else if (x) {
      ++nbins;
      // TODO: set underflow flag
    } else {
      // TODO: unset underflow flag
    }
  } else {
    ++nbins;
    // TODO: set underflow flag
  }
  PyObject* o = kwargs ? PyDict_GetItemString(kwargs,"o") : NULL;
  if (o) {
    int x = PyObject_IsTrue(o);
    if (x==-1) {
      return -1;
    } else if (x) {
      ++nbins;
      // TODO: set overflow flag
    } else {
      // TODO: unset overflow flag
    }
  } else {
    ++nbins;
    // TODO: set overflow flag
  }

  printf("%ld %f %f %"PRIu32"\n",n,a,b,nbins);
  return 0;
}

static
int hist_init(hist* self, PyTupleObject* targs, PyObject* kwargs) {
  const Py_ssize_t nargs = Py_SIZE(targs);
  if (nargs < 1) {
    PyErr_SetString(PyExc_ValueError,"hist requires at least 1 argument");
    return -1;
  } else if (nargs > (Py_ssize_t)((uint8_t)-1)) {
    PyErr_SetString(PyExc_ValueError,"too many arguments");
    return -1;
  }
  PyObject** args = targs->ob_item;

  if (PyLong_Check(args[0])) { // single uniform axis
    if (nargs!=3) {
      PyErr_SetString(PyExc_ValueError,"invalid arguments");
      return -1;
    }

    if (parse_uniform_axis(args,kwargs)) return -1;

  } else { // first argument is not int
    // assume every argument is iterable
    for (PyObject **arg = args, **const end = args+nargs;;) {
      PyObject* iter = PyObject_GetIter(*arg);
      if (!iter) {
        if (!PyErr_Occurred())
          PyErr_SetString(PyExc_ValueError,"invalid argument: not iterable");
        return -1;
      }
      for (PyObject* item; (item = PyIter_Next(iter)); ) {


        Py_DECREF(item);
      }

      if (!(++arg < end)) break;
    }
  }

  return 0;
}

static
PyObject* hist_fill(hist* self, PyTupleObject* targs, PyObject* kwargs) {
  unsigned b = 0; // bin index
  unsigned d = 0; // dimension index
  for (PyObject **arg=targs->ob_item, **const end=arg+Py_SIZE(targs);
       arg < end; ++arg
  ) {
    if (d == self->ndim) {
      // TODO: too many arguments, throw
      goto ret;
    }

    ++d;
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
