#include <Python.h>
#include "pinproc.h"

extern "C" {

typedef struct {
    PyObject_HEAD
    /* Type-specific fields go here. */
	PRHandle handle;
} pinproc_PinPROCObject;

static PyObject *
PinPROC_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    pinproc_PinPROCObject *self;

    self = (pinproc_PinPROCObject *)type->tp_alloc(type, 0);
    if (self != NULL) {
		self->handle = kPRHandleInvalid;
    }

    return (PyObject *)self;
}

static void
PinPROC_dealloc(PyObject* _self)
{
	pinproc_PinPROCObject *self = (pinproc_PinPROCObject *)_self;
	if (self->handle != kPRHandleInvalid)
	{
		PRDelete(self->handle);
		self->handle = kPRHandleInvalid;
	}
    self->ob_type->tp_free((PyObject*)self);
}

void ConfigureDrivers(PRHandle proc, PRMachineType machineType)
{
    int i;
    int mappedDriverGroupEnableIndex[kPRDriverGroupsMax];
    int mappedWPCDriverGroupEnableIndex[] = {0, 0, 0, 0, 0, 2, 4, 3, 1, 5, 7, 7, 7, 7, 7, 7, 7, 7, 0, 0, 0, 0, 0, 0, 0, 0};
    int rowEnableIndex1;
    int rowEnableIndex0;
    bool tickleSternWatchdog;
    bool globalPolarity;
    bool activeLowMatrixRows;
    int driverLoopTime;
    int watchdogResetTime;
    int slowGroupTime;

    switch (machineType) 
    {
        case kPRMachineWPC: {
            memcpy(mappedDriverGroupEnableIndex,mappedWPCDriverGroupEnableIndex, sizeof(mappedDriverGroupEnableIndex)); 
            rowEnableIndex1 = 6; // Unused in WPC
            rowEnableIndex0 = 6;
            tickleSternWatchdog = false;
            globalPolarity = false;
            activeLowMatrixRows = true;
            driverLoopTime = 4; // milliseconds
            watchdogResetTime = 1000; // milliseconds
            slowGroupTime = driverLoopTime * 100; // microseconds
            break;
        }
    }

    PRDriverGlobalConfig globals;
    globals.enableOutputs = false;
    globals.globalPolarity = globalPolarity;
    globals.useClear = false;
    globals.strobeStartSelect = false;
    globals.startStrobeTime = driverLoopTime; // milliseconds per output loop
    globals.matrixRowEnableIndex1 = rowEnableIndex1;
    globals.matrixRowEnableIndex0 = rowEnableIndex0;
    globals.activeLowMatrixRows = activeLowMatrixRows;
    globals.tickleSternWatchdog = tickleSternWatchdog;
    globals.encodeEnables = false;
    globals.watchdogExpired = false;
    globals.watchdogEnable = true;
    globals.watchdogResetTime = watchdogResetTime;

    // We want to start up safely, so we'll update the global driver config twice.
    // When we toggle enableOutputs like this P-ROC will reset the polarity:

    // Enable now without the outputs enabled:
    PRDriverUpdateGlobalConfig(proc, &globals);

    // Now enable the outputs:  (TODO: Why?)
    globals.enableOutputs = true;
    PRDriverUpdateGlobalConfig(proc, &globals);

    // Configure the groups.  Each group corresponds to 8 consecutive drivers, starting
    // with driver #32.  The following 6 groups are configured for coils/flashlamps.

    PRDriverGroupConfig group;
    for (i = 4; i < 10; i++)
    {
        PRDriverGetGroupConfig(proc, i, &group);
        group.slowTime = 0;
        group.enableIndex = mappedDriverGroupEnableIndex[i];
        group.rowActivateIndex = 0;
        group.rowEnableSelect = 0;
        group.matrixed = false;
        group.polarity = false;
        group.active = 1;
        group.disableStrobeAfter = false;

        PRDriverUpdateGroupConfig(proc, &group);
    }

    // The following 8 groups are configured for the feature lamp matrix.
    for (i = 10; i < 18; i++) {
        PRDriverGetGroupConfig(proc, i, &group);
        group.slowTime = slowGroupTime;
        group.enableIndex = mappedDriverGroupEnableIndex[i];
        group.rowActivateIndex = i - 10;
        group.rowEnableSelect = 0;
        group.matrixed = 1;
        group.polarity = 0;
        group.active = 1;
        group.disableStrobeAfter = 1;
        PRDriverUpdateGroupConfig(proc, &group);
    }
}

