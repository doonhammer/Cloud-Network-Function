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
	#include <stdbool.h>
	#include <stdio.h>
	#include <string.h>
	#include <stdlib.h>
	#include <unistd.h>
	#include <errno.h>
	#include <fcntl.h>
	//
	#include <arpa/inet.h>
	#include <linux/if_packet.h>
	//
	#include <sys/types.h>
	#include <sys/ioctl.h>
	#include <sys/socket.h>
	#include <sys/mman.h>
	#include <sys/stat.h>
	#include <sys/epoll.h>
	//
	#include <netinet/ip.h>
	#include <netinet/in.h>
	#include <net/ethernet.h>
	#include <net/if.h>

	#include "vnfapp.h"
	#include "vnferror.h"


	#define MAX_BUF 65536
	#define MAX_EVENTS 64

	uint16_t display_ethernet(uint8_t *buf);
	uint16_t write_ethernet(FILE *fp,uint8_t *buffer);
	uint16_t display_ip(uint8_t *buf);
	bool checkipv4(uint8_t *buffer);
	uint16_t write_packet(FILE *fp, intf_config_t *ingress,uint8_t *buf, char *comment);
	void display_icmp(uint8_t *buf);
	bool compare_mac(struct ifreq *ingress, struct ethhdr *packet);
	/*
	* Read and write packets between to RAW interfaces
	*/
	void read_write_one(intf_config_t *config){
		int ready;
		int j;
		int ep_fd;
		struct epoll_event e_evf;
		struct epoll_event evlist[1];
		unsigned int ringr_offset = 0;
		unsigned int ringw_offset = 0;
		unsigned int data_start = 0;
		//unsigned int data_len = 0;
		struct tpacket2_hdr *header_r, *header_w;
		unsigned int len, sent_len;
		uint8_t *cur_r, *cur_w;
		size_t sendlen;
		size_t remlen; 
		uint8_t *curpos;
	#ifdef DEBUG_ALL
		uint8_t *buf;
		uint16_t eth_proto;
		uint16_t ip_proto;
	#endif


		memset(&ep_fd,0,sizeof(ep_fd));
		ep_fd = epoll_create(1);
		if ( ep_fd == -1){
			err_fatal(config->logfile,"epoll create failed %d", errno);
		}

		memset(&e_evf,0,sizeof(e_evf));
		e_evf.data.fd = config->fd;
		e_evf.events = EPOLLIN;
		if (epoll_ctl(ep_fd,EPOLL_CTL_ADD,config->fd, &e_evf) == -1){
			err_fatal(config->logfile,"epoll_ctl failed %d", errno);	
		}
		
		while(true) {
				
			ready = epoll_wait( ep_fd, evlist, 1, -1); 
			if (ready == -1) {
				if (errno == EINTR) {
					err_fatal(config->logfile,"Interrupt"); 
				/* Restart if interrupted by signal */ 
				} else {
					err_fatal(config->logfile,"epoll_wait failed %d", errno);
				} 
			}
			
			for (j = 0; j < ready; j++) {
	#ifdef DEBUG_ALL
				err_info(config->logfile," fd = %d; events: %s %s %s %s\n", evlist[j].data.fd,
					(evlist[j].events & EPOLLIN)  ? "EPOLLIN "  : "", 
					(evlist[j].events & EPOLLOUT) ? "EPOLLOUT " : "", 
					(evlist[j].events & EPOLLHUP) ? "EPOLLHUP " : "", 
					(evlist[j].events & EPOLLERR) ? "EPOLLERR " : "");
	#endif
				if ((evlist[j].events & EPOLLIN) && (evlist[j].data.fd == config->fd) ){
					cur_r = config->r_ring + (ringr_offset *  config->max_frame_size);
					header_r = (struct tpacket2_hdr *)cur_r;
	#ifdef DEBUG_ALL
						printf("ready: %d, j:%d, cur_r: %p, tp_status: %u, ringr_offset: %d\n", ready,j,cur_r, (uint8_t)header_r->tp_status,ringr_offset);
						printf("cur_r: %lu, tp_status: %u, header_len %u, tp_mac %u\n", cur_r, (uint8_t)header_r->tp_status,TPACKET2_HDRLEN, header_r->tp_mac);
						printf("\nPacket MMAP Header:\t%s,%s,%s,%s,%s,%s,%s\n",
							(header_r->tp_status & TP_STATUS_KERNEL) ? "STATUS KERNEL " : "",
							(header_r->tp_status & TP_STATUS_USER) ? "STATUS USER " : "",
							(header_r->tp_status & TP_STATUS_COPY) ? "STATUS COPY " : "",
							(header_r->tp_status & TP_STATUS_LOSING) ? "STATUS LOSING " : "",
							(header_r->tp_status & TP_STATUS_CSUMNOTREADY) ? "STATUS CSUMNOTREADY " : "",
							(header_r->tp_status & TP_STATUS_VLAN_VALID) ? "STATUS VLAN VALID " : "",
							(header_r->tp_status & TP_STATUS_BLK_TMO) ? "STATUS BLK_TMO " : ""
							);
	#endif
						len = header_r->tp_len;
	#ifdef DEBUG_ALL
						buf = cur_r + header_r->tp_mac;
						eth_proto = display_ethernet(buf);
						if (eth_proto == 0x0806) {
							printf("ARP Packet\n");
						} if (eth_proto == 0x0800) {
							ip_proto = display_ip(buf);
							if (ip_proto == 1){
								display_icmp(buf);
							}
						}
	#endif
		                /*
		                * buffer over MTU if buffer bigger than MTU
		                */
						sendlen = MIN(len, config->mtu_size);
						remlen = len;
						curpos = cur_r + header_r->tp_mac;
						//
						data_start = TPACKET2_HDRLEN - sizeof(struct sockaddr_ll);
						//data_len = config->max_frame_size -  data_start;
						while (remlen > 0){
							
							cur_w = config->w_ring + (ringw_offset * config->max_frame_size);
							header_w = (struct tpacket2_hdr *)cur_w;
							header_w->tp_mac = data_start;
							/*
							* Wait for buffer to be ready
							*/
							while(header_w->tp_status != TP_STATUS_AVAILABLE);
							header_w->tp_len = sendlen;
							//memset(cur_w + data_start, 0, data_len);
							memcpy(cur_w + data_start, curpos , header_w->tp_len);
							header_w->tp_status = TP_STATUS_SEND_REQUEST;
							/*
							* Poke kernel to send
							*/
							sent_len = sendto(config->fd, NULL, 0,0,NULL,0);
							if (sent_len == -1) {
								err_fatal(config->logfile,"Error writing to intf: cur_r: %p, tp_status: %u, ringw_offset: %u, remlen %zu, sendlen: %zu, curpos: %p", cur_r, header_w->tp_status, ringr_offset,remlen,sendlen,curpos);
							} else {
								curpos += sent_len;
								remlen -= sent_len;
								sendlen = MIN(remlen,config->mtu_size);
							}
							ringw_offset = (ringw_offset + 1) & ((config->max_ring_frames * config->max_ring_blocks)- 1);
						}

				} else {
					err_info(config->logfile,"Not for us packet - should never get here");
					if (evlist[j].events & (EPOLLHUP | EPOLLERR)) { 
					/* After the epoll_wait(), EPOLLIN and EPOLLHUP may both have been set. 
					 * But we'll only get here, and thus close the file descriptor, if EPOLLIN was not set. 
					 * This ensures that all outstanding input (possibly more than MAX_BUF bytes) is 
					 * consumed (by further loop iterations) before the file descriptor is closed. 
					 */ 
						err_fatal(config->logfile,"Closing fd %d", evlist[j].data.fd);
					}
				} /* if evlist EPOLLIN or EPOLLERR */
				header_r->tp_status = TP_STATUS_KERNEL;
				ringr_offset = (ringr_offset + 1) & ((config->max_ring_frames * config->max_ring_blocks)- 1);
				cur_r = config->r_ring + (ringr_offset *  config->max_frame_size);
			} /* for ready */
			// update consumer pointer
		} /* while true */
	}
	void read_write_two(intf_config_t *f_config, intf_config_t *s_config){

		int ready;
		int j;
		int ep_fd;
		struct epoll_event e_evf, e_evs;
		struct epoll_event evlist[2];
		unsigned int fringr_offset = 0;
		unsigned int sringr_offset = 0;
		unsigned int fringw_offset = 0;
		unsigned int sringw_offset = 0;
		unsigned int data_start = 0;
		//unsigned int data_len = 0;
		unsigned int packet_interval_ingress = 0;
		unsigned int packet_interval_egress = 0;
		struct tpacket2_hdr *header, *header_w;
		unsigned int len, sent_len;
		uint8_t *cur_sr, *cur_fr, *cur_sw, *cur_fw;
		size_t sendlen;
		size_t remlen; 
		uint8_t *curpos;
		uint8_t *buf;
		/*
		*
		*/
		memset(&ep_fd,0,sizeof(ep_fd));
		ep_fd = epoll_create(2);
		if ( ep_fd == -1){
			err_fatal(f_config->logfile,"Error: epoll create failed %d\n", errno);
		}

		memset(&e_evf,0,sizeof(e_evf));
		e_evf.data.fd = f_config->fd;
		e_evf.events = EPOLLIN;
		if (epoll_ctl(ep_fd,EPOLL_CTL_ADD,f_config->fd, &e_evf) == -1){
			err_fatal(f_config->logfile,"Error: epoll_ctl failed %d\n", errno);
		}
		memset(&e_evs,0,sizeof(e_evs));
		e_evs.data.fd = s_config->fd;
		e_evs.events = EPOLLIN;
		if (epoll_ctl(ep_fd,EPOLL_CTL_ADD,s_config->fd, &e_evs) == -1){
			err_fatal(f_config->logfile,"Error: epoll_ctl failed %d\n", errno);
		}

		while(true){

			ready = epoll_wait( ep_fd, evlist,2, -1); 
			if (ready == -1) {
				if (errno == EINTR) {
					err_fatal(f_config->logfile,"Interrupt");
					/* Restart if interrupted by signal */ 
				} else {
					err_fatal(f_config->logfile,"Error: epoll_wait failed %d\n", errno);
				} 
			}
			/* Deal with returned list of events */ 
			for (j = 0; j < ready; j++) {
#ifdef DEBUG_ALL
				printf("\n\nPoll event: fd = %d; events: %s %s %s %s\n", evlist[j].data.fd,
					(evlist[j].events & EPOLLIN)  ? "EPOLLIN "  : "", 
					(evlist[j].events & EPOLLOUT) ? "EPOLLOUT " : "", 
					(evlist[j].events & EPOLLHUP) ? "EPOLLHUP " : "", 
					(evlist[j].events & EPOLLERR) ? "EPOLLERR " : "");
#endif
				if (evlist[j].events & EPOLLIN) { 

					if (evlist[j].data.fd == s_config->fd){
						cur_sr = s_config->r_ring + (sringr_offset *  s_config->max_frame_size);
						header = (struct tpacket2_hdr *)cur_sr;
#ifdef DEBUG_ALL
						printf("\n\nFrom second interface: %s fd: %d\n",s_config->name, s_config->fd);
					    printf("ready: %d, j:%d, cur_sr: %p, tp_status: %u, sringr_offset: %d\n", ready,j,cur_sr, (uint8_t)header->tp_status,sringr_offset);
						printf("tp_status: %u, header_len %u, tp_mac %u\n", (uint8_t)header->tp_status,TPACKET2_HDRLEN, header->tp_mac);
						printf("Packet-mmap header:\n, %s,%s,%s,%s,%s,%s,%s\n",
							(header->tp_status & TP_STATUS_KERNEL) ? "STATUS KERNEL " : "",
							(header->tp_status & TP_STATUS_USER) ? "STATUS USER " : "",
							(header->tp_status & TP_STATUS_COPY) ? "STATUS COPY " : "",
							(header->tp_status & TP_STATUS_LOSING) ? "STATUS LOSING " : "",
							(header->tp_status & TP_STATUS_CSUMNOTREADY) ? "STATUS CSUMNOTREADY " : "",
							(header->tp_status & TP_STATUS_VLAN_VALID) ? "STATUS VLAN VALID " : "",
							(header->tp_status & TP_STATUS_BLK_TMO) ? "STATUS BLK_TMO " : ""
							);
						printf("header->tp_status & TP_STATUS: %u\n",(header->tp_status & TP_STATUS_USER));
#endif
						if (header->tp_status & TP_STATUS_USER){
							len = header->tp_len;							
							/*
							* buf is the packet depending on the configuration
							* either/and log packets and NAT packets
							*/
							if (f_config->packet_log > 0 ){
								packet_interval_ingress++;
								if (packet_interval_ingress ==  f_config->packet_log){
								    // Write to logfile for debugging
									buf = cur_sr + header->tp_mac;
									write_packet(f_config->logfile, s_config, buf,"Reading from second writing to first");
									packet_interval_ingress = 0;
									}
							}		
					        buf = cur_sr + header->tp_mac;
							 /*
					        * buffer over MTU if buffer bigger than MTU
					        */
							sendlen = MIN(len, f_config->mtu_size);
							remlen = len;
							curpos = cur_sr + header->tp_mac;
							data_start = TPACKET2_HDRLEN - sizeof(struct sockaddr_ll);
							while (remlen > 0){
								cur_fw = f_config->w_ring + (fringw_offset * f_config->max_frame_size);
								header_w = (struct tpacket2_hdr *)cur_fw;
								header_w->tp_mac = data_start;
								/*
								* Wait for buffer to be ready
								*/
								while((header_w->tp_status & TP_STATUS_AVAILABLE) != TP_STATUS_AVAILABLE){
#ifdef DEBUG_ALL
									printf("Packet-mmap header:\n, %s,%s,%s,%s,%s,%s,%s\n",
										(header->tp_status & TP_STATUS_KERNEL) ? "STATUS KERNEL " : "",
										(header->tp_status & TP_STATUS_USER) ? "STATUS USER " : "",
										(header->tp_status & TP_STATUS_COPY) ? "STATUS COPY " : "",
										(header->tp_status & TP_STATUS_LOSING) ? "STATUS LOSING " : "",
										(header->tp_status & TP_STATUS_CSUMNOTREADY) ? "STATUS CSUMNOTREADY " : "",
										(header->tp_status & TP_STATUS_VLAN_VALID) ? "STATUS VLAN VALID " : "",
										(header->tp_status & TP_STATUS_BLK_TMO) ? "STATUS BLK_TMO " : ""
										);
#endif
								} ;
								header_w->tp_len = sendlen;
								//memset(cur_sw + data_start, 0, data_len);
								memcpy(cur_fw + data_start, curpos , header_w->tp_len);
								header_w->tp_status = TP_STATUS_SEND_REQUEST;
								/*
								* Poke kernel to send
								*/
								sent_len = sendto(f_config->fd, NULL, 0,0,NULL,0);
#ifdef DEBUG_ALL
								printf("\n\nWriting to first interface: %s\n",f_config->name);
								printf("tp_mac: %d, tp_len: %u, curpos: %p, remlen: %zu, sendlen: %zu, sent_len: %u",
									header_w->tp_mac,header_w->tp_len,curpos,remlen,sendlen,sent_len);
#endif
								if (sent_len == -1) {
									err_fatal(f_config->logfile,"Error writing to read_intf: %s\n",f_config->name);
								} else {
									curpos += sent_len;
									remlen -= sent_len;
									sendlen = MIN(remlen,f_config->mtu_size);
								}
								fringw_offset = (fringw_offset + 1) & ((MAX_RING_FRAMES * MAX_RING_BLOCKS)- 1);
							}
				 			// update consumer pointer
							header->tp_status = TP_STATUS_KERNEL;
							sringr_offset = (sringr_offset + 1) & ((MAX_RING_FRAMES * MAX_RING_BLOCKS)- 1);
						} else {
							err_fatal(f_config->logfile,"Header status not user: %u\n", header->tp_status);
						}
					} else if ( evlist[j].data.fd == f_config->fd ) {
						cur_fr = f_config->r_ring + (fringr_offset * f_config->max_frame_size);
						header = (struct tpacket2_hdr *)cur_fr;
#ifdef DEBUG_ALL
						printf("\n\nFrom first interface: %s fd: %d\n",f_config->name, f_config->fd);
					    printf("ready: %d, j:%d, cur_fr: %p, tp_status: %u, fringr_offset: %d\n", ready,j,cur_fr, (uint8_t)header->tp_status,fringr_offset);
						printf("tp_status: %u, header_len %u, tp_mac %u\n", (uint8_t)header->tp_status,TPACKET2_HDRLEN, header->tp_mac);
						printf("Packet-mmap header:\n, %s,%s,%s,%s,%s,%s,%s\n",
							(header->tp_status & TP_STATUS_KERNEL) ? "STATUS KERNEL " : "",
							(header->tp_status & TP_STATUS_USER) ? "STATUS USER " : "",
							(header->tp_status & TP_STATUS_COPY) ? "STATUS COPY " : "",
							(header->tp_status & TP_STATUS_LOSING) ? "STATUS LOSING " : "",
							(header->tp_status & TP_STATUS_CSUMNOTREADY) ? "STATUS CSUMNOTREADY " : "",
							(header->tp_status & TP_STATUS_VLAN_VALID) ? "STATUS VLAN VALID " : "",
							(header->tp_status & TP_STATUS_BLK_TMO) ? "STATUS BLK_TMO " : ""
							);
						printf("header->tp_status & TP_STATUS: %u\n",(header->tp_status & TP_STATUS_USER));
#endif
						if (header->tp_status & TP_STATUS_USER){
							len = header->tp_len;
							/*
							* buf is the packet depending on the configuration
							* either/and log packets and NAT packets
							*/
							if (f_config->packet_log > 0 ){
								packet_interval_egress++;
								if (packet_interval_egress ==  f_config->packet_log){
									//printf("Logging packets\n");
								    // Write to logfile for debugging
									buf = cur_fr + header->tp_mac;
									write_packet(f_config->logfile, f_config, buf,"Reading from first writing to second");
									packet_interval_egress = 0;
									}
							}	
							buf = cur_fr + header->tp_mac;
  					        sendlen = MIN(len, s_config->mtu_size);
							remlen = len;
							curpos = cur_fr + header->tp_mac;
							data_start = TPACKET2_HDRLEN - sizeof(struct sockaddr_ll);
							while (remlen > 0){
								cur_sw = s_config->w_ring + (sringw_offset * s_config->max_frame_size);
								header_w = (struct tpacket2_hdr *)cur_sw;
								header_w->tp_mac = data_start;
								while((header_w->tp_status & TP_STATUS_AVAILABLE) != TP_STATUS_AVAILABLE){
#ifdef DEBUG_ALL
									printf("Packet-mmap header:\n, %s,%s,%s,%s,%s,%s,%s\n",
										(header->tp_status & TP_STATUS_KERNEL) ? "STATUS KERNEL " : "",
										(header->tp_status & TP_STATUS_USER) ? "STATUS USER " : "",
										(header->tp_status & TP_STATUS_COPY) ? "STATUS COPY " : "",
										(header->tp_status & TP_STATUS_LOSING) ? "STATUS LOSING " : "",
										(header->tp_status & TP_STATUS_CSUMNOTREADY) ? "STATUS CSUMNOTREADY " : "",
										(header->tp_status & TP_STATUS_VLAN_VALID) ? "STATUS VLAN VALID " : "",
										(header->tp_status & TP_STATUS_BLK_TMO) ? "STATUS BLK_TMO " : ""
										);
#endif
								} ;
								header_w->tp_len = sendlen;
								memcpy(cur_sw + data_start, curpos, header_w->tp_len);
								header_w->tp_status = TP_STATUS_SEND_REQUEST;

								sent_len = sendto(s_config->fd,NULL, 0,0,NULL,0);
#ifdef DEBUG_ALL
								printf("\n\nWriting to second interface: %s\n",s_config->name);
								printf("tp_mac: %d, tp_len: %u, curpos: %p, remlen: %zu, sendlen: %zu, sent_len: %u",
									header_w->tp_mac,header_w->tp_len,curpos,remlen, sendlen,sent_len);
#endif
								if (sent_len == -1) {
									err_fatal(f_config->logfile,"Error writing to read fd: %s\n",s_config->name);
								} else {
									curpos += sent_len;
									remlen -= sent_len;
									sendlen = MIN(remlen,s_config->mtu_size);
								}
								sringw_offset = (sringw_offset + 1) & ((MAX_RING_FRAMES * MAX_RING_BLOCKS)- 1);	       
							}     		
							header->tp_status = TP_STATUS_KERNEL;
			 				// update consumer pointer
							fringr_offset = (fringr_offset + 1) & ((MAX_RING_FRAMES * MAX_RING_BLOCKS) - 1);
						} else {
							err_fatal(f_config->logfile,"Header status is not user: %u\n", header->tp_status);
						}					
					} else {
						err_fatal(f_config->logfile,"Not for us packet\n");
	} /* if fd */
					} else {
						if (evlist[j].events & (EPOLLHUP | EPOLLERR)) { 
							/* After the epoll_wait(), EPOLLIN and EPOLLHUP may both have been set. 
							 * But we'll only get here, and thus close the file descriptor, if EPOLLIN was not set. 
							 * This ensures that all outstanding input (possibly more than MAX_BUF bytes) is 
							 * consumed (by further loop iterations) before the file descriptor is closed. 
							 */ 
							err_fatal(f_config->logfile," closing fd %d\n", evlist[j].data.fd);
						}
	} /* if evlist EPOLLIN or EPOLLERR */
	} /* for ready */
	} /* while */
	}
