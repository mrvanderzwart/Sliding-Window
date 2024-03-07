/*
 * Skeleton-code behorende bij het college Netwerken, opleiding Informatica,
 * Universiteit Leiden.
 */

/* TODO: Add function declarations for the public functions of your ASP implementation */
#ifndef ASP_H
#define ASP_H
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>


#define MAX_DATA_SIZE	65507 
#define PACKET_SIZE 32000
#define W_PACKET_SIZE 3*sizeof(int)

#pragma pack(1)

struct header
{
    int number;
    int sample_size;
    int nSamples;
    int checksum;
};

#pragma pack(0)
time_t *window_sent_time;
time_t TMIN;
//bool *window_ack_status, *frame_sent;
//int lar, lfs, window_len, 
int ACK_SIZE;


void create_socket();
void server_information();
void bind_socket();
void receive_from();
int send_to (uint8_t *packet, size_t size);
void client_information(int bind);
void send_to_c(char* send);
int receive_from_c(uint8_t* receive_packet);
int compute_checksum(uint8_t* samples, int nSamples, int sample_size);
void create_ack(int packet_number, int positive, void *ack);
void send_ack(void *ack, size_t size);
int send_window( void *window);
int receive_window(void *window);
bool check_ack(char *ack);
void receive_ack(void* ack,size_t size);
void close_socket ();

#endif

