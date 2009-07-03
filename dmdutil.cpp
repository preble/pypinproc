#include "dmdutil.h"

extern "C" {

static PyObject *
DMDBuffer_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    pinproc_DMDBufferObject *self;

    self = (pinproc_DMDBufferObject *)type->tp_alloc(type, 0);
    if (self != NULL) {
		self->buffer = NULL;
    }

    return (PyObject *)self;
}

static void
DMDBuffer_dealloc(PyObject* _self)
{
	pinproc_DMDBufferObject *self = (pinproc_DMDBufferObject *)_self;
	if (self->buffer != NULL)
	{
		free(self->buffer);
		self->buffer = NULL;
	}
    self->ob_type->tp_free((PyObject*)self);
}

static int
DMDBuffer_init(pinproc_DMDBufferObject *self, PyObject *args, PyObject *kwds)
{
	static char *kwlist[] = {"width", "height", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "ii", kwlist, &self->width, &self->height))
	{
		return -1;
	}
	self->buffer = (char*)calloc(self->width * self->height, 1);
	if (self->buffer == NULL)
	{
		PyErr_SetString(PyExc_IOError, "Failed to allocate memory");
		return -1;
	}
    return 0;
}
static PyObject *
DMDBuffer_clear(pinproc_DMDBufferObject *self, PyObject *args)
{
	memset(self->buffer, 0, self->width * self->height);
	Py_INCREF(Py_None);
	return Py_None;
}
static PyObject *
DMDBuffer_set_data(pinproc_DMDBufferObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *data_str;
	static char *kwlist[] = {"data", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &data_str))
	{
		return NULL;
	}
	int frame_size = self->width * self->height;
	if (PyString_Size(data_str) != frame_size)
	{
		fprintf(stderr, "length=%d != %d", (int)PyString_Size(data_str), frame_size);
		PyErr_SetString(PyExc_ValueError, "Buffer length is incorrect");
		return NULL;
	}

	memcpy(self->buffer, PyString_AsString(data_str), frame_size);

	Py_INCREF(Py_None);
	return Py_None;
}
static PyObject *
DMDBuffer_get_dot(pinproc_DMDBufferObject *self, PyObject *args, PyObject *kwds)
{
	int x, y;
	static char *kwlist[] = {"x", "y", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "ii", kwlist, &x, &y))
	{
		return NULL;
	}
	if (x >= self->width || y >= self->height)
	{
		PyErr_SetString(PyExc_ValueError, "X or Y are out of range");
		return NULL;
	}

	return Py_BuildValue("i", self->buffer[y * self->width + x]);
}
static PyObject *
DMDBuffer_copy_to_rect(pinproc_DMDBufferObject *self, PyObject *args, PyObject *kwds)
{
	pinproc_DMDBufferObject *src = self;
	pinproc_DMDBufferObject *dst;
	int dst_x, dst_y, src_x, src_y, width, height;
	static char *kwlist[] = {"dst", "dst_x", "dst_y", "src_x", "src_y", "width", "height", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "Oiiiiii", kwlist, &dst, &dst_x, &dst_y, &src_x, &src_y, &width, &height))
	{
		return NULL;
	}
	//fprintf(stderr, "Before: srcw=%d, srch=%d, dstw=%d, dstw=%d, dst_x=%d, dst_y=%d, src_x=%d, src_y=%d, width=%d, height=%d\n", src->width, src->height, dst->width, dst->height, dst_x, dst_y, src_x, src_y, width, height);
	if (dst_x < 0)
	{
		src_x += -dst_x;
		width -= -dst_x;
		dst_x = 0;
	}
	if (dst_y < 0)
	{
		src_y += -dst_y;
		height -= -dst_y;
		dst_y = 0;
	}
	if (src_x < 0)
	{
		dst_x += -src_x;
		width -= -src_x;
		src_x = 0;
	}
	if (src_y < 0)
	{
		dst_y += -src_y;
		height -= -src_y;
		src_y = 0;
	}
	if (src_x + width  > src->width)  width = src->width - src_x;
	if (src_y + height > src->height) height = src->height - src_y;
	if (dst_x + width  > dst->width)  width = dst->width - dst_x;
	if (dst_y + height > dst->height) height = dst->height - dst_y;
	//fprintf(stderr, "After: srcw=%d, srch=%d, dstw=%d, dstw=%d, dst_x=%d, dst_y=%d, src_x=%d, src_y=%d, width=%d, height=%d\n\n", src->width, src->height, dst->width, dst->height, dst_x, dst_y, src_x, src_y, width, height);
	
	for (int y = 0; y < height; y++)
	{
		char *src_ptr = &src->buffer[(src_y + y) * src->width + src_x];
		char *dst_ptr = &dst->buffer[(dst_y + y) * dst->width + dst_x];
		memcpy(dst_ptr, src_ptr, width);
	}
	
	Py_INCREF(Py_None);
	return Py_None;
}

PyMethodDef DMDBuffer_methods[] = {
    {"clear", (PyCFunction)DMDBuffer_clear, METH_VARARGS,
     "Sets the DMD surface to be all black."
    },
    {"set_data", (PyCFunction)DMDBuffer_set_data, METH_VARARGS|METH_KEYWORDS,
     "Sets the DMD surface to the contents of the given string."
    },
    {"get_dot", (PyCFunction)DMDBuffer_get_dot, METH_VARARGS|METH_KEYWORDS,
     "Returns the value of the given dot."
    },
    {"copy_to_rect", (PyCFunction)DMDBuffer_copy_to_rect, METH_VARARGS|METH_KEYWORDS,
     "Copies a rect from this buffer to the given buffer."
    },
    {NULL, NULL, NULL, NULL}  /* Sentinel */
};

PyTypeObject pinproc_DMDBufferType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "pinproc.DMDBuffer",         /*tp_name*/
    sizeof(pinproc_DMDBufferObject), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    DMDBuffer_dealloc,           /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "DMDBuffer object",         /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    DMDBuffer_methods,             /* tp_methods */
    0, //PinPROC_members,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)DMDBuffer_init,      /* tp_init */
    0,                         /* tp_alloc */
    DMDBuffer_new,                 /* tp_new */
};

} // Extern "C"