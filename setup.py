from distutils.core import setup, Extension

import glob

setup(
    name = 'jacker',
    version = '0.1',
    description = 'JACK API for Python',
    author = 'Fabian Peter Hammerle',
    author_email = 'fabian.hammerle@gmail.com',
    url = 'https://github.com/fphammerle/jacker',
    download_url = 'https://github.com/fphammerle/jacker/tarball/0.3',
    keywords = [],
    classifiers = [],
    ext_modules = [
        Extension(
            'jack', 
            sources = glob.glob('*.c'),
            libraries = ['jack'],
            )
        ],
    )
