from distutils.core import setup, Extension

_remctl = Extension('_remctl', 
		    sources=['_remctlmodule.c'],
		    libraries=['remctl'])

setup ( name='pyremctl',
	version = '0.4',
	author = 'Thomas L. Kula', 
	author_email = 'kula@tproa.net',
	url = 'http://kula.tproa.net/code/pyremctl/',
	description = 'An interface to remctl',
	ext_modules = [_remctl],
	py_modules = ['remctl'])
