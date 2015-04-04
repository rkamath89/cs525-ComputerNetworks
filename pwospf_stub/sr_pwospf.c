/*-----------------------------------------------------------------------------
 * file: sr_pwospf.c
 * date: Tue Nov 23 23:24:18 PST 2004
 * Author: Martin Casado
 *
 * Description:
 *
 *---------------------------------------------------------------------------*/

#include "sr_pwospf.h"
#include "sr_router.h"
#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>

#define SUBNET 1
#define ROUTER 0

int isPrimaryRouter,isDynamicMode;
struct in_addr routerId;
long int routerIdDup;
int ifaceCount=0,vertexId =0,seqCountLsu =1;
int *isLinkUp;
struct sr_neighbours *neighbour;
struct graph *subnetGraph = NULL;
struct timeval tv;
uint8_t multicastMac[ETHER_ADDR_LEN];
pthread_t helloPcktThread;
pthread_t lsuPcktThread;
pthread_t arpThreadThread;
/* -- declaration of main thread function for pwospf subsystem --- */
static void* pwospf_run_thread(void* arg);

/*---------------------------------------------------------------------
 * Method: pwospf_init(..)
 *
 * Sets up the internal data structures for the pwospf subsystem
 *
 * You may assume that the interfaces have been created and initialized
 * by this point.
 *---------------------------------------------------------------------*/

int pwospf_init(struct sr_instance* sr)
{
	assert(sr);
	helloPcktThread = (pthread_t*)malloc(sizeof(pthread_t));
	lsuPcktThread = (pthread_t*)malloc(sizeof(pthread_t));
	arpThreadThread = (pthread_t*)malloc(sizeof(pthread_t));
	sr->ospf_subsys = (struct pwospf_subsys*)malloc(sizeof(struct
			pwospf_subsys));

	assert(sr->ospf_subsys);
	pthread_mutex_init(&(sr->ospf_subsys->lock), 0);

	routerIdDup = 0;
	// Handle all Initializations here
	initializeAllValues(sr);

	/* -- start thread subsystem -- */
	if( pthread_create(&sr->ospf_subsys->thread, 0, pwospf_run_thread, sr)) {
		perror("pthread_create");
		assert(0);
	}
	if(isDynamicMode == 1)
	{
		// Send Hello packets Thread
		pthread_create(&helloPcktThread, NULL, sendHellos, sr);
		// Send LSU Packets Thread
		pthread_create(&lsuPcktThread, NULL, sendLSUPacket, sr);
	//	pthread_create(&arpThreadThread, NULL, sendArpPacket, sr);
	}

	return 0; /* success */
} /* -- pwospf_init -- */

/*
 * Initialize all values required, graphs, neighbours , vertex etc
 */
