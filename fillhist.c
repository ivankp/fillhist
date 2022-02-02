#define PY_SSIZE_T_CLEAN
#include <Python.h>

#define MODULE_NAME "fillhist"

enum hist_flag {
  h_obj_bins = 1 << 0,
  h_int_bins = 1 << 1,
  a_flt_ = 1 << 1,
};

typedef struct {
  PyObject_HEAD
  void *bins; // either array of `double`s, `int64_t`s or `PyObject`s
  void *dim; // array of pointers to arrays of axes
  uint32_t nbins; // number of bins
  uint8_t ndim; // number of dimensions
  uint8_t flags;
} hist;

static PyObject* hist_fill(hist* self, PyObject* args, PyObject* kwargs) {
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

static PyMethodDef hist_methods[] = {
  { "fill", (PyCFunction) hist_fill, METH_VARARGS,
    "Fill histogram bin corresponding to the provided point"
  },
  { NULL }
};

static PyTypeObject hist_type = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name = MODULE_NAME ".hist",
  .tp_doc = "Histogram object type",
  .tp_basicsize = sizeof(hist),
  .tp_itemsize = 0,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  /* .tp_new = hist_new, */
  /* .tp_init = (initproc) hist_init, */
  /* .tp_dealloc = (destructor) hist_dealloc, */
  // .tp_members = hist_members,
  .tp_call = (ternaryfunc) hist_fill,
  .tp_methods = hist_methods,
};

/* static PyMethodDef phyhist_methods[] = { */
/*   { NULL } */
/* }; */

static PyModuleDef phyhist_module = {
  PyModuleDef_HEAD_INIT,
  .m_name = MODULE_NAME,
  .m_doc = "Multi-dimensional histograms that can be filled in a loop",
  .m_size = -1,
  // .m_methods = phyhist_methods,
};

PyMODINIT_FUNC PyInit_phyhist(void) {
  if (PyType_Ready(&hist_type) < 0) return NULL;

  PyObject *m = PyModule_Create(&phyhist_module);
  if (m == NULL) return NULL;

  Py_INCREF(&hist_type);
  if (PyModule_AddObject(m,"hist",(PyObject*)&hist_type) < 0) {
    Py_DECREF(&hist_type);
    Py_DECREF(m);
    return NULL;
  }

  return m;
}
