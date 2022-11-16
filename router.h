// Kacper Solecki, 316720
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/queue.h>

#ifndef ROUTER_H_
#define ROUTER_H_

#ifndef min
#define min(a, b)                                                              \
  ({                                                                           \
    __typeof__(a) _a = (a);                                                    \
    __typeof__(b) _b = (b);                                                    \
    _a < _b ? _a : _b;                                                         \
  })
#endif

#ifndef max
#define max(a, b)                                                              \
  ({                                                                           \
    __typeof__(a) _a = (a);                                                    \
    __typeof__(b) _b = (b);                                                    \
    _a > _b ? _a : _b;                                                         \
  })
#endif

#ifdef DEBUG
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...)
#endif

#define TURN 1500          /* turn length in miliseconds */
#define NETWORK_TIMEOUT 4   /* time in turns of not responding before setting distance to inf */
#define NETWORK_STALE 6     /* time in turns of not responding before removing network */
#define INF 16U
#define DATAGRAM_SIZE 9
#define IP_SIZE 32

#define ADDR sin_addr.s_addr

typedef struct network{
    TAILQ_ENTRY(network) vec_handle;    /* distance vector handle */
    struct sockaddr_in address;         /* address of network */
    struct sockaddr_in conn_via;        /* address of first router on path */
    struct in_addr *direct_conn;        /* own ip address (NULL if not directly connected) */
    uint32_t distance;                  /* distance to network */
    uint32_t original_distance;         /* for directly connected - initial distance to network */
    uint8_t mask;                       /* network mask */
    uint8_t last_responded;             /* time in turns since last got message from network */
} network_t;

typedef TAILQ_HEAD(network_vector, network) network_vector_t;
typedef long long timestamp_t;

timestamp_t get_current_timestamp();
in_addr_t trim_address(in_addr_t addr, int8_t mask);
void init(network_vector_t *dist_vector);
void print_vector(network_vector_t *dist_vector);
int send_datagram(int sockfd, char *message, network_t recepient);
void consider_broken(network_t *network, network_vector_t *dist_vector);
void send_vector(int sockfd, network_vector_t *dist_vector);
int recieve_datagram(int sockfd, u_int8_t *buffer, struct sockaddr_in *sender);
int addr_in_network(struct sockaddr_in *addr, network_t *network);
void remove_bypasses(network_t *network, network_vector_t *dist_vector);
void handle_inf_message(in_addr_t addr, in_addr_t sender_addr, network_vector_t *dist_vector);
void recieve_vectors(int sockfd, network_vector_t *dist_vector);
void check_networks(network_vector_t *dist_vector);

#endif