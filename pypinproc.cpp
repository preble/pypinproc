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
	if (PyInt_Check(machineTypeObj))
		return (PRMachineType)PyInt_AsLong(machineTypeObj);
	
	if (!PyString_Check(machineTypeObj))
		return kPRMachineInvalid;

	PyObject *intObject = PyInt_FromString(PyString_AsString(machineTypeObj), NULL, 0);
	if (intObject)
	{
		PRMachineType mt = (PRMachineType)PyInt_AsLong(intObject);
		Py_DECREF(intObject);
		return mt;
	}
	// If an exception occurred in PyInt_FromString(), clear it:
	if (PyErr_Occurred())
		PyErr_Clear();
	
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
	static char *kwlist[] = {"machine_type", NULL};
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
PinPROC_driver_update_global_config(pinproc_PinPROCObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *enableOutputs = Py_False;
	PyObject *globalPolarity = Py_False;
	PyObject *useClear = Py_False;
	PyObject *strobeStartSelect = Py_False;
        int startStrobeTime;
	int matrixRowEnableIndex0;
	int matrixRowEnableIndex1;
	PyObject *activeLowMatrixRows = Py_False;
	PyObject *tickleSternWatchdog = Py_False;
	PyObject *encodeEnables = Py_False;
	PyObject *watchdogExpired = Py_False;
	PyObject *watchdogEnable = Py_False;
	int watchdogResetTime;

	static char *kwlist[] = {"enable_outputs", "global_polarity", "use_clear", "strobe_start_select", "start_strobe_time", "matrix_row_enable_index_0", "matrix_row_enable_index_1", "active_low_matrix_rows", "tickle_stern_watchdog", "encode_enables", "watchdog_expired", "watchdog_enable", "watchdog_reset_time", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "OOOOiiiOOOOOi", kwlist, &enableOutputs, &globalPolarity, &useClear, &strobeStartSelect, &startStrobeTime, &matrixRowEnableIndex0, &matrixRowEnableIndex1, &activeLowMatrixRows, &tickleSternWatchdog, &encodeEnables, &watchdogExpired, &watchdogEnable, &watchdogResetTime))
	{
		return NULL;
	}
	
	PRDriverGlobalConfig globals;
	globals.enableOutputs = enableOutputs == Py_True;
	globals.globalPolarity = globalPolarity == Py_True;
	globals.useClear = useClear == Py_True;
	globals.strobeStartSelect = strobeStartSelect == Py_True;
	globals.startStrobeTime = startStrobeTime; // milliseconds per output loop
	globals.matrixRowEnableIndex0 = matrixRowEnableIndex0;
	globals.matrixRowEnableIndex1 = matrixRowEnableIndex1;
	globals.activeLowMatrixRows = activeLowMatrixRows == Py_True;
	globals.tickleSternWatchdog = tickleSternWatchdog == Py_True;
	globals.encodeEnables = encodeEnables == Py_True;
	globals.watchdogExpired = watchdogExpired == Py_True;
	globals.watchdogEnable = watchdogEnable == Py_True;
	globals.watchdogResetTime = watchdogResetTime;

	PRResult res;
        res = PRDriverUpdateGlobalConfig(self->handle, &globals);

	if (res == kPRSuccess)
	{
		Py_INCREF(Py_None);
		return Py_None;
	}
	else
	{
		PyErr_SetString(PyExc_IOError, "Error configuring driver globals");
		return NULL;
	}
}

static PyObject *
PinPROC_driver_update_group_config(pinproc_PinPROCObject *self, PyObject *args, PyObject *kwds)
{
	int groupNum;
        int slowTime;
	int enableIndex;
	int rowActivateIndex;
	int rowEnableSelect;
	PyObject *matrixed = Py_False;
	PyObject *polarity = Py_False;
	PyObject *active = Py_False;
	PyObject *disableStrobeAfter = Py_False;

	static char *kwlist[] = {"group_num", "slow_time", "enable_index", "row_activate_index", "row_enable_select", "matrixed", "polarity", "active", "disable_strobe_after", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "iiiiiOOOO", kwlist, &groupNum, &slowTime, &enableIndex, &rowActivateIndex, &rowEnableSelect, &matrixed, &polarity, &active, &disableStrobeAfter))
	{
		return NULL;
	}
	
	PRDriverGroupConfig group;
	group.groupNum = groupNum;
        group.slowTime = slowTime;
        group.enableIndex = enableIndex;
        group.rowActivateIndex = rowActivateIndex;
        group.rowEnableSelect = rowEnableSelect;
        group.matrixed = matrixed == Py_True;
        group.polarity = polarity == Py_True;
        group.active = active == Py_True;
        group.disableStrobeAfter = disableStrobeAfter == Py_True;

	PRResult res;
        res = PRDriverUpdateGroupConfig(self->handle, &group);

	if (res == kPRSuccess)
	{
		Py_INCREF(Py_None);
		return Py_None;
	}
	else
	{
		PyErr_SetString(PyExc_IOError, "Error configuring driver group");
		return NULL;
	}
}

