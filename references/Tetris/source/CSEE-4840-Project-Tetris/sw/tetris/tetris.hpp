#ifndef TETRIS_HPP
#define TETRIS_HPP
#include <array>
#include <cstdint>
#include <functional>


enum class PieceType { I, J, L, O, S, T, Z };

constexpr int COLS = 15;
constexpr int ROWS = 20;

//The color to palette mapping (matches assets.h)
enum Cell : uint8_t {
    EMPTY = 0,
    BLUE = 2,
    GREEN = 3,
    RED = 4,
    CYAN = 5,
    YELLOW = 6,
    MAG = 7,
    PURPLE = 8
};

struct Tetromino {
    std::array<std::array<uint8_t,4>,4> mask{};
    PieceType type = PieceType::I;  // Default of I will be overwritten later in make_piece function
};

class Tetris {
public:
    Tetris();
    void step();             
    void move_left();
    void move_right();
    void rotate();
    void soft_drop();
    void hard_drop();
    void toggle_pause();
    void reset();

    
    uint8_t playfield(int x, int y) const { 
        return field[y][x]; 
    }
    void for_each_block(std::function<void(int, int, uint8_t)> cb) const;
    void for_each_next (std::function<void(int, int, uint8_t)> cb) const;
    int score() const { 
        return score_val; 
    }
    int lines() const { 
        return lines_cleared; 
    }
    int get_level() const { 
        return level; 
    }
    int get_px() const { 
        return px; 
    }
    int get_py() const { 
        return py; 
    }
    int get_rot() const { 
        return rot; 
    }
    const Tetromino& get_cur() const { 
        return cur; 
    }
    bool game_over() const { 
        return over; 
    }
    bool can_place(int nx, int ny) const {
        return !collision(nx, ny, cur, rot);
    }
    Tetromino get_rotated_piece() const {
        return rotate_piece(cur, rot);
    }

private:
    std::array<std::array<uint8_t,COLS>,ROWS> field{};
    Tetromino cur, nxt;
    int   px = 5, py = 0, rot = 0;
    int   tick = 0;
    bool  paused=false, over=false;
    int   score_val = 0;
    int   lines_cleared = 0;
    int   level         = 1;

    void spawn();
    bool collision(int nx,int ny,const Tetromino& t,int r) const;
    void lock_piece();
    void clear_lines();
    Tetromino rotate_piece(const Tetromino& t,int r) const;
};

#endif