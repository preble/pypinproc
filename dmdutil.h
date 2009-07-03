#include <Python.h>

typedef struct {
    PyObject_HEAD
    /* Type-specific fields go here. */
	int width;
	int height;
	char *buffer;
} pinproc_DMDBufferObject;
