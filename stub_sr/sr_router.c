/**********************************************************************
 * file:  sr_router.c
 * date:  Mon Feb 18 12:50:42 PST 2002
 * Contact: casado@stanford.edu
 *
 * Description:
 *
 * This file contains all the functions that interact directly
 * with the routing table, as well as the main entry method
 * for routing. 11
 * 90904102
 **********************************************************************/

#include <stdio.h>
#include <assert.h>
#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"


struct ip_mac_cache cached_mac_addresses[100] ; // Cached MAC ADDRESS
//struct packet_cache  cached_packets[100]; // CACHED MAC ADDRESS
static int free_pos_for_caching = 0;
//static int free_pos_for_caching_packet = 0;
uint32_t mask_field_check_sum = 0xFFFF;
double ARP_CACHE_TIME_LIMIT = 15;

struct packet_cache* head_packet_cache = NULL;
struct ip_packet_count* head_ip_packet_count = NULL;

/* TRYING TO IMPLEMENT PACKET CACHING STRUCTURE
 *
 */




/*---------------------------------------------------------------------
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Initialize the routing subsystem
 *
 *---------------------------------------------------------------------*/
void sr_init(struct sr_instance* sr)
{
	/* REQUIRES */
	assert(sr);

	/* Add initialization code here! */

} /* -- sr_init -- */

/*
 * Fetch the Ip_HDR from the Packet
 */
struct ip *get_ip_hdr(uint8_t *packet)
{
	return (struct ip *) (packet + sizeof (struct sr_ethernet_hdr));
}
/*
 * Fetch the ICMP_HDR from the Packet
 */
struct sr_icmphdr *get_icmp_hdr(uint8_t *packet, struct ip* ip_hdr)
{
	return (struct sr_icmphdr *) (packet + sizeof (struct sr_ethernet_hdr) + ip_hdr->ip_hl * 4);
}
/*
 * Fetch the ETHERNET_HDR from the Packet
 */
struct sr_ethernet_hdr *get_ethernet_hdr(uint8_t *packet)
{
	return (struct sr_ethernet_hdr *) packet;
}
/*
 *  Increment the ip_packet_count List
 */
void increment_ip_packet_count(uint32_t ip_addr_to_cache)
{
	printf("\n Incerement IP Packet count for :: %s",inet_ntoa(*(struct in_addr*)(&ip_addr_to_cache)));
	struct ip_packet_count* packet_count ;
	packet_count = (struct ip_packet_count*)malloc(sizeof(struct ip_packet_count));
	if(head_ip_packet_count == NULL)
	{
		packet_count->ip_address = ip_addr_to_cache;
		packet_count->count = 1;
		packet_count->next = NULL;
		head_ip_packet_count = packet_count;
	}
	else
	{
		struct ip_packet_count* temp = head_ip_packet_count;
		struct ip_packet_count* prev ;
		int packet_found = 0; // 0 mean NO
		while(temp != NULL)
		{
			if(temp->ip_address == ip_addr_to_cache)// Exisiting IP Increment Count for it
			{
				temp->count = temp->count + 1;
				packet_found = 1;
				break;
			}
			prev = temp;
			temp = temp->next;

		}
		if(packet_found == 0)// 1st Time Need to ADD
		{
			packet_count->ip_address = ip_addr_to_cache;
			packet_count->count = 1;
			packet_count->next = NULL;
			prev->next = packet_count;
		}
	}
	printf("\n Packet Count is  %d",packet_count->count);
}
/* This method caches the Packet before sending a ARP Request to fetch MAC from Server
 *
 */
void add_packet_to_cache(uint8_t *packet, struct sr_ethernet_hdr * eth_hdr, int len,struct sr_instance* sr,char *interface,int is_packet_for_gateway)
{


	if(ntohs(eth_hdr->ether_type) == ETHERTYPE_IP)
	{
		printf("\nCaching the Packet\n");
		uint32_t dest_ip_addr;
		dest_ip_addr = get_ip_hdr(packet)->ip_dst.s_addr;
		struct packet_cache* packet_being_cached;

		packet_being_cached = (struct packet_cache*)malloc(sizeof(struct packet_cache));

		//Cache the Packet structure
		packet_being_cached->packet = (uint8_t*)malloc(sizeof(uint8_t) * len);//Allocate space for the Packet
		memcpy(packet_being_cached->packet,packet,(sizeof(uint8_t) * len));
		packet_being_cached->length=len;
		packet_being_cached->interface=interface;
		packet_being_cached->sr=sr;
		packet_being_cached->next = NULL;
		packet_being_cached->ip_address = dest_ip_addr;
		packet_being_cached->is_packet_for_gateway = is_packet_for_gateway;

		if(head_packet_cache == NULL)
		{
			printf("\nCaching the Packet as HEAD\n");
			head_packet_cache = packet_being_cached;
		}
		else
		{
			printf("\nCaching the Packet as NEXT\n");
			struct packet_cache* temp = head_packet_cache;
			while(temp->next != NULL)
			{
				temp = temp->next;
			}
			temp->next = packet_being_cached;
		}
		increment_ip_packet_count(dest_ip_addr);
	}

}
/*This method will send the cached packet
 *
 */
