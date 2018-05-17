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
/*
* Utility functions for NFV Application
*/
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <syslog.h>
#include <signal.h>

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
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/ip_icmp.h>

#include "vnfapp.h"
#include "vnferror.h"


#define MAX_BUF 65536
#define MAX_EVENTS 5

uint16_t display_ethernet(uint8_t *buf);
uint16_t display_ip(uint8_t *buf);
void display_icmp(uint8_t *buf);
uint16_t ip_checksum(const void *buf, size_t hdr_len);

struct tcp_udp_pseudo
{
    uint32_t src_addr;
    uint32_t dst_addr;
    uint8_t  zero;
    uint8_t  proto;
    uint16_t length;
} pseudo_header;

/* Useful union for checksum function */
union tcp_udp_u {
    struct tcphdr tcp;
    struct udphdr udp;
};

int set_socket_non_blocking (int sfd) {
  int flags, s;

  flags = fcntl (sfd, F_GETFL, 0);
  if (flags == -1)
    {
      return -1;
    }

  flags |= O_NONBLOCK;
  s = fcntl (sfd, F_SETFL, flags);
  if (s == -1)
    {
      return -1;
    }

  return 0;
}

bool set_promiscous_mode(FILE *fp, int fd, char *intf_name){

	bool status = true;
	int rtn;
	struct ifreq ifr;

	
	memset(&ifr, 0, sizeof(ifr));
	/* Set interface to promiscuous mode - do we need to do this every time? */
	strncpy(ifr.ifr_name, intf_name, IFNAMSIZ-1);
	rtn = ioctl(fd, SIOCGIFFLAGS, &ifr);
	if (rtn == -1){
		err_fatal(fp,"SIOCGIFFLAGS");
	}
	ifr.ifr_flags |= IFF_PROMISC;
	rtn = ioctl(fd, SIOCSIFFLAGS, &ifr);
	if (rtn == -1){
	   err_fatal(fp,"SIOCSIFFLAGS");
	}


	return status;
}
/*
* Get status of an interface
*/
bool get_interface_status(FILE *fp,int fd, char * intf_name){
	bool status = true;
  struct ifreq ifr;
 	int rtn;
  memset(&ifr, 0, sizeof(ifr));

    /* set the name of the interface we wish to check */
    strncpy(ifr.ifr_name, intf_name, IFNAMSIZ-1);
    /* grab flags associated with this interface */
    rtn = ioctl(fd, SIOCGIFFLAGS, &ifr);
    if (rtn == -1){
		    err_fatal(fp,"SIOCSIFFLAGS");
	}
    if (ifr.ifr_flags & IFF_PROMISC) {
       status = true;
    } else {
        err_info(fp,"%s is NOT in promiscuous mode",
               ifr.ifr_name);
        status = false;
    }

	return status;
}
/*
* Configure ring buffer for socket
*/
int set_pmap(intf_config_t *vnf_config, uint8_t **read_ring, uint8_t **write_ring){
 	struct tpacket_req treq_rx, treq_tx;
 	int status = 0;
 	unsigned long memlen;
 	int v = TPACKET_V2;

 	memset(&treq_rx, 0, sizeof(treq_rx));
 	treq_rx.tp_block_size =  vnf_config->max_ring_frames * vnf_config->max_frame_size;
	treq_rx.tp_block_nr   =  vnf_config->max_ring_blocks;
	treq_rx.tp_frame_size =  vnf_config->max_frame_size;
	treq_rx.tp_frame_nr   =  vnf_config->max_ring_frames * vnf_config->max_ring_blocks;

	memset(&treq_tx, 0, sizeof(treq_tx));
	treq_tx.tp_block_size = vnf_config->max_ring_frames * vnf_config->max_frame_size;
	treq_tx.tp_block_nr   = vnf_config->max_ring_blocks;
	treq_tx.tp_frame_size = vnf_config->max_frame_size;
	treq_tx.tp_frame_nr   = vnf_config->max_ring_frames * vnf_config->max_ring_blocks;

	if (setsockopt(vnf_config->fd , SOL_PACKET , PACKET_VERSION , &v , sizeof(v)) == -1){  
		close(vnf_config->fd);
    err_fatal(vnf_config->logfile,"PACKET_VERSION");
	}
	if (setsockopt(vnf_config->fd , SOL_PACKET , PACKET_RX_RING , (void*)&treq_rx , sizeof(treq_rx)) == -1){
		err_fatal(vnf_config->logfile,"PACKET_RX_RING");
	}
   if (setsockopt(vnf_config->fd, SOL_PACKET , PACKET_TX_RING , (void*)&treq_tx , sizeof(treq_tx)) == -1){
		err_fatal(vnf_config->logfile,"PACKET_TX_RING");
	}
	memlen = treq_rx.tp_block_size * treq_rx.tp_block_nr;

  	*read_ring = mmap(NULL, 2 * memlen, PROT_READ | PROT_WRITE, MAP_SHARED, vnf_config->fd, 0);
  	if (*read_ring == MAP_FAILED) {
   		err_fatal(vnf_config->logfile,"mmap failed: %d", errno);
   	}
   	*write_ring = *read_ring+memlen;

	return status;
}
int get_mtu_size(int fd, char *name){

	struct ifreq ifr;
	strcpy(ifr.ifr_name, name);
	if (!ioctl(fd, SIOCGIFMTU, &ifr)) {
   		return ifr.ifr_mtu;// Contains current mtu value
	} 

	return -1;
}
bool is_power_two(int n)
{
  /*
  * Zero is not a power of 2
  */
  if (n == 0)
    return false;
  while (n != 1)
  {
  	/*
  	* If remainder not power of 2
  	*/
    if (n%2 != 0)
      return false;
    n = n/2;
  }
  return true;
}
void vnfShutdown(){
  syslog(LOG_INFO, "Shutting down VNF");
}
/*
*  Signal Handler
*/
void signal_handler(int signo)
{
   switch(signo)
        {
            case SIGHUP:
                syslog(LOG_WARNING, "Received SIGHUP signal.");
                break;
            case SIGINT:
            case SIGKILL:
            case SIGTERM:
            case SIGSEGV:
                syslog(LOG_INFO, "Daemon exiting");
                vnfShutdown();
                exit(EXIT_SUCCESS);
                break;
            default:
                syslog(LOG_WARNING, "Unhandled signal %s", strsignal(signo));
                break;
        }
}
/*
* Utilities to print out network headers
*/
uint16_t display_ethernet(uint8_t *buffer){

	struct ethhdr *eth = (struct ethhdr *)(buffer);
	printf("\nEthernet Header\n");
	printf("\t|-Source Address : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X\n",eth->h_source[0],eth->h_source[1],eth->h_source[2],eth->h_source[3],eth->h_source[4],eth->h_source[5]);
	printf("\t|-Destination Address : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X\n",eth->h_dest[0],eth->h_dest[1],eth->h_dest[2],eth->h_dest[3],eth->h_dest[4],eth->h_dest[5]);
	printf("\t|-Protocol : 0x%04x\n",ntohs(eth->h_proto));
  fflush(stdout);
	return ntohs(eth->h_proto);
}

