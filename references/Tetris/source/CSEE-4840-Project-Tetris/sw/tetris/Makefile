# Makefile

CXX        := g++
CXXFLAGS   := -std=c++17 -Wall -Wextra -I.
LDLIBS     := -lmpg123 -lao

# Executables
TETRIS_EXE := tetris
MUSIC_EXE  := audio
RUN_EXE    := runner

# Sources
TETRIS_SRC := main.cpp tetris.cpp
MUSIC_SRC  := audio.cpp
RUN_SRC    := runner.cpp

.PHONY: all run clean

all: $(TETRIS_EXE) $(MUSIC_EXE) $(RUN_EXE)

$(TETRIS_EXE): $(TETRIS_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(MUSIC_EXE): $(MUSIC_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(RUN_EXE): $(RUN_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ -pthread

run: all
	./$(RUN_EXE)

clean:
	rm -f $(TETRIS_EXE) $(MUSIC_EXE) $(RUN_EXE)