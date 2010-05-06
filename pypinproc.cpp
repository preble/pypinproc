#include <Python.h>
#include "pinproc.h"
#include "dmdutil.h"

extern "C" {

static PRMachineType g_machineType;

#define ReturnOnErrorAndSetIOError(res) { if (res != kPRSuccess) { PyErr_SetString(PyExc_IOError, PRGetLastErrorText()); return NULL; } }

const static int dmdMappingSize = 16;

typedef struct {
    PyObject_HEAD
    /* Type-specific fields go here. */
	PRHandle handle;
	PRMachineType machineType; // We save it here because there's no "get machine type" in libpinproc.
	bool dmdConfigured;
	unsigned char dmdMapping[dmdMappingSize];
} pinproc_PinPROCObject;

static PyObject *
PinPROC_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    pinproc_PinPROCObject *self;

    self = (pinproc_PinPROCObject *)type->tp_alloc(type, 0);
    if (self != NULL) {
		self->handle = kPRHandleInvalid;
		self->dmdConfigured = false;
		for (int i = 0; i < dmdMappingSize; i++)
		{
			self->dmdMapping[i] = i;
		}
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
	else if (strcmp(PyString_AsString(machineTypeObj), "wpcAlphanumeric") == 0)
		return kPRMachineWPCAlphanumeric;
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
	self->machineType = PyObjToMachineType(machineTypeObj);
	if (self->machineType == kPRMachineInvalid)
	{
		PyErr_SetString(PyExc_ValueError, "Unknown machine type.  Expecting wpc, wpc95, sternSAM, sternWhitestar, or custom.");
		return -1;
	}
	//PRLogSetLevel(kPRLogVerbose);
	self->handle = PRCreate(self->machineType);
	
	if (self->handle == kPRHandleInvalid)
	{
		PyErr_SetString(PyExc_IOError, PRGetLastErrorText());
		return -1;
	}

	//if (self->machineType != kPRMachineCustom)
	//{
	//	PRDriverLoadMachineTypeDefaults(self->handle, machineType);
	//}

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
		res = PRDriverWatchdogTickle(self->handle);
		ReturnOnErrorAndSetIOError(res);
		res = PRFlushWriteData(self->handle);
		ReturnOnErrorAndSetIOError(res);
		
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
		res = PRFlushWriteData(self->handle);
		ReturnOnErrorAndSetIOError(res);
		
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
		res = PRFlushWriteData(self->handle);
		ReturnOnErrorAndSetIOError(res);
		
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
PinPROC_driver_pulsed_patter(pinproc_PinPROCObject *self, PyObject *args, PyObject *kwds)
{
	int number, millisOn, millisOff, millisPatterTime;
	static char *kwlist[] = {"number", "milliseconds_on", "milliseconds_off", "milliseconds_overall_patter_time", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "iiii", kwlist, &number, &millisOn, &millisOff, &millisPatterTime))
		return NULL;
	
	PRResult res;
	res = PRDriverPulsedPatter(self->handle, number, millisOn, millisOff, millisPatterTime);
	if (res == kPRSuccess)
	{
		res = PRFlushWriteData(self->handle);
		ReturnOnErrorAndSetIOError(res);
		
		Py_INCREF(Py_None);
		return Py_None;
	}
	else
	{
		PyErr_SetString(PyExc_IOError, "Error pulse-pattering driver");
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
		res = PRDriverWatchdogTickle(self->handle);
		ReturnOnErrorAndSetIOError(res);
		res = PRFlushWriteData(self->handle);
		ReturnOnErrorAndSetIOError(res);
		
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
PyObject *PyDictFromAuxCommand(PRDriverAuxCommand *auxCommand)
{
	PyObject *dict = PyDict_New();
	DICT_SET_STRING_INT("active", auxCommand->active);
	DICT_SET_STRING_INT("delayTime", auxCommand->delayTime);
	DICT_SET_STRING_INT("jumpAddr", auxCommand->jumpAddr);
	DICT_SET_STRING_INT("command", auxCommand->command);
	DICT_SET_STRING_INT("data", auxCommand->data);
	DICT_SET_STRING_INT("extraData", auxCommand->extraData);
	DICT_SET_STRING_INT("enables", auxCommand->enables);
	DICT_SET_STRING_INT("muxEnables", auxCommand->muxEnables);
	return dict;
}
bool PyDictToAuxCommand(PyObject *dict, PRDriverAuxCommand *auxCommand)
{
	DICT_GET_STRING_INT("active", auxCommand->active);
	DICT_GET_STRING_INT("delayTime", auxCommand->delayTime);
	DICT_GET_STRING_INT("jumpAddr", auxCommand->jumpAddr);
	DICT_GET_STRING_INT("command", auxCommand->command);
	DICT_GET_STRING_INT("data", auxCommand->data);
	DICT_GET_STRING_INT("extraData", auxCommand->extraData);
	DICT_GET_STRING_INT("enables", auxCommand->enables);
	DICT_GET_STRING_INT("muxEnables", auxCommand->muxEnables);
	return true;
}

PyObject *PyDictFromSwitchRule(PRSwitchRule *sw)
{
	PyObject *dict = PyDict_New();
	DICT_SET_STRING_INT("notifyHost", sw->notifyHost);
	DICT_SET_STRING_INT("reloadActive", sw->reloadActive);
	return dict;
}
bool PyDictToSwitchRule(PyObject *dict, PRSwitchRule *sw)
{
	DICT_GET_STRING_INT("notifyHost", sw->notifyHost);
	DICT_GET_STRING_INT("reloadActive", sw->reloadActive);
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
	if (linked_driversObj != NULL) {
		numDrivers = (int)PyList_Size(linked_driversObj);
	}

	bool use_column_8;
	use_column_8 = g_machineType == kPRMachineWPC;
        static bool firstTime = true;
        if (firstTime)
        {
            firstTime = false;
            PRSwitchConfig switchConfig;
            switchConfig.clear = false;
            switchConfig.use_column_8 = use_column_8;
            switchConfig.use_column_9 = false; // No WPC machines actually use this
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
	PRResult res = PRFlushWriteData(self->handle);
	ReturnOnErrorAndSetIOError(res);
}

static PyObject *
PinPROC_aux_send_commands(pinproc_PinPROCObject *self, PyObject *args, PyObject *kwds)
{
	int address;
	const char *eventTypeStr = NULL;
	PyObject *commandsObj = NULL;
	static char *kwlist[] = {"address", "aux_commands", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "iO", kwlist, &address, &commandsObj))
	{
		return NULL;
	}
	
	PRDriverAuxCommand *commands = NULL;
	int numCommands = 0;
	if (commandsObj != NULL) {
		numCommands = (int)PyList_Size(commandsObj);
	}
	//fprintf(stderr, "AuxNumCommands = %d\n", numCommands);

	if (numCommands > 0)
	{
		commands = (PRDriverAuxCommand*)malloc(numCommands * sizeof(PRDriverAuxCommand));
		for (int i = 0; i < numCommands; i++)
		{
			if (!PyDictToAuxCommand(PyList_GetItem(commandsObj, i), &commands[i]))
			{
				free(commands);
				return NULL;
			}
			else {
/*
				fprintf(stderr, "\n\nAuxCommand #%d\n", i);
				fprintf(stderr, "active:%d\n", commands[i].active);
				fprintf(stderr, "muxEnables:%d\n", commands[i].muxEnables);
				fprintf(stderr, "command:%d\n", commands[i].command);
				fprintf(stderr, "enables:%d\n", commands[i].enables);
				fprintf(stderr, "extraData:%d\n", commands[i].extraData);
				fprintf(stderr, "data:%d\n", commands[i].data);
				fprintf(stderr, "delayTime:%d\n", commands[i].delayTime);
				fprintf(stderr, "jumpAddr:%d\n", commands[i].jumpAddr);
*/
			}
		}
	}

	fprintf(stderr, "\n\nSending Aux Commands: numCommands:%d, addr:%d\n\n", numCommands, address);
	
	if (PRDriverAuxSendCommands(self->handle, commands, numCommands, address) == kPRSuccess)
	{
		if (commands)
			free(commands);
		Py_INCREF(Py_None);
		return Py_None;
	}
	else
	{
		if (commands)
			free(commands);
		PyErr_SetString(PyExc_IOError, PRGetLastErrorText()); //"Error sending aux commands");
		return NULL;
	}
	PRResult res = PRFlushWriteData(self->handle);
	ReturnOnErrorAndSetIOError(res);
}

static PyObject *
PinPROC_watchdog_tickle(pinproc_PinPROCObject *self, PyObject *args)
{
	PRResult res;
	PRDriverWatchdogTickle(self->handle);
	res = PRFlushWriteData(self->handle);
	ReturnOnErrorAndSetIOError(res);
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
	if (numEvents < 0)
	{
		PyErr_SetString(PyExc_IOError, PRGetLastErrorText());
		return NULL;
	}
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

void PRDMDConfigPopulateDefaults(PRDMDConfig *dmdConfig)
{
	memset(dmdConfig, 0x0, sizeof(PRDMDConfig));
	dmdConfig->enableFrameEvents = true;
	dmdConfig->numRows = kDMDRows;
	dmdConfig->numColumns = kDMDColumns;
	dmdConfig->numSubFrames = kDMDSubFrames;
	dmdConfig->numFrameBuffers = kDMDFrameBuffers;
	dmdConfig->autoIncBufferWrPtr = true;

	for (int i = 0; i < dmdConfig->numSubFrames; i++)
	{
		dmdConfig->rclkLowCycles[i] = 15;
		dmdConfig->latchHighCycles[i] = 15;
		dmdConfig->dotclkHalfPeriod[i] = 1;
	}
	
	dmdConfig->deHighCycles[0] = 90;
	dmdConfig->deHighCycles[1] = 190; //250;
	dmdConfig->deHighCycles[2] = 50;
	dmdConfig->deHighCycles[3] = 377; // 60fps
}

static PyObject *
PinPROC_dmd_update_config(pinproc_PinPROCObject *self, PyObject *args, PyObject *kwds)
{
	int i;
	PyObject *high_cycles_list = NULL;
	static char *kwlist[] = {"high_cycles", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O", kwlist, &high_cycles_list))
	{
		return NULL;
	}
	
	PRDMDConfig dmdConfig;
	PRDMDConfigPopulateDefaults(&dmdConfig);

	if (high_cycles_list != NULL)
	{
		int cycles_len = PySequence_Length(high_cycles_list);
		if (cycles_len != 4)
		{
			PyErr_SetString(PyExc_ValueError, "len(high_cycles) must be 4");
			return NULL;
		}
		for (i = 0; i < 4; i++)
		{
			PyObject *item = PySequence_GetItem(high_cycles_list, i);
			if (PyInt_Check(item) == 0)
			{
				PyErr_SetString(PyExc_ValueError, "high_cycles members must be integers");
				return NULL;
			}
			dmdConfig.deHighCycles[i] = PyInt_AsLong(item);
			fprintf(stderr, "dmdConfig.deHighCycles[%d] = %d\n", i, dmdConfig.deHighCycles[i]);
		}
	}
	
	PRDMDUpdateConfig(self->handle, &dmdConfig);
	self->dmdConfigured = true;

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
PinPROC_dmd_set_color_mapping(pinproc_PinPROCObject *self, PyObject *args, PyObject *kwds)
{
	int i;
	PyObject *mapping_list = NULL;
	static char *kwlist[] = {"mapping", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &mapping_list))
	{
		return NULL;
	}
	
	int len = PySequence_Length(mapping_list);
	if (len != dmdMappingSize)
	{
		PyErr_SetString(PyExc_ValueError, "len(mapping) incorrect");
		return NULL;
	}
	for (i = 0; i < dmdMappingSize; i++)
	{
		PyObject *item = PySequence_GetItem(mapping_list, i);
		if (PyInt_Check(item) == 0)
		{
			PyErr_SetString(PyExc_ValueError, "mapping members must be integers");
			return NULL;
		}
		self->dmdMapping[i] = PyInt_AsLong(item);
		fprintf(stderr, "dmdMapping[%d] = %d\n", i, self->dmdMapping[i]);
	}
	
	Py_INCREF(Py_None);
	return Py_None;
}
#define drawdot(subFrame) dots[subFrame*(kDMDColumns*kDMDRows/8) + ((row*kDMDColumns+col)/8)] |= 1 << (col % 8)

static PyObject *
PinPROC_dmd_draw(pinproc_PinPROCObject *self, PyObject *args)
{
	int i;
	PRResult res;
	PyObject *dotsObj;
	if (!PyArg_ParseTuple(args, "O", &dotsObj))
		return NULL;
	
	if (!self->dmdConfigured)
	{
		PRDMDConfig dmdConfig;
		PRDMDConfigPopulateDefaults(&dmdConfig);
		res = PRDMDUpdateConfig(self->handle, &dmdConfig);
		ReturnOnErrorAndSetIOError(res);
		self->dmdConfigured = true;
	}
	
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
		for (int row = 0; row < kDMDRows; row++)
		{
			for (int col = 0; col < kDMDColumns; col++)
			{
				char dot = buffer->buffer[row*buffer->width+col];
				if (dot == 0)
					continue;
				else
				{
					dot &= 0x0f;
					dot = self->dmdMapping[dot]; // Apply the mapping from dmd_set_color_mapping()
					int mappedColors[] = {0, 2, 8, 10, 1, 3, 9, 11, 4, 6, 12, 14, 5, 7, 13, 15};
					dot = mappedColors[dot];
					if (dot & 0x1) drawdot(0);
					if (dot & 0x2) drawdot(1);
					if (dot & 0x4) drawdot(2);
					if (dot & 0x8) drawdot(3);
				}
			}
		}
	}	
	
	res = PRDMDDraw(self->handle, dots);
	ReturnOnErrorAndSetIOError(res);
	res = PRFlushWriteData(self->handle);
	ReturnOnErrorAndSetIOError(res);
	
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
    {"set_dmd_color_mapping", (PyCFunction)PinPROC_dmd_set_color_mapping, METH_VARARGS | METH_KEYWORDS,
     "Configures the DMD color mapping"
    },
    {"dmd_update_config", (PyCFunction)PinPROC_dmd_update_config, METH_VARARGS | METH_KEYWORDS,
     "Configures the DMD"
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
    {"driver_pulsed_patter", (PyCFunction)PinPROC_driver_pulsed_patter, METH_VARARGS | METH_KEYWORDS,
     "Pulse-Patters the specified driver"
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
    {"aux_send_commands", (PyCFunction)PinPROC_aux_send_commands, METH_VARARGS | METH_KEYWORDS,
     "Writes aux port commands into the Aux port instruction memory"
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
	g_machineType = machineType;
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

static PyObject *
pinproc_driver_state_pulsed_patter(PyObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *dict;
	int milliseconds_on, milliseconds_off, milliseconds_patter_time;
	static char *kwlist[] = {"state", "milliseconds_on", "milliseconds_off", "milliseconds_overal_patter_time", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "Oiii", kwlist, &dict, &milliseconds_on, &milliseconds_off, &milliseconds_patter_time))
		return NULL;
	PRDriverState driver;
	PyDictToDriverState(dict, &driver);
	PRDriverStatePulsedPatter(&driver, milliseconds_on, milliseconds_off, milliseconds_patter_time);
	return PyDictFromDriverState(&driver);
}

static PyObject *
pinproc_aux_command_output_custom(PyObject *self, PyObject *args, PyObject *kwds)
{
	int data, extra_data, enables, mux_enables;
	static char *kwlist[] = {"data", "extra_data", "enables", "mux_enables", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "iiii", kwlist, &data, &extra_data, &enables, &mux_enables))
		return NULL;
	PRDriverAuxCommand auxCommand;
	PRDriverAuxPrepareOutput(&auxCommand, data, extra_data, enables, mux_enables);
	return PyDictFromAuxCommand(&auxCommand);
}

static PyObject *
pinproc_aux_command_output_primary(PyObject *self, PyObject *args, PyObject *kwds)
{
	int data, extra_data;
	static char *kwlist[] = {"data", "extra_data", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "ii", kwlist, &data, &extra_data))
		return NULL;
	PRDriverAuxCommand auxCommand;
	if (g_machineType == kPRMachineWPCAlphanumeric) { 
		PRDriverAuxPrepareOutput(&auxCommand, data, extra_data, 8, 0);
	}
	else if (g_machineType == kPRMachineSternWhitestar ||
                 g_machineType == kPRMachineSternSAM) {
		PRDriverAuxPrepareOutput(&auxCommand, data, 0, 6, 1);
	}
	else return NULL;
	return PyDictFromAuxCommand(&auxCommand);
}

static PyObject *
pinproc_aux_command_output_secondary(PyObject *self, PyObject *args, PyObject *kwds)
{
	int data, extra_data;
	static char *kwlist[] = {"data", "extra_data", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "ii", kwlist, &data, &extra_data))
		return NULL;
	PRDriverAuxCommand auxCommand;
	if (g_machineType == kPRMachineSternWhitestar ||
                 g_machineType == kPRMachineSternSAM) {
		PRDriverAuxPrepareOutput(&auxCommand, data, 0, 11, 1);
	}
	else return NULL;
	return PyDictFromAuxCommand(&auxCommand);
}

static PyObject *
pinproc_aux_command_delay(PyObject *self, PyObject *args, PyObject *kwds)
{
	int delay_time;
	static char *kwlist[] = {"delay_time", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &delay_time))
		return NULL;
	PRDriverAuxCommand auxCommand;
	PRDriverAuxPrepareDelay(&auxCommand, delay_time);
	return PyDictFromAuxCommand(&auxCommand);
}

static PyObject *
pinproc_aux_command_jump(PyObject *self, PyObject *args, PyObject *kwds)
{
	int jump_address;
	static char *kwlist[] = {"jump_address", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &jump_address))
		return NULL;
	PRDriverAuxCommand auxCommand;
	PRDriverAuxPrepareJump(&auxCommand, jump_address);
	return PyDictFromAuxCommand(&auxCommand);
}

static PyObject *
pinproc_aux_command_disable(PyObject *self, PyObject *args, PyObject *kwds)
{
	PRDriverAuxCommand auxCommand;
	PRDriverAuxPrepareDisable(&auxCommand);
	return PyDictFromAuxCommand(&auxCommand);
}

PyMethodDef methods[] = {
		{"decode", (PyCFunction)pinproc_decode, METH_VARARGS | METH_KEYWORDS, "Decode a switch, coil, or lamp number."},
		{"driver_state_disable", (PyCFunction)pinproc_driver_state_disable, METH_VARARGS | METH_KEYWORDS, "Return a copy of the given driver state to disable the driver"},
		{"driver_state_pulse", (PyCFunction)pinproc_driver_state_pulse, METH_VARARGS | METH_KEYWORDS, "Return a copy of the given driver state to pulse the driver"},
		{"driver_state_schedule", (PyCFunction)pinproc_driver_state_schedule, METH_VARARGS | METH_KEYWORDS, "Return a copy of the given driver state to schedule the driver"},
		{"driver_state_patter", (PyCFunction)pinproc_driver_state_patter, METH_VARARGS | METH_KEYWORDS, "Return a copy of the given driver state to patter the driver"},
		{"driver_state_pulsed_patter", (PyCFunction)pinproc_driver_state_pulsed_patter, METH_VARARGS | METH_KEYWORDS, "Return a copy of the given driver state to pulsed-patter the driver"},
		{"aux_command_output_custom", (PyCFunction)pinproc_aux_command_output_custom, METH_VARARGS | METH_KEYWORDS, "Return a copy of the given aux output command"},
		{"aux_command_output_primary", (PyCFunction)pinproc_aux_command_output_primary, METH_VARARGS | METH_KEYWORDS, "Return a copy of the given primary aux output command"},
		{"aux_command_output_secondary", (PyCFunction)pinproc_aux_command_output_secondary, METH_VARARGS | METH_KEYWORDS, "Return a copy of the given secondary aux output command"},
		{"aux_command_delay", (PyCFunction)pinproc_aux_command_delay, METH_VARARGS | METH_KEYWORDS, "Return a copy of the given aux delay command"},
		{"aux_command_jump", (PyCFunction)pinproc_aux_command_jump, METH_VARARGS | METH_KEYWORDS, "Return a copy of the given aux jump command"},
		{"aux_command_disable", (PyCFunction)pinproc_aux_command_disable, METH_VARARGS | METH_KEYWORDS, "Return a copy of the given aux command disabled"},
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
