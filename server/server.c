/*
 * Skeleton-code behorende bij het college Netwerken, opleiding Informatica,
 * Universiteit Leiden.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <netinet/in.h>

#include "../communication/asp/asp.h"

static int asp_socket_fd = -1;

struct wave_header {
    char riff_id[4];
    uint32_t size;
    char wave_id[4];
    char format_id[4];
    uint32_t format_size;
    uint16_t w_format_tag;
    uint16_t n_channels;
    uint32_t n_samples_per_sec;
    uint32_t n_avg_bytes_per_sec;
    uint16_t n_block_align;
    uint16_t w_bits_per_sample;
};

// wave file handle
struct wave_file {
    struct wave_header *wh;
    int fd;

    void *data;
    uint32_t data_size;

    uint8_t *samples;
    uint32_t payload_size;
};

/*
struct for sender window
*/
struct window
{
    int begin;
    int end;
    int total;
    int len;
    int lap;
    int lnr;
};

/*
setup so that client can connect with server
waits for signal from client.
*/
void setup () {

    create_socket();
    server_information();
    bind_socket();
    receive_from();
}

static struct wave_file wf = {0,};

// open the wave file
static int open_wave_file(struct wave_file *wf, const char *filename) {
    // Open the file for read only access
    wf->fd = open(filename, O_RDONLY);
    if (wf->fd < 0) {
        fprintf(stderr, "couldn't open %s\n", filename);
        return -1;
    }

    struct stat statbuf;
    // Get the size of the file
    if (fstat(wf->fd, &statbuf) < 0)
        return -1;

    wf->data_size = statbuf.st_size; // Total size of the file

    // Map the file into memory
    wf->data = mmap(0x0, wf->data_size, PROT_READ, MAP_SHARED, wf->fd, 0);
    if (wf->data == MAP_FAILED) {
        fprintf(stderr, "mmap failed\n");
        return -1;
    }

    wf->wh = wf->data;

    // Check whether the file is a wave file
    if (strncmp(wf->wh->riff_id, "RIFF", 4)
            || strncmp(wf->wh->wave_id, "WAVE", 4)
            || strncmp(wf->wh->format_id, "fmt", 3)) {
        fprintf(stderr, "%s is not a valid wave file\n", filename);
        return -1;
    }

    // Skip to actual data fragment
    uint8_t* p = (uint8_t*) wf->data + wf->wh->format_size + 16 + 4;
    uint32_t* size = (uint32_t*) (p + 4);
    do {
        if (strncmp((char*) p, "data", 4))
            break;

        wf->samples = p;
        wf->payload_size = *size;
        p += 8 + *size;
    } while (strncmp((char*) p, "data", 4) && (uint32_t) (((uint8_t*) p) - (uint8_t*) wf->data) < statbuf.st_size);

    if (wf->wh->w_bits_per_sample != 16) {
        fprintf(stderr, "can't play sample with bitsize %d\n",
                        wf->wh->w_bits_per_sample);
        return -1;
    }

    float playlength = (float) *size / (wf->wh->n_channels * wf->wh->n_samples_per_sec * wf->wh->w_bits_per_sample / 8);

    printf("file %s, mode %s, samplerate %u, time %.1f sec\n", filename, wf->wh->n_channels == 2 ? "Stereo" : "Mono", wf->wh->n_samples_per_sec, playlength);
    return 0;
}

// close the wave file/clean up
static void close_wave_file(struct wave_file *wf) {
    munmap(wf->data, wf->data_size);
    close(wf->fd);
}

void send_packets (int packet_num, int sample_size, int samples_per_packet, uint8_t* samples)
{
    struct header new_header = {packet_num, sample_size, 
                                samples_per_packet,
                                compute_checksum(samples,
                                samples_per_packet,
                                sample_size)};              
    uint8_t *buf = malloc(sizeof(new_header)+sample_size*samples_per_packet);
    memcpy(buf, &new_header,sizeof(new_header));
    memcpy(buf+sizeof(new_header),samples,sample_size*samples_per_packet);
    if (send_to(buf, sizeof(new_header)+sample_size*samples_per_packet) == asp_socket_fd)
    {
        perror("send_to()");
        exit(1);
    }
    free (buf);
    //samples += (sample_size*samples_per_packet); DIT MOET WEG
}
/*
sets window values
*/
void set_window(int nPackets, int window_len, struct window *win)
{
    win -> begin = 0;
    win -> end = window_len -1;
    win -> len = window_len;
    win -> total = nPackets;
    win -> lnr = 0;
}

