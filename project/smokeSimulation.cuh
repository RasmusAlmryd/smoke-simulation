
#include <vector>

void getGPUProperties(void);
void updateGrid(float* grid, std::vector<int> grid_size, float delta);

void simulate(float* smoke_grid, float dt);

void initializeVolume(float* smoke_grid, unsigned int width, unsigned int heigth, unsigned int depth);
void deleteVolume();


int addObstacle(float x, float y, float z, float vx, float vy, float vz, float r);
int addSmokeSource(float x, float y, float z, float r);
void updateObjectPos(int id, float x, float y, float z);

float* getBuoyancy();
float* getGravity();

