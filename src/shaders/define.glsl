#define FLOAT_EPS 0.00000001f

#define NULL_VEC4 vec4(0.0, 0.0, 0.0, 0.0)
#define ABS_GRAVITY_ACC_Y 9.81f
#define GRAVITY_ACC vec4(0.0, -ABS_GRAVITY_ACC_Y, 0.0, 0.0)
#define FAR_DIST 1000000.0f

// Domain is a cube of [-2 2] in x y z directions
#define ABS_WALL_X 1.0f
#define ABS_WALL_Y 1.0f
#define ABS_WALL_Z 1.0f
#define MIN_BOUNDS vec4(-ABS_WALL_X, -ABS_WALL_Y, -ABS_WALL_Z, 0.0)
#define MAX_BOUNDS vec4(ABS_WALL_X, ABS_WALL_Y, ABS_WALL_Z, 0.0)

// Domain is partioned into a grid of 8 x 8 x 8 cells
#define GRID_NUM_CELLS 512
#define GRID_CELL_SIZE_XYZ 0.250f // 2 / 8
#define GRID_RES_X 8
#define GRID_RES_Y 8
#define GRID_RES_Z 8

#define UINT32_MAX 0xFFFFFFFF
#define NUM_MAX_PARTS_IN_CELL 2000