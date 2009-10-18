#include <Python.h>
#include "pinproc.h"
#include "dmdutil.h"

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

PRMachineType PyObjToMachineType(PyObject *machineTypeObj)
{
	if (strcmp(PyString_AsString(machineTypeObj), "wpc") == 0)
		return kPRMachineWPC;
	else if (strcmp(PyString_AsString(machineTypeObj), "wpc95") == 0)
		return kPRMachineWPC95;
	else if (strcmp(PyString_AsString(machineTypeObj), "sternSAM") == 0)
		return  kPRMachineSternSAM;
	else if (strcmp(PyString_AsString(machineTypeObj), "sternWhitestar") == 0)
		return  kPRMachineSternWhitestar;
	else if (strcmp(PyString_AsString(machineTypeObj), "custom") == 0)
		return kPRMachineCustom;
	return kPRMachineInvalid;
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
	PRMachineType machineType = PyObjToMachineType(machineTypeObj);
	if (machineType == kPRMachineInvalid)
	{
		PyErr_SetString(PyExc_ValueError, "Unknown machine type.  Expecting wpc, wpc95, sternSAM, sternWhitestar, or custom.");
		return -1;
	}
	//PRLogSetLevel(kPRLogVerbose);
	self->handle = PRCreate(machineType);
	
	if (self->handle == kPRHandleInvalid)
	{
		PyErr_SetString(PyExc_IOError, PRGetLastErrorText());
		return -1;
	}
	PRDriverLoadMachineTypeDefaults(self->handle, machineType);

    return 0;
}

static PyObject *
PinPROC_reset(pinproc_PinPROCObject *self, PyObject *args)
{
	uint32_t resetFlags;
	if (!PyArg_ParseTuple(args, "i", &resetFlags))
		return NULL;
	if (PRReset(self->handle, resetFlags) == kPRFailure)
	{
		PyErr_SetString(PyExc_IOError, PRGetLastErrorText());
		return NULL;
	}
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
PinPROC_driver_pulse(pinproc_PinPROCObject *self, PyObject *args, PyObject *kwds)
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

static PyObject *
PinPROC_driver_schedule(pinproc_PinPROCObject *self, PyObject *args, PyObject *kwds)
{
	int number, cycleSeconds;
	long long schedule;
	PyObject *now;
	static char *kwlist[] = {"number", "schedule", "cycle_seconds", "now", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "iLiO", kwlist, &number, &schedule, &cycleSeconds, &now))
	{
		return NULL;
	}
	
	PRResult res;
	res = PRDriverSchedule(self->handle, number, (uint32_t)schedule, cycleSeconds, now == Py_True);
	if (res == kPRSuccess)
	{
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

static PyObject *
PinPROC_driver_patter(pinproc_PinPROCObject *self, PyObject *args, PyObject *kwds)
{
	int number, millisOn, millisOff, originalOnTime;
	static char *kwlist[] = {"number", "milliseconds_on", "milliseconds_off", "original_on_time", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "iiii", kwlist, &number, &millisOn, &millisOff, &originalOnTime))
		return NULL;
	
	PRResult res;
	res = PRDriverPatter(self->handle, number, millisOn, millisOff, originalOnTime);
	if (res == kPRSuccess)
	{
		PRFlushWriteData(self->handle);
		
		Py_INCREF(Py_None);
		return Py_None;
	}
	else
	{
		PyErr_SetString(PyExc_IOError, "Error pattering driver");
		return NULL;
	}
}

static PyObject *
PinPROC_driver_disable(pinproc_PinPROCObject *self, PyObject *args, PyObject *kwds)
{
	int number;
	static char *kwlist[] = {"number", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &number))
	{
		return NULL;
	}
	
	PRResult res;
	res = PRDriverDisable(self->handle, number);
	if (res == kPRSuccess)
	{
		PRDriverWatchdogTickle(self->handle);
		PRFlushWriteData(self->handle);
		
		Py_INCREF(Py_None);
		return Py_None;
	}
	else
	{
		PyErr_SetString(PyExc_IOError, "Error disabling driver");
		return NULL;
	}
}

#define DICT_SET_STRING_INT(name, value) PyDict_SetItemString(dict, name, Py_BuildValue("i", value))
#define DICT_GET_STRING_INT(name, value) { long v = PyInt_AsLong(PyDict_GetItemString(dict, name)); if (v == -1 && PyErr_Occurred()) return false; value = v; }