uint16_t write_ethernet(FILE *fp,uint8_t *buffer){

  struct ethhdr *eth = (struct ethhdr *)(buffer);
  fprintf(fp,"\nEthernet Header\n");
  fprintf(fp,"\t|-Source Address : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X\n",eth->h_source[0],eth->h_source[1],eth->h_source[2],eth->h_source[3],eth->h_source[4],eth->h_source[5]);
  fprintf(fp,"\t|-Destination Address : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X\n",eth->h_dest[0],eth->h_dest[1],eth->h_dest[2],eth->h_dest[3],eth->h_dest[4],eth->h_dest[5]);
  fprintf(fp,"\t|-Protocol : 0x%04x\n",ntohs(eth->h_proto));
  fflush(fp);
  return ntohs(eth->h_proto);
}


bool compare_mac(struct ifreq *ingress, struct ethhdr *packet){

  if ( packet->h_source[0] == ingress->ifr_addr.sa_data[0] &&
        packet->h_source[1] == ingress->ifr_addr.sa_data[1] &&
        packet->h_source[2] == ingress->ifr_addr.sa_data[2] &&
        packet->h_source[3] == ingress->ifr_addr.sa_data[3] &&
        packet->h_source[4] == ingress->ifr_addr.sa_data[4] &&
        packet->h_source[5] == ingress->ifr_addr.sa_data[5]) { 
    return true;
  }else {
    return false;
  }
}
bool checkipv4(uint8_t *buffer){
  
  struct ethhdr *eth = (struct ethhdr *)(buffer);
  if (ETHERTYPE_IP == ntohs(eth->h_proto)){
    return true;
  }
  return false;
}
uint16_t display_ip(uint8_t *buffer){

	struct sockaddr_storage source,dest; 
	struct iphdr *ip = (struct iphdr*)(buffer + sizeof(struct ethhdr));
	memset(&source, 0, sizeof(source));
	((struct sockaddr_in *)&source)->sin_addr.s_addr = ip->saddr;
	memset(&dest, 0, sizeof(dest));
	((struct sockaddr_in *)&dest)->sin_addr.s_addr = ip->daddr;

  if ((unsigned int)ip->version == 4){
  	printf("\nIPV4 Header\n");
  	printf("\t|-Version : %u\n",(unsigned int)ip->version);
  	printf("\t|-Internet Header Length : %u DWORDS or %u Bytes\n",(unsigned int)ip->ihl,((unsigned int)(ip->ihl))*4); 
  	printf("\t|-Type Of Service : %d\n",(unsigned int)ip->tos); 
  	printf("\t|-Total Length : %d Bytes\n",ntohs(ip->tot_len)); 
  	printf("\t|-Identification : %d\n",ntohs(ip->id)); 
  	printf("\t|-Time To Live : %d\n",(unsigned int)ip->ttl); 
  	printf("\t|-Protocol : %d\n",(unsigned int)ip->protocol);
  	printf("\t|-Header Checksum : %d\n",ntohs(ip->check)); 
  	printf("\t|-Source IP : %s\n", inet_ntoa(((struct sockaddr_in *)&source)->sin_addr) ) ;
  	printf("\t|-Destination IP : %s\n",inet_ntoa(((struct sockaddr_in *)&dest)->sin_addr) );
  } else if ((unsigned int)ip->version == 6){
    printf("\n\nIPV6 Header\n");
  } else {
    printf("\n\n Unrecognized IP Header: %u\n",(unsigned int)ip->version);
  }

	return ip->protocol;
}

