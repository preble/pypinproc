# From: http://superjared.com/entry/anatomy-python-c-module/
from distutils.core import setup, Extension

module1 = Extension("pinproc",
					include_dirs = ['../../include'],
					libraries = ['usb', 'ftdi', 'pinproc'],
					library_dirs = ['/usr/local/lib', '../../bin'],
					extra_compile_args = ['-arch', 'i386', '-O0', '-g'], # Note: Force i386... would prefer to not build universal, just current platform...
					extra_link_args = ['-arch', 'i386'],
					sources = ['pypinproc.cpp'])

setup(name = "pinproc",
      version = "0.1",
      ext_modules = [module1])