PyObject *PyDictFromDriverState(PRDriverState *driver)
{
	PyObject *dict = PyDict_New();
	DICT_SET_STRING_INT("driverNum", driver->driverNum);
	DICT_SET_STRING_INT("outputDriveTime", driver->outputDriveTime);
	DICT_SET_STRING_INT("polarity", driver->polarity);
	DICT_SET_STRING_INT("state", driver->state);
	DICT_SET_STRING_INT("waitForFirstTimeSlot", driver->waitForFirstTimeSlot);
	DICT_SET_STRING_INT("timeslots", driver->timeslots);
	DICT_SET_STRING_INT("patterOnTime", driver->patterOnTime);
	DICT_SET_STRING_INT("patterOffTime", driver->patterOffTime);
	DICT_SET_STRING_INT("patterEnable", driver->patterEnable);
	return dict;
}
bool PyDictToDriverState(PyObject *dict, PRDriverState *driver)
{
	DICT_GET_STRING_INT("driverNum", driver->driverNum);
	DICT_GET_STRING_INT("outputDriveTime", driver->outputDriveTime);
	DICT_GET_STRING_INT("polarity", driver->polarity);
	DICT_GET_STRING_INT("state", driver->state);
	DICT_GET_STRING_INT("waitForFirstTimeSlot", driver->waitForFirstTimeSlot);
	DICT_GET_STRING_INT("timeslots", driver->timeslots);
	DICT_GET_STRING_INT("patterOnTime", driver->patterOnTime);
	DICT_GET_STRING_INT("patterOffTime", driver->patterOffTime);
	DICT_GET_STRING_INT("patterEnable", driver->patterEnable);
	return true;
}

PyObject *PyDictFromSwitchRule(PRSwitchRule *sw)
{
	PyObject *dict = PyDict_New();
	DICT_SET_STRING_INT("notifyHost", sw->notifyHost);
	return dict;
}
bool PyDictToSwitchRule(PyObject *dict, PRSwitchRule *sw)
{
	DICT_GET_STRING_INT("notifyHost", sw->notifyHost);
	return true;
}

static PyObject *
PinPROC_driver_get_state(pinproc_PinPROCObject *self, PyObject *args, PyObject *kwds)
{
	int number;
	static char *kwlist[] = {"number", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &number))
	{
		return NULL;
	}
	
	PRResult res;
	PRDriverState driver;
	res = PRDriverGetState(self->handle, number, &driver);
	if (res == kPRSuccess)
	{
		return PyDictFromDriverState(&driver);
	}
	else
	{
		PyErr_SetString(PyExc_IOError, "Error getting driver state");
		return NULL;
	}
}

static PyObject *
PinPROC_driver_update_state(pinproc_PinPROCObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *dict;
	static char *kwlist[] = {"number", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &dict))
	{
		return NULL;
	}
	
	PRDriverState driver;
	if (!PyDictToDriverState(dict, &driver))
		return NULL;

	if (PRDriverUpdateState(self->handle, &driver) == kPRSuccess)
	{
		Py_INCREF(Py_None);
		return Py_None;
	}
	else
	{
		PyErr_SetString(PyExc_IOError, "Error getting driver state");
		return NULL;
	}
}


static PyObject *
PinPROC_switch_get_states(pinproc_PinPROCObject *self, PyObject *args)
{
	const int numSwitches = kPRSwitchPhysicalLast + 1;
	PyObject *list = PyList_New(numSwitches);
	PREventType procSwitchStates[numSwitches];
    // Get all of the switch states from the P-ROC.
    if (PRSwitchGetStates(self->handle, procSwitchStates, numSwitches) == kPRFailure)
    {
		PyErr_SetString(PyExc_IOError, "Error getting driver state");
		return NULL;
	}
	
	for (int i = 0; i < numSwitches; i++)
		PyList_SetItem(list, i, Py_BuildValue("i", procSwitchStates[i]));
    
	return list;
}	


