#include "tetris.hpp"
#include <algorithm>
#include <array>
#include <random>

/*
Game logic based on open source Tetris cpp code by Nuruzzaman Milon: (https://github.com/milon/Tetris)
SRS system logic based on Harddrop Wiki article: (https://harddrop.com/wiki/SRS) 
*/

//Full Super‑Rotation System (SRS) wall‑kick tables
static const int SRS_KICKS_JLSTZ[4][5][2] = {
    { {0, 0}, {-1, 0}, {-1, 1}, {0, -2}, {-1, -2} },
    { {0, 0}, {1, 0}, {1, -1}, {0, 2}, {1, 2} },
    { {0, 0}, {1, 0}, {1, 1}, {0, -2}, {1, -2} },
    { {0, 0}, {-1, 0}, {-1, -1}, { 0, 2}, {-1, 2} }
};

static const int SRS_KICKS_I[4][5][2] = {
    { {0, 0}, {-2, 0}, {1, 0}, {-2, -1}, {1, 2} },
    { {0, 0}, {-1, 0}, {2, 0}, {-1,  2}, {2, -1} },
    { {0, 0}, {2, 0}, {-1, 0}, {2, 1}, {-1, -2} },
    { {0, 0}, {1, 0}, {-2, 0}, {1, -2}, {-2, 1} }
};


//Make Tetromino
static Tetromino make_piece(std::initializer_list<const char*> rows,
                            uint8_t tile,
                            PieceType type) {
    Tetromino t{};
    t.type = type;
    int row = 0;
    for (auto line : rows) {
        for (int col = 0; col < 4 && line[col]; ++col)
            if (line[col] == '#') t.mask[row][col] = tile;
        ++row;
    }
    return t;
}

//Define Tetromino shapes and colors
static const std::array<Tetromino, 7> SHAPES = {
    make_piece({"....","####","....","...."}, BLUE, PieceType::I), // I spawn
    make_piece({"....","#...","###.","...."}, RED, PieceType::J), // J spawn
    make_piece({"....","..#.","###.","...."}, YELLOW, PieceType::L), // L spawn
    make_piece({"....",".##.",".##.","...."}, MAG, PieceType::O), // O spawn
    make_piece({"....","..##",".##.","...."}, GREEN, PieceType::S), // S spawn
    make_piece({"....",".###","..#.","...."}, CYAN, PieceType::T), // T spawn
    make_piece({"....",".##.","..##","...."}, PURPLE, PieceType::Z) // Z spawn
};

// Rotate a single 4×4 Tetromino mask 90° clockwise
static Tetromino rot_right(const Tetromino& t) {
    Tetromino r{};
    r.type = t.type;
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
            r.mask[x][3 - y] = t.mask[y][x];
    return r;
}

//Rotate Tetromino a set number of times
Tetromino Tetris::rotate_piece(const Tetromino& t, int num_rot) const {
    int rcount = ((num_rot % 4) + 4) % 4;
    Tetromino q = t;
    for(int i = 0; i < rcount; ++i) {
        q = rot_right(q);
    }
    return q;
}

//Generate random Tetromino
static Tetromino rnd_piece() {
    static std::mt19937 gen{std::random_device{}()};
    static std::uniform_int_distribution<int>d(0, 6);
    return SHAPES[d(gen)];
}

//Set Tetromino
Tetris::Tetris() { 
    cur = rnd_piece(); 
    nxt = rnd_piece(); 
    spawn();
}

//Spawn current Tetromino
void Tetris::spawn() {
    px = 5; 
    py = 0; 
    rot = 0; 
    cur = nxt; 
    nxt = rnd_piece();
    if (collision(px, py, cur, rot)) { 
        over = true; 
    }
}

//Calculate collisions
bool Tetris::collision(int nx, int ny,
                       const Tetromino& pc,
                       int r) const
{
    Tetromino p = rotate_piece(pc, r);
    for(int y = 0; y < 4; ++y) {
        for(int x = 0; x < 4; ++x) {
            if (!p.mask[y][x]) continue;
            int gx = nx + x;
            int gy = ny + y;
            
            // Check again going off the left/right or below bottom
            if (gx < 0 || gx >= COLS || gy >= ROWS)
                return true;
            
            // Check against overlapping
            if (gy >= 0 && field[gy][gx])
                return true;
        }
    }
    return false;
}

