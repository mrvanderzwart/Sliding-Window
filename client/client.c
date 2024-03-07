/*
 * Skeleton-code behorende bij het college Netwerken, opleiding Informatica,
 * Universiteit Leiden.
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>

#include <alsa/asoundlib.h>
#include "../communication/asp/asp.h"
#include "client/musicplayer/player.h"

#define BIND_PORT 1235

int expected_packet =0;

/*
struct for receiving window
*/
struct window
{
    int begin;
    int end;
    int len;
    int largest;
    int LAP;
    int LPR;
};

/*
conection stetup with the server and receives window information 
and sends ack back.
*/
void set_up(struct window *win, void *ack)
{
    create_socket();
    client_information(BIND_PORT);
    send_to_c("music");
    void *window = malloc(W_PACKET_SIZE);
    while (receive_window(window) < 0)
    {
        create_ack(-1,0,ack);
        send_ack(ack,sizeof(int)+sizeof(int));
        memset(ack,0,sizeof(int)+sizeof(int));
    }
    create_ack(-1,1,ack);
    send_ack(ack, sizeof(int)+sizeof(int));
    memset(ack,0,sizeof(int)+sizeof(int));
    memcpy(&win->begin,window,sizeof(win->begin));
    memcpy(&win->len, (char*)window+sizeof(win->begin),sizeof(win->len));
    memcpy(&win->largest,(char*)window+sizeof(win->begin)+sizeof(win->len),sizeof(win->largest));
    win->end = win -> len -1;
    win->LAP = win ->end;
    free(window);
}

/*
update window when packet is received correctly.
*/
void update_window(struct window *win)
{
    win->begin = win->LPR+1;
    if (win -> LAP > win -> largest)
        win -> end = win -> largest;
    else 
        win -> end = win->LAP;
}

/*
receives packets from server checks if packet is in window and 
when checksum correct en inorder then positive else positive is false.
if last packet received correct then done is true.
*/
void receive_packet (uint8_t *data ,struct header *head, struct window *win, bool *done, int *positive)
{
    uint8_t* packet = malloc(PACKET_SIZE);
    receive_from_c(packet);
    
    memcpy(head, packet,sizeof(*head));
    memset(data,0,PACKET_SIZE-sizeof(*head));

    if(head->number >= win->begin && head->number <= win->end )
    {
        
        memcpy(data, packet+sizeof(*head), head->sample_size*head->nSamples);

        if(head->number == expected_packet && head->checksum == compute_checksum(data, head->nSamples, head->sample_size))
        {
            win->LPR = head->number;
            win->LAP = win->LAP+1;
            *positive = 1;
            expected_packet++;
            if (expected_packet > win->largest-1)
                *done = true;
            else
                *done = false;
        }
        else
        {
            *positive = 0;
            *done = false;
        }
    }
    free(packet);
}

/*
checks the remaining size of the receive buffer.
*/
bool remaining_size (struct header head, int offset, int buffer_size)
{
    if (head.nSamples*head.sample_size + offset > buffer_size)
    {
        return false;
    }
    return true;
}

/*
fills the receivebuffer inorder with packet data and when buffer is full 
or last packet is received then sends data to audio player 
this is done until last packet correct received.
if last packet received the socket will be closed.
*/
void play_music(player_t *player, uint8_t *recvbuffer, int buffer_size)
{
    struct header head;
    struct window win;
    uint8_t* data = malloc (PACKET_SIZE - sizeof(head));
    bool done = false;
    bool buf_room = true;
    int positive;
    void *ack = malloc(sizeof(int)+sizeof(int));
    set_up(&win, ack);
    //set_false_received_window(rec_win,win.len);

    int offset = 0;

    // Play
    printf("playing...\n");
    bool total_done = false;
    while (!total_done) 
    { 
        while(!done && buf_room) 
        {       
            positive = 0;
            receive_packet(data,&head, &win, &done, &positive);            
            if (positive == 1)
            {   

                memcpy(recvbuffer+offset,data, head.sample_size*head.nSamples);
                if (!done)
                    offset += head.sample_size*head.nSamples;

            }
            update_window(&win);        
            create_ack(head.number,positive,ack);
            send_ack(ack, sizeof(int)+sizeof(int));
            memset(ack,0,sizeof(int)+sizeof(int));
            
            if (offset == buffer_size || !remaining_size(head,offset,buffer_size))
            {

                buf_room =false;
            }
        }
        player_play(player, recvbuffer, offset); 
        printf("%0.f %% \n", 100*(double)win.LPR/(double)win.largest);
        memset(recvbuffer, 0, buffer_size); // Reset array
        buf_room = true;
        offset = 0;
        if (done)
        {
            close_socket();
            free(ack);
            total_done = true;
        }
    }
    printf("done.\n");
}


int main(int argc, char **argv) {
    int buffer_size = 1024*1024;

    player_t* player = malloc(sizeof(player_t));
    player_init(player);

    if (argc > 2) 
    {
        fprintf(stderr, "to manny commandline argument for blocksize provided\n");
        return -1;
    }
    if (argc == 2)
    {
        if ( atoi(argv[1]) < buffer_size)
        {
            fprintf(stderr, "invalid blocksize provided\n");
            return -1;
        }
        buffer_size = atoi (argv[1]);
    }   
    
    // set up buffer/queue  
    uint8_t* recvbuffer = malloc(buffer_size);
   
    play_music(player, recvbuffer, buffer_size);

    // clean up
    free(recvbuffer);
    player_free(player);
    free(player);

    return 0;
}