void initializeAllValues(struct sr_instance *sr)
{
	// Check if Dynamic mode or not
	struct sr_rt* rtable;
	rtable = sr->routing_table;
	int rTableCount =0;
	while(rtable)
	{
		rTableCount++;
		rtable = rtable->next;
	}
	if(rTableCount == 0)
	{
		isPrimaryRouter = 0;
		isDynamicMode = 1;
	}
	else if(rTableCount == 1)
	{
		isPrimaryRouter = 1;// Vhost 1 will have only one entru in Rtable i.e default gateway
		isDynamicMode = 1;
	}
	if(isDynamicMode == 1)
	{
		// End
		// Initialize MAC Address
		//http://arstechnica.com/civis/viewtopic.php?t=607865
		uint8_t multicastMac[ETHER_ADDR_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
		// End
		// Sett the rid and count ifaces
		struct sr_if *iface;
		iface = sr->if_list;
		if(routerIdDup == 0)
		{
			while(iface != NULL)
			{
				if (iface->ip < routerIdDup || routerIdDup == 0)
				{
					routerIdDup = iface->ip;
				}
				ifaceCount++;
				iface = iface->next;
			}

		}
		printf("-> PWOSPF: Router has %d Interfaces and The router ID is %s\n", ifaceCount,inet_ntoa(*(struct in_addr*)(&routerIdDup)));
		//routerId.s_addr=routerIdDup.s_addr;
		// End
		// Initialize the neighbour list for Router
		isLinkUp = (int*)malloc(ifaceCount * sizeof(int));
		neighbour = (struct sr_neighbours*)malloc(ifaceCount * sizeof(struct sr_neighbours));
		int i=0;
		for(i = 0; i < ifaceCount; ++i)
		{
			isLinkUp[i] = 1;
			initializeNeighbor(i);
		}
		// End
		subnetGraph = initializeGraph();
		initializeRouter(subnetGraph);
		addInterfaceVertices(subnetGraph, sr);
	}

}
/*
 * Dijkstras to find Path
 */

void dijkstra(struct graph* graphVal, struct vertexList *vertexlst)
{

    initializeDijkstras(subnetGraph, vertexlst);
    struct path *path1 = NULL;
    struct adjacencyList *adjList;
    struct vertexList *vrtx1, *vrtx2, *vrtx3;
    vrtx1 = graphVal->vrtxList;
    while(vrtx1 != NULL)
    {
    	vrtx3 = getMinVertex(graphVal->vrtxList);
        if(vrtx3)
        {
        	vrtx3->vrtx.visited = 1;
            addValue(path1, vrtx3);
            adjList = vrtx3->vrtx.head;
            while(adjList)
            {
            	vrtx2 = getVertexFromEdge(adjList->edg.dst);
            	computeValues(vrtx3, vrtx2, adjList->edg.weight);
                adjList = adjList->next;
            }
        }
        vrtx1 = vrtx1->next;
    }
}
void initializeDijkstras(struct graph *subntgraph, struct vertexList *vlist)
{
    struct vertexList *vertexlst =subntgraph->vrtxList;
    while(vertexlst)
    {
    	vertexlst->vrtx.parent = -1;
    	vertexlst->vrtx.d = 7;
        vertexlst->vrtx.visited = 0;
        vertexlst = vertexlst->next;
    }
    vlist->vrtx.d = 0;
}
/* Graph Initialization
 *
 */
struct graph* initializeGraph()
{
    struct graph *g = (struct graph*)malloc(sizeof(struct graph));
    g->val= 0;
    g->vrtxList = NULL;
    return g;
}
void computeValues(struct vertexList *vrtx1, struct vertexList *vrtx2, int vrtx3)
{
    if(vrtx2->vrtx.d > vrtx1->vrtx.d + vrtx3)
    {
    	vrtx2->vrtx.d = vrtx1->vrtx.d + vrtx3;
    	vrtx2->vrtx.parent = vrtx1->vrtx.id;
    }
}
struct vertexList* getMinVertex(struct vertexList *vlist)
{
    int min = 7+1;
    struct vertexList *tmp;
    while(vlist)
    {
        if(vlist->vrtx.visited == 0)
        {
            if(vlist->vrtx.d < min)
            {
            	min = vlist->vrtx.d;
            	tmp = vlist;
            }
        }
        vlist = vlist->next;
    }
    return tmp;
}

void addInterfaceVertices(struct graph *subnetGraph1, struct sr_instance *sr)
{
	struct in_addr dst,gateWay,mask;
	struct sr_if *ifList = sr->if_list;
	struct vertexList *router = getRouterById(subnetGraph1, routerIdDup);
	while(ifList)
	{
		addVertex(subnetGraph1, routerIdDup, (ifList->ip & ifList->mask),ifList->mask, SUBNET);
		addEdge(router, subnetGraph1->vrtxList, 1);
		dst.s_addr = (ifList->ip & ifList->mask);
		gateWay.s_addr = 0;
		mask.s_addr = ifList->mask;

		if(isDynamicMode == 1)
		{
			sr_add_rt_entry(sr,dst,gateWay,mask,ifList->name);// Create Routing Table Entry here
		}
		ifList = ifList->next;
	}
}
struct vertexList* getVertexFromEdge(int val)
{
    struct vertexList *vrtxLst = subnetGraph->vrtxList;
    while(vrtxLst)
    {
        if(vrtxLst->vrtx.id == val)
        {
            return vrtxLst;
        }
        vrtxLst = vrtxLst->next;
    }

    return NULL;
}
/* Fetch Router info based on the Router Id
 *	VType 0 is Router , 1 is Subnet
 */
struct vertexList* getRouterById(struct graph *subnetGraph1, uint32_t rtrId)
{
    struct vertexList *tempVertex = subnetGraph1->vrtxList;
    while(tempVertex)
    {
        if(tempVertex->vrtx.type == ROUTER && tempVertex->vrtx.rid == rtrId)
        {
            return tempVertex;
        }
        tempVertex = tempVertex->next;
    }

    return NULL;
}
void addValue(struct path *path1, struct vertexList *vertex1)
{
    struct path* temp = (struct path*)malloc(sizeof(struct path));
    memcpy(&temp->vrtx, &vertex1->vrtx, sizeof(struct vertex));
    temp->next = path1;
    path1 = temp;
}
struct vertexList* getNodeForSubnet(struct graph *subnetGraph1, uint32_t subnet, uint32_t rtrId)
{
    struct vertexList *tempVertex = subnetGraph1->vrtxList;
    while(tempVertex)
    {
        if((tempVertex->vrtx.type == SUBNET) && (tempVertex->vrtx.subnet == subnet)
        		&& (tempVertex->vrtx.rid == rtrId))
        {
            return tempVertex;
        }
        tempVertex = tempVertex->next;
    }

    return NULL;
}
void addEdge(struct vertexList *vrtex1, struct vertexList *vrtex2, int weight)
{
    struct adjacencyList *list = (struct adjacencyList*)malloc(sizeof(struct adjacencyList));

    list->edg.src = vrtex1->vrtx.id;
    list->edg.dst = vrtex2->vrtx.id;
    list->edg.weight = weight;

    list->next = vrtex1->vrtx.head;
    vrtex1->vrtx.head = list;
}

struct adjacencyList* checkIfEdgeExists(struct graph* subnetGraph, int src, int dst)
{
    struct vertexList *tmp = subnetGraph->vrtxList;
    struct adjacencyList * val;
    while(tmp)
    {
    	val = tmp->vrtx.head;
        while(val)
        {
            if(val->edg.src == src && val->edg.dst == dst)
                return val;
            val = val->next;
        }
        tmp = tmp->next;
    }
    return 0;
}
/* This initializes the neighbour for the Router
 *
 */

void initializeNeighbor(int val)
{
    neighbour[val].routerId = 0;
    neighbour[val].ip = 0;
    char interface[10];
    sprintf(interface,"eth%d",val);
    neighbour[val].interFace = (char*)malloc(10);
    strcpy(neighbour[val].interFace, interface);
    neighbour[val].alive = 1;
    neighbour[val].netMask = 0;
    gettimeofday(&tv, NULL);
    neighbour[val].timeStamp = tv.tv_sec;
}
/*
 * Initialize the values for the current Router
 */
void initializeRouter(struct graph *subNet)
{
    addVertex(subNet, routerIdDup, 0, 0, ROUTER);// 0 means it is a Router
}
/*
 *	Create the vertex for the graph TYPE :: 0 is Router 1 is Interface
 */
void addVertex(struct graph *subnetGraph1, uint32_t rtrId, uint32_t subnet, uint32_t networkMask, int type)
{
	struct vertexList *vertex;
	vertex = (struct vertexList*)malloc(sizeof(struct vertexList));
	subnetGraph1->val++;
	vertex->vrtx.id = vertexId++;
	vertex->vrtx.rid = rtrId;
	vertex->vrtx.subnet = subnet;
	vertex->vrtx.networkMask = networkMask;
	vertex->vrtx.seqNbrSeen = 0;
	vertex->vrtx.type = type;
	vertex->vrtx.visited = 0;
	vertex->vrtx.head = NULL;
	vertex->next = subnetGraph1->vrtxList;
	subnetGraph1->vrtxList = vertex;

}
/*---------------------------------------------------------------------
 * Method: pwospf_lock
 *
 * Lock mutex associated with pwospf_subsys
 *
 *---------------------------------------------------------------------*/

void pwospf_lock(struct pwospf_subsys* subsys)
{
    if ( pthread_mutex_lock(&subsys->lock) )
    { assert(0); }
} /* -- pwospf_subsys -- */

/*---------------------------------------------------------------------
 * Method: pwospf_unlock
 *
 * Unlock mutex associated with pwospf subsystem
 *
 *---------------------------------------------------------------------*/

void pwospf_unlock(struct pwospf_subsys* subsys)
{
    if ( pthread_mutex_unlock(&subsys->lock) )
    { assert(0); }
} /* -- pwospf_subsys -- */

/*---------------------------------------------------------------------
 * Method: pwospf_run_thread
 *
 * Main thread of pwospf subsystem.
 *
 *---------------------------------------------------------------------*/

static void* pwospf_run_thread(void* arg)
{

} /* -- run_ospf_thread -- */
int handleLsuPacket(struct sr_instance* sr,uint8_t * packet/* lent */,unsigned int len,char* interface/* lent */)
{
	//printf("\nReceived LSU Packet on InterFace  %s",interface);

    uint8_t *tempPacket;
    struct sr_if *ifList;
    struct ospfv2_lsu* ospfLsuArray;
    struct ospfv2_lsu*  ospfLsu ;
    struct ospfv2_lsu_hdr* ospfLsuHdr ;
    struct sr_ethernet_hdr* ethHdr ;

    long int advNum;
    //printf("\n A");
    tempPacket = (uint8_t*)(packet + sizeof(struct sr_ethernet_hdr));
    struct ospfv2_hdr *ospf_hdr = (struct ospfv2_hdr*)(tempPacket+ sizeof(struct ip));
    struct vertexList *initialVertex;
    struct vertexList  *src,*dst;
    ospfLsuHdr = (struct ospfv2_lsu_hdr*)(tempPacket + sizeof(struct ip)+ sizeof(struct ospfv2_hdr));
    //printf("\n B");

    advNum = ntohl(ospfLsuHdr->num_adv);
   // printf("\nNumber Of lsu Adv :: %ld",advNum);

    ospfLsuArray = (struct ospfv2_lsu*)malloc(advNum * sizeof(struct ospfv2_lsu));
    ospfLsu = (struct ospfv2_lsu*)(tempPacket + sizeof(struct ip)+ sizeof(struct ospfv2_hdr)+ sizeof(struct ospfv2_lsu_hdr));
    memcpy(ospfLsuArray, ospfLsu, advNum * sizeof(struct ospfv2_lsu));

    //printf("\n C");
    if(ospf_hdr->rid == routerIdDup)
    {
    	//printf("\n Same Router Drop LSU");
        return 0;
    }
    else
    {
    	initialVertex = getRouterById(subnetGraph, ospf_hdr->rid);
        if(initialVertex != NULL)
        {
            if(initialVertex->vrtx.seqNbrSeen >= ntohs(ospfLsuHdr->seq))
            {
            	//printf("\n SeqNbr already Seen Drop LSU");
                return 0;
            }
            else
            {
            	initialVertex->vrtx.seqNbrSeen = ntohs(ospfLsuHdr->seq);
            }
        }
    }
    //printf("\n D");
    if(getRouterById(subnetGraph, ospf_hdr->rid) == NULL)
    {
    	//printf("\n Adding a Vertex ");
        addVertex(subnetGraph, ospf_hdr->rid, 0, 0, ROUTER);
        initialVertex = getRouterById(subnetGraph, ospf_hdr->rid);
        initialVertex->vrtx.seqNbrSeen = ntohs(ospfLsuHdr->seq);
    }
    int i=0;
    for(i = 0; i < advNum;i++)
    {
    	//printf("\n Sending For ADV %d",i);
        if(getNodeForSubnet(subnetGraph, ospfLsuArray[i].subnet, ospf_hdr->rid) == NULL)
        {
            addVertex(subnetGraph, ospf_hdr->rid, ospfLsuArray[i].subnet,ospfLsuArray[i].mask, SUBNET);
        }

        src = getRouterById(subnetGraph, ospf_hdr->rid);
        dst = getNodeForSubnet(subnetGraph, ospfLsuArray[i].subnet, ospf_hdr->rid);

        if(!checkIfEdgeExists(subnetGraph, src->vrtx.id, dst->vrtx.id))
        {
            addEdge(src, dst, 1);
        }

        if(ospfLsuArray[i].rid != 0)
        {
            if(getRouterById(subnetGraph, ospfLsuArray[i].rid) == NULL)
            {
                addVertex(subnetGraph, ospfLsuArray[i].rid, 0, 0, ROUTER);
            }
        }

        src = getRouterById(subnetGraph, ospf_hdr->rid);
        dst = getRouterById(subnetGraph, ospfLsuArray[i].rid);

        if(src!= NULL && dst!= NULL)
        {
            if(!checkIfEdgeExists(subnetGraph, src->vrtx.id, dst->vrtx.id))
            {
            	addEdge(src, dst, 1);
            }
            if(!checkIfEdgeExists(subnetGraph,dst->vrtx.id,src->vrtx.id))
            {
            	addEdge(dst,src,1);
            }
        }
        ospfLsuHdr->ttl--;
        ifList = sr->if_list;
        while(ifList)
        {
        	ethHdr = (struct sr_ethernet_hdr*)packet;
        	for (int i = 0; i < ETHER_ADDR_LEN; i++)
        	{
        		ethHdr->ether_shost[i] = ifList->addr[i];
        	}
        	if(strcmp(ifList->name, interface) != 0)
        	{
        		//struct ospfv2_lsu_hdr* ospfLsuHdrTemp = (struct ospfv2_lsu_hdr*)(tempPacket + sizeof(struct ip)+ sizeof(struct ospfv2_hdr));
        		//long int abcd = ntohl(ospfLsuHdrTemp->num_adv);
        		//printf("\n ***Number Of Adv :: %d , Iface :: %s  ",abcd,ifList->name);
        		sr_send_packet(sr, packet, len, ifList->name);
        	}

        	ifList = ifList->next;
        }
    }
   // printf("\nDijkstra Started");
   // printGraph(subnetGraph);
    struct vertexList *dijkstrasRouter;
    dijkstrasRouter = getRouterById(subnetGraph, routerIdDup);
    dijkstra(subnetGraph, dijkstrasRouter);
   // printf("\nDijkstra Ended");
  //  printGraph(subnetGraph);

    pthread_mutex_lock(&(sr->ospf_subsys->lock));
  //  printf("\nCreating Routing Table");
    populateRoutingTable(subnetGraph, sr);
  //  printf("\nRouting Table Done");
    printf("\nRouting Table\n");
    //sr_print_routing_table(sr);
    pthread_mutex_unlock(&(sr->ospf_subsys->lock));
    //printGraph(subnetGraph);
    return 0;

	/*
	printf("\n Got LSU Packet\n");
	uint8_t *tempPacket;
	tempPacket = (uint8_t*)(packet + sizeof(struct sr_ethernet_hdr));
	struct vertexList *initialVertex;
	struct vertexList  *src,*dst;
	struct ip* ipHdr;
	struct sr_ethernet_hdr* ethHdr ;
	struct ospfv2_hdr* ospfHdr = (struct ospfv2_hdr*)(tempPacket + sizeof(struct ip));
	struct ospfv2_lsu_hdr* ospfLsuHdr ;
	struct ospfv2_lsu* ospfLsuArray;
	struct ospfv2_lsu*  ospfLsu ;


	// Fetch all the Frames from Packet
	//printf("\n1");
	ethHdr = (struct sr_ethernet_hdr*)packet;
	//printf("\n2");
	ipHdr = (struct ip*)(packet + sizeof(struct sr_ethernet_hdr));
	//printf("\n3");
	ospfHdr = (struct ospfv2_hdr*)(packet + sizeof(struct sr_ethernet_hdr)+ sizeof(struct ip));
	//printf("\n4");
	ospfLsuHdr = (struct ospfv2_lsu_hdr*)(packet + sizeof(struct sr_ethernet_hdr)+ sizeof(struct ip)+sizeof(struct ospfv2_hdr));
	//printf("\n5");
	ospfLsu = (struct ospfv2_lsu*) (packet + sizeof(struct sr_ethernet_hdr)+ sizeof(struct ip) + sizeof(struct ospfv2_hdr)+ sizeof(struct ospfv2_lsu_hdr));
	printf("\n6");
	// End
	// Create an Array for the lsu based on number advertised and copy the LSU from into it for indexing
	ospfLsuHdr = (struct ospfv2_lsu_hdr*)(tempPacket + sizeof(struct ip)+ sizeof(struct ospfv2_hdr));

	long int advNum = ntohl(ospfLsuHdr->num_adv);
	printf("\n Number Of Adv :: %d ",advNum);
	ospfLsuArray = (struct ospfv2_lsu*)malloc(ntohl(ospfLsuHdr->num_adv) * sizeof(struct ospfv2_lsu));
	ospfLsu = (struct ospfv2_lsu*)(tempPacket + sizeof(struct ip)+ sizeof(struct ospfv2_hdr)+ sizeof(struct ospfv2_lsu_hdr));



	memcpy(ospfLsuArray, ospfLsu, advNum * sizeof(struct ospfv2_lsu));
	//printf("\n 2-Number Of Adv :: %d ",ospfLsuHdr->num_adv);
	printf("\n ------------------------------------------------------------------");
	//ospfLsuArray =  (struct ospfv2_lsu*)malloc(sizeof(struct ospfv2_lsu) * ntohl(ospfLsuHdr->num_adv));
	//printf("\n7");
	//memcpy(ospfLsuArray, ospfLsu, ntohl(ospfLsuHdr->num_adv) * sizeof(struct ospfv2_lsu));
	// End of Array Creation

	//If router id matches just return , since the same packet was fwd back
	printf("\n A");
	if(ospfHdr->rid == routerIdDup)
	{
		printf("\n Dropping LSU Packet source is same as routerId");
		return 0;
	}
	else
	{
		initialVertex = getRouterById(subnetGraph, ospfHdr->rid);
		if(initialVertex != NULL)
		{
			if(initialVertex->vrtx.seqNbrSeen >= ntohs(ospfLsuHdr->seq))
			{
				return 0;
			}
			else
			{
				initialVertex->vrtx.seqNbrSeen = ntohs(ospfLsuHdr->seq);
			}
		}
	}
	initialVertex = getRouterById(subnetGraph, ospfHdr->rid);
	if(initialVertex == NULL)
	{
		//printf("\n E");
		addVertex(subnetGraph,ospfHdr->rid,0,0,ROUTER);// Add a vertex for the Router 0 Means Router
		initialVertex = getRouterById(subnetGraph, ospfHdr->rid);
		initialVertex->vrtx.seqNbrSeen = ntohs(ospfLsuHdr->seq);
	}
	//int n = ntohl(ospfLsuHdr->num_adv);
	//printf("\n F :: %d",n);
	int i=0;
	for(i=0;i<advNum;i++)
	{
		if(getNodeForSubnet(subnetGraph, ospfLsuArray[i].subnet, ospfHdr->rid) == NULL)
		{
			addVertex(subnetGraph, ospfHdr->rid, ospfLsuArray[i].subnet,ospfLsuArray[i].mask, SUBNET);// Add a vertex for subnet
		}
		src = getRouterById(subnetGraph, ospfHdr->rid);
		dst = getNodeForSubnet(subnetGraph, ospfLsuArray[i].subnet, ospfHdr->rid);

		if(!checkIfEdgeExists(subnetGraph, src->vrtx.id, dst->vrtx.id))
		{
			addEdge(src, dst, 1);
		}
		if(ospfLsuArray[i].rid != 0)
		{
			if(getRouterById(subnetGraph, ospfLsuArray[i].rid) == NULL)
			{
				addVertex(subnetGraph, ospfLsuArray[i].rid, 0, 0 , ROUTER);
			}
		}
		src = getRouterById(subnetGraph, ospfHdr->rid);
		dst = getRouterById(subnetGraph, ospfLsuArray[i].rid);

		if(src != NULL && dst != NULL)
		{
			// Add edge in both Directions
			if(!checkIfEdgeExists(subnetGraph, src->vrtx.id, dst->vrtx.id))
			{
				addEdge(src,dst, 1);
			}
			if(!checkIfEdgeExists(subnetGraph, dst->vrtx.id, src->vrtx.id))
			{
				addEdge(dst, src, 1);
			}
		}
		ospfLsuHdr->ttl--;
		struct sr_if* ifaceList = sr->if_list;
		while(ifaceList)
		{
			ethHdr = (struct sr_ethernet_hdr*)packet;
			for (int i = 0; i < ETHER_ADDR_LEN; i++)
			{
				ethHdr->ether_shost[i] = ifaceList->addr[i];
			}
			//int len = sizeof(struct ospfv2_hdr) + ( sizeof(struct ospfv2_lsu_hdr) * advNum);

			//ospfHdr->csum=htons(calculateCheckSumHelloPacket(packet + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip),len));

			if(strcmp(ifaceList->name, interface) != 0)
			{
				struct ospfv2_lsu_hdr* ospfLsuHdrTemp = (struct ospfv2_lsu_hdr*)(tempPacket + sizeof(struct ip)+ sizeof(struct ospfv2_hdr));
				long int abcd = ntohl(ospfLsuHdrTemp->num_adv);
				printf("\n ***Number Of Adv :: %d , Iface :: %s  ",abcd,ifaceList->name);
				sr_send_packet(sr, packet, len, ifaceList->name);
			}

			ifaceList = ifaceList->next;
		}

	}

	//printGraph(subnetGraph);
	return 0;

*/}
/*
 * Handle LSU Packets based on ALIVE Neighbours
 */
void* sendLSUPacket(void* arg)
{
	//printf("\n Creating LSU Packet\n");
	uint8_t* lsupacket;
	struct sr_instance* sr = (struct sr_instance*)arg;
	int packetLength=0,aliveInterFaces=0;
	// Packet Headers
	struct ip* ipHdr;
	struct sr_ethernet_hdr* ethHdr ;
	struct ospfv2_hdr* ospfHdr ;
	struct ospfv2_lsu_hdr* ospfLsuHdr ;
	struct ospfv2_lsu* ospfLsuArray;
	struct ospfv2_lsu*  ospfLsu ;
	struct sr_if *tempIface ;// Used while creating LSU per Interface

	while(1)
	{

		pthread_mutex_lock(&(sr->ospf_subsys->lock));
		struct sr_if *ifaceList = sr->if_list;
		// Check how many Infterfaces exist
		int i=0;
		aliveInterFaces =0;
		for(i=0;i<ifaceCount;i++)
		{
			if(neighbour[i].alive == 1)
			{
				aliveInterFaces++;
			}
		}
		// End
		//printf("\n %d interfaces are Alive",aliveInterFaces);


		while(ifaceList)
		{
			packetLength = sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) + sizeof(struct ospfv2_hdr) + sizeof(struct ospfv2_lsu_hdr) +
					(aliveInterFaces * sizeof(struct ospfv2_lsu));
			//printf("\nCreating Packet of Length %d",packetLength);
			lsupacket = (uint8_t*)malloc(packetLength);
			// Eth Frame
			ethHdr = (struct sr_ethernet_hdr*)lsupacket;
			for (int i = 0; i < ETHER_ADDR_LEN; i++)
			{
				ethHdr->ether_dhost[i] = multicastMac[i];
			}
			for (int i = 0; i < ETHER_ADDR_LEN; i++)
			{
				ethHdr->ether_shost[i] = ifaceList->addr[i];
			}
			ethHdr->ether_type = htons(ETHERTYPE_IP);
			// Eht Frame end
			// Ip Frame
			//printf("\n Eth Frame over");
			ipHdr = (struct ip*)(lsupacket + sizeof(struct sr_ethernet_hdr));
			ipHdr->ip_v = 4;
			ipHdr->ip_hl = 5;
			ipHdr->ip_tos = 0;
			ipHdr->ip_len = htons(packetLength - sizeof(struct sr_ethernet_hdr));

			struct timeval tv;
			gettimeofday(&tv, NULL);
			srand(tv.tv_sec * tv.tv_usec);
			ipHdr->ip_id = htons(0x2607);
			ipHdr->ip_off = htons(IP_DF);
			ipHdr->ip_ttl = 64;
			ipHdr->ip_p = IP_PROTO_OSPFv2;
			ipHdr->ip_sum = 0;
			ipHdr->ip_src.s_addr = ifaceList->ip;
			ipHdr->ip_dst.s_addr = htonl(OSPF_AllSPFRouters);
			setIPchecksum(ipHdr);
			// End Ip
			//printf("\n IP Frame over");
			ospfHdr = (struct ospfv2_hdr*)(lsupacket + sizeof(struct sr_ethernet_hdr)+ sizeof(struct ip));
			//OSPF Hdr Frame
			ospfHdr->version = OSPF_V2;
			ospfHdr->type = OSPF_TYPE_LSU;
			ospfHdr->len = htons(packetLength-sizeof(struct sr_ethernet_hdr)- sizeof(struct ip));
			ospfHdr->rid = routerIdDup;
			ospfHdr->aid = htonl(0); // Setting it to 0
			ospfHdr->csum = 0;
			ospfHdr->autype = 0;
			ospfHdr->audata = 0;
			// End OSPF Hdr Frame
			//printf("\n OSPF Frame over");
			ospfLsuHdr = (struct ospfv2_lsu_hdr*)(lsupacket + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) + sizeof(struct ospfv2_hdr));
			//OSPF LSU PACKET CONTENTS
			ospfLsuHdr->seq = htons(seqCountLsu);
			ospfLsuHdr->unused = 0;
			ospfLsuHdr->ttl = 64;
			ospfLsuHdr->num_adv = htonl(ifaceCount);
			// End
			//printf("\n OSPFLsu Frame over");
			// Create LSU Packet for each interface
			ospfLsuArray =  (struct ospfv2_lsu*)malloc(aliveInterFaces * sizeof(struct ospfv2_lsu));
			ospfLsu = (struct ospfv2_lsu*) (lsupacket + sizeof(struct sr_ethernet_hdr)+ sizeof(struct ip) + sizeof(struct ospfv2_hdr)+ sizeof(struct ospfv2_lsu_hdr));
			int count =0;
			tempIface = sr->if_list;
			while(tempIface)
			{
				uint32_t tempmask =tempIface->ip & tempIface->mask;
				/*printf("\n **Count :: %d , Subnet :: %s , Mask :: %s ",count,inet_ntoa(*(struct in_addr*)(&tempmask)),
						inet_ntoa(*(struct in_addr*)(&tempIface->mask)));*/
				ospfLsuArray[count].subnet = tempIface->ip & tempIface->mask;
				ospfLsuArray[count].mask = tempIface->mask;
				uint32_t tempRouterId = findRouterIdForNeighbourFromSubNet(ospfLsuArray[count].subnet);
				ospfLsuArray[count].rid = tempRouterId;
				if(tempRouterId == 0)
				{
					int k =findNeighborUsingRouterId(0);
					int index = neighbour[k].interFace[3] - (int)'0';
					if(index == 0) // If ETH 0
					{
						ospfLsuArray[count].subnet = 0;
						ospfLsuArray[count].mask = 0;
					}

				}
				/*printf("\n Count :: %d , Subnet :: %s , Mask :: %s ",count,inet_ntoa(*(struct in_addr*)(&ospfLsuArray[count].subnet)),
						inet_ntoa(*(struct in_addr*)(&ospfLsuArray[count].mask)));*/
				tempIface = tempIface->next;
				count++;
			}
			//printf("\n LSU Array Frame over %d elements",count);

			memcpy(ospfLsu, ospfLsuArray,	aliveInterFaces * sizeof(struct ospfv2_lsu));

			int len = sizeof(struct ospfv2_hdr) + ( sizeof(struct ospfv2_lsu_hdr) * aliveInterFaces);
			//ospfHdr->csum=htons(calculateCheckSumHelloPacket(lsupacket + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip),len));

			//printf("\n Sending LSU Packet size %d\n",packetLength);
			/*int j=0;
			for(j=0;j<packetLength;j++)
			{
				printf("%p ",lsupacket[j]);
			}
			printf("\n Done Printing packet");*/
			struct ospfv2_lsu_hdr* tempPkt = (struct ospfv2_lsu_hdr*)(lsupacket + sizeof(struct sr_ethernet_hdr)+ sizeof(struct ip)+sizeof(struct ospfv2_hdr));
			//printf("\n Nbr of ADV :: %d",htonl(tempPkt->num_adv));
			sr_send_packet(sr,lsupacket, packetLength,ifaceList->name);
			//sr_send_packet(sr, lsupacket, packetLength, ifaceList->name);
			ifaceList = ifaceList->next;
		}
	//	printf("\n Done with one set %d",seqCountLsu);
		pthread_mutex_unlock(&(sr->ospf_subsys->lock));
		sleep(OSPF_DEFAULT_LSUINT);
		seqCountLsu++;
	}
	return NULL;

}