//Move Left Function
void Tetris::move_left() { 
    if (!paused && !over && !collision(px - 1, py, cur, rot)) --px; 
}

//Move Right Function
void Tetris::move_right() { 
    if (!paused && !over && !collision(px + 1, py, cur, rot)) ++px; 
}

//Rotate Function
void Tetris::rotate() {
    if (paused || over) return; 

    int old_r = rot;
    int new_r = (old_r + 1) & 3;
    const int (*kicks)[2] = nullptr;

    //Determine kick table according to Tetromino type
    if (cur.type == PieceType::I) kicks = SRS_KICKS_I[old_r]; //I
    else if (cur.type == PieceType::O) { //O
        //O Tetrominos rotate in place
        rot = new_r;
        return;
    }
    else kicks = SRS_KICKS_JLSTZ[old_r]; //JLSTZ

    //Try each of the 5 SRS tests
    for (int i = 0; i < 5; ++i) {
        int dx = kicks[i][0], dy = kicks[i][1];
        if (!collision(px + dx, py + dy, cur, new_r)) {
            px += dx;
            py += dy;
            rot = new_r;
            return;
        }
    }
}

//Soft Drop Function
void Tetris::soft_drop() { 
    if (!paused && !over && !collision(px, py + 1, cur, rot)) ++py; 
}

//Hard Drop Function
void Tetris::hard_drop() {
    if (paused || over) return;
    while (!collision(px, py + 1, cur, rot)) ++py;
    lock_piece();
}

//Pause Function
void Tetris::toggle_pause() { 
    if (!over) paused = !paused; 
}

//Gravity for Tetrominos
void Tetris::step() {
    if (paused || over) return;

    //Max speed achieved at level 30 (speed actually maxes out at a lower since interval is an int)
    int interval = std::max(1, 30 / level);

    if (++tick % interval == 0) {
        if(!collision(px, py + 1, cur, rot)) ++py;
        else lock_piece();
    }
}

//Lock Tetromino in place
void Tetris::lock_piece() {
    Tetromino p = rotate_piece(cur, rot);
    for(int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
            if (p.mask[y][x]) field[py + y][px + x] = p.mask[y][x];
    clear_lines();
    spawn();
}

//Clear lines
void Tetris::clear_lines() {
    int cleared = 0;
    for(int y = 0; y < ROWS; ++y) {
        bool line_full = true;
        for(int x = 0; x < COLS; ++x) {
            if (!field[y][x]) { line_full = false; break; }
        }
        if (line_full) {
            for(int k = y; k > 0; --k) {
                field[k] = field[k - 1];
            }
            field[0].fill(0);
            ++cleared;
        }
    }

    if (cleared > 0) {
        lines_cleared += cleared;

        //increase level every 10 lines cleared
        level = (lines_cleared / 10) + 1; 

        // Tetris scoring: 1 = 100, 2 = 300, 3 = 500, 4 = 800
        static const int SCORE_TABLE[5] = { 0, 100, 300, 500, 800 };

        // mod 5 is a failsafe for if somehow more than 4 lines are cleared at once
        score_val += SCORE_TABLE[cleared % 5];
        
    }
}

//Reset game state
void Tetris::reset() {
    lines_cleared = 0;
    score_val = 0;
    over = 0;
    level = 1; 
    tick  = 0; 
    for (int i = 0; i < 20; ++i) field[i].fill(0); //Fill playfield with empty tiles
}

//Render helper functions for rendering each of the 4 tiles that make up a Tetromino
void Tetris::for_each_block(std::function<void(int, int, uint8_t)> cb) const
{
    Tetromino t = rotate_piece(cur,rot);
    for(int y = 0; y < 4; ++y)
        for(int x = 0; x < 4; ++x)
            if(t.mask[y][x]) cb(px + x, py + y, t.mask[y][x]);
}
void Tetris::for_each_next(std::function<void(int, int, uint8_t)> cb) const
{
    for(int y=0; y<4; ++y)
        for(int x=0; x<4; ++x)
            if(nxt.mask[y][x]) cb(x, y, nxt.mask[y][x]);
}