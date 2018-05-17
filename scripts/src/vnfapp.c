
/*
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define _GNU_SOURCE
#include <sched.h>

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>


#include <linux/if_packet.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <net/if.h>
#include <net/ethernet.h>

#include "vnfapp.h"
#include "vnferror.h"

#define FCS_LEN        4

bool set_promiscous_mode(FILE *fp, int fd, char *intf_name);
bool get_interface_status(FILE *fp,int fd, char *intf_name);

int set_pmap(intf_config_t *config, uint8_t **read_ring, uint8_t **write_ring);
void *read_write_one(intf_config_t *f_config);
void *read_write_two(intf_config_t *f_config, intf_config_t *s_config);
int set_socket_non_blocking(int fd);
int get_mtu_size(int fd, char *name);

void vnfapp(arg_config_t *arg_config){
	
	int tstatus, ec;
    intf_config_t f_config, s_config;
    struct sockaddr_ll saddr;
    struct ifreq ifr; 
    int mtu_size;
    int sock_mark = (0x9999);
    socklen_t bufSize;

#ifdef DEBUG
    unsigned int rcvBufferSize;
    unsigned int sndBufferSize;
#endif

    errno=0;

    if ( (strcmp(arg_config->first, "") == 0)  || (strcmp(arg_config->second, "") == 0)  || (strcmp(arg_config->first,arg_config->second) == 0) ){
        if  (strcmp(arg_config->first, "") != 0){
            memset(&f_config,0,sizeof(f_config));
            strncpy(f_config.name,arg_config->first,IFNAMSIZ-1);
            f_config.max_ring_frames = arg_config->max_ring_frames;
            f_config.max_ring_blocks = arg_config->max_ring_blocks;
            f_config.max_frame_size = arg_config->max_frame_size;
            f_config.mtu_size = arg_config->mtu_size;
            f_config.single = true;
            f_config.logfile = arg_config->logfile;
            f_config.packet_log = arg_config->packet_log;
            f_config.nat_enable = arg_config->nat_enable;

        } else if (strcmp(arg_config->first, "") != 0){
            memset(&s_config,0,sizeof(s_config));
            strncpy(s_config.name,arg_config->second,IFNAMSIZ-1);
            s_config.max_ring_frames = arg_config->max_ring_frames;
            s_config.max_ring_blocks = arg_config->max_ring_blocks;
            s_config.max_frame_size = arg_config->max_frame_size;
            s_config.mtu_size = arg_config->mtu_size;
        } else {
            err_fatal(f_config.logfile,"Interface not set");
        }

    } else {
        memset(&f_config,0,sizeof(f_config));
        strncpy(f_config.name,arg_config->first,IFNAMSIZ-1);
        f_config.max_ring_frames = arg_config->max_ring_frames;
        f_config.max_ring_blocks = arg_config->max_ring_blocks;
        f_config.max_frame_size = arg_config->max_frame_size;
        f_config.mtu_size = arg_config->mtu_size;
        f_config.single = false;
        f_config.logfile = arg_config->logfile;
        f_config.packet_log = arg_config->packet_log;
        f_config.nat_enable = arg_config->nat_enable;
        memset(&s_config,0,sizeof(s_config));
        strncpy(s_config.name,arg_config->second,IFNAMSIZ-1);
        s_config.max_ring_frames = arg_config->max_ring_frames;
        s_config.max_ring_blocks = arg_config->max_ring_blocks;
        s_config.max_frame_size = arg_config->max_frame_size;
        s_config.mtu_size = arg_config->mtu_size;
        f_config.single = false;
    }
    if (f_config.single == true) {
        err_info(f_config.logfile,"Initializing Single Interface VNF APP for interface: %s", arg_config->first);
    } else {
        err_info(f_config.logfile,"Initializing Dual Interface VNF APP for interfaces: %s and %s", arg_config->first,arg_config->second);
    }
	/*
	* Create sockets
	*/
    int n = 1;
    f_config.fd = socket(PF_PACKET,SOCK_RAW,htons(ETH_P_ALL));
	if (f_config.fd == -1){
		err_fatal(f_config.logfile,"Opening first socket");
	}
    mtu_size = get_mtu_size(f_config.fd, f_config.name);
    if (mtu_size == -1 ){
        err_fatal(f_config.logfile,"Getting MTU size for: %s", f_config.name);
    } else {
        mtu_size = mtu_size + sizeof(struct ethhdr) + FCS_LEN;
        f_config.mtu_size = mtu_size;
    }
    if (setsockopt(f_config.fd, SOL_SOCKET, SO_BROADCAST, &n, sizeof n) < 0) {
        err_fatal(f_config.logfile,"SO_BROADCAST");
    }
    //if (setsockopt(f_config.fd, SOL_SOCKET, SO_MARK, &sock_mark, sizeof sock_mark) < 0) {
    //    err_fatal(f_config.logfile,"SO_MARK");
    //}
    /*
    * Configure interfaces for promiscous mode
    */