int handleHelloPackets(struct sr_instance* sr,uint8_t * packet/* lent */,unsigned int len,char* interface/* lent */)
{
	//printf("\n Hello Packet Received");
	int val = interface[3]-(int)'0';
	struct timeval tm;
	uint8_t * tempPacket = (uint8_t*)(packet + sizeof(struct sr_ethernet_hdr));
	struct sr_if *iface;
	struct ip *ipHdr = (struct ip*)tempPacket;
	struct ospfv2_hdr *ospfHdr = (struct ospfv2_hdr*)(tempPacket+ sizeof(struct ip));
	struct ospfv2_hello_hdr *ospfHelloHdr = (struct ospfv2_hello_hdr*)(tempPacket + sizeof(struct ip)+ sizeof(struct ospfv2_hdr));
	struct sr_ethernet_hdr* ethr_hdr = get_ethernet_hdr(packet);
	gettimeofday(&tm, NULL);

	iface = sr_get_interface(sr, interface);
	if(ospfHelloHdr->nmask != iface->mask)
	{
		return 0;
	}
	else if(ntohs(ospfHelloHdr->helloint) != OSPF_DEFAULT_HELLOINT)
	{
	        return 0;
	}
	/*struct in_addr temp;
	temp.s_addr = ospfHdr->rid;*/
	//printf("\nNeighbour:%s",inet_ntoa(*(struct in_addr*)(&ipHdr->ip_src.s_addr)));//
//	printf("\nRouterId: %s",inet_ntoa(routerIdDup));//
	//inet_ntoa(*(struct in_addr*)(&ospfHdr->rid)));
	cacheMACForIp(ipHdr,ethr_hdr);
	if(neighbour[val].routerId == 0)
	{
		//printf("\n Entry Created");
		neighbour[val].routerId = ospfHdr->rid;
		neighbour[val].alive = 1;
		neighbour[val].timeStamp = tm.tv_sec;
		neighbour[val].ip = ipHdr->ip_src.s_addr;
		neighbour[val].netMask = ospfHelloHdr->nmask;
	}
	else
	{
		//printf("\n Entry Updated");
		// Just update last seen value as per specs
		neighbour[val].timeStamp = tm.tv_sec;
	}

	return 0;
}
/*
 * Send Hello Packets on ALl Interfaces
 */
