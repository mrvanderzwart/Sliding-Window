#include <stdint.h>

#include "player.h"

#define NUM_CHANNELS 2
#define SAMPLE_RATE 44100
#define BLOCK_SIZE 1024
// 1 Frame = Stereo 16 bit = 32 bit = 4 bytes
#define FRAME_SIZE 4

// Opens audio device to play music to
static void player_open_audio_device(player_t* const player) {
    int err = 0;
    
    if ((err = snd_pcm_open(&(player->snd_handle), "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("couldnt open audio device: %s\n", snd_strerror(err));
        exit(-1);
    }

    // Configure parameters of PCM output
    err = snd_pcm_set_params(
         player->snd_handle,
         SND_PCM_FORMAT_S16_LE,
         SND_PCM_ACCESS_RW_INTERLEAVED,
         NUM_CHANNELS,
         SAMPLE_RATE,
         0,              // Allow software resampling
         500000);        // 0.5 seconds latency
    if (err < 0) {
        printf("couldnt configure audio device: %s\n", snd_strerror(err));
        exit(-1);
    }
}

void player_init(player_t* const player) {
    player->snd_handle = NULL;
    player_open_audio_device(player);
}

void player_play(player_t* const player, uint8_t* const data_to_play, size_t data_size) {
    /* write frames to ALSA */

    snd_pcm_sframes_t frames = snd_pcm_writei(player->snd_handle, data_to_play, data_size / 4);
    /* Check for errors */
    int ret = 0;
    if (frames < 0)
        ret = snd_pcm_recover(player->snd_handle, frames, 0);
    if (ret < 0) {
        fprintf(stderr, "ERROR: Failed writing audio with snd_pcm_writei(): %i\n", ret);
        exit(-1);
    }
}

void player_free(player_t* const player) {
    snd_pcm_drain(player->snd_handle);
    snd_pcm_hw_free(player->snd_handle);
    snd_pcm_close(player->snd_handle);
}