static PyObject *
PinPROC_switch_update_rule(pinproc_PinPROCObject *self, PyObject *args, PyObject *kwds)
{
	int number;
	const char *eventTypeStr = NULL;
	PyObject *ruleObj = NULL, *linked_driversObj = NULL;
	static char *kwlist[] = {"number", "event_type", "rule", "linked_drivers", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "isO|O", kwlist, &number, &eventTypeStr, &ruleObj, &linked_driversObj))
	{
		return NULL;
	}
	
	PREventType eventType;
	if (strcmp(eventTypeStr, "closed_debounced") == 0)
		eventType = kPREventTypeSwitchClosedDebounced;
	else if (strcmp(eventTypeStr, "open_debounced") == 0)
		eventType = kPREventTypeSwitchOpenDebounced;
	else if (strcmp(eventTypeStr, "closed_nondebounced") == 0)
		eventType = kPREventTypeSwitchClosedNondebounced;
	else if (strcmp(eventTypeStr, "open_nondebounced") == 0)
		eventType = kPREventTypeSwitchOpenNondebounced;
	else
	{
		PyErr_SetString(PyExc_ValueError, "event_type is unrecognized; valid values are <closed|open>_[non]debounced");
		return NULL;
	}
	
	PRSwitchRule rule;
	if (!PyDictToSwitchRule(ruleObj, &rule))
		return NULL;
	
	PRDriverState *drivers = NULL;
	int numDrivers = 0;
	if (linked_driversObj != NULL)
		numDrivers = (int)PyList_Size(linked_driversObj);

        static bool firstTime = true;
        if (firstTime)
        {
            firstTime = false;
            PRSwitchConfig switchConfig;
            switchConfig.clear = false;
            switchConfig.hostEventsEnable = true;
            switchConfig.directMatrixScanLoopTime = 2; // milliseconds
            switchConfig.pulsesBeforeCheckingRX = 10;
            switchConfig.inactivePulsesAfterBurst = 12;
            switchConfig.pulsesPerBurst = 6;
            switchConfig.pulseHalfPeriodTime = 13; // milliseconds
            PRSwitchUpdateConfig(self->handle, &switchConfig);
        }

	if (numDrivers > 0)
	{
		drivers = (PRDriverState*)malloc(numDrivers * sizeof(PRDriverState));
		for (int i = 0; i < numDrivers; i++)
		{
			if (!PyDictToDriverState(PyList_GetItem(linked_driversObj, i), &drivers[i]))
			{
				free(drivers);
				return NULL;
			}
		}
	}

	if (PRSwitchUpdateRule(self->handle, number, eventType, &rule, drivers, numDrivers) == kPRSuccess)
	{
		if (drivers)
			free(drivers);
		Py_INCREF(Py_None);
		return Py_None;
	}
	else
	{
		if (drivers)
			free(drivers);
		PyErr_SetString(PyExc_IOError, PRGetLastErrorText()); //"Error updating switch rule");
		return NULL;
	}
}


