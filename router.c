// Kacper Solecki, 316720
#include "router.h"
#include <sys/time.h>

/* Returns time since Epoch in miliseconds */
timestamp_t get_current_timestamp(){
    struct timeval tv;
    if(gettimeofday(&tv, NULL) < 0){
        fprintf(stderr, "gettimeofday error: %s\n", strerror(errno)); 
		exit(EXIT_FAILURE);
    }
    return (timestamp_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/* Obtains network address from address of some computer in it */
in_addr_t trim_address(in_addr_t addr, int8_t mask){
    in_addr_t network_addr = ntohl(addr);
    int shift = IP_SIZE - mask;
    network_addr = ((network_addr >> shift) << shift);
    return htonl(network_addr);
}

void init(network_vector_t *dist_vector){
    int n;
    scanf("%d", &n);

    for(int i = 0; i < n; i++){
        char input_addr[20];
        int distance;
        network_t *network = malloc(sizeof(network_t));
        struct in_addr *direct_conn = malloc(sizeof(struct in_addr));
        scanf(" %s", input_addr);

        /* extract network mask */
        char *network_mask = strchr(input_addr, '/') + 1;
        network->mask = atoi(network_mask);

        /* extract ip address */
        *(network_mask - 1) = '\0';
        if(inet_pton(AF_INET, input_addr, direct_conn) < 0){
            fprintf(stderr, "inet_pton error: %s\n", strerror(errno)); 
            exit(EXIT_FAILURE);
        }
        in_addr_t network_addr = trim_address(direct_conn->s_addr,
                                              network->mask);
        network->address.ADDR = network_addr;

        /* fill other fields */
        scanf(" %s %d", input_addr, &distance);
        network->distance = distance;
        network->original_distance = distance;
        network->direct_conn = direct_conn;
        network->last_responded = 0;
        network->address.sin_port = htons(54321);
        network->address.sin_family = AF_INET;

        /* add address to distance vector */
        TAILQ_INSERT_TAIL(dist_vector, network, vec_handle);
    }
}

/* Prints the distance vector */
void print_vector(network_vector_t *dist_vector){
    network_t *network;
    char addr_buffer[20]; 
    TAILQ_FOREACH(network, dist_vector, vec_handle){
        if(!inet_ntop(AF_INET, &(network->address.sin_addr), addr_buffer, 20)){
            fprintf(stderr, "inet_ntop error: %s\n", strerror(errno)); 
            exit(EXIT_FAILURE);
        }
        printf("%s/%u ", addr_buffer, network->mask);
        if(network->distance < INF)
            printf("distance %u ", network->distance);
        else
            printf("unreachable ");
        debug("(last responded: %d) ", network->last_responded);
        if(network->direct_conn)
            printf("connected directly\n");
        else if(network->distance < INF){
            if(!inet_ntop(AF_INET, &(network->conn_via.sin_addr), addr_buffer, 20)){
                fprintf(stderr, "inet_ntop error: %s\n", strerror(errno)); 
                exit(EXIT_FAILURE);
            }  
            printf("via %s\n", addr_buffer);
        }
        else
            printf("\n");
    }
    printf("\n");
}

/* Sends a single datagram with given message to recepient by sockfd.
 * Returns 0 if successful, -1 when something's gone wrong
 */
int send_datagram(int sockfd, char *message, network_t recepient){
    /* convert address to broadcast */
    in_addr_t broadcast_addr = ntohl(recepient.address.ADDR);
    broadcast_addr |= (__UINT32_MAX__ >> recepient.mask);
    recepient.address.ADDR = htonl(broadcast_addr);

    if(sendto(sockfd,
              message,
              DATAGRAM_SIZE,
              0,
              (struct sockaddr*)&recepient.address,
              sizeof(recepient.address)) != DATAGRAM_SIZE){
        /* 101 = network unreachable */
        if(errno == 101)
            return -1;
        fprintf(stderr, "sendto error: %s\n", strerror(errno)); 
        exit(EXIT_FAILURE);	
    }
    return 0;
}

/* Sets infinite distances to networks reached via given network */
void consider_broken(network_t *network, network_vector_t *dist_vector){
    network_t *distant_network;
    network->last_responded = max(network->last_responded, NETWORK_TIMEOUT);
    TAILQ_FOREACH(distant_network, dist_vector, vec_handle){
        if(!distant_network->direct_conn
                && distant_network->conn_via.ADDR
                == network->address.ADDR){
            distant_network->distance = INF;
            distant_network->last_responded = network->last_responded;
        }
    }
}

/* Sends distance vector to all neighbours */
void send_vector(int sockfd, network_vector_t *dist_vector){
    network_t *recepient, *network;
    char message[DATAGRAM_SIZE + 1];
    TAILQ_FOREACH(recepient, dist_vector, vec_handle){
        /* directly connected networks are at the beggining of vector */
        if(!recepient->direct_conn)
            break;
        TAILQ_FOREACH(network, dist_vector, vec_handle){
            /* don't send stale unreachable networks */
            if(network->last_responded >= NETWORK_STALE
                    && network->distance == INF)
                continue;
            *(uint32_t*)message = network->address.ADDR;
            *(message + 4) = network->mask;
            if(network->distance == INF)
                *(uint32_t*)(message + 5) = __UINT32_MAX__;
            else
                *(uint32_t*)(message + 5) = htonl(network->distance);
            if(send_datagram(sockfd, message, *recepient) < 0){
                recepient->distance = INF;
                consider_broken(recepient, dist_vector);
            }
            else
                recepient->distance = min(recepient->distance,
                                          recepient->original_distance);
        }
    }
}

/* Recieves a single datagram with distance vector entry 
 * Returns 1 when successful, 0 when there aren't any to recieve
 */
int recieve_datagram(int sockfd, u_int8_t *buffer, struct sockaddr_in *sender){
    socklen_t sender_len = sizeof(*sender);
    ssize_t packet_len = recvfrom(
            sockfd,
            buffer,
            IP_MAXPACKET,
            MSG_DONTWAIT,
            (struct sockaddr*)sender,
            &sender_len
    );
    if (packet_len < 0) {
        if(errno == EWOULDBLOCK)
            return 0;
        fprintf(stderr, "recvfrom error: %s\n", strerror(errno)); 
        exit(EXIT_FAILURE);
    }
    return 1;
}

/* Checks if address is in given network */
int addr_in_network(struct sockaddr_in *addr, network_t *network){
    return (ntohl(network->address.ADDR) >> network->mask)
            == (ntohl(addr->ADDR) >> network->mask);
}

/* Removes networks that are indirect bypasses to directly connected
 * network whose connection has just been reestablished
 */
void remove_bypasses(network_t *network, network_vector_t *dist_vector){
    network_t *bypass = TAILQ_FIRST(dist_vector), *next;
    while(bypass){
        next = TAILQ_NEXT(bypass, vec_handle);
        if(bypass->direct_conn){
            bypass = next;
            continue;
        }
        if(bypass->address.ADDR == network->address.ADDR
                && bypass->mask == network->mask){
            TAILQ_REMOVE(dist_vector, bypass, vec_handle);
        }
        bypass = next;
    }
}

/* Handles message from neighbour with infinite distance */
void handle_inf_message(in_addr_t addr, in_addr_t sender_addr, network_vector_t *dist_vector){
    network_t *network = TAILQ_FIRST(dist_vector), *next;
    while(network){
        next = TAILQ_NEXT(network, vec_handle);
        if(network->direct_conn){
            network = next;
            continue;
        }
        if(addr == network->address.ADDR
                && network->conn_via.ADDR == sender_addr){
            network->distance = INF;
            break;
        }
        network = next;
    }
}

/* Recieves all available distance vector entries
 * and updates the vector
 */
void recieve_vectors(int sockfd, network_vector_t *dist_vector){
    uint8_t buffer[DATAGRAM_SIZE];
    struct sockaddr_in sender;
    network_t *network;

    while(recieve_datagram(sockfd, buffer, &sender)){
        in_addr_t addr = *((uint32_t*)buffer);
        uint8_t mask = buffer[4];
        uint32_t distance = min(INF, ntohl(*((uint32_t*)(buffer + 5))));
        if(distance == INF){
            handle_inf_message(addr, trim_address(sender.ADDR, mask), dist_vector);
            continue;
        }
        int network_found = 0;
        TAILQ_FOREACH(network, dist_vector, vec_handle){
            /* search for sender's network */
            if(addr_in_network(&sender, network)){
                if(sender.ADDR == network->direct_conn->s_addr){
                    /* it's our own message... */
                    network->last_responded = 0;
                    break;
                }
                if(addr == network->address.ADDR
                            && mask == network->mask){
                    /* sender sent message with network linking us */
                    network->distance = min(network->distance, distance);
                    remove_bypasses(network, dist_vector);
                    network_found = 1;
                }
                else{
                    /* we have to add distance to sender */
                    distance = min(INF, distance + network->distance);
                    sender = network->address;
                }
                network->last_responded = 0;
                break;
            }
        }
        /* do I already know this network? */
        TAILQ_FOREACH(network, dist_vector, vec_handle){
            if(network_found)
                break;
            if(addr == network->address.ADDR
                            && mask == network->mask){
                if(distance < network->distance){
                    if(network->direct_conn && network->distance == INF){
                        /* indirect path to directly connected & unreachable network
                           => need to create new network object (bypass) */
                        continue;
                    }
                    /* if new distance is better update distance vector */
                    network->distance = distance;
                    network->conn_via = sender;
                }
                if(network->conn_via.ADDR == sender.ADDR
                        && distance == network->distance){
                    network->last_responded = 0;
                }
                network_found = 1;
            }
        }
        /* new network - add it to the vector */
        if(!network_found){
            struct sockaddr_in new_address;
            new_address.ADDR = addr;
            new_address.sin_port = htons(54321);
            new_address.sin_family = AF_INET;
            network_t *new_network = malloc(sizeof(network_t));
            new_network->address = new_address;
            new_network->conn_via = sender;
            new_network->distance = distance;
            new_network->mask = mask;
            new_network->last_responded = 0;
            new_network->direct_conn = NULL;
            new_network->original_distance = 0;
            TAILQ_INSERT_TAIL(dist_vector, new_network, vec_handle);
        }
    }           
}

/* Recognises obsolete networks not sending messages for a long time */
void check_networks(network_vector_t *dist_vector){
    network_t *network = TAILQ_FIRST(dist_vector), *next;
    while(network){
        next = TAILQ_NEXT(network, vec_handle);
        network->last_responded = min(network->last_responded + 1, NETWORK_STALE);
        if(network->last_responded >= NETWORK_STALE
                    && !network->direct_conn){
            /* remove stale, not directly connected networks */
            TAILQ_REMOVE(dist_vector, network, vec_handle);
            free(network);
        }
        else if(network->last_responded >= NETWORK_TIMEOUT){
            if(network->direct_conn)
                consider_broken(network, dist_vector);
            else
                network->distance = INF;
        }
        network = next;
    }
}
