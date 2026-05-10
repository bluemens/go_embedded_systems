#include <iostream>
#include <mpg123.h>
#include <ao/ao.h>

int main() {
    const char *filename = "Tetris.mp3"; //MP3 Filename

    //Initialize mpg123
    if (mpg123_init() != MPG123_OK) {
        std::cerr << "Unable to initialize mpg123\n";
        return 1;
    }

    int music_handler_error;
    mpg123_handle *music_handler = mpg123_new(NULL, &music_handler_error);
    if (!music_handler) {
        std::cerr << "Failed to create mpg123_handle: " << mpg123_plain_strerror(music_handler_error) << "\n";
        mpg123_exit();
        return 1;
    }

    //Open the MP3 file
    if (mpg123_open(music_handler, filename) != MPG123_OK) {
        std::cerr << "Error opening `" << filename << "`\n";
        mpg123_delete(music_handler);
        mpg123_exit();
        return 1;
    }

    //Get audio format
    long rate;
    int channels, encoding;
    if (mpg123_getformat(music_handler, &rate, &channels, &encoding) != MPG123_OK) {
        std::cerr << "Failed to get MP3 format information\n";
        mpg123_close(music_handler);
        mpg123_delete(music_handler);
        mpg123_exit();
        return 1;
    }

    //Initialize libao with format from mp3 file
    ao_initialize();
    ao_sample_format format;
    format.bits = mpg123_encsize(encoding) * 8;
    format.rate = rate;
    format.channels = channels;
    format.byte_format = AO_FMT_NATIVE;
    format.matrix = nullptr;

    //Open the ALSA drivers
    int driver = ao_driver_id("alsa");
    if (driver < 0) {
        std::cerr << "ALSA driver not available\n";
        ao_shutdown();
        mpg123_close(music_handler);
        mpg123_delete(music_handler);
        mpg123_exit();
        return 1;
    }

    //Explicitly open card 1, device 0 (Our audio device)
    ao_option ao_opts[] = {
        {"dev", "plughw:1,0"}
    };

    ao_device *usb_audio_device = ao_open_live(driver, &format, ao_opts);
    if (!usb_audio_device) {
        std::cerr << "ao_open_live failed\n";
        ao_shutdown();
        mpg123_close(music_handler);
        mpg123_delete(music_handler);
        mpg123_exit();
        return 1;
    }

    //Decode and play audio
    unsigned char buffer[8192];
    size_t done = 0;

    //Loop playback forever
    while (true) {

        //Rewind to start
        mpg123_seek(music_handler, 0, SEEK_SET);

        //Play track
        while (mpg123_read(music_handler, buffer, 8192, &done) == MPG123_OK && done > 0) {
            ao_play(usb_audio_device, reinterpret_cast<char*>(buffer), done);
        }

    }
    return 0;
}