/*
sends window values to client and waits for ack
*/
void send_window_information (struct window win, void *ack )
{
    void *window = malloc(W_PACKET_SIZE);
    int positive = 0;
    memcpy(window,&win.begin,sizeof(win.begin));
    memcpy((char*)window+sizeof(win.begin),&win.len, sizeof(win.len));
    memcpy((char*)window+sizeof(win.begin)+sizeof(win.len),
            &win.total, sizeof(win.total));
    while (positive != 1)
    {
        if (send_window(window)==asp_socket_fd)
        {
            perror("send_window()");
            exit(1);

        }
        receive_ack(ack, sizeof(int)+sizeof(int));
        memcpy(&positive, (char*)ack+sizeof(int), sizeof(positive));
        memset(ack,0,sizeof(int)+sizeof(int));
    }

    //receive ack
    free(window);
}
/*
update window depending on wich ack is received.
*/
void update_window (struct window *win)
{
    win->begin = win -> lnr;

    if (win ->lnr + win -> len +1 > win -> total)
        win -> end = win -> total;
    else
        win -> end =  win -> lnr+win->len -1;
}

/*
sets bool arrays false
*/
void set_false(bool *packet_success, bool *packet_send, time_t *packet_send_time, int nPackets)
{
    for (int i = 0; i < nPackets; i++)
    {
        packet_success[i] = false;
        packet_send[i] = false;
        packet_send_time[i] = time(NULL);
    }
}
/*
returns first not received packet.
*/
int first_not_received (bool *packet_success, struct window win)
{
    for (int i = win.begin; i <= win.end; i++)
    {
        if(!packet_success[i])
            return i;
    }
    return win.end+1;
}


//dit moet nog aangepast worden 
bool check_done( bool *packet_success, int nPackets, int *welke)
{
    for (int i = 0; i < nPackets; i++ )
    {
        if(!packet_success[i])
        {
            *welke = packet_success[i];
            return false;

        }
    }
    return true;
}

/*
caculate the total amount of packets depending on sample_size, total samples and packet_size.
returns remaining samples in last packet.
*/
int amount_of_packets(int nSamples, int *nSamplesPerPacket, int sample_size, int packet_size, int *nPackets)
{
    int remaining_samples;
    struct header head;
    int temp;

    int packet_sample_room = packet_size - sizeof(head);
    *nSamplesPerPacket = packet_sample_room / sample_size;
    *nPackets = nSamples / *nSamplesPerPacket;
    temp = *nPackets * *nSamplesPerPacket;
    if (nSamples - temp == 0)
    {
        remaining_samples = 0;
    }
    else
    {
        remaining_samples = nSamples - temp;
        *nPackets = *nPackets+1;
    }
    return remaining_samples;
}
/*
implemtion of sliding window sends packets in window and receives acks.
keep track of how many packets have been correctly received
when last ack packet is received then finsihed
*/
void sliding_window()
{
    
    int nPackets;
    int nSamples = wf.payload_size / wf.wh->n_block_align;//total samples
    int packet_number;
    int positive =1;
    int window_len =8; //8 packets per window
    uint8_t* wf_samples;
    int sample_size =  wf.wh->n_channels * wf.wh->w_bits_per_sample/8;

    int nSamplesPerPacket;
    int remaining_samples_lp = amount_of_packets(nSamples,&nSamplesPerPacket, sample_size,PACKET_SIZE,&nPackets);

    bool packet_send[nPackets];
    bool packet_success[nPackets];

    time_t packet_send_time[nPackets];
    void *ack = malloc (sizeof(int)+sizeof(int));

    set_false(packet_success, packet_send, packet_send_time, nPackets);

    struct window win;
    set_window(nPackets, window_len, &win);
    send_window_information(win, ack);

    printf("sending ... \n");
    bool read_done = false;
    while (!read_done)
    {
        update_window(&win);
        for (int i = win.begin; i <= win.end; i++)
        {

            if (!packet_send[i] || (packet_send[i] && (time(NULL) - packet_send_time[i] >= 3)))
            {
                if (i == nPackets-1 && remaining_samples_lp !=0) //DIT MOET AANGEPAST WORDEN
                {
                    wf_samples = wf.samples + (i*(sample_size*remaining_samples_lp));
                }
                else
                {
                    wf_samples = wf.samples + (i*(sample_size*nSamplesPerPacket)); 
                }
              send_packets(i, sample_size,nSamplesPerPacket,wf_samples);
              packet_send[i] = true;
              packet_send_time[i] = time(NULL);
            }


        }

        for (int i = win.begin; i <= win.end; i++)
        {      
            receive_ack(ack, sizeof(int)+sizeof(int));
            memcpy(&packet_number,ack,sizeof(int));
            memcpy(&positive, (char*)ack+sizeof(packet_number), sizeof(positive));
            if (positive ==1)
            {
                packet_success[packet_number]= true;
                win.lap = packet_number;
            }
            memset(ack,0,sizeof(int)+sizeof(int));
        }
        win.lnr = first_not_received(packet_success,win);
        if (packet_success[nPackets-1] == true)
        {
            free(ack);
            read_done = true;
        }
    }
    printf("done.\n");
}

int main(int argc, char **argv) {

    if (argc != 2)
    {
        printf("no input file or too many input files \n");
        return -1;
    }
      
    char *filename;
    filename = argv[1];

    // Open the WAVE file
    if (open_wave_file(&wf, filename) < 0)
        return -1;

    setup();

    sliding_window();

    // Clean up
    close_wave_file(&wf);

    return 0;
}
