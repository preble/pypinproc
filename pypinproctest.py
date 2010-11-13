# This script is intended to be run:
# 
#   python -i pypinproctest.py
# 
import pinproc
import yaml

machine_type = pinproc.MachineTypeWPC

pr = pinproc.PinPROC(machine_type)
pr.reset(1)

def pulse(n, t = 20):
	"""docstring for pulse"""
	pr.driver_pulse(pinproc.decode(machine_type, str(n)), t)

del pr