#ifdef CNTR
    if (set_promiscous_mode(f_config.logfile,f_config.fd,f_config.name) == true){
        if (get_interface_status(f_config.logfile,f_config.fd,f_config.name) ==true) {
            err_info(f_config.logfile,"Interface: %s is in promiscous mode",f_config.name);
        } else {
            err_fatal(f_config.logfile,"Setting promiscous (read) mode on: %s", f_config.name);
        }
    }
#endif
    tstatus = set_socket_non_blocking(f_config.fd);
    if (tstatus == -1) {
        err_fatal(f_config.logfile,"Setting non-blocking on first interface");
    }
    tstatus = set_pmap(&f_config, &(f_config.r_ring), &(f_config.w_ring));
    if (tstatus == -1){
        err_fatal(f_config.logfile,"Configuring first pmap on: %s", f_config.name);
    }
#ifdef DEBUG
    bufSize = sizeof(rcvBufferSize);
    getsockopt(f_config.fd, SOL_SOCKET, SO_RCVBUF, &rcvBufferSize, &bufSize);
    err_info(f_config.logfile,"initial socket receive buf %d", rcvBufferSize);
#endif
    bufSize = arg_config->max_ring_frames * arg_config->max_frame_size;
    if (setsockopt(f_config.fd, SOL_SOCKET, SO_RCVBUF, &bufSize, sizeof(bufSize)) == -1) {
        err_fatal(f_config.logfile,"SO_RCVBUF");
    }
#ifdef DEBUG
    bufSize = sizeof(rcvBufferSize);
    getsockopt(f_config.fd, SOL_SOCKET, SO_RCVBUF, &rcvBufferSize, &bufSize);
    err_info(f_config.logfile,"after set socket receive buf %d", rcvBufferSize);
#endif

#ifdef DEBUG
    bufSize = sizeof(sndBufferSize);
    getsockopt(f_config.fd, SOL_SOCKET, SO_SNDBUF, &sndBufferSize, &bufSize);
    err_info(f_config.logfile,"initial socket send buf %d", sndBufferSize);
#endif
    //n = pmmap_tx_buf_num*PAN_PACKET_MMAP_FRAME_SIZE; // To improve performance
    bufSize= arg_config->max_ring_frames * arg_config->max_frame_size;
    if (setsockopt(f_config.fd, SOL_SOCKET, SO_SNDBUF, &bufSize, sizeof(bufSize)) == -1) {
        err_fatal(f_config.logfile,"SO_SNDBUF");
    }
#ifdef DEBUG
    bufSize = sizeof(sndBufferSize);
    getsockopt(f_config.fd, SOL_SOCKET, SO_SNDBUF, &sndBufferSize, &bufSize);
    err_info(f_config.logfile,"after set socket send buf %d", sndBufferSize);
