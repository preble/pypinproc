# To run pyprocgame under Snow Leopard when libs are compiled 32-bit: export VERSIONER_PYTHON_PREFER_32_BIT=yes

# From: http://superjared.com/entry/anatomy-python-c-module/
from distutils.core import setup, Extension
import os
import sys


extra_compile_args = ['-O0', '-g']
extra_compile_args.append('-Wno-write-strings') # fix "warning: deprecated conversion from string constant to 'char*'"
extra_link_args = []

# To use the ARCH flag simply:
#   ARCH=x86_64 python setup.py build
if 'ARCH' in os.environ:
	extra_compile_args += ['-arch', os.environ['ARCH']]
	extra_link_args += ['-arch', os.environ['ARCH']]

module1 = Extension("pinproc",
					include_dirs = ['../libpinproc/include'],
					libraries = ['usb', 'ftdi', 'pinproc'],
					library_dirs = ['/usr/local/lib', '../libpinproc/bin'],
					extra_compile_args = extra_compile_args,
					extra_link_args = extra_link_args,
					sources = ['pypinproc.cpp', 'dmdutil.cpp'])

setup(name = "pinproc",
      version = "0.2",
      ext_modules = [module1])