void send_cached_packet(uint32_t ip_addr)
{
	printf("\n Sending the Cached Packet\n");
	struct packet_cache* previous_packet = NULL;
	struct packet_cache* current_packet = head_packet_cache;
	struct packet_cache* packet_found = NULL;
	/*assert(sr);
	assert(packet);
	assert(interface);*/
	struct sr_rt* fetchedInterface;
	struct ip* ip_hdr;

	while(current_packet != NULL)
	{
		printf("\nPacket exists");
		uint8_t* packet = current_packet->packet;
		ip_hdr = get_ip_hdr(packet);
		uint32_t dst_ip = ip_hdr->ip_dst.s_addr;
		int is_ip_for_gate_way = is_ip_addr_for_gateway(ip_addr,current_packet->sr);// Will Return 1 if it is for GATEWAY
		printf("\n Is Reply from Gateway :: %d ",is_ip_for_gate_way);
		if( (ip_hdr->ip_ttl > 1 && ip_addr == dst_ip) || (ip_hdr->ip_ttl > 1 && is_ip_for_gate_way && current_packet->is_packet_for_gateway))
		{
			if(previous_packet == NULL)
			{
				packet_found = current_packet;
				current_packet = current_packet->next;
				head_packet_cache = current_packet;
			}
			else
			{
				packet_found = current_packet;
				previous_packet->next = current_packet->next;
				current_packet = current_packet->next;

			}
			// Send packet here and then free this Packet
			fetchedInterface = getInterFaceForIp(packet_found->sr,dst_ip);
			sr_route_packet(packet_found->sr,packet_found->packet,packet_found->length,packet_found->interface,fetchedInterface);
			printf("\n Done sending Cached packet");
			free(packet_found);
			// Add code to Decrement counter here
		}
		else
		{
			previous_packet = current_packet;
			current_packet = current_packet->next;
		}

	}
}

/*
 * This method can be used to Debug the MAC Address
 */
void print_mac_address(unsigned char* CachedValue)
{

	int i;
	printf("\nCached MAC Value ");
	for(i=0; i<6; i++)
	{
		printf("%x:", CachedValue[i]);
	}
	printf("\n");
}
/*
 * Compute the CHECK SUM for ICMP Packet
 */
void setICMPchecksum(struct sr_icmphdr* icmphdr, uint8_t * packet, int length)
{
	//printf("\nCalculating checksum for ICMP ");
	uint32_t sum = 0;
	uint16_t* packet_temp = (uint16_t *) packet;
	int i;
	for (i = 0; i < length / 2; i++)
	{
		sum = sum + packet_temp[i];
	}

	sum = (sum >> 16) + (sum & 0xFFFF);
	sum = sum + (sum >> 16);
	icmphdr->icmp_sum = ~sum;
	//printf("CheckSum :: %d", calculatedSum);
}
/*
 * Compute the CHECK SUM for IP Packet
 */
void setIPchecksum(struct ip* ip_hdr)
{
	//printf("\nCalculating checksum for IP ");
	int i;

	uint32_t calculatedSum = 0;
	uint32_t sum = 0;
	uint16_t* tmp = (uint16_t *) ip_hdr;

	ip_hdr->ip_sum = 0;

	for (i = 0; i < ip_hdr->ip_hl * 2; i++)
	{
		sum = sum + tmp[i];
	}

	sum = (sum >> 16) + (sum & 0xFFFF);
	sum = sum + (sum >> 16);
	calculatedSum = ~sum;
	ip_hdr->ip_sum = calculatedSum;

	//printf("CheckSum :: %d", calculatedSum);
}
/*
 * Check if MAC Exists if it does return the MAC Address
 */
unsigned char* does_mac_address_exist(uint32_t ip_addr,int isFromCacheMac)
{
	struct timeval tv;
	double timeNow;
	printf("\nChecking if MAC exists for IP :: %s ",inet_ntoa(*(struct in_addr*)(&ip_addr)));
	int j=0,i=0;
	unsigned char mac_addr [6] = {'\0'};

	struct ip_mac_cache cachedValue;
	gettimeofday(&tv, NULL);
	cachedValue = cached_mac_addresses[j];// Take the 1st Record

	while(cachedValue.ip_address != NULL && cachedValue.mac_address != NULL && cachedValue.valid == 1)
	{
		cachedValue = cached_mac_addresses[j];

		timeNow = tv.tv_sec + (tv.tv_usec/1000000.0);
		double timeElapsed = (double)(timeNow-cachedValue.entryTime);

		if(isFromCacheMac == 1 && timeElapsed > ARP_CACHE_TIME_LIMIT )
		{
			cached_mac_addresses[j].entryTime = timeNow;
		}

		if(cachedValue.ip_address == ip_addr && timeElapsed < ARP_CACHE_TIME_LIMIT )
		{
			printf("\nCached_ip ::%s",inet_ntoa(*(struct in_addr*)(&cachedValue.ip_address)));
			printf("\nCached MAC Value :: ");
			for(i=0; i<6; i++)
			{
				printf("%x:", cachedValue.mac_address[i]);
			}
			memcpy(mac_addr, cachedValue.mac_address, ETHER_ADDR_LEN);
			break;
		}
		j++;
	}

	if(mac_addr[0] != '\0')
	{
		printf("\nMAC Address Found");
		/*printf("\nFound MAC FOR IP  :: %s ",inet_ntoa(*(struct in_addr*)(&cachedValue.ip_address)));
		for(i=0; i<6; i++)
		{
			printf("%x:", mac_addr[i]);
		}
		printf("\n");*/
	}
	else
	{
		printf("\nMAC Not Found");
	}

	return mac_addr;

}
/*
 * Check if MAC Exists 1st , if not store it
 */
