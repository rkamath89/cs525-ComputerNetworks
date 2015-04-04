SimpleRouter
==============================
This project implements a fully functional IP router that routes real network traffic.

Goal of the Project

The aim of the project is to implement the following basic router functionalities. The router is declared to be functioning correctly if and only if:

The router can successfully route packets between the firewall and the application servers.
The router correctly handles ARP requests and replies.
The router correctly handles traceroutes through it (where it is not the end host) and to it (where it is the end host).
The router responds correctly to ICMP echo requests.
The router handles TCP/UDP packets sent to one of its interfaces. In this case the router should respond with an ICMP port unreachable.
The router maintains an ARP cache whose entries are invalidated after a timeout period (timeouts should be on the order of 15 seconds).
The router queues all packets waiting for outstanding ARP replies. If a host does not respond to 5 ARP requests, the queued packet is dropped and an ICMP host unreachable message is sent back to the source of the queued packet.
The router does not needlessly drop packets (for example when waiting for an ARP reply).

PWOSPF (Pee-Wee OSPF)
This project involves building advanced functionality on top of the SimpleRouter project. The goal is to develop a simple dynamic routing protocol, PWOSPF, so that your router can generate its forwarding table automatically based on routes advertised by other routers on the network. By the end of this project, your router is expected to be able to build its forwarding table from link-state advertisements sent from other routers, and route traffic through complex topologies containing multiple nodes.

================================================================================================
PHASE-2
================================================================================================
PWOSPF

The routing protocol you will be implementing is a link state protocol that is loosely based on OSPFv2. Note that while PWOSPF is based on OSPFv2 it is sufficiently different that referring to the OSPFv2 as a reference will not be of much help and contrarily may confuse or mislead you.

The task is to implement PWOSPF within your existing router so that your router will be able to do the following:

build the correct forwarding tables on the assignment topology.
detect when routers join/or leave the topology and correct the forwarding tables correctly.
inter-operate with a third party reference solution that implements pwosp.
Running Multiple Routers

Since this project requires multiple instances of your router to run simultaneously you will want to use the -r and the -v command line options. -r allows you to specify the routing table file you want to use (e.g. -r rtable.vhost1) and -v allows you to specify the host you want to connect to on the topology (e.g. -v vhost3). Connecting to vhost3 on topology 300 should look something like:

./sr -t 300 -v vhost3 -r rtable.vhost3
For more information check Stanford Virtual Network System.