uint16_t write_packet(FILE *fp, intf_config_t *ingress,uint8_t *buffer, char * comment){
  uint16_t ethtype;
  struct sockaddr_storage source,dest; 
  struct iphdr *ip = (struct iphdr*)(buffer + sizeof(struct ethhdr));
  memset(&source, 0, sizeof(source));
  ((struct sockaddr_in *)&source)->sin_addr.s_addr = ip->saddr;
  memset(&dest, 0, sizeof(dest));
  ((struct sockaddr_in *)&dest)->sin_addr.s_addr = ip->daddr;
  fprintf(fp,"\n\n-------------------------------------\n");
  fprintf(fp,"\n %s\n", comment);
  fprintf(fp,"Packet from interface: %s\n", ingress->name);
  ethtype = write_ethernet(fp,buffer);
  if (ethtype == ETHERTYPE_IP){
    fprintf(fp,"\n\nIPV4 Header\n");
    fprintf(fp,"\t|-Interface: %s\n",ingress->name);
    fprintf(fp,"\t|-MTU Size: %u\n",ingress->mtu_size);
    fprintf(fp,"\t|-Version : %u\n",(unsigned int)ip->version);
    fprintf(fp,"\t|-Internet Header Length : %u DWORDS or %u Bytes\n",(unsigned int)ip->ihl,((unsigned int)(ip->ihl))*4); 
    fprintf(fp,"\t|-Type Of Service : %d\n",(unsigned int)ip->tos); 
    fprintf(fp,"\t|-Total Length : %d Bytes\n",ntohs(ip->tot_len)); 
    fprintf(fp,"\t|-Identification : %d\n",ntohs(ip->id)); 
    fprintf(fp,"\t|-Time To Live : %d\n",(unsigned int)ip->ttl); 
    fprintf(fp,"\t|-Protocol : %d\n",(unsigned int)ip->protocol);
    fprintf(fp,"\t|-Header Checksum : %d\n",ntohs(ip->check)); 
    fprintf(fp,"\t|-Source IP : %s\n", inet_ntoa(((struct sockaddr_in *)&source)->sin_addr) ) ;
    fprintf(fp,"\t|-Destination IP : %s\n",inet_ntoa(((struct sockaddr_in *)&dest)->sin_addr) );
    fprintf(fp,"\n-------------------------------------\n");
  }
  fflush(fp);
  return ethtype;
}

bool ipv4_compare(unsigned long first, unsigned long second){
  bool result = false;
  printf("first IP: %lu, second IP: %lu\n",first,second);
  if (first == second){
    printf("First IP matches second IP\n");
    return true;
  }

  return result;
}

/**
 * Compute IP/ICMP checksum
 * Note: Before starting the calculation, the checksum field must be zero
 *
 * @param addr   The pointer on the begginning of the packet
 * @param length Length which be computed
 * @return The 16 bits unsigned integer checksum
 */
