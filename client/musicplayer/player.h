#ifndef PLAYER
#define PLAYER

#include <alsa/asoundlib.h>

// Code to let ALSA play music, completely implemented.

typedef struct {
    snd_pcm_t* snd_handle;
} player_t;

// Initialize a player_t.
void player_init(player_t* const player);

// Play some music before returning.
// Note: Calling this function blocks until ALSA has enough room to store the provided music.
// Args:
//     player: Our music player.
//     data_to_play: Points to the first byte of music we want to give to ALSA.
//     data_size: Amount of bytes in the buffer to send to ALSA.
void player_play(player_t* const player, uint8_t* const data_to_play, size_t data_size);

// Free a player_t.
void player_free(player_t* const player);

#endif