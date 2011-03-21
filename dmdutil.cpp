#include "dmdutil.h"

#ifndef MIN
#define	MIN(a,b) (((a)<(b))?(a):(b))
#endif /* MIN */
#ifndef MAX
#define	MAX(a,b) (((a)>(b))?(a):(b))
#endif	/* MAX */

extern "C" {

static PyObject *
DMDBuffer_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    pinproc_DMDBufferObject *self;

    self = (pinproc_DMDBufferObject *)type->tp_alloc(type, 0);
    if (self != NULL) {
		self->frame = NULL;
    }

    return (PyObject *)self;
}

static void
DMDBuffer_dealloc(PyObject* _self)
{
	pinproc_DMDBufferObject *self = (pinproc_DMDBufferObject *)_self;
	if (self->frame != NULL)
	{
		DMDFrameDelete(self->frame);
		self->frame = NULL;
	}
    self->ob_type->tp_free((PyObject*)self);
}

static int
DMDBuffer_init(pinproc_DMDBufferObject *self, PyObject *args, PyObject *kwds)
{
	unsigned width, height;
	static char *kwlist[] = {"width", "height", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "II", kwlist, &width, &height))
	{
		return -1;
	}
	self->frame = DMDFrameCreate(DMDSizeMake(width, height));
	if (self->frame == NULL)
	{
		PyErr_SetString(PyExc_IOError, "Failed to allocate memory");
		return -1;
	}
    return 0;
}
static PyObject *
DMDBuffer_clear(pinproc_DMDBufferObject *self, PyObject *args)
{
	memset(self->frame->buffer, 0, DMDFrameGetBufferSize(self->frame));
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
	unsigned frame_size = DMDFrameGetBufferSize(self->frame);
	if (PyString_Size(data_str) != frame_size)
	{
		fprintf(stderr, "length=%d != %d", (int)PyString_Size(data_str), frame_size);
		PyErr_SetString(PyExc_ValueError, "Buffer length is incorrect");
		return NULL;
	}

	memcpy(self->frame->buffer, PyString_AsString(data_str), frame_size);

	Py_INCREF(Py_None);
	return Py_None;
}
static PyObject *
DMDBuffer_get_data(pinproc_DMDBufferObject *self, PyObject *args, PyObject *kwds)
{
	return PyString_FromStringAndSize((char *)self->frame->buffer, DMDFrameGetBufferSize(self->frame));
}
static PyObject *
DMDBuffer_get_data_mult(pinproc_DMDBufferObject *self, PyObject *args, PyObject *kwds)
{
	DMDFrame *scratch = DMDFrameCopy(self->frame);
	for (unsigned i = 0; i < self->frame->size.width * self->frame->size.height; i++)
	{
		unsigned char c = (self->frame->buffer[i] + 1) * 16 - 1;
		scratch->buffer[i] = c > 15 ? c : 0;
	}
	PyObject *output = PyString_FromStringAndSize((char*)scratch->buffer, DMDFrameGetBufferSize(self->frame));
	DMDFrameDelete(scratch);
	return output;
}
static PyObject *
DMDBuffer_get_dot(pinproc_DMDBufferObject *self, PyObject *args, PyObject *kwds)
{
	unsigned x, y;
	static char *kwlist[] = {"x", "y", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "II", kwlist, &x, &y))
	{
		return NULL;
	}
	if (x >= self->frame->size.width || y >= self->frame->size.height)
	{
		PyErr_SetString(PyExc_ValueError, "X or Y are out of range");
		return NULL;
	}

	return Py_BuildValue("i", DMDFrameGetDot(self->frame, DMDPointMake(x, y)));
}
static PyObject *
DMDBuffer_set_dot(pinproc_DMDBufferObject *self, PyObject *args, PyObject *kwds)
{
	unsigned x, y, value;
	static char *kwlist[] = {"x", "y", "value", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "III", kwlist, &x, &y, &value))
	{
		return NULL;
	}
	if (x >= self->frame->size.width || y >= self->frame->size.height)
	{
		PyErr_SetString(PyExc_ValueError, "X or Y are out of range");
		return NULL;
	}
	
	DMDFrameSetDot(self->frame, DMDPointMake(x, y), value);

	Py_INCREF(Py_None);
	return Py_None;
}
static PyObject *
DMDBuffer_fill_rect(pinproc_DMDBufferObject *self, PyObject *args, PyObject *kwds)
{
	unsigned x0, y0, width, height, value;
	static char *kwlist[] = {"x", "y", "width", "height", "value", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "IIIII", kwlist, &x0, &y0, &width, &height, &value))
	{
		return NULL;
	}
	
	DMDFrameFillRect(self->frame, DMDRectMake(x0, y0, width, height), (DMDColor)value);
	
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
DMDBuffer_copy_to_rect(pinproc_DMDBufferObject *self, PyObject *args, PyObject *kwds)
{
	pinproc_DMDBufferObject *src = self;
	pinproc_DMDBufferObject *dst;
	unsigned dst_x, dst_y, src_x, src_y, width, height;
	const char *opStr = NULL;
	static char *kwlist[] = {"dst", "dst_x", "dst_y", "src_x", "src_y", "width", "height", "op", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "OIIIIII|s", kwlist, &dst, &dst_x, &dst_y, &src_x, &src_y, &width, &height, &opStr))
	{
		return NULL;
	}
	
	DMDBlendMode blendMode = DMDBlendModeCopy;
	
	if (opStr == NULL || strcmp(opStr, "copy") == 0)
		blendMode = DMDBlendModeCopy;
	else if(strcmp(opStr, "add") == 0)
		blendMode = DMDBlendModeAdd;
	else if(strcmp(opStr, "sub") == 0)
		blendMode = DMDBlendModeSubtract;
	else if(strcmp(opStr, "blacksrc") == 0)
		blendMode = DMDBlendModeBlackSource;
	else if(strcmp(opStr, "alpha") == 0)
		blendMode = DMDBlendModeAlpha;
	else if(strcmp(opStr, "alphaboth") == 0)
		blendMode = DMDBlendModeAlphaBoth;
	else
	{
		PyErr_SetString(PyExc_ValueError, "Operation type not recognized.");
		return NULL;
	}
	
	DMDRect srcRect = DMDRectMake(src_x, src_y, width, height);
	DMDPoint dstPoint = DMDPointMake(dst_x, dst_y);
	DMDFrameCopyRect(src->frame, srcRect, dst->frame, dstPoint, blendMode);
	
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
	{"get_data", (PyCFunction)DMDBuffer_get_data, METH_VARARGS|METH_KEYWORDS,
     "Gets the dots of the DMD surface in string format."
    },
	{"get_data_mult", (PyCFunction)DMDBuffer_get_data_mult, METH_VARARGS|METH_KEYWORDS,
     "Gets the dots of the DMD surface in string format, values scaled to 0-240."
    },
    {"get_dot", (PyCFunction)DMDBuffer_get_dot, METH_VARARGS|METH_KEYWORDS,
     "Returns the value of the given dot."
    },
    {"set_dot", (PyCFunction)DMDBuffer_set_dot, METH_VARARGS|METH_KEYWORDS,
     "Assigns the value of the given dot."
    },
    {"fill_rect", (PyCFunction)DMDBuffer_fill_rect, METH_VARARGS|METH_KEYWORDS,
     "Fills a rectangle with the given dot value."
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