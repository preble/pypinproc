#ifndef _DMDUTIL_H_
#define _DMDUTIL_H_

#include <Python.h>
#include "dmd.h"

typedef struct {
    PyObject_HEAD
    /* Type-specific fields go here. */
    DMDFrame *frame;
} pinproc_DMDBufferObject;

extern "C" {
	extern PyTypeObject pinproc_DMDBufferType;
}

#endif /* _DMDUTIL_H_ */
