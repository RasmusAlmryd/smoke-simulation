
#include <cuda.h>
#include <cuda_runtime.h>
#include <iostream>
#include <vector>

//#include <sys/time.h>
#include "device_launch_parameters.h"
#include "smokeSimulation.cuh"


void getGPUProperties() {
	//Discover GPU attributes
	cudaError_t err;
	int devices;	//In case there are more than one GPUs
	cudaDeviceProp prop;
	err = cudaGetDeviceCount(&devices);
	if (!err) {
		for (int i = 0; i < devices; i++) {
			printf("CUDA Device - ID %d\n", i);
			err = cudaGetDeviceProperties(&prop, i);
			if (!err) {
				printf("Max threads per block: 		%d\n", prop.maxThreadsPerBlock);
				printf("Max block dimensions: 		(%d, %d, %d)\n", prop.maxThreadsDim[0], prop.maxThreadsDim[1], prop.maxThreadsDim[2]);
				printf("Max grid dimensions: 		(%d, %d, %d)\n", prop.maxGridSize[0], prop.maxGridSize[1], prop.maxGridSize[2]);
				printf("Shared memory per block: 	%.2lfKB\n", prop.sharedMemPerBlock / 1024.);
			}
			printf("\n");
		}
	}
	else {
		fprintf(stderr, "Error finding available GPUs, now exiting\n");
		exit(-1);
	}
}


__global__ void smokeKernal(float *grid, int width, int height, int depth) {
	int x = threadIdx.x + blockDim.x * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y;
	int z = threadIdx.z + blockDim.z * blockIdx.z;

	if (x < width && y < height && z < depth) {
		if (y < height - 1) {
			if (grid[x + (y + 1) * width + z * width * height] > 0.0f) {
				grid[x + y * width + z * width * height] = 1.0f;
			}
			else {
				grid[x + y * width + z * width * height] = 0.0f;
			}
		}
		else {
			grid[x + y * width + z * width * height] = 0.0f;
		}
		
		//grid[4] = 1.0f;
		//grid[8] = 1.0f;
		//grid[27] = 1.0f;
	}
}

void updateGrid(float *grid, std::vector<int> grid_size, float delta) {
	cudaError_t err;

	int width = grid_size[0]; //x
	int heigth = grid_size[1]; //y
	int depth = grid_size[2]; //z

	printf("width: %d, height: %d, depth: %d \n", width, heigth, depth);

	int size = width * heigth * depth * sizeof(float);

	float* dev_grid;

	err = cudaMalloc(&dev_grid, size);
	if (err != 0) {
		fprintf(stderr, "Error allocating gird on GPU\n");
		exit(-1);
	}

	
 
	

	err = cudaMemcpy(dev_grid, grid, size, cudaMemcpyHostToDevice); 
	if (err != cudaSuccess) { 
		fprintf(stderr, "Error copying data from host to device: %s\n", cudaGetErrorString(err)); 
		exit(-1); 
	}

	dim3 dimBlock(8, 8, 8); //512 
	dim3 dimGrid(ceil(width / 8.0), ceil(heigth / 8.0), ceil(depth / 8.0)); 

	printf("width: %d, height: %d, depth: %d \n", dimGrid.x, dimGrid.y, dimGrid.z); 

	smokeKernal<<<dimGrid, dimBlock>>> (dev_grid, width, heigth, depth);

	// Copy data from device to host
	err = cudaMemcpy(grid, dev_grid, size, cudaMemcpyDeviceToHost);  
	if (err != cudaSuccess) { 
		fprintf(stderr, "Error copying data from device to host: %s\n", cudaGetErrorString(err)); 
		exit(-1); 
	}

	cudaFree(dev_grid);

}