void cache_mac_address(struct sr_arphdr* arp_Header)
{
	int i;
	int isFromCacheMac = 1;// 0 MEANS NO , 1 MEANS YES
	time_t timeNow;
	struct timeval tv;
	//printf("\nInside cache_mac_address\n");
	struct ip_mac_cache cache ;
	unsigned char*  existing_mac_addr ;
	existing_mac_addr = does_mac_address_exist(arp_Header->ar_sip,isFromCacheMac);
	if(existing_mac_addr[0] != '\0')
	{
		printf("\nMAC Exists Not Performing Cache");
		for(i=0; i<6; i++)
		{
			printf("%x:", existing_mac_addr[i]);
		}
		return;
	}
	printf("\n Caching MAC for ip_address :: %s",inet_ntoa(*(struct in_addr*)(&arp_Header->ar_sip))); //Source is SERVER
	cache.ip_address = arp_Header->ar_sip;
	memcpy(cache.mac_address, arp_Header->ar_sha,ETHER_ADDR_LEN);
	gettimeofday(&tv, NULL);
	cache.entryTime = tv.tv_sec + (tv.tv_usec/1000000.0);
	cache.valid =1;
	//printf("\n savinf to cache time entry  :: %f",cache.entryTime);
	// DEBUGGING
	/*printf("\n MAC Addr cached Is ");
	for(i=0; i<6; i++)
	{
		printf("%x:", cache.mac_address[i]);
	}*/

	//cache->mac_address = arp_Header->ar_sha;
	cached_mac_addresses[free_pos_for_caching] = cache;
	free_pos_for_caching++;
}
/*
 * This function returns the INTERFACE(eth0,eth1,eth2) for the ipAddressProvided
 */

struct sr_rt* getInterFaceForIp(struct sr_instance* sr,uint32_t ipAddToMatch)
{
	//printf("\nInside getInterFaceForIp()\n");

	struct sr_rt* rtable;
	rtable = sr->routing_table;
	//rt_walker = rt_walker->next;// Skipping The default GateWay here , if required modify code to pass a flag return 1st entry
	while(rtable)
	{
		//Gateway will be populated if it is gateway else will be 0.0.0.0
		if( rtable->gw.s_addr ==0 && rtable->mask.s_addr > 0 && (rtable->mask.s_addr & ipAddToMatch) == rtable->dest.s_addr)
		{
			printf("\nFetched InterFace :: %s",rtable->interface);
			return rtable;
		}
		rtable = rtable->next;
	}
	return rtable;

}
/*
 * This function returns the INTERFACE(eth0,eth1,eth2) for the gateway
 */
struct sr_rt* getInterFaceForGateWay(struct sr_instance* sr,uint32_t ipAddToMatch)
{
	struct sr_rt* rt_walker;
	rt_walker = sr->routing_table;
	while(rt_walker)
	{
		//Gateway will be populated if it is gateway else will be 0.0.0.0
		if( rt_walker->gw.s_addr !=0 && rt_walker->mask.s_addr == 0 )
		{
			printf("\nFetched Gateway InterFace :: %s",rt_walker->interface);
			return rt_walker;
		}
		rt_walker = rt_walker->next;
	}
	return rt_walker;

}

/*
 * This function checks if IP_ADDR is a gateway
 */
int  is_ip_addr_for_gateway(uint32_t ipAddToMatch, struct sr_instance* sr)
{
	struct sr_rt* routing_table =sr->routing_table;
	while(routing_table != NULL)
	{
		if(routing_table->gw.s_addr == ipAddToMatch)
		{
			return 1;// Packet is for gateway
		}
		routing_table = routing_table->next;

	}
	return 0;

}
/*---------------------------------------------------------------------
 * Method: sr_handlepacket(uint8_t* p,char* interface)
 * Scope:  Global
 *
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Both the packet buffer and the character's memory are handled
 * by sr_vns_comm.c that means do NOT delete either.  Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 *
 *---------------------------------------------------------------------*/
