/*
 * Skeleton-code behorende bij het college Netwerken, opleiding Informatica,
 * Universiteit Leiden.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <netinet/in.h>

#include <math.h>

#include "asp.h"

/* An asp socket descriptor for information about the sockets current state */
struct asp_socket_info {
    int sockfd;

    struct sockaddr_in local_addr;
    socklen_t local_addrlen;

    struct sockaddr_in remote_addr;
    socklen_t remote_addrlen;

    struct asp_socket_info *next;

    int current_quality_level;
    int sequence_count;

    int packets_received;
    int packets_missing;

    struct timeval last_packet_received;
    struct timeval last_problem;
    struct timeval timeout;

    unsigned int is_connected : 1;
    unsigned int has_accepted : 1;
};

struct asp_socket_info sock;

/*
create a socket
*/
void create_socket() 
{   
    if ((sock.sockfd = socket(AF_INET,SOCK_DGRAM,0)) < 0){
        perror ("socket creation failed");
        exit(EXIT_FAILURE);
    }
    printf("socket created\n");
}

/*
sets server information
*/
void server_information()
{
    memset(&sock.local_addr, 0, sizeof(sock.local_addr)); 
    memset(&sock.remote_addr, 0, sizeof(sock.remote_addr)); 

    sock.local_addr.sin_family =AF_INET;
    sock.local_addr.sin_addr.s_addr =INADDR_ANY;
    sock.local_addr.sin_port = htons(1235);
}

/*
sets client information
*/
void client_information(int bind)
{
    memset(&sock.local_addr, 0, sizeof(sock.local_addr)); 
      
    sock.local_addr.sin_family = AF_INET; 
    sock.local_addr.sin_port = htons(bind); 
    sock.local_addr.sin_addr.s_addr = INADDR_ANY; 
      
}

/*
binds with socekt
*/
void bind_socket()
{
    if ( bind(sock.sockfd, (const struct sockaddr *)&sock.local_addr,  
            sizeof(sock.local_addr)) < 0 ) 
    { 
        perror("bind failed"); 
        exit(EXIT_FAILURE); 
    } 
    /*sock.has_accepted = accept(sock.sockfd, NULL, NULL);
    if (sock.has_accepted != -1) 
    {
        perror("Sorry, this server is too busy.Try again later!\r\n");
        close(sock.has_accepted);
    }*/
    printf("bind completed\n");
}

/*
receives string form client with requist for music
*/
void receive_from()
{
    int n; 
    char buffer[1024]; 

  
    sock.remote_addrlen = sizeof(sock.remote_addr);  //len is value/resuslt 
  
    n = recvfrom(sock.sockfd, (char *)buffer, 1024,  
                MSG_WAITALL, ( struct sockaddr *) &sock.remote_addr, 
                &sock.remote_addrlen); 
    buffer[n] = '\0'; 
}

/*
sends packets to client with music data
*/
int send_to(uint8_t* packet, size_t size)
{   
    int len = sizeof(sock.remote_addr);
    int sender;
 
    sender = sendto(sock.sockfd, packet, size , 0, (const struct sockaddr *) &sock.remote_addr, len);

    return sender; 
}

/*
sends string to server with requist for music
*/
void send_to_c (char* send)
{
    char* hello = send;
    sendto(sock.sockfd, (const char *)hello, strlen(hello), 
        MSG_CONFIRM, (const struct sockaddr *) &sock.local_addr,  
            sizeof(sock.local_addr)); 
}

/*
receives packets with music data from server
*/
int receive_from_c(uint8_t* receive_packet)
{
    int recv_len;

    sock.local_addrlen = sizeof(sock.local_addr);
    recv_len = recvfrom(sock.sockfd, receive_packet, PACKET_SIZE, 0, (struct sockaddr *) &sock.local_addr, &sock.local_addrlen);
    return recv_len;
}

/*
computes checksum over packtes
*/
int compute_checksum(uint8_t* samples, int nSamples, int sample_size)
{
    int sum = 0;
    for (int i = 0; i < nSamples*sample_size; i += sample_size)
    {
        sum += (samples[i] & 0xFFFF0000);
    }
    return (sum & 0xFFFF0000);
}

/*
sends ack with packet number and postivive or non-positive to server
*/
void send_ack(void *ack, size_t size)
{
    sock.local_addrlen = sizeof(sock.local_addr);
    sendto(sock.sockfd, ack, size, 0, (const struct sockaddr *) &sock.local_addr, sock.local_addrlen);
}

/*
receive ack with packet number and postivie or non-postive from client
*/
void receive_ack(void *ack, size_t size)

{
    sock.timeout.tv_sec = 2;
    sock.timeout.tv_usec = 0;
    setsockopt(sock.sockfd, SOL_SOCKET, SO_RCVTIMEO, &sock.timeout, sizeof(struct timeval));
    setsockopt(sock.sockfd, SOL_SOCKET, SO_SNDTIMEO, &sock.timeout, sizeof(struct timeval));
    sock.remote_addrlen = sizeof(sock.remote_addr);
    recvfrom(sock.sockfd, ack, size, 0, (struct sockaddr *) &sock.remote_addr, &sock.remote_addrlen);
}

/*
sends buffer with window information to client
*/
int send_window(void *window)
{
    int len;
    sock.remote_addrlen = sizeof(sock.remote_addr);
    len = sendto(sock.sockfd, window, W_PACKET_SIZE , 0, (const struct sockaddr *) &sock.remote_addr, sock.remote_addrlen);
    return len;
}

/*
client receives buffer with window information from server.
*/
int receive_window(void *window)
{
    int len;
    sock.local_addrlen = sizeof(sock.local_addr);
    len = recvfrom(sock.sockfd, window, W_PACKET_SIZE, 0, (struct sockaddr *) &sock.local_addr, &sock.local_addrlen);
    return len;
}

/*
fills the ack buffer.
*/
void create_ack(int packet_number,int positive, void *ack)
{
    memcpy(ack,&packet_number,sizeof(int));
    memcpy((char*)ack+sizeof(int),&positive,sizeof(int));
}

/*
checks if checksum is correct from received packet.
*/
bool check_ack(char *ack)
{
  uint8_t *data = malloc(ack[1]*ack[2]);
  memcpy(data, ack+1, ack[2]);
  return (ack[ACK_SIZE-1] != compute_checksum(data, ack[2], ack[1]));
}

/*

*/
void close_socket ()
{
    close(sock.sockfd); 
}

