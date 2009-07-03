#ifndef _DMDUTIL_H_
#define _DMDUTIL_H_

#include <Python.h>

typedef struct {
    PyObject_HEAD
    /* Type-specific fields go here. */
	int width;
	int height;
	char *buffer;
} pinproc_DMDBufferObject;

#endif /* _DMDUTIL_H_ */