void sr_handlepacket(struct sr_instance* sr,
		uint8_t * packet/* lent */,
		unsigned int len,
		char* interface/* lent */)
{
	/* REQUIRES */
	int isPacketForInterface = 0;// 0[NO] indicates it is not for interface , 1[YES] is for interface
	assert(sr);
	assert(packet);
	assert(interface);
	struct sr_rt* fetchedInterface;

	struct sr_ethernet_hdr* ethr_hdr = get_ethernet_hdr(packet);
	struct ip* ip_hdr = get_ip_hdr(packet);

	uint32_t dst_ip = ip_hdr->ip_dst.s_addr;
	uint32_t src_ip = ip_hdr->ip_src.s_addr;

	printf("\nReceived IP Src addr :: %s",inet_ntoa(*(struct in_addr*)(&src_ip)));
	printf("\nReceived IP Destin addr :: %s\n",inet_ntoa(*(struct in_addr*)(&dst_ip)));
	printf("\nPacket TTL :: %d ",ip_hdr->ip_ttl);

	switch (ntohs(ethr_hdr->ether_type))
	{
	//Received ICMP / IP Packet
	case ETHERTYPE_IP:

		assert(ethr_hdr);

		printf("\nETHERTYPE_IP");

		//sr_cache_host(sr,ip_hdr,interface);
		struct sr_if * iface = sr->if_list;

		struct sr_icmphdr* received_icmp = (struct sr_icmphdr*)(packet+sizeof(struct sr_ethernet_hdr)+sizeof(struct ip));

		while(iface)// Check if Packet was meant for THE INTERFACE
		{
			//printf("\nIP Protocol :: %d",ip_hdr->ip_p);
			if(iface->ip == dst_ip )// IF The Packet is meant for router and is a ICMP Packet //&& ip_hdr->ip_p == IPPROTO_ICMP && received_icmp->icmp_code == ICMP_TYPE_ECHO_REQUEST
			{
				//printf("\nICMP Packet for Interface %s\n",iface->name);
				isPacketForInterface =1;
				sr_handle_icmp_packet(sr,len,packet,interface,isPacketForInterface);// Forumlate the ICMP PACKET TO SEND
				/*if(ip_hdr->ip_ttl >1 && (ip_hdr->ip_p != IPPROTO_UDP || ip_hdr->ip_p != IPPROTO_TCP))// Send Ping only if TTL > 1
				{
					printf("\nSending packet ttl > 1");
					sr_send_packet(sr, packet, len, interface);
				}*/
				break;
			}
			iface = iface->next;
		}
		sr_handle_arp_packet(sr,len,interface,packet);// THis API Handles ARP Request and Response Be Careful
		if (isPacketForInterface == 0 && ip_hdr->ip_ttl > 1)
		{
			//printf("\nSending the Packet TTL :: %d ",ip_hdr->ip_ttl);
			//Finding the InterFace
			//printf("\nFinding the InterFace\n");
			fetchedInterface = getInterFaceForIp(sr,dst_ip);
			sr_route_packet(sr,packet,len,interface,fetchedInterface);
		}
		else if(isPacketForInterface == 0 && ip_hdr->ip_ttl <=1 )// Means it is not for interface
		{
			printf("\nPacket timedOut :: %d ",ip_hdr->ip_ttl);
			sr_handle_icmp_packet(sr,len,packet,interface,isPacketForInterface);// Forumlate the ICMP PACKET TO SEND
		}
		break;


	case ETHERTYPE_ARP:
		//printf("Received packet of length %d \n",len);
		sr_handle_arp_packet(sr,len,interface,packet);
		break;


	}

}
/*
 * This function Routes the Packet on the Interface provided
 */
