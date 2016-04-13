from distutils.core import setup, Extension

import glob

setup(
    name = 'jacker',
    version = '0.3.1',
    description = 'Python bindings for the JACK Audio Server C API',
    author = 'Fabian Peter Hammerle',
    author_email = 'fabian.hammerle@gmail.com',
    url = 'https://github.com/fphammerle/jacker',
    download_url = 'https://github.com/fphammerle/jacker/tarball/0.3.1',
    keywords = ['audio', 'jack'],
    classifiers = [],
    ext_modules = [
        Extension(
            'jack', 
            sources = glob.glob('*.c'),
            libraries = ['jack'],
            )
        ],
    )
