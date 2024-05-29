
#include <vector>

void getGPUProperties(void);
void updateGrid(float* grid, std::vector<int> grid_size, float delta);

void simulate(float* smoke_grid, float dt);

void initializeVolume(float* smoke_grid, unsigned int width, unsigned int heigth, unsigned int depth);
void deleteVolume();
void setSpherePos(float x, float y, float z);