static PyObject *
PinPROC_watchdog_tickle(pinproc_PinPROCObject *self, PyObject *args)
{
	PRDriverWatchdogTickle(self->handle);
	PRFlushWriteData(self->handle);
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
PinPROC_get_events(pinproc_PinPROCObject *self, PyObject *args)
{
	PyObject *list = PyList_New(0);
	
	const int maxEvents = 16;
	PREvent events[maxEvents];
	int numEvents = PRGetEvents(self->handle, events, maxEvents);
	for (int i = 0; i < numEvents; i++)
	{
		PyObject *dict = PyDict_New();
		PyDict_SetItemString(dict, "type", Py_BuildValue("i", events[i].type));
		PyDict_SetItemString(dict, "value", Py_BuildValue("i", events[i].value));
		PyList_Append(list, dict);
	}
	return list;
}

#define kDMDColumns (128)
#define kDMDRows (32)
#define kDMDSubFrames (4)
#define kDMDFrameBuffers (3)
#define drawdot(subFrame) dots[subFrame*(kDMDColumns*kDMDRows/8) + ((row*kDMDColumns+col)/8)] |= 1 << (col % 8)

static PyObject *
PinPROC_dmd_draw(pinproc_PinPROCObject *self, PyObject *args)
{
	int i;
	
	static bool firstTime = true;
	if (firstTime)
	{
		firstTime = false;
        // Create the structure that holds the DMD settings
        PRDMDConfig dmdConfig;
        memset(&dmdConfig, 0x0, sizeof(dmdConfig));
        
        dmdConfig.numRows = kDMDRows;
        dmdConfig.numColumns = kDMDColumns;
        dmdConfig.numSubFrames = kDMDSubFrames;
        dmdConfig.numFrameBuffers = kDMDFrameBuffers;
        dmdConfig.autoIncBufferWrPtr = true;
        dmdConfig.enableFrameEvents = true;
        
        for (i = 0; i < dmdConfig.numSubFrames; i++)
        {
            dmdConfig.rclkLowCycles[i] = 15;
            dmdConfig.latchHighCycles[i] = 15;
            dmdConfig.dotclkHalfPeriod[i] = 1;
        }
        
        dmdConfig.deHighCycles[0] = 250;
        dmdConfig.deHighCycles[1] = 400;
        dmdConfig.deHighCycles[2] = 180;
        dmdConfig.deHighCycles[3] = 800;
        
        PRDMDUpdateConfig(self->handle, &dmdConfig);
	}
	
	
	PyObject *dotsObj;
	if (!PyArg_ParseTuple(args, "O", &dotsObj))
		return NULL;
	
	uint8_t dots[4*kDMDColumns*kDMDRows/8];
	memset(dots, 0, sizeof(dots));
	
	if (PyString_Check(dotsObj))
	{
		const int rgbaSize = 4*kDMDColumns*kDMDRows;
	
		if (PyString_Size(dotsObj) != rgbaSize)
		{
			fprintf(stderr, "length=%d", (int)PyString_Size(dotsObj));
			PyErr_SetString(PyExc_ValueError, "Buffer length is incorrect");
			return NULL;
		}
		uint32_t *source = (uint32_t *)PyString_AsString(dotsObj);
		for (int row = 0; row < kDMDRows; row++)
		{
			for (int col = 0; col < kDMDColumns; col++)
			{
				uint32_t dot = source[row*kDMDColumns + col];
				uint32_t luma = ((dot>>24) & 0xff) + ((dot>>16) & 0xff) + ((dot>>8) & 0xff) + (dot & 0xff);
	            switch (luma/(1020/4)) // for testing
	            {
	                case 0:
	                    break;
	                case 1: drawdot(0); break;
	                case 2: drawdot(0); drawdot(2); break;
	                case 3: drawdot(0); drawdot(1); drawdot(2); drawdot(3); break;
	            }
			}
		}
	}
	else
	{
		// Assume that it is a DMDBuffer object
		// TODO: Check that this is a DMDBuffer object!
		pinproc_DMDBufferObject *buffer = (pinproc_DMDBufferObject *)dotsObj;
		if (buffer->width != kDMDColumns || buffer->height != kDMDRows)
		{
			fprintf(stderr, "w=%d h=%d", buffer->width, buffer->height);
			PyErr_SetString(PyExc_ValueError, "Buffer dimensions are incorrect");
			return NULL;
		}
		int sum = 0;
		for (int row = 0; row < kDMDRows; row++)
		{
			for (int col = 0; col < kDMDColumns; col++)
			{
				char dot = buffer->buffer[row*buffer->width+col];
				switch (dot)
	            {
	                case 0:
	                    break;
	                case 1: drawdot(0); break;
	                case 2: drawdot(0); drawdot(2); break;
	                case 3: drawdot(0); drawdot(1); drawdot(2); drawdot(3); break;
	            }
				sum += dot;
			}
		}
	}	
	
	PRDMDDraw(self->handle, dots);
	
	Py_INCREF(Py_None);
	return Py_None;
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
    {"dmd_draw", (PyCFunction)PinPROC_dmd_draw, METH_VARARGS,
     "Fetches recent events from P-ROC."
    },
    {"driver_pulse", (PyCFunction)PinPROC_driver_pulse, METH_VARARGS | METH_KEYWORDS,
     "Pulses the specified driver"
    },
    {"driver_schedule", (PyCFunction)PinPROC_driver_schedule, METH_VARARGS | METH_KEYWORDS,
     "Schedules the specified driver"
    },
    {"driver_patter", (PyCFunction)PinPROC_driver_patter, METH_VARARGS | METH_KEYWORDS,
     "Patters the specified driver"
    },
    {"driver_disable", (PyCFunction)PinPROC_driver_disable, METH_VARARGS | METH_KEYWORDS,
     "Disables the specified driver"
    },
    {"driver_get_state", (PyCFunction)PinPROC_driver_get_state, METH_VARARGS | METH_KEYWORDS,
     "Returns the state of the specified driver"
    },
    {"driver_update_state", (PyCFunction)PinPROC_driver_update_state, METH_VARARGS | METH_KEYWORDS,
     "Sets the state of the specified driver"
    },
    {"switch_get_states", (PyCFunction)PinPROC_switch_get_states, METH_VARARGS,
     "Gets the current state of all of the switches"
    },
    {"switch_update_rule", (PyCFunction)PinPROC_switch_update_rule, METH_VARARGS | METH_KEYWORDS,
     "Sets the state of the specified driver"
    },
	{"watchdog_tickle", (PyCFunction)PinPROC_watchdog_tickle, METH_VARARGS, 
	 "Tickles the watchdog"
	},
    {"get_events", (PyCFunction)PinPROC_get_events, METH_VARARGS,
     "Fetches recent events from P-ROC."
    },
    {"reset", (PyCFunction)PinPROC_reset, METH_VARARGS,
     "Loads defaults into memory and optionally writes them to hardware."
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


static PyObject *
pinproc_decode(PyObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *str, *machineTypeObj;
	static char *kwlist[] = {"machine_type", "number", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO", kwlist, &machineTypeObj, &str))
		return NULL;
	PRMachineType machineType = PyObjToMachineType(machineTypeObj);
	return Py_BuildValue("i", PRDecode(machineType, PyString_AsString(str)));
}

static PyObject *
pinproc_driver_state_disable(PyObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *dict;
	static char *kwlist[] = {"state", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &dict))
		return NULL;
	PRDriverState driver;
	PyDictToDriverState(dict, &driver);
	PRDriverStateDisable(&driver);
	return PyDictFromDriverState(&driver);
}

static PyObject *
pinproc_driver_state_pulse(PyObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *dict;
	int ms;
	static char *kwlist[] = {"state", "milliseconds", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "Oi", kwlist, &dict, &ms))
		return NULL;
	PRDriverState driver;
	PyDictToDriverState(dict, &driver);
	PRDriverStatePulse(&driver, ms);
	return PyDictFromDriverState(&driver);
}

static PyObject *
pinproc_driver_state_schedule(PyObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *dict;
	int schedule, seconds, now;
	static char *kwlist[] = {"state", "schedule", "seconds", "now", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "Oiii", kwlist, &dict, &schedule, &seconds, &now))
		return NULL;
	PRDriverState driver;
	PyDictToDriverState(dict, &driver);
	PRDriverStateSchedule(&driver, schedule, seconds, now);
	return PyDictFromDriverState(&driver);
}

static PyObject *
pinproc_driver_state_patter(PyObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *dict;
	int milliseconds_on, milliseconds_off, original_on_time;
	static char *kwlist[] = {"state", "milliseconds_on", "milliseconds_off", "original_on_time", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "Oiii", kwlist, &dict, &milliseconds_on, &milliseconds_off, &original_on_time))
		return NULL;
	PRDriverState driver;
	PyDictToDriverState(dict, &driver);
	PRDriverStatePatter(&driver, milliseconds_on, milliseconds_off, original_on_time);
	return PyDictFromDriverState(&driver);
}