void sr_route_packet(struct sr_instance * sr, uint8_t * packet, int len, char* interface,struct sr_rt* fetchedInterface)
{
	printf("\nRouting the packet ");
	unsigned char* mac_addr_cached;
	struct ip* ip_header = (struct ip*)(packet+sizeof(struct sr_ethernet_hdr));
	struct sr_ethernet_hdr * eth_hdr = (struct sr_ethernet_hdr *) packet;

	uint32_t dst_ip = ip_header->ip_dst.s_addr;//This is destination Address
	uint32_t src_ip =	ip_header->ip_src.s_addr;
	//printf("\nRoute Packet to :: %s",inet_ntoa(*(struct in_addr*)(&dst_ip)));
	mac_addr_cached = does_mac_address_exist(dst_ip,0);// Check for MAC Address

	if(mac_addr_cached[0] == '\0')// Cache Miss
	{
		printf("\nCache Missed [MIGHT BE FOR GATEWAY]");
		struct sr_rt* doesInterfaceExistForSourceIp = getInterFaceForIp(sr,src_ip);// If Source is on the Network (Skip gateway here)
		if(doesInterfaceExistForSourceIp != NULL) // Message from Server
		{
			printf("\nMessage From for IP : %s",inet_ntoa(*(struct in_addr*)(&src_ip)));
			printf("\nExists on SubNet : %s",doesInterfaceExistForSourceIp->interface);

			//struct sr_if * ifaceForGateway = sr->if_list;// Send On GateWay
			struct sr_rt* gatewayEntry =  getInterFaceForGateWay(sr,dst_ip);

			unsigned char* macForGw = does_mac_address_exist(gatewayEntry->gw.s_addr,0);// Get MAC For GateWaay
			fetchedInterface = gatewayEntry;//Send on GateWay
			interface = fetchedInterface->interface;//Gateways Interface
			dst_ip = gatewayEntry->gw.s_addr;
			if(macForGw[0] != '\0')
			{
				printf("\nFound the MAC to send to Gateway ");
				int k =0;
				for(k=0;k<ETHER_ADDR_LEN;k++)
				{
					eth_hdr->ether_dhost[k] =  (uint8_t)macForGw[k];
					//printf("\nCopied  %x",mac_addr_cached[m]);
				}
				printf("\nRouting Packet on Default GateWay %s",gatewayEntry->interface);
				ip_header->ip_ttl = ip_header->ip_ttl -1 ;// Reduce the TTL Before Sending
				setIPchecksum(ip_header);
				struct sr_if * iface = sr->if_list;
				int j=0;
				while (iface)
				{
					if (strcmp(iface->name,gatewayEntry->interface) == 0)// If they match
					{
						//printf("\nMac addr for the Interface :: %s",iface->name);
						//printf("\n");
						for(j=0;j<ETHER_ADDR_LEN;j++)
						{
							eth_hdr->ether_shost[j] = iface->addr[j];
							//printf("%x:", iface->addr[j]);

						}
						//print_mac_address(mac_addr_cached);// Just to Debug
					}
					iface = iface->next;
				}
				printf("\nSending the Packet to gateWay\n");
				//printf("\nRoute Packet to :: %s",inet_ntoa(*(struct in_addr*)(&dst_ip)));
				sr_send_packet(sr,packet,len,fetchedInterface->interface);
			}
		}
	}

	// Check if destIp is in rtable/iface else take default gateway and check cache for that ip and then send packet to that address
	if(mac_addr_cached[0] != '\0')
	{
		int m=0;
		for(m=0;m<ETHER_ADDR_LEN;m++)// DO NOT MOVE THIS CODE , CAUSES WEIRD ISSUE WITH MAC ADDRESS OR ADD ANYTHING ABOVE THIS
		{
			eth_hdr->ether_dhost[m] =  (uint8_t)mac_addr_cached[m];
			//printf("\nCopied  %x",mac_addr_cached[m]);
		}
		printf("\nRouting Paacket it was a ARP CACHE HIT");
		//print_mac_address(mac_addr_cached);// Just to Debug
		ip_header->ip_ttl = ip_header->ip_ttl -1 ;// Reduce the TTL Before Sending
		setIPchecksum(ip_header);
		//print_mac_address(mac_addr_cached);// Just to Debug

		struct sr_if * iface = sr->if_list;
		int j=0;
		while (iface)
		{
			if (strcmp(iface->name,fetchedInterface->interface) == 0)// If they match
			{
				//printf("\nMac addr for the Interface :: %s",iface->name);printf("\n");
				for(j=0;j<ETHER_ADDR_LEN;j++)
				{
					eth_hdr->ether_shost[j] = iface->addr[j];
					//printf("%x:", iface->addr[j]);
				}
				//print_mac_address(mac_addr_cached);// Just to Debug
			}
			iface = iface->next;
		}
		//DEBUG
		/*printf("\n Cached value of MAC IS : ");
		int k=0;
		for(k=0;k<ETHER_ADDR_LEN;k++)
		{
			printf("%x:",mac_addr_cached[k]);
		}

		//memcpy(eth_hdr->ether_dhost,mac_addr_cached,ETHER_ADDR_LEN);
		printf("\nMac addr In the EthDest :: ");
		int l=0;
		for(l=0;l<ETHER_ADDR_LEN;l++)
		{
			printf("%x:", eth_hdr->ether_dhost[l]);
		}*/
		printf("\nSending the Packet\n");
		sr_send_packet(sr,packet,len,fetchedInterface->interface);

	}
	else// Cache miss send ARP Request
	{
		int is_packet_for_gateway = 0; // 0 is NO
		printf("\nARP Request to  :: %s",inet_ntoa(*(struct in_addr*)(&dst_ip)));
		is_packet_for_gateway = is_ip_addr_for_gateway(dst_ip,sr);// Check if IP is for gateway
		add_packet_to_cache(packet,eth_hdr,len,sr,interface,is_packet_for_gateway);// CACHE THE PACKET
		send_arp_request(sr, dst_ip, interface,fetchedInterface);
	}
}

/*
 * This function handles ARP Packet [Request /Reply ]
 */