u_int16_t ip_icmp_calc_checksum(const u_int16_t *addr, int length)
{
    int left = length;
    const u_int16_t *w = addr;
    u_int16_t answer;
    register int sum = 0;

    while (left > 1)  {
        sum += *w++;
        left -= 2;
    }

    /* mop up an odd byte, if necessary */
    if (left == 1)
        sum += htons(*(u_char *)w << 8);

    /* add back carry outs from top 16 bits to low 16 bits */
    sum = (sum >> 16) + (sum & 0xffff);    /* add hi 16 to low 16 */
    sum += (sum >> 16);    /* add carry */
    answer = ~sum; /* truncate to 16 bits */
    return answer;
}

/**
 * Compute TCP/UDP checksum
 * Note: Before starting the calculation, the checksum field must be zero
 *
 * @param ip       The pointer on the IP packet
 * @param tcp_udp  The pointer on the TCP/UDP packet
 * @param protocol The protocol (IPPROTO_TCP or IPPROTO_UDP)
 * @return The 16 bits unsigned integer checksum
 */
u_int16_t tcp_udp_calc_checksum(const struct iphdr *ip,
        union tcp_udp_u *tcp_udp, int protocol)
{
    uint16_t ip_tlen = ntohs(ip->tot_len);
    uint16_t tcp_udp_tlen = ip_tlen - ip->ihl * 4;
    uint16_t tcp_udp_hlen = tcp_udp_tlen - sizeof(tcp_udp);
    uint16_t tcp_udp_dlen = tcp_udp_tlen - tcp_udp_hlen;
    uint16_t new_tcp_udp_len = sizeof(pseudo_header) + sizeof(tcp_udp)
        + tcp_udp_dlen;
    unsigned short *new_tcp_udp;

    switch(protocol) {
        case IPPROTO_TCP:
            tcp_udp->tcp.check = 0;
            break;
        case IPPROTO_UDP:
            tcp_udp->udp.check = 0;
            break;
        default:
            return 0;
    }

    pseudo_header.src_addr = ip->saddr;
    pseudo_header.dst_addr = ip->daddr;
    pseudo_header.zero = 0;
    pseudo_header.proto = protocol;
    pseudo_header.length = htons(tcp_udp_tlen);

    if ((new_tcp_udp = (unsigned short *)malloc(
                    new_tcp_udp_len * sizeof(unsigned short))) == NULL) {
        perror("Unable to allocate the pseudo TCP/UDP header");
        exit(1);
    }

    memcpy((u_short *)new_tcp_udp, &pseudo_header, sizeof(pseudo_header));
    memcpy((u_short *)new_tcp_udp + sizeof(pseudo_header),
            (u_char *)tcp_udp, sizeof(tcp_udp));
    memcpy((u_short *)new_tcp_udp + sizeof(pseudo_header) + sizeof(tcp_udp),
            (u_char *)tcp_udp + sizeof(tcp_udp), tcp_udp_dlen);

    return ip_icmp_calc_checksum((const u_short *)new_tcp_udp,
            new_tcp_udp_len);
}

uint16_t nat_ipv4(uint8_t *buffer,struct sockaddr_in *ingress, struct sockaddr_in *egress, bool ingress_dir){

  int status = 1;
  int natted = 0;
  struct tcphdr *tcp;
  struct udphdr *udp;
  struct icmphdr *icmp;
  //struct sockaddr_storage source,dest; 
  struct iphdr *ip = (struct iphdr*)(buffer + sizeof(struct ethhdr));
  //memset(&source, 0, sizeof(source));
  //((struct sockaddr_in *)&source)->sin_addr.s_addr = ip->saddr;
  //memset(&dest, 0, sizeof(dest));
//struct sockaddr_in *)&dest)->sin_addr.s_addr = ip->daddr;

  if ((unsigned int)ip->version == 6){
    printf("IP V6 packet not natted\n");
    return 0;
    } 
  /* set ports pointers */
    switch (ip->protocol) {
        case IPPROTO_TCP:
            tcp = (struct tcphdr *)(buffer + sizeof(struct ethhdr) +
                    sizeof(struct iphdr));
            //src_port = &(tcp->source);
            //dst_port = &(tcp->dest);
            break;

        case IPPROTO_UDP:
            udp = (struct udphdr *)(buffer + sizeof(struct ethhdr) +
                    sizeof(struct iphdr));
            //src_port = &(udp->source);
            //dst_port = &(udp->dest);
            break;

        case IPPROTO_ICMP:
            icmp = (struct icmphdr *)(buffer + sizeof(struct ethhdr));
            /* hack a bit: we use ICMP "identifier" field instead of port ;) */
            //src_port = dst_port = &(icmp->un.echo.id);
            break;

        default:
            fprintf(stderr, "Unknown IP protocol:%u , do not NAT the packet\n",ip->protocol);
            return 0;
    }
  /* 
  * If src address is from the DNAT'ed address reset to the original address
  */
  if ((unsigned int)ip->version == 4){ 
    if (ingress_dir == true){
      printf("Ingress true natting to %s\n",inet_ntoa(egress->sin_addr));
      if (ingress->sin_addr.s_addr != ip->saddr){
        ip->daddr = egress->sin_addr.s_addr;
      }
    } else {
      //ip->saddr = ingress->sin_addr.s_addr;
      printf("Ingress false natting to %s\n",inet_ntoa(ingress->sin_addr));
    }
    natted = 1;
  }

   if (natted) {
        /* compute TCP/UDP checksum */
        switch (ip->protocol) {
            case IPPROTO_TCP:
                tcp->check = tcp_udp_calc_checksum(ip, (union tcp_udp_u *)tcp,
                        IPPROTO_TCP);
                break;

            case IPPROTO_UDP:
                udp->check = tcp_udp_calc_checksum(ip, (union tcp_udp_u *)udp,
                        IPPROTO_UDP);
                break;

            case IPPROTO_ICMP:
                icmp->checksum = 0;
                icmp->checksum = ip_icmp_calc_checksum((u_short *)icmp,
                        ip->tot_len - ip->ihl * 4);
                break;
        }

        /* compute IP checksum */
        /* checksum field must be null before computing new checksum */
        ip->check = 0;
        ip->check = ip_icmp_calc_checksum((u_short *)ip, ip->ihl * 4);

  }
  return status;
}


