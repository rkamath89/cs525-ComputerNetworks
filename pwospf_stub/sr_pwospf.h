/*-----------------------------------------------------------------------------
 * file:  sr_pwospf.h
 * date:  Tue Nov 23 23:21:22 PST 2004 
 * Author: Martin Casado
 *
 * Description:
 *
 *---------------------------------------------------------------------------*/

#ifndef SR_PWOSPF_H
#define SR_PWOSPF_H

#include <pthread.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <stdio.h>
/* forward declare */
struct sr_instance;

struct sr_neighbours
{

	int alive;
	char *interFace;
	uint32_t ip;
	uint32_t routerId;
	uint32_t netMask;
	double timeStamp;
};
struct vertex
{
	uint32_t subnet;
	uint32_t rid;
	uint32_t id;
	uint32_t networkMask;
	int type;
	int visited;
	int d, parent;
	int seqNbrSeen;
	struct adjacencyList *head;
};
struct vertexList
{
	struct vertex vrtx;
	struct vertexList *next;
};

struct graph
{
	int val;
	struct vertexList *vrtxList;
};
struct edge
{
	int src;
	int dst;
	int alive;
	int weight;// Alwys 1 in this case
};
struct adjacencyList
{
	struct edge edg;
	struct adjacencyList *next;
};
struct path
{
	struct vertex vrtx;
	struct path *next;
};
struct pwospf_subsys
{
    /* -- pwospf subsystem state variables here -- */


    /* -- thread and single lock for pwospf subsystem -- */
    pthread_t thread;
    pthread_mutex_t lock;
};

int pwospf_init(struct sr_instance* sr);
void* sendHellos(void* arg);
struct graph* initializeGraph();
void addVertex(struct graph *subnetGraph, uint32_t rtrId, uint32_t subnet, uint32_t networkMask, int type);
void initializeRouter(struct graph *subnet);
void initializeNeighbor(int val);
void addValue(struct path *path1, struct vertexList *vertex1);
struct vertexList* getRouterById(struct graph *subnetGraph, uint32_t rid);
void addInterfaceVertices(struct graph *subnet, struct sr_instance *sr);
int handleHelloPackets(struct sr_instance* sr,uint8_t * packet/* lent */,unsigned int len,char* interface/* lent */);
int handleLsuPacket(struct sr_instance* sr,uint8_t * packet/* lent */,unsigned int len,char* interface/* lent */);
uint32_t findRouterIdForNeighbourFromSubNet(uint32_t subnet);
void* sendLSUPacket(void* arg);
struct adjacencyList* checkIfEdgeExists(struct graph* subnetGraph, int src, int dst);
struct vertexList* getNodeForSubnet(struct graph *subnetGraph, uint32_t subnet, uint32_t rtrId);
int findNeighborUsingRouterId(uint32_t routId);
struct vertexList* getVertexFromEdge(int val);
void computeValues(struct vertexList *vrtx1, struct vertexList *vrtx2, int vrtx3);
void addEdge(struct vertexList *vrtex1, struct vertexList *vrtex2, int weight);
void dijkstra(struct graph* graphVal, struct vertexList *vertexlst);
void initializeDijkstras(struct graph *subntgraph, struct vertexList *vlist);
struct vertexList* getMinVertex(struct vertexList *vlist);
#endif /* SR_PWOSPF_H */