void sr_handle_arp_packet(struct sr_instance* sr, unsigned int len, char* interface, uint8_t* packet)
{
	//printf("\nInside sr_handle_arp_packet()\n");
	struct sr_rt* fetchedInterface;
	struct sr_ethernet_hdr* ethr_hd = (struct sr_ethernet_hdr *) packet;
	struct sr_arphdr* arp_hdr = (struct sr_arphdr *) (packet + sizeof (struct sr_ethernet_hdr));
	//printf("\ ARP_HDR OpCode :: %u",ntohs(arp_hdr->ar_op));
	if (arp_hdr && arp_hdr->ar_op == ntohs(ARP_REQUEST))
	{
		printf("\nARP_REQUEST Received Formulating Reply packet");


		cache_mac_address(arp_hdr);// Cache MAC of any Incoming packet


		struct sr_arphdr* arp_reply = (struct sr_arphdr *) (packet + sizeof (struct sr_ethernet_hdr));

		//printf("\nSending on Interface :: %s",interface);
		unsigned char senderAddress[6] ;
		struct sr_if* interfaceFetched = sr_get_interface(sr,interface);
		memcpy(senderAddress,  interfaceFetched->addr, ETHER_ADDR_LEN);

		memcpy(arp_reply->ar_tha, arp_reply->ar_sha, sizeof (arp_reply->ar_sha));
		memcpy(arp_reply->ar_sha, senderAddress, sizeof (senderAddress));

		memcpy(ethr_hd->ether_dhost, ethr_hd->ether_shost, sizeof (ethr_hd->ether_dhost));
		memcpy(ethr_hd->ether_shost, interfaceFetched->addr, sizeof (ethr_hd->ether_shost));


		// Debuggging
		/*int i=0;
    	printf("\nETHER Sender Host::");
    	for(i=0;i<6;i++)
    	{
    		printf("%x",ethr_hd->ether_shost[i]);
    	}
    	printf("\nETHER Dest Host ::");
    	for(i=0;i<6;i++)
    	{
    		printf("%x",ethr_hd->ether_dhost[i]);
    	}*/

		ethr_hd->ether_type = htons(ETHERTYPE_ARP);
		//Finding the InterFace

		fetchedInterface = getInterFaceForIp(sr,arp_reply->ar_tip);

		arp_reply->ar_hrd = htons(ARPHDR_ETHER);
		arp_reply->ar_pro = htons(ETHERTYPE_IP);
		arp_reply->ar_hln = 06;
		arp_reply->ar_pln = 04;
		arp_reply->ar_op = htons(ARP_REPLY);

		uint32_t tmp = arp_reply->ar_tip;
		arp_reply->ar_tip = arp_reply->ar_sip;
		arp_reply->ar_sip = tmp;

		// Debugging
		/*printf("\nTarget IP :: %s",inet_ntoa(*(struct in_addr*)(&arp_reply->ar_tip)));
    	printf("\nSource IP :: %s",inet_ntoa(*(struct in_addr*)(&arp_reply->ar_sip)));
    	//int i=0;
    	printf("\nARP Sender Host	");
    	for(i=0;i<6;i++)
    	{
    		printf("%x",arp_reply->ar_sha[i]);
    	}
    	printf("\nARP Dest Host	");
    	for(i=0;i<6;i++)
    	{
    		printf("%x",arp_reply->ar_tha[i]);
    	}*/

		printf("\nSending ARP REPLY Packet");
		sr_send_packet(sr, packet, len, interface);
	}
	else if(arp_hdr && arp_hdr->ar_op == ntohs(ARP_REPLY))	// Message Received From Server check if we need to cache
	{
		printf("\nGOT ARP REPLY WITH MAC ADDRESS!!!");
		cache_mac_address(arp_hdr); // Cache MAC ADDRESS
		//Check here if ARP Reply Was From GATEWAY

		send_cached_packet(arp_hdr->ar_sip);	// HANDLE THE CACHED MESSAGE

	}



}
/*
 * This method will send a ARP request on the Interface passed
 */
void send_arp_request(struct sr_instance * sr, uint32_t dst_ip, char* interface,struct sr_rt* fetchedInterface)
{
	printf("\nSending ARP Request \n");

	uint8_t * packet = malloc(sizeof(struct sr_ethernet_hdr) + sizeof(struct sr_arphdr));

	struct sr_if * interfaceTosendOn = sr->if_list;
	struct sr_ethernet_hdr * eth_hdr = (struct sr_ethernet_hdr *) packet;
	struct sr_arphdr * arpHdr = (struct sr_arphdr *) (packet + sizeof (struct sr_ethernet_hdr));

	arpHdr->ar_hrd = ntohs(1);
	eth_hdr->ether_type = ntohs(ETHERTYPE_ARP);
	arpHdr->ar_op = ntohs(ARP_REQUEST);
	arpHdr->ar_pro = ntohs(ETHERTYPE_IP);

	eth_hdr->ether_dhost[0] = 255;
	eth_hdr->ether_dhost[1] = 255;
	eth_hdr->ether_dhost[2] = 255;
	eth_hdr->ether_dhost[3] = 255;
	eth_hdr->ether_dhost[4] = 255;
	eth_hdr->ether_dhost[5] = 255;

	arpHdr->ar_pln = 4;
	arpHdr->ar_hln = 6;
	arpHdr->ar_tip = dst_ip;

	while (interfaceTosendOn)
	{
		if (strcmp(interfaceTosendOn->name,fetchedInterface->interface) == 0)// If they match
				{
			//printf("\nSending the ARP Request on Interface :: %s",fetchedInterface->interface);
			int j;
			for (j = 0; j < ETHER_ADDR_LEN; j++)
			{
				arpHdr->ar_sha[j] = interfaceTosendOn->addr[j];
				eth_hdr->ether_shost[j] = arpHdr->ar_sha[j];
			}
			arpHdr->ar_sip = interfaceTosendOn->ip;
			//printf("\nInterface to send on :: %s\n",interfaceTosendOn->name);
			sr_send_packet(sr, packet, sizeof (struct sr_ethernet_hdr) + sizeof (struct sr_arphdr), interfaceTosendOn->name);
				}
		interfaceTosendOn = interfaceTosendOn->next;
	}
	//free(packet);
}
/*
 * This function handles ICMP packets
 */
