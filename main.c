// Kacper Solecki, 316720
#include "router.h"
#include <poll.h>

int main(){
    /* create & bind socket */
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in address;
    bzero (&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(54321);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind (sockfd, (struct sockaddr*)&address, sizeof(address)) < 0){
		fprintf(stderr, "bind error: %s\n", strerror(errno)); 
		return EXIT_FAILURE;
	}
    /* allow sending to broadcast address */
    int broadcastPermission = 1;
    if(setsockopt (sockfd,
                SOL_SOCKET,
                SO_BROADCAST,
                (void *)&broadcastPermission,
                sizeof(broadcastPermission)) < 0){
        fprintf(stderr, "setsockopt error: %s\n", strerror(errno)); 
		return EXIT_FAILURE;
    }

    /* initialize distance vector */
    network_vector_t dist_vector;
    TAILQ_INIT(&dist_vector);
    init(&dist_vector);

    /* main loop */
    while(1){
        print_vector(&dist_vector);
        send_vector(sockfd, &dist_vector);

        /* recieve vectors from others */
        struct pollfd pfd;
        pfd.fd = sockfd;
        pfd.events = POLLIN;
        timestamp_t starttime = get_current_timestamp();
        timestamp_t time_past = 0;
        while(time_past < TURN){
            pfd.revents = 0;
            if(poll(&pfd, 1, TURN - time_past) < 0){
                fprintf(stderr, "poll error: %s\n", strerror(errno)); 
		        return EXIT_FAILURE;
            }
            if(pfd.revents & POLLIN)
                recieve_vectors(sockfd, &dist_vector);
            timestamp_t currtime = get_current_timestamp();
            time_past = currtime - starttime;
        }
        check_networks(&dist_vector);
    }
}