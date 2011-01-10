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
DMDBuffer_get_data(pinproc_DMDBufferObject *self, PyObject *args, PyObject *kwds)
{
	return PyString_FromStringAndSize(self->buffer, self->width * self->height);
}
static PyObject *
DMDBuffer_get_data_mult(pinproc_DMDBufferObject *self, PyObject *args, PyObject *kwds)
{
	char *scratchBuffer = (char *)malloc(self->width * self->height);
	for (int i = 0; i < self->width * self->height; i++)
	{
		unsigned char c = (self->buffer[i] + 1) * 16 - 1;
		scratchBuffer[i] = c > 15 ? c : 0;
	}
	PyObject *output = PyString_FromStringAndSize(scratchBuffer, self->width * self->height);
	free(scratchBuffer);
	return output;
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
DMDBuffer_set_dot(pinproc_DMDBufferObject *self, PyObject *args, PyObject *kwds)
{
	int x, y, value;
	static char *kwlist[] = {"x", "y", "value", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "iii", kwlist, &x, &y, &value))
	{
		return NULL;
	}
	if (x >= self->width || y >= self->height)
	{
		PyErr_SetString(PyExc_ValueError, "X or Y are out of range");
		return NULL;
	}
	
	self->buffer[y * self->width + x] = (char)value;

	Py_INCREF(Py_None);
	return Py_None;
}
static PyObject *
DMDBuffer_fill_rect(pinproc_DMDBufferObject *self, PyObject *args, PyObject *kwds)
{
	int x0, y0, width, height, value;
	static char *kwlist[] = {"x", "y", "width", "height", "value", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "iiiii", kwlist, &x0, &y0, &width, &height, &value))
	{
		return NULL;
	}
	
	for (int x = x0; x < x0 + width; x++)
		for (int y = y0; y < y0 + height; y++)
			if (x >= 0 && x < self->width && y >= 0 && y < self->height)
				self->buffer[y * self->width + x] = (char)value;
	
	Py_INCREF(Py_None);
	return Py_None;
}

