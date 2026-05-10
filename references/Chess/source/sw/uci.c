#include <stdio.h>
#include <sys/types.h>  
#include <unistd.h>  
#include <stdlib.h>
#include <string.h>

#include "uci.h"

static FILE *uci_in = NULL;
static FILE *uci_out = NULL;

/* Launch Stockfish and prepare communication pipes */
int uci_init() {
    int to_engine[2], from_engine[2];

    if (pipe(to_engine) < 0 || pipe(from_engine) < 0) {
        perror("pipe");
        return -1;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child: connect pipes and exec Stockfish
        dup2(to_engine[0], STDIN_FILENO);
        dup2(from_engine[1], STDOUT_FILENO);
        close(to_engine[1]);
        close(from_engine[0]);

        execl(ENGINE_PATH, ENGINE_PATH, NULL);
        perror("execl failed");
        exit(1);
    }

    // Parent
    close(to_engine[0]);
    close(from_engine[1]);

    uci_in = fdopen(to_engine[1], "w");
    uci_out = fdopen(from_engine[0], "r");

    if (!uci_in || !uci_out) {
        perror("fdopen");
        return -1;
    }

    fprintf(uci_in, "uci\n");
    fflush(uci_in);

    // Wait for "uciok"
    char line[128];
    while (fgets(line, sizeof(line), uci_out)) {
        if (strncmp(line, "uciok", 5) == 0) break;
    }

    fprintf(uci_in, "isready\n");
    fflush(uci_in);
    while (fgets(line, sizeof(line), uci_out)) {
        if (strncmp(line, "readyok", 7) == 0) break;
    }

    return 0;
}

/* Send moves and get bestmove */
int uci_get_bestmove(const char *moves, char *out_move) {
    fprintf(uci_in, "position startpos moves %s\n", moves);
    fprintf(uci_in, "go\n");
    fflush(uci_in);

    char line[128];
    while (fgets(line, sizeof(line), uci_out)) {
        if (strncmp(line, "bestmove ", 9) == 0) {
            strncpy(out_move, line + 9, 5);
            out_move[5] = '\0';
            return 0;
        }
    }

    if (strncmp(line, "bestmove ", 9) == 0) {
        if (strncmp(line + 9, "(none", 5) == 0) {
            out_move[0] = '\0';  // mark as no move
            return 1;            // signal: no legal move
        }
        strncpy(out_move, line + 9, 5);
        out_move[5] = '\0';
        return 0;
    }
}

/* Cleanup */
void uci_close() {
    if (uci_in) {
        fprintf(uci_in, "quit\n");
        fflush(uci_in);
        fclose(uci_in);
    }
    if (uci_out) {
        fclose(uci_out);
    }
}