void sr_handle_icmp_packet(struct sr_instance* sr, unsigned int len,uint8_t* packet,char *interface,int isPacketForInterface)
{
	struct ip* ip_header = get_ip_hdr(packet);
	struct sr_icmphdr* icmpHeader = get_icmp_hdr(packet,ip_header);
	struct sr_ethernet_hdr* eth_header = get_ethernet_hdr(packet);
	int length = len - sizeof (struct sr_ethernet_hdr) - ip_header->ip_hl * 4;
	printf("\nFormulating ICMP ECHO REPLY  TTl :: %d",ip_header->ip_ttl);
	if (icmpHeader->icmp_type == ICMP_TYPE_ECHO_REQUEST && ip_header->ip_ttl > 1)
	{
		printf("\nFormulating ICMP ECHO REPLY ");
		int i;
		int tmp;
		uint32_t destAddress = ip_header->ip_src.s_addr;
		for (i = 0; i < ETHER_ADDR_LEN; i++)
		{
			tmp = eth_header->ether_dhost[i];// Need to swap the address
			eth_header->ether_dhost[i] = eth_header->ether_shost[i];
			eth_header->ether_shost[i] = tmp;
		}

		eth_header->ether_type = htons(ETHERTYPE_IP);

		ip_header->ip_src.s_addr = ip_header->ip_dst.s_addr;
		ip_header->ip_dst.s_addr = destAddress;

		icmpHeader->icmp_type = ICMP_TYPE_ECHO_REPLY;
		icmpHeader->icmp_sum = 0;
		setICMPchecksum(icmpHeader, packet + sizeof (struct sr_ethernet_hdr) + ip_header->ip_hl * 4,length);// Compute checksum in the end
		sr_send_packet(sr, packet, len, interface);

	}
	if( (isPacketForInterface == 0 && ip_header->ip_ttl <= 1 )|| (isPacketForInterface == 1 && (ip_header->ip_p == IPPROTO_UDP || ip_header->ip_p == IPPROTO_TCP)))
	{
		printf("\nICMP Packet ttl < 1 || TCP || UDP ");

		uint8_t sizeOfData = sizeof(struct sr_ethernet_hdr)+sizeof(struct ip)+sizeof(struct sr_icmphdr)+4 +20 +8;//14+20+4+4+20+8=70
		printf("\nsize Of Data :: %d ",sizeOfData);
		printf("\nsize Of sr_ethernet_hdr :: %d ",sizeof(struct sr_ethernet_hdr));
		printf("\nsize Of ip :: %d ",sizeof(struct ip));
		printf("\nsize Of sr_icmphdr :: %d ",sizeof(struct sr_icmphdr));
		uint8_t* new_packet = (uint8_t*)malloc(sizeOfData);

		// COPY ETHERNET HEADER 1sr
		struct sr_ethernet_hdr * eth_hdr = (struct sr_ethernet_hdr *) new_packet;
		int i;
		int tmp;
		for (i = 0; i < ETHER_ADDR_LEN; i++)
		{
			// Need to swap the address
			eth_hdr->ether_dhost[i] = eth_header->ether_shost[i];
			eth_hdr->ether_shost[i] = eth_header->ether_dhost[i];
		}
		eth_hdr->ether_type=htons(ETHERTYPE_IP);
		// COPY IP PART NOW
		struct ip* new_ip = (struct ip *) (new_packet + sizeof (struct sr_ethernet_hdr));
		createIpHdr(ip_header,new_ip,sr,interface);// Create a new IP Packet

		//printf("\nIP Src addr B4 Mem Copy ZERO:: %s",inet_ntoa(*(struct in_addr*)(&new_ip->ip_src.s_addr)));
		//printf("\nIP Destin addr B4 Mem Cpy ZERO:: %s",inet_ntoa(*(struct in_addr*)(&new_ip->ip_dst.s_addr)));

		int position =  sizeof(struct sr_ethernet_hdr)+sizeof (struct ip)+sizeof(struct sr_icmphdr);
		//printf("\nPosition :: %d ",position);
		for(i=position;i<(position+4);i++)
		{
			new_packet[i]=0;
		}
		printf("\nIP Src addr B4 Mem Copy:: %s",inet_ntoa(*(struct in_addr*)(&new_ip->ip_src.s_addr)));
		printf("\nIP Destin addr B4 Mem Cpy:: %s",inet_ntoa(*(struct in_addr*)(&new_ip->ip_dst.s_addr)));
		//Copy IP + 8 Bytes

		struct sr_icmphdr* new_icmp =(struct sr_icmphdr *) (new_packet + sizeof (struct sr_ethernet_hdr) + 20);
		memcpy((new_packet + sizeof(struct sr_ethernet_hdr)+sizeof(struct ip)+sizeof(struct sr_icmphdr) + 4),ip_header,28);


		if(isPacketForInterface == 0 && ip_header->ip_ttl <= 1) // TIME TO LIVE EXCEDED
		{
			printf("\nCreating ICMP for TTL < 1");
			createICMPHdr(ICMP_TYPE_TIME_EXCEEDED,0,new_packet,new_icmp);
		}
		else if((ip_header->ip_p == IPPROTO_UDP || ip_header->ip_p == IPPROTO_TCP) && isPacketForInterface ==1)
		{
			printf("\nCreating ICMP for UnReachable TCP/UDP");
			createICMPHdr(ICMP_CODE_DEST_PORT_UNREACHABLE,3,new_packet,new_icmp);// Refer RFC https://tools.ietf.org/html/rfc792 for code and Type
		}

		int length = 70 - sizeof (struct sr_ethernet_hdr) - new_ip->ip_hl * 4;//36
		new_icmp->icmp_sum = 0;// MANDATORY DO NOT REMOVE IT
		setICMPchecksum(new_icmp,(new_packet + sizeof (struct sr_ethernet_hdr) + new_ip->ip_hl*4),length);//Calculate check sum

		setIPchecksum(new_ip);//Calculate check sum


		printf("\nPACKET COMPLETEdD");
		printf("\nIP Src addr :: %s",inet_ntoa(*(struct in_addr*)(&new_ip->ip_src.s_addr)));
		printf("\nIP Destin addr :: %s",inet_ntoa(*(struct in_addr*)(&new_ip->ip_dst.s_addr)));
		printf("\n Dest Host ::");
		for(i=0;i<6;i++)
		{
			printf("%x",eth_hdr->ether_dhost[i]);
		}
		printf("\n Sender Host ::");
		for(i=0;i<6;i++)
		{
			printf("%x",eth_hdr->ether_shost[i]);
		}

		sr_send_packet(sr, new_packet, sizeof (struct sr_ethernet_hdr) + 20 + 8 + new_ip->ip_hl * 4 + 8, interface);
		free(new_packet);
	}


}
/*
 * This function returns a new Instance of the IP_HDR
 */