#define DEBUG_COPY_TO_RECT(statement)
//#define DEBUG_COPY_TO_RECT(statement) statement
bool ConstrainToBuffers(pinproc_DMDBufferObject *dst, int &dst_x, int &dst_y, pinproc_DMDBufferObject *src, int &src_x, int &src_y, int &width, int &height)
{
	DEBUG_COPY_TO_RECT(fprintf(stderr, "Before: src(%dx%d) @ %d, %d  dst(%dx%d) @ %d, %d  size=%dx%d\n", src->width, src->height, src_x, src_y, dst->width, dst->height, dst_x, dst_y, width, height));
	if (dst_x < 0)
	{
		src_x += -dst_x;
		width -= -dst_x;
		dst_x = 0;
		DEBUG_COPY_TO_RECT(fprintf(stderr, "% 6d: src(%dx%d) @ %d, %d  dst(%dx%d) @ %d, %d  size=%dx%d\n", __LINE__, src->width, src->height, src_x, src_y, dst->width, dst->height, dst_x, dst_y, width, height));
	}
	if (dst_y < 0)
	{
		src_y += -dst_y;
		height -= -dst_y;
		dst_y = 0;
		DEBUG_COPY_TO_RECT(fprintf(stderr, "% 6d: src(%dx%d) @ %d, %d  dst(%dx%d) @ %d, %d  size=%dx%d\n", __LINE__, src->width, src->height, src_x, src_y, dst->width, dst->height, dst_x, dst_y, width, height));
	}
	if (src_x < 0)
	{
		dst_x += -src_x;
		width -= -src_x;
		src_x = 0;
		DEBUG_COPY_TO_RECT(fprintf(stderr, "% 6d: src(%dx%d) @ %d, %d  dst(%dx%d) @ %d, %d  size=%dx%d\n", __LINE__, src->width, src->height, src_x, src_y, dst->width, dst->height, dst_x, dst_y, width, height));
	}
	if (src_y < 0)
	{
		dst_y += -src_y;
		height -= -src_y;
		src_y = 0;
		DEBUG_COPY_TO_RECT(fprintf(stderr, "% 6d: src(%dx%d) @ %d, %d  dst(%dx%d) @ %d, %d  size=%dx%d\n", __LINE__, src->width, src->height, src_x, src_y, dst->width, dst->height, dst_x, dst_y, width, height));
	}
	if ((dst_x >= dst->width) || (dst_y >= dst->height) || (src_x >= src->width) || (src_y >= src->height) || (width < 0) || (height < 0))
	{
		return false;
	}
	
	if (src_x + width  > src->width)  width = src->width - src_x;
	if (src_y + height > src->height) height = src->height - src_y;
	if (dst_x + width  > dst->width)  width = dst->width - dst_x;
	if (dst_y + height > dst->height) height = dst->height - dst_y;
	DEBUG_COPY_TO_RECT(fprintf(stderr, " After: src(%dx%d) @ %d, %d  dst(%dx%d) @ %d, %d  size=%dx%d\n", src->width, src->height, src_x, src_y, dst->width, dst->height, dst_x, dst_y, width, height));
	return true;
}
static PyObject *
DMDBuffer_copy_to_rect(pinproc_DMDBufferObject *self, PyObject *args, PyObject *kwds)
{
	pinproc_DMDBufferObject *src = self;
	pinproc_DMDBufferObject *dst;
	int dst_x, dst_y, src_x, src_y, width, height;
	const char *opStr = NULL;
	static char *kwlist[] = {"dst", "dst_x", "dst_y", "src_x", "src_y", "width", "height", "op", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "Oiiiiii|s", kwlist, &dst, &dst_x, &dst_y, &src_x, &src_y, &width, &height, &opStr))
	{
		return NULL;
	}
	
	bool anything_to_do = ConstrainToBuffers(dst, dst_x, dst_y, src, src_x, src_y, width, height);
	if (!anything_to_do)
	{
		Py_INCREF(Py_None);
		return Py_None;
	}

	if (opStr == NULL || strcmp(opStr, "copy") == 0)
	{
		for (int y = 0; y < height; y++)
		{
			char *src_ptr = &src->buffer[(src_y + y) * src->width + src_x];
			char *dst_ptr = &dst->buffer[(dst_y + y) * dst->width + dst_x];
			memcpy(dst_ptr, src_ptr, width);
		}
	}
	else if(strcmp(opStr, "add") == 0)
	{
		for (int y = 0; y < height; y++)
		{
			char *src_ptr = &src->buffer[(src_y + y) * src->width + src_x];
			char *dst_ptr = &dst->buffer[(dst_y + y) * dst->width + dst_x];
			for (int x = 0; x < width; x++)
			{
				dst_ptr[x] = MIN(dst_ptr[x] + src_ptr[x], 0xF);
			}
		}
	}
	else if(strcmp(opStr, "sub") == 0)
	{
		for (int y = 0; y < height; y++)
		{
			char *src_ptr = &src->buffer[(src_y + y) * src->width + src_x];
			char *dst_ptr = &dst->buffer[(dst_y + y) * dst->width + dst_x];
			for (int x = 0; x < width; x++)
			{
				dst_ptr[x] = MAX(dst_ptr[x] - src_ptr[x], 0);
			}
		}
	}
	else if(strcmp(opStr, "blacksrc") == 0)
	{
		for (int y = 0; y < height; y++)
		{
			char *src_ptr = &src->buffer[(src_y + y) * src->width + src_x];
			char *dst_ptr = &dst->buffer[(dst_y + y) * dst->width + dst_x];
			for (int x = 0; x < width; x++)
			{
				// Only write dots into black dots.
				if ((src_ptr[x] & 0xf) != 0)
					dst_ptr[x] = (dst_ptr[x] & 0xf0) | (src_ptr[x] & 0xf);
			}
		}
	}
	else if(strcmp(opStr, "alpha") == 0)
	{
		for (int y = 0; y < height; y++)
		{
			char *src_ptr = &src->buffer[(src_y + y) * src->width + src_x];
			char *dst_ptr = &dst->buffer[(dst_y + y) * dst->width + dst_x];
			for (int x = 0; x < width; x++)
			{
				char dst_dot = dst_ptr[x] & 0xf;
				char src_dot = src_ptr[x] & 0xf;
				char src_a = (src_ptr[x]  >> 4) & 0xf;
				char dot = src_a;
				dot = ((src_dot * (src_a)) + (dst_dot * (0xf - src_a))) >> 4;
				dst_ptr[x] = (dst_ptr[x] & 0xf0) | (dot & 0xf); // Maintain destination alpha
			}
		}
	}
	else
	{
		PyErr_SetString(PyExc_ValueError, "Operation type not recognized.");
		return NULL;
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