static int
PinPROC_init(pinproc_PinPROCObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *machineTypeObj = NULL;
	static char *kwlist[] = {"machineType", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &machineTypeObj))
	{
		return -1;
	}
	PRMachineType machineType = kPRMachineInvalid;
	if (strcmp(PyString_AsString(machineTypeObj), "wpc") == 0)
		machineType = kPRMachineWPC;
	else if (strcmp(PyString_AsString(machineTypeObj), "stern") == 0)
		machineType = kPRMachineStern;
	else
	{
		PyErr_SetString(PyExc_ValueError, "Unknown machine type.  Expecting 'wpc' or 'stern'.");
		return -1;
	}
	
	self->handle = PRCreate(machineType);
	
	if (self->handle == kPRHandleInvalid)
	{
		PyErr_SetString(PyExc_IOError, "Error creating P-ROC handle");
		return -1;
	}
	
	ConfigureDrivers(self->handle, machineType);

    return 0;
}

static PyObject *
PinPROC_DriverPulse(pinproc_PinPROCObject *self, PyObject *args, PyObject *kwds)
{
	int number, milliseconds;
	static char *kwlist[] = {"number", "milliseconds", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "ii", kwlist, &number, &milliseconds))
	{
		return NULL;
	}
	
	PRResult res;
	res = PRDriverPulse(self->handle, number, milliseconds);
	if (res == kPRSuccess)
	{
		PRDriverWatchdogTickle(self->handle);
		PRFlushWriteData(self->handle);
		
		Py_INCREF(Py_None);
		return Py_None;
	}
	else
	{
		PyErr_SetString(PyExc_IOError, "Error pulsing driver");
		return NULL;
	}
}

// static PyMemberDef PinPROC_members[] = {
//     // {"first", T_OBJECT_EX, offsetof(Noddy, first), 0,
//     //  "first name"},
//     // {"last", T_OBJECT_EX, offsetof(Noddy, last), 0,
//     //  "last name"},
//     // {"number", T_INT, offsetof(Noddy, number), 0,
//     //  "noddy number"},
//     {NULL, NULL, NULL, 0, NULL}  /* Sentinel */
// };
static PyMethodDef PinPROC_methods[] = {
    {"driver_pulse", (PyCFunction)PinPROC_DriverPulse, METH_VARARGS | METH_KEYWORDS,
     "Pulses the specified driver"
    },
    {NULL, NULL, NULL, NULL}  /* Sentinel */
};

static PyTypeObject pinproc_PinPROCType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "pinproc.PinPROC",         /*tp_name*/
    sizeof(pinproc_PinPROCObject), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    PinPROC_dealloc,           /*tp_dealloc*/
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
    "PinPROC objects",         /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    PinPROC_methods,             /* tp_methods */
    0, //PinPROC_members,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)PinPROC_init,      /* tp_init */
    0,                         /* tp_alloc */
    PinPROC_new,                 /* tp_new */
};


static PyObject *Test(PyObject *self, PyObject *args)
{
	return Py_BuildValue("i", 42);
	//  	fprintf(stderr, "\nHowdy\n\n");
	// // PyObject *list = PyList_New(0);
	// // return list;
	// int a = 0, b = 1, c, n;
	// if (!PyArg_ParseTuple(args, "i", &n))
	//     return NULL;
	// PyObject *list = PyList_New(0);
	// while(b < n){
	//     PyList_Append(list, PyInt_FromLong(b));
	//     c = a+b;
	//     a = b;
	//     b = c;
	// }
	// return list;
}

PyMethodDef methods[] = {
		{"test", Test, METH_VARARGS, "Test function"},
		{NULL, NULL, 0, NULL}};

PyMODINIT_FUNC initpinproc()
{
	//pinproc_PinPROCType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&pinproc_PinPROCType) < 0)
        return;
	
	PyObject *m = Py_InitModule("pinproc", methods);
	
	Py_INCREF(&pinproc_PinPROCType);
	PyModule_AddObject(m, "PinPROC", (PyObject*)&pinproc_PinPROCType);
}

}
