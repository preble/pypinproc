# To run pyprocgame under Snow Leopard when libs are compiled 32-bit: export VERSIONER_PYTHON_PREFER_32_BIT=yes

# From: http://superjared.com/entry/anatomy-python-c-module/
from distutils.core import setup, Extension
import os

uname = os.uname()

extra_compile_args = ['-O0', '-g']
extra_link_args = []
if uname[0] == 'Darwin': # Assuming that Darwin is our only platform with i386 and ppc binaries, we'll force this to build for the current platform:
	extra_compile_args += ['-arch', uname[4]]
	extra_link_args += ['-arch', uname[4]]

module1 = Extension("pinproc",
					include_dirs = ['../../include'],
					libraries = ['usb', 'ftdi', 'pinproc'],
					library_dirs = ['/usr/local/lib', '../../bin'],
					extra_compile_args = extra_compile_args,
					extra_link_args = extra_link_args,
					sources = ['pypinproc.cpp', 'dmdutil.cpp'])

setup(name = "pinproc",
      version = "0.2",
      ext_modules = [module1])