static PyObject *
PinPROC_driver_group_disable(pinproc_PinPROCObject *self, PyObject *args, PyObject *kwds)
{
	int number;
	static char *kwlist[] = {"number", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i", kwlist, &number))
	{
		return NULL;
	}
	
	PRResult res;
	res = PRDriverGroupDisable(self->handle, number);
	if (res == kPRSuccess)
	{
		Py_INCREF(Py_None);
		return Py_None;
	}
	else
	{
		PyErr_SetString(PyExc_IOError, "Error disabling driver group");
		return NULL;
	}
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
	PyObject *drive_outputs_now = Py_False;
	static char *kwlist[] = {"number", "event_type", "rule", "linked_drivers", "drive_outputs_now", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "isO|OO", kwlist, &number, &eventTypeStr, &ruleObj, &linked_driversObj, &drive_outputs_now))
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

	if (PRSwitchUpdateRule(self->handle, number, eventType, &rule, drivers, numDrivers, drive_outputs_now == Py_True) == kPRSuccess)
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
PinPROC_aux_send_commands(pinproc_PinPROCObject *self, PyObject *args, PyObject *kwds)
{
	int address;
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
}

static PyObject *
PinPROC_write_data(pinproc_PinPROCObject *self, PyObject *args, PyObject *kwds)
{
	int module;
	int address;
	int data;
	static char *kwlist[] = {"module", "address", "data", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "iii", kwlist, &module, &address, &data))
	{
		return NULL;
	}
	
	fprintf(stderr, "\n\nWriting data:%x to addr:%d\n\n", data, address);

	if (PRWriteData(self->handle, module, address, 1, (uint32_t *)&data) == kPRSuccess)
	{
		Py_INCREF(Py_None);
		return Py_None;
	}
	else
	{
		PyErr_SetString(PyExc_IOError, PRGetLastErrorText()); //"Error writing data");
		return NULL;
	}
}