void display_icmp(uint8_t *buffer)
{
    unsigned short iphdrlen;
     
    struct iphdr *iph = (struct iphdr*)(buffer + sizeof(struct ethhdr));
    iphdrlen = iph->ihl*4;
    struct icmphdr *icmph = (struct icmphdr *)(buffer + iphdrlen + sizeof(struct ethhdr));
         
         
    printf("\nICMP Header\n");
    printf("\t|Type : %u\t",(uint8_t)(icmph->type));
    printf("\t\t|Type Name:");
    if (icmph->type == ICMP_ECHOREPLY) {
    		printf("\t\tICMP ECHOREPLY\n");
    		printf("\t\tID: %" PRIu16 "\n",(uint16_t)ntohs(icmph->un.echo.id));
    		printf("\t\tSequence number: %" PRIu16 "\n",(uint16_t)ntohs(icmph->un.echo.sequence));
    	};
    if (icmph->type == ICMP_DEST_UNREACH) printf("\t\tICMP DEST_UNREACH\n");
    if (icmph->type == ICMP_SOURCE_QUENCH) printf("\t\tICMP SOURCE_QUENCH\n");
    if (icmph->type == ICMP_ECHO) {
		printf("\t\tICMP ECHO\n");
		printf("\t\tID: %" PRIu16 "\n",(uint16_t)ntohs(icmph->un.echo.id));
    	printf("\t\tSequence number: %" PRIu16 "\n",(uint16_t)ntohs(icmph->un.echo.sequence));
    }
    if (icmph->type == ICMP_TIME_EXCEEDED) printf("\t\tICMP TIME_EXCEEDED\n");
    if (icmph->type == ICMP_PARAMETERPROB) printf("\t\tICMP PARAMETERPROB\n");
    if (icmph->type == ICMP_TIMESTAMP) printf("\t\tICMP TIMESTAMP\n");
    if (icmph->type == ICMP_TIMESTAMPREPLY) printf("\t\tICMP TIMESTAMPREPLY\n");
    if (icmph->type == ICMP_INFO_REPLY) printf("\t\tICMP INFO_REPLY\n");
    if (icmph->type == ICMP_ADDRESS) printf("\t\tICMP ADDRESS\n");
    if (icmph->type == ICMP_ADDRESSREPLY) printf("\t\tICMP ADDRESSREPLY\n");
    if (icmph->type ==  NR_ICMP_TYPES) printf("\t\tNR_ICMP_TYPES\n");

    printf("\n\t|-Code : %d\n",(uint8_t)(icmph->code));
    printf("\t|-Checksum : %d\n",ntohs(icmph->checksum));
    //printf("\t|-ID       : %d\n",ntohs(icmph->id));
    //printf("\t|-Sequence : %d\n",ntohs(icmph->sequence));
    //fprintf(logfile,"Data Payload\n");  
    //PrintData(Buffer + iphdrlen + sizeof icmph , (Size - sizeof icmph - iph->ihl * 4));
}