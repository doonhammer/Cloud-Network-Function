##
#
# NFV BITW Makefile
#
##

#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
##
#
# Source Directories
#
###
C_SRC_DIR := src
vpath  %.c $(C_SRC_DIR)
#
H_SRC_DIR := include
vpath  %.h $(H_SRC_DIR)
#
# Output directories
#
##
LIB_DIR := lib
OBJ_DIR := obj
BIN_DIR := bin
##
#
# Compiler flags
#
##
CC = gcc
#CFLAGS = -c -g -I include -I src -std=gnu99 -Wall -fPIC -DDEBUG -DFORK
CFLAGS = -c -g -I include -I src -std=gnu99 -Wall -fPIC -DFORK
##
#
# Link Flags
#
##
LD = gcc -g
LDFLAGS = -lc
##
#
# Object files
#
##
OBJS = \
	$(OBJ_DIR)/vnferror.o \
	$(OBJ_DIR)/vnftest.o \
	$(OBJ_DIR)/vnfapp.o \
    $(OBJ_DIR)/vnfutil.o \
   	$(OBJ_DIR)/vnfrw.o



all: vnf

addshm.o: addshm.c
	$(CC) $(CFLAGS) $< -o $(OBJ_DIR)/$@

vnferror.o: vnferror.c vnferror.h
	$(CC) $(CFLAGS) $< -o $(OBJ_DIR)/$@

vnftest.o: vnftest.c vnfapp.h vnferror.h
	$(CC) $(CFLAGS) $< -o $(OBJ_DIR)/$@

vnfapp.o: vnfapp.c vnfapp.h vnferror.h
	$(CC) $(CFLAGS) $< -o $(OBJ_DIR)/$@

vnfutil.o: vnfutil.c vnfapp.h vnferror.h
	$(CC) $(CFLAGS) $< -o $(OBJ_DIR)/$@

vnfrw.o: vnfrw.c vnfapp.h vnferror.h
	$(CC) $(CFLAGS) $< -o $(OBJ_DIR)/$@

vnf: vnferror.o vnftest.o vnfutil.o vnfapp.o vnfrw.o
	$(LD)  $(OBJS) $(LDFLAGS) -o $(BIN_DIR)/$@

ctnr:
	cp ./bin/vnf ./scripts ; cp ./bin/addshm ./scripts ; sudo docker build -t docker-panos-ctnr.af.paloaltonetworks.local/centos:vnf scripts

gcr:
	cp ./bin/vnf ./scripts ; cp ./bin/addshm ./scripts ; cp ./src/*.c ./scripts/src ; cp ./include/*.h ./scripts/include ;sudo docker build -t gcr.io/gcp-eng-dev/vnf:0.0.3 scripts ; gcloud docker -- push gcr.io/gcp-eng-dev/vnf:0.0.3

gcrenvoy:
	cp ./bin/vnf ./scripts ; cp ./bin/addshm ./scripts ; cp ./src/*.c ./scripts/src ; cp ./include/*.h ./scripts/include ;sudo docker build -t gcr.io/gcp-eng-dev/envoy:0.0.1 scripts ; gcloud docker -- push gcr.io/gcp-eng-dev/envoy:0.0.1


gcrvnf:
	cp ./bin/vnf ./scripts ; cp ./bin/addshm ./scripts ; cp ./src/*.c ./scripts/src ; cp ./include/*.h ./scripts/include ;sudo docker build -t gcr.io/gcp-eng-dev/vnftest:0.0.1 scripts ; gcloud docker -- push gcr.io/gcp-eng-dev/vnftest:0.0.1

addshm: addshm.o
	$(LD)  $(OBJ_DIR)/addshm.o $(LDFLAGS) -o $(BIN_DIR)/$@

.PHONY: clean,ctnr

clean:
	rm -f obj/*.o \
	rm -f bin/*