static PyObject *
PinPROC_watchdog_tickle(pinproc_PinPROCObject *self, PyObject *args)
{
	PRDriverWatchdogTickle(self->handle);
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

static PyObject *
PinPROC_flush(pinproc_PinPROCObject *self, PyObject *args)
{
	PRResult res = PRFlushWriteData(self->handle);
	ReturnOnErrorAndSetIOError(res);
	Py_INCREF(Py_None);
	return Py_None;
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

static PyObject *
PinPROC_dmd_draw(pinproc_PinPROCObject *self, PyObject *args)
{
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
	
	if (PyObject_TypeCheck(dotsObj, &pinproc_DMDBufferType))
	{
		pinproc_DMDBufferObject *buffer = (pinproc_DMDBufferObject *)dotsObj;
		if (buffer->frame->size.width != kDMDColumns || buffer->frame->size.height != kDMDRows)
		{
			fprintf(stderr, "w=%d h=%d", buffer->frame->size.width, buffer->frame->size.height);
			PyErr_SetString(PyExc_ValueError, "Buffer dimensions are incorrect");
			return NULL;
		}
		DMDFrameCopyPROCSubframes(buffer->frame, dots, kDMDColumns, kDMDRows, 4, self->dmdMapping);
	}
	else
	{
		PyErr_SetString(PyExc_ValueError, "Expected DMDBuffer or string.");
		return NULL;
	}
	
	res = PRDMDDraw(self->handle, dots);
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
    {"driver_update_global_config", (PyCFunction)PinPROC_driver_update_global_config, METH_VARARGS | METH_KEYWORDS,
     "Sets the driver global configuratiaon"
    },
    {"driver_update_group_config", (PyCFunction)PinPROC_driver_update_group_config, METH_VARARGS | METH_KEYWORDS,
     "Sets the driver group configuratiaon"
    },
    {"driver_group_disable", (PyCFunction)PinPROC_driver_group_disable, METH_VARARGS | METH_KEYWORDS,
     "Disables the specified driver group"
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
    {"flush", (PyCFunction)PinPROC_flush, METH_VARARGS,
     "Writes out all buffered data to the hardware"
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
    {"write_data", (PyCFunction)PinPROC_write_data, METH_VARARGS | METH_KEYWORDS,
     "Write data directly to a P-ROC memory address"
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
pinproc_normalize_machine_type(PyObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *machineTypeObj;
	static char *kwlist[] = {"machine_type", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &machineTypeObj))
		return NULL;
	PRMachineType machineType = PyObjToMachineType(machineTypeObj);
	return Py_BuildValue("i", machineType);
}

static PyObject *
pinproc_driver_state_disable(PyObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *dict;
	static char *kwlist[] = {"state", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &dict))
		return NULL;
	PRDriverState driver;
	if (!PyDictToDriverState(dict, &driver))
		return NULL;
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
	if (!PyDictToDriverState(dict, &driver))
		return NULL;
	PRDriverStatePulse(&driver, ms);
	return PyDictFromDriverState(&driver);
}

static PyObject *
pinproc_driver_state_schedule(PyObject *self, PyObject *args, PyObject *kwds)
{
	PyObject *dict;
	long long schedule;
	int seconds, now;
	static char *kwlist[] = {"state", "schedule", "seconds", "now", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "OLii", kwlist, &dict, &schedule, &seconds, &now))
		return NULL;
	PRDriverState driver;
	if (!PyDictToDriverState(dict, &driver))
		return NULL;
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
	if (!PyDictToDriverState(dict, &driver))
		return NULL;
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
	if (!PyDictToDriverState(dict, &driver))
		return NULL;
	PRDriverStatePulsedPatter(&driver, milliseconds_on, milliseconds_off, milliseconds_patter_time);
	return PyDictFromDriverState(&driver);
}

static PyObject *
pinproc_aux_command_output_custom(PyObject *self, PyObject *args, PyObject *kwds)
{
	int data, extra_data, enables, mux_enables, delay_time;
	static char *kwlist[] = {"data", "extra_data", "enables", "mux_enables", "delay_time", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "iiiii", kwlist, &data, &extra_data, &enables, &mux_enables, &delay_time))
		return NULL;
	PRDriverAuxCommand auxCommand;
	PRDriverAuxPrepareOutput(&auxCommand, data, extra_data, enables, mux_enables, delay_time);
	return PyDictFromAuxCommand(&auxCommand);
}

static PyObject *
pinproc_aux_command_output_primary(PyObject *self, PyObject *args, PyObject *kwds)
{
	int data, extra_data, delay_time;
	static char *kwlist[] = {"data", "extra_data", "delay_time", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "iii", kwlist, &data, &extra_data, &delay_time))
		return NULL;
	PRDriverAuxCommand auxCommand;
	if (g_machineType == kPRMachineWPCAlphanumeric) { 
		PRDriverAuxPrepareOutput(&auxCommand, data, extra_data, 8, 0, delay_time);
	}
	else if (g_machineType == kPRMachineSternWhitestar ||
                 g_machineType == kPRMachineSternSAM) {
		PRDriverAuxPrepareOutput(&auxCommand, data, 0, 6, 1, delay_time);
	}
	else return NULL;
	return PyDictFromAuxCommand(&auxCommand);
}

static PyObject *
pinproc_aux_command_output_secondary(PyObject *self, PyObject *args, PyObject *kwds)
{
	int data, extra_data, delay_time;
	static char *kwlist[] = {"data", "extra_data", "delay_time", NULL};
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "iii", kwlist, &data, &extra_data, &delay_time))
		return NULL;
	PRDriverAuxCommand auxCommand;
	if (g_machineType == kPRMachineSternWhitestar ||
                 g_machineType == kPRMachineSternSAM) {
		PRDriverAuxPrepareOutput(&auxCommand, data, 0, 11, 1, delay_time);
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
		{"normalize_machine_type", (PyCFunction)pinproc_normalize_machine_type, METH_VARARGS | METH_KEYWORDS, "Converts a string to an integer style machine type.  Integers pass through."},
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
	
    PyModule_AddIntConstant(m, "EventTypeSwitchClosedDebounced", kPREventTypeSwitchClosedDebounced);
    PyModule_AddIntConstant(m, "EventTypeSwitchOpenDebounced", kPREventTypeSwitchOpenDebounced);
    PyModule_AddIntConstant(m, "EventTypeSwitchClosedNondebounced", kPREventTypeSwitchClosedNondebounced);
    PyModule_AddIntConstant(m, "EventTypeSwitchOpenNondebounced", kPREventTypeSwitchOpenNondebounced);
    PyModule_AddIntConstant(m, "EventTypeDMDFrameDisplayed", kPREventTypeDMDFrameDisplayed);
    PyModule_AddIntConstant(m, "MachineTypeWPC", kPRMachineWPC);
    PyModule_AddIntConstant(m, "MachineTypeWPCAlphanumeric", kPRMachineWPCAlphanumeric);
    PyModule_AddIntConstant(m, "MachineTypeWPC95", kPRMachineWPC95);
    PyModule_AddIntConstant(m, "MachineTypeSternSAM", kPRMachineSternSAM);
    PyModule_AddIntConstant(m, "MachineTypeSternWhitestar", kPRMachineSternWhitestar);
    PyModule_AddIntConstant(m, "MachineTypeCustom", kPRMachineCustom);
    PyModule_AddIntConstant(m, "MachineTypeInvalid", kPRMachineInvalid);
    
}

}