#endif
    /* convert interface name to index (in ifr.ifr_ifindex) */
    memset(&ifr,0,sizeof(ifr));
    strncpy(ifr.ifr_name, f_config.name, sizeof(ifr.ifr_name));
    ec = ioctl(f_config.fd, SIOCGIFINDEX, &ifr);
    if (ec < 0) {
        err_fatal(f_config.logfile,"Failed to find interface %s",f_config.name);
    }
    /* Bind the interface */
    memset(&saddr, 0, sizeof(saddr));
    saddr.sll_family = PF_PACKET;
    saddr.sll_protocol = htons(ETH_P_ALL);
    saddr.sll_ifindex = ifr.ifr_ifindex;
    if (bind(f_config.fd, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
        err_fatal(f_config.logfile,"Bind failed for first read socket");
    }
    if (f_config.single == false) {
        s_config.fd = socket(PF_PACKET,SOCK_RAW,htons(ETH_P_ALL));
        if (s_config.fd == -1){
    		err_fatal(f_config.logfile,"Opening second socket");
    	}
        mtu_size = get_mtu_size(s_config.fd, s_config.name);
        if (mtu_size == -1 ){
            err_fatal(f_config.logfile,"Getting MTU size for: %s", s_config.name);
        } else {
            mtu_size = mtu_size + sizeof(struct ethhdr) + FCS_LEN;
            s_config.mtu_size = mtu_size;
        }
        if (setsockopt(s_config.fd, SOL_SOCKET, SO_BROADCAST, &n, sizeof n) < 0) {
           err_fatal(f_config.logfile,"SO_BROADCAST");
        }
        //if (setsockopt(s_config.fd, SOL_SOCKET, SO_MARK, &sock_mark, sizeof sock_mark) < 0) {
        //    err_fatal(f_config.logfile,"SO_MARK");
        //}
        /*
        * Set no routing on lo
        */
        //int one = 1;
        //if (setsockopt(s_config.fd, SOL_SOCKET, SO_DONTROUTE, (char*)&one, sizeof(one))==-1) {
        //      err_fatal(f_config.logfile,"SO_DONTROUTE");
        //}
#ifdef CTNR
        if (set_promiscous_mode(f_config.logfile,s_config.fd,s_config.name) == true){
            if (get_interface_status(f_config.logfile,s_config.fd,s_config.name) ==true) {
                err_info(f_config.logfile,"Interface: %s is in promiscous mode",s_config.name);
            } else {
            err_fatal(f_config.logfile,"Setting promiscous (write) mode on: %s", s_config.name);
            }
        }
#endif
        tstatus = set_socket_non_blocking (s_config.fd);
        if (tstatus == -1) {
            err_fatal(f_config.logfile,"Setting non-blocking on second interface");
        }
        tstatus = set_pmap(&s_config, &(s_config.r_ring), &(s_config.w_ring));
        if (tstatus == -1){
           err_fatal(f_config.logfile,"Configuring second pmap on: %s", s_config.name);
        } 
#ifdef DEBUG
        bufSize = sizeof(rcvBufferSize);
        getsockopt(s_config.fd, SOL_SOCKET, SO_RCVBUF, &rcvBufferSize, &bufSize);
        err_info(f_config.logfile,"Initial socket receive buf %d", rcvBufferSize);
#endif
        n = arg_config->max_ring_frames * arg_config->max_frame_size;
        if (setsockopt(s_config.fd, SOL_SOCKET, SO_RCVBUF, &n, sizeof(n)) == -1) {
            err_fatal(f_config.logfile,"SO_RCVBUF");
        }
#ifdef DEBUG
        bufSize = sizeof(rcvBufferSize);
        getsockopt(s_config.fd, SOL_SOCKET, SO_RCVBUF, &rcvBufferSize, &bufSize);
        err_info(f_config.logfile,"After set socket receive buf %d", rcvBufferSize);
#endif

#ifdef DEBUG
        bufSize = sizeof(sndBufferSize);
        getsockopt(s_config.fd, SOL_SOCKET, SO_SNDBUF, &sndBufferSize, &bufSize);
        err_info(f_config.logfile,"Initial socket send buf %d", sndBufferSize);
    //n = pmmap_tx_buf_num*PAN_PACKET_MMAP_FRAME_SIZE; // To improve performance
#endif
        n = arg_config->max_ring_frames * arg_config->max_frame_size;
        if (setsockopt(s_config.fd, SOL_SOCKET, SO_SNDBUF, &n, sizeof(n)) == -1) {
            err_fatal(f_config.logfile,"SO_SNDBUF");
        }
#ifdef DEBUG
        bufSize = sizeof(sndBufferSize);
        getsockopt(s_config.fd, SOL_SOCKET, SO_SNDBUF, &sndBufferSize, &bufSize);
        err_info(f_config.logfile,"after set socket send buf %d", sndBufferSize);
#endif
        /* convert interface name to index (in ifr.ifr_ifindex) */
        memset(&ifr,0,sizeof(ifr));
        strncpy(ifr.ifr_name, s_config.name, sizeof(ifr.ifr_name));
        ec = ioctl(s_config.fd, SIOCGIFINDEX, &ifr);
        if (ec < 0) {
            err_fatal(f_config.logfile,"Failed to find interface %s", s_config.name);
        }
        /* 
        * Bind the interfaces
        */
        memset(&saddr, 0, sizeof(saddr));
        saddr.sll_family = PF_PACKET;
        saddr.sll_protocol = htons(ETH_P_ALL);
        saddr.sll_ifindex = ifr.ifr_ifindex;
        if (bind(s_config.fd, (struct sockaddr*)&saddr, sizeof(saddr)) < 0) {
            err_fatal(f_config.logfile,"Bind failed for second read socket");
        }
    }
	/*
	* Read from interface and write to other interface
	*/
    if (f_config.single == false) {
        read_write_two(&f_config, &s_config);
    } else {
        read_write_one(&f_config);
    }
}