void* sendHellos(void* arg)
{
	struct sr_instance* sr = (struct sr_instance*)arg;

	struct ip* ipHdr;
	struct sr_ethernet_hdr* ethHdr ;
	struct ospfv2_hello_hdr* ospfHelloHdr ;
	struct ospfv2_hdr* ospfHdr ;

	uint8_t* helloPacket;
	int packetLen = sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) + sizeof(struct ospfv2_hdr) + sizeof(struct ospfv2_hello_hdr);


	while(1)
	{

		pthread_mutex_lock(&(sr->ospf_subsys->lock));
		struct sr_if *ifaceList = sr->if_list;
		while(ifaceList)
		{
			helloPacket = ((uint8_t*)(malloc(packetLen)));
			// Eth Frame
			ethHdr = (struct sr_ethernet_hdr*)helloPacket;
			for (int i = 0; i < ETHER_ADDR_LEN; i++)
			{
				ethHdr->ether_dhost[i] = multicastMac[i];
			}
			for (int i = 0; i < ETHER_ADDR_LEN; i++)
			{
				ethHdr->ether_shost[i] = ifaceList->addr[i];
			}
			ethHdr->ether_type = htons(ETHERTYPE_IP);
			// Eht Frame end
			// Ip Frame
			ipHdr = (struct ip*)(helloPacket + sizeof(struct sr_ethernet_hdr));
			ipHdr->ip_v = 4;
			ipHdr->ip_hl = 5;
			ipHdr->ip_tos = 0;
			ipHdr->ip_len = htons(packetLen - sizeof(struct sr_ethernet_hdr));

			struct timeval tv;
			gettimeofday(&tv, NULL);
			srand(tv.tv_sec * tv.tv_usec);
			ipHdr->ip_id = htons(rand());
			ipHdr->ip_off = htons(IP_DF);
			ipHdr->ip_ttl = 64;
			ipHdr->ip_p = IP_PROTO_OSPFv2;
			ipHdr->ip_sum = 0;
			ipHdr->ip_src.s_addr = ifaceList->ip;
			ipHdr->ip_dst.s_addr = htonl(OSPF_AllSPFRouters);
			setIPchecksum(ipHdr);
			// End Ip
			//OSPF Hdr Frame
			ospfHdr = (struct ospfv2_hdr*)(helloPacket + sizeof(struct sr_ethernet_hdr)+ sizeof(struct ip));
			ospfHdr->version = OSPF_V2;
			ospfHdr->type = OSPF_TYPE_HELLO;
			ospfHdr->len = htons(packetLen - sizeof(struct sr_ethernet_hdr) - sizeof(struct ip));
			ospfHdr->rid = routerIdDup;
			ospfHdr->aid = htonl(0); // Setting it to 0
			ospfHdr->csum = 0;
			ospfHdr->autype = 0;
			ospfHdr->audata = 0;
			// End OSPF Hdr Frame
			// Hello Packet
			ospfHelloHdr = (struct ospfv2_hello_hdr*)(helloPacket + sizeof(struct sr_ethernet_hdr)+ sizeof(struct ip)+ sizeof(struct ospfv2_hdr));
			ospfHelloHdr->nmask = ifaceList->mask;//htonl(0xfffffffe);
			ospfHelloHdr->helloint = htons(OSPF_DEFAULT_HELLOINT);
			ospfHelloHdr->padding = 0;
			// ENd Hello Packet
			// End
			((struct ospfv2_hdr*)(helloPacket + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip)))->csum =
					htons(calculateCheckSumHelloPacket(helloPacket + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip), sizeof(struct ospfv2_hdr) + sizeof(struct ospfv2_hello_hdr)));

			//printf("\n Sending HELLO Packet of length = %d", packet_len, ifaceList->name);
			sr_send_packet(sr, ((uint8_t*)(helloPacket)), packetLen,ifaceList->name);
			ifaceList = ifaceList->next;
		}
		pthread_mutex_unlock(&(sr->ospf_subsys->lock));
		sleep(OSPF_DEFAULT_HELLOINT);
	}

	return NULL;
}
uint32_t findRouterIdForNeighbourFromSubNet(uint32_t subnet)
{

    int i = 0;
    uint32_t val;
    for(i = 0; i < ifaceCount;i++)
    {
    	val = neighbour[i].ip & neighbour[i].netMask;
        if(val == subnet)
        {
            if(neighbour[i].alive == 1)
            {
                return neighbour[i].routerId;
            }
        }
    }
    return 0;


}

