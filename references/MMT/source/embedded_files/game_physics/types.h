#ifndef TYPES_H_
#define TYPES_H_

#define CONTINUE 0
#define WON 1
#define LOST 2

typedef struct {
    double x;
    double y;
    double z;
} Vec3;

typedef struct {
    double x;
    double y;
} Vec2;

typedef struct {
    Vec3 pos3D;
    Vec3 velocity;
} MarbleState3D;

typedef struct {
    int x_idx;
    int y_idx;
    int z_idx;
    int type;
} Tile;

#endif