struct ip* createIpHdr(struct ip* old_ip,struct ip* new_ip,struct sr_instance* sr,char *interface)
{
	// Create IP_HEADER
	struct sr_if * iface = sr->if_list;
	//struct ip* ip_hdr_new = malloc(sizeof(struct ip));
	new_ip->ip_v = 4;
	new_ip->ip_hl = 5;
	new_ip->ip_tos = 0;
	//new_ip->ip_len = 70;
	new_ip->ip_len = htons(20 + 8 + new_ip->ip_hl * 4 + 8);//56
	new_ip->ip_id = old_ip->ip_id;
	new_ip->ip_off=htons(IP_DF);
	new_ip->ip_ttl = 64;
	new_ip->ip_p = IPPROTO_ICMP;


	while (iface)
	{
		char* iface_name = iface->name;
		if (strcmp(iface_name,interface) == 0)// If they match
		{
			new_ip->ip_src.s_addr = iface->ip;
		}
		iface = iface->next;
	}

	//new_ip->ip_src = old_ip->ip_dst;
	new_ip->ip_dst.s_addr = old_ip->ip_src.s_addr;

	//printf("\nCopied Src addr :: %s",inet_ntoa(*(struct in_addr*)(&new_ip->ip_src.s_addr)));
	//printf("\nCopied Destin addr :: %s\n",inet_ntoa(*(struct in_addr*)(&new_ip->ip_dst.s_addr)));

	return new_ip;


}
/*
 * This function returns a new Instance of the ICMP_HDR with the code and type specified
 */
struct sr_icmphdr* createICMPHdr(uint8_t icmp_type,uint8_t icmp_code,uint8_t *packet,struct sr_icmphdr* icmp_hdr_new)
{
	// Create IP_HEADER
	//struct sr_icmphdr* icmp_hdr_new = malloc(sizeof(struct sr_icmphdr));
	icmp_hdr_new->icmp_type=icmp_type;
	icmp_hdr_new->icmp_code = icmp_code;

	return icmp_hdr_new;


}

