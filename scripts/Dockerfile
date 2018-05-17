#
# Simple Dockerfile to build a VNF
#
FROM lyft/envoy:845747db88f102c0fd262ab234308e9e22f693a1
MAINTAINER John McDowall <jmcdowall@paloaltonetworks.com>
#
# Copy the VNF image from host machine
#
#COPY /home/jmcdowall/repository/sample-vnf/bin/vnf /usr/bin/vnf
COPY src/*.c /usr/share/vnf/src/
#COPY /home/jmcdowall/repository/sample-vnf/src/vnfrw.c /usr/share/vnf/src
#COPY /home/jmcdowall/repository/sample-vnf/src/vnfutil.c /usr/share/vnf/src
#COPY /home/jmcdowall/repository/sample-vnf/src/vnferror.c /usr/share/vnf/src
#
#COPY /home/jmcdowall/repository/sample-vnf/include/vnfapp.h /usr/share/vnf/include
COPY include/*.h /usr/share/vnf/include/
#
COPY vnf /usr/bin/vnf
COPY addshm /usr/bin/addshm
#
# Set up shared memory
#
# RUN addshm -m 2048
#
# Run the VNF in the backgound - assuming eth0 is
# the dataplane interface
#
RUN apt-get -y update
RUN apt-get -y install iproute
RUN apt-get  -y install net-tools
RUN apt-get -y  install tcpdump
RUN apt-get -y install iperf3
#RUN apt install gcc
#RUN apt install gdb
#RUN debuginfo-install -y glibc-2.17-196.el7_4.2.x86_64
# ENTRYPOINT ["vnf"]
# CMD ["-f eth0 &"]
#CMD addshm -m 2048; vnf -f eth0 &
#CMD addshm -m 2048
