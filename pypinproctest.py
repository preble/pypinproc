# This script is intended to be run:
# 
#   python -i pypinproctest.py
# 
import pinproc

pr = pinproc.PinPROC('wpc')
pr.reset(1)

def pulse(n, t = 50):
	"""docstring for pulse"""
	pr.driver_pulse(n, t)