PyMethodDef methods[] = {
		{"decode", (PyCFunction)pinproc_decode, METH_VARARGS | METH_KEYWORDS, "Decode a switch, coil, or lamp number."},
		{"driver_state_disable", (PyCFunction)pinproc_driver_state_disable, METH_VARARGS | METH_KEYWORDS, "Return a copy of the given driver state to disable the driver"},
		{"driver_state_pulse", (PyCFunction)pinproc_driver_state_pulse, METH_VARARGS | METH_KEYWORDS, "Return a copy of the given driver state to pulse the driver"},
		{"driver_state_schedule", (PyCFunction)pinproc_driver_state_schedule, METH_VARARGS | METH_KEYWORDS, "Return a copy of the given driver state to schedule the driver"},
		{"driver_state_patter", (PyCFunction)pinproc_driver_state_patter, METH_VARARGS | METH_KEYWORDS, "Return a copy of the given driver state to patter the driver"},
		{NULL, NULL, 0, NULL}};

extern PyTypeObject pinproc_DMDBufferType;

PyMODINIT_FUNC initpinproc()
{
	//pinproc_PinPROCType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&pinproc_PinPROCType) < 0)
        return;
    if (PyType_Ready(&pinproc_DMDBufferType) < 0)
        return;
	
	PyObject *m = Py_InitModule("pinproc", methods);
	
	Py_INCREF(&pinproc_PinPROCType);
	PyModule_AddObject(m, "PinPROC", (PyObject*)&pinproc_PinPROCType);
	Py_INCREF(&pinproc_DMDBufferType);
	PyModule_AddObject(m, "DMDBuffer", (PyObject*)&pinproc_DMDBufferType);
}

}
