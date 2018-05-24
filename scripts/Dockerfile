#
# Simple Dockerfile to build a VNF
#
FROM centos
MAINTAINER John McDowall <jmcdowall@paloaltonetworks.com>
#
# Copy the VNF image from host machine
#
COPY src/*.c /usr/share/vnf/src/
COPY include/*.h /usr/share/vnf/include/
#
COPY vnf /usr/bin/vnf
COPY addshm /usr/bin/addshm
#
# add soome tools for debugging
#
RUN yum -y install iproute
RUN yum -y install net-tools
RUN yum -y install iperf3
RUN yum -y install tcpdump
RUN yum -y install gcc
RUN yum -y install gdb
RUN debuginfo-install -y glibc-2.17-196.el7_4.2.x86_64
# 
