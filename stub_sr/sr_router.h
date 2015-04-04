/*-----------------------------------------------------------------------------
 * File: sr_router.h
 * Date: ?
 * Authors: Guido Apenzeller, Martin Casado, Virkam V.
 * Contact: casado@stanford.edu
 * 90904102
 *---------------------------------------------------------------------------*/

#ifndef SR_ROUTER_H
#define SR_ROUTER_H

#include <netinet/in.h>
#include <sys/time.h>
#include <stdio.h>

#include "sr_protocol.h"
#ifdef VNL
#include "vnlconn.h"
#endif

/* we dont like this debug , but what to do for varargs ? */
#ifdef _DEBUG_
#define Debug(x, args...) printf(x, ## args)
#define DebugMAC(x) \
  do { int ivyl; for(ivyl=0; ivyl<5; ivyl++) printf("%02x:", \
  (unsigned char)(x[ivyl])); printf("%02x",(unsigned char)(x[5])); } while (0)
#else
#define Debug(x, args...) do{}while(0)
#define DebugMAC(x) do{}while(0)
#endif
#define INIT_TTL 255
#define PACKET_DUMP_SIZE 1024
#define ICMP_TYPE_ECHO_REPLY 0
#define ICMP_TYPE_DEST_UNREACHABLE 3
#define ICMP_TYPE_ECHO_REQUEST 8
#define ICMP_TYPE_TIME_EXCEEDED 11
#define ICMP_TYPE_TRACE_ROUTE 30
#define ICMP_CODE_ECHO_REPLY 0
#define ICMP_CODE_DEST_HOST_UNREACHABLE 1
#define ICMP_CODE_DEST_PORT_UNREACHABLE 3
#define ICMP_CODE_DEST_PROTOCOL_UNREACHABLE 2
#define ICMP_CODE_DEST_HOST_UNKNOWN 7
#define ICMP_CODE_TRACE_CODE 0
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define ICMP_CODE_ECHO_REQUEST 0
#define ICMP_CODE_TTL_EXPIRED

/* forward declare */
struct sr_if;
struct sr_rt;

/* ----------------------------------------------------------------------------
 * struct sr_instance
 *
 * Encapsulation of the state for a single virtual router.
 *
 * -------------------------------------------------------------------------- */

struct sr_instance
{
	int  sockfd;   /* socket to server */
#ifdef VNL
	struct VnlConn* vc;
#endif
	char user[32]; /* user name */
	char host[32]; /* host name */
	char template[30]; /* template name if any */
	char auth_key_fn[64]; /* auth key filename */
	unsigned short topo_id;
	struct sockaddr_in sr_addr; /* address to server */
	struct sr_if* if_list; /* list of interfaces */
	struct sr_rt* routing_table; /* routing table */
	FILE* logfile;
};


/* Structure for the ICMP
 *
 */
struct sr_icmphdr
{
	uint8_t icmp_type;
	uint8_t icmp_code;
    uint16_t icmp_sum;

} sr_icmphdr;
/* Structure for the cache
 *
 */
struct ip_mac_cache
{
	unsigned char mac_address [ETHER_ADDR_LEN];
	uint32_t ip_address;
	int valid;
	double entryTime;

} ip_mac_cache;
/* Structure for the ip-count
 *
 */
struct ip_packet_count
{
	uint32_t ip_address;
	int count;
	volatile int arpReplyReceived;
	struct ip_packet_count *next;

}ip_packet_count;
/* Structure for the queue
 *
 */
struct packet_cache
{
	uint32_t ip_address;
	uint8_t * packet;
	int length;
	struct sr_instance* sr;
	char *interface;
	struct packet_cache *next;
	int is_packet_for_gateway;

}packet_cache;

/* -- sr_main.c -- */
int sr_verify_routing_table(struct sr_instance* sr);

/* -- sr_vns_comm.c -- */
int sr_send_packet(struct sr_instance* , uint8_t* , unsigned int , const char*);
int sr_connect_to_server(struct sr_instance* ,unsigned short , char* );
int sr_read_from_server(struct sr_instance* );

/* -- sr_router.c -- */
void sr_init(struct sr_instance* );
void sr_handlepacket(struct sr_instance* , uint8_t * , unsigned int , char* );

/* -- sr_if.c -- */
void sr_add_interface(struct sr_instance* , const char* );
void sr_set_ether_ip(struct sr_instance* , uint32_t );
void sr_set_ether_addr(struct sr_instance* , const unsigned char* );
void sr_print_if_list(struct sr_instance* );

unsigned char* does_mac_address_exist(uint32_t ip_addr,int isFromCacheMac);

void setIPchecksum(struct ip* ip_hdr);
void print_mac_address(unsigned char* CachedValue);
void cache_mac_address(struct sr_arphdr* arp_Header);
struct sr_rt* getInterFaceForIp(struct sr_instance* sr,uint32_t ipAddToMatch);
struct sr_rt* getInterFaceForGateWay(struct sr_instance* sr,uint32_t ipAddToMatch);
struct ip *get_ip_hdr(uint8_t *packet);
struct sr_icmphdr *get_icmp_hdr(uint8_t *packet, struct ip* ip_hdr);
struct sr_ethernet_hdr *get_ethernet_hdr(uint8_t *packet);
void sr_route_packet(struct sr_instance * sr, uint8_t * packet, int len, char* interface,struct sr_rt* fetchedInterface);
void sr_handle_arp_packet(struct sr_instance* sr, unsigned int len, char* interface, uint8_t* packet);
void send_arp_request(struct sr_instance * sr, uint32_t dst_ip, char* interface,struct sr_rt* fetchedInterface);
void add_packet_to_cache(uint8_t *packet, struct sr_ethernet_hdr * eth_hdr, int len,struct sr_instance* sr,char *interface,int is_packet_for_gateway);
void send_cached_packet(uint32_t ip_addr);
void process_cached_packet(uint32_t ip_addr,struct sr_instance* sr, char *interface );
void sr_handle_icmp_packet(struct sr_instance* sr, unsigned int len,uint8_t* packet,char *interface,int isPacketForInterface);
int  is_ip_addr_for_gateway(uint32_t ipAddToMatch, struct sr_instance* sr);
struct ip* createIpHdr(struct ip* old_ip,struct ip* new_ip,struct sr_instance* sr,char* interface);
struct sr_icmphdr* createICMPHdr(uint8_t icmp_type,uint8_t icmp_code,uint8_t *packet,struct sr_icmphdr* icmp_hdr_new);
void increment_ip_packet_count(uint32_t ip_addr_to_cache);
#endif /* SR_ROUTER_H */
