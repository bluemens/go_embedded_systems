#include <stdio.h>
#include <sys/types.h>  
#include <unistd.h>   
#include <stdlib.h>
#include <string.h>  

#define ENGINE_PATH "/usr/games/stockfish"

int uci_init();
int uci_get_bestmove(const char *moves, char *out_move);
void uci_close();