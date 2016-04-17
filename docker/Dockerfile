FROM ubuntu:14.04

RUN apt-get update && apt-get install -y \
    git \
    python-dev \
    python-pip
    
RUN pip install pytest

RUN git clone https://github.com/jackaudio/jack2
RUN cd jack2 && ./waf configure && ./waf build && ./waf install

# add search path to find libjackserver.so.0
RUN echo '/usr/local/lib' >>/etc/ld.so.conf && ldconfig

RUN echo 'jackd -d dummy &' >>$HOME/.bash_history
