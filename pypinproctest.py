# This script is intended to be run:
# 
#   python -i pypinproctest.py
# 
import pinproc

pr = pinproc.PinPROC('wpc')
pr.reset(1)

def pulse(n, t = 20):
	"""docstring for pulse"""
	pr.driver_pulse(pinproc.decode(str(n)), t)