void printGraph(struct graph *subnetGraph)
{
    struct vertexList *i;

    for(i = subnetGraph->vrtxList; i; i = i->next)
    {
        struct adjacencyList *cur = i->vrtx.head;
        printf("\nAdjacency list of vertex %d rid %s ", i->vrtx.id
                    ,inet_ntoa(*(struct in_addr*)&i->vrtx.rid));
        /*printf("snet %s type %d d=%d parent=%d root "*/
        printf("snet %s type %d d=%d parent=%d root "
                            ,inet_ntoa(*(struct in_addr*)&i->vrtx.subnet)
                            ,i->vrtx.type
                           ,i->vrtx.d
                            ,i->vrtx.parent);
        printf("snet %s type %d \nroot "
                    ,inet_ntoa(*(struct in_addr*)&i->vrtx.subnet)
                    ,i->vrtx.type);
        while (cur)
        {
            printf("-> %d", cur->edg.dst);
            cur = cur->next;
        }
        printf("\n");
    }
}

int findNeighborUsingRouterId(uint32_t routId)
{
    int i = 0;
    for(i = 0; i < ifaceCount; i++)
    {
        if(neighbour[i].routerId == routId)
        {
            if(neighbour[i].alive == 1)
            {
                return i;
            }
        }
    }
    return 0;
}
void populateRoutingTable(struct graph* graphTmp, struct sr_instance *sr)
{
    struct in_addr dest_addr;
    struct in_addr gw_addr;
    struct in_addr mask_addr;

    struct vertexList *walker = graphTmp->vrtxList, *u;
    struct sr_rt *cur = sr->routing_table;

    int idx, flag = 10, i;
    while(walker)
    {
        if(walker->vrtx.type == SUBNET)
        {
            u = getVertexFromEdge(walker->vrtx.parent);
            if(u != NULL)
            {
                idx = findNeighborUsingRouterId(u->vrtx.rid);
                dest_addr.s_addr = walker->vrtx.subnet;
                gw_addr.s_addr = neighbour[idx].ip;
                mask_addr.s_addr = walker->vrtx.networkMask;

                cur = sr->routing_table;
                while(cur)
                {
                    if(cur->dest.s_addr == walker->vrtx.subnet)
                    {
                        flag = 0;
                    }
                    cur = cur->next;
                }
                if(flag)
                {
                	sr_add_rt_entry(sr,dest_addr,gw_addr,mask_addr,neighbour[idx].interFace);

                }
                else
                {
                	flag  = 10;
                }
            }
        }

        walker = walker->next;
    }

}
