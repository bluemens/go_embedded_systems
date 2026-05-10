#include <iostream>
#include <cstdlib>
#include <pthread.h>

void* run_music(void*) {
    std::cout << "Starting music loop..." << std::endl;
    int ret = std::system("./audio");
    std::cout << "Music loop exited with code " << ret << std::endl;
    return nullptr;
}

void* run_game(void*) {
    std::cout << "Starting Tetris..." << std::endl;
    int ret = std::system("./tetris");
    std::cout << "Tetris exited with code " << ret << std::endl;
    return nullptr;
}

int main() {
    pthread_t music_thread, game_thread;

    if (pthread_create(&music_thread, nullptr, run_music, nullptr)) {
        std::cerr << "Error creating music thread" << std::endl;
        return 1;
    }
    if (pthread_create(&game_thread, nullptr, run_game, nullptr)) {
        std::cerr << "Error creating game thread" << std::endl;
        return 1;
    }

    pthread_join(music_thread, nullptr);
    pthread_join(game_thread, nullptr);

    std::cout << "Both music and game have exited." << std::endl;
    return 0;
}