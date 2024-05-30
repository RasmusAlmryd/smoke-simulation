
#include <cuda.h>
#include <cuda_runtime.h>
#include <iostream>
#include <vector>

//#include <sys/time.h>
#include "device_launch_parameters.h"
#include "smokeSimulation.cuh"

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
using namespace glm;


uint3 smokeDim;
uint3 smokeStaggeredDim;

int smokeIndex = 0; // (0,1)
float* dev_smoke[2];
float* dev_u[2];
float* dev_v[2];
float* dev_w[2];
bool* dev_s;



float gravity = -9.82; // m/s^2
float buoyancy_alpha = 2.0f;

float* getBuoyancy() {
	return &buoyancy_alpha;
}
float* getGravity() {
	return &gravity;
}

#define OVER_RELAXATION 1.9
#define MAX_VELOCITY_PER_STEP 3.0f //squared

float* dev_obstacles;
float* dev_smokeSources; 


struct Sphere {
	int id;
	float3 pos;
	float3 vel;
	float radius;
	short type; // 0: obstacle, 1:smoke 

};

std::vector<Sphere> objects;

int max_objects = 3;

int currentId = 0;



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


int addObstacle(float x, float y, float z, float vx, float vy, float vz, float r) {
	float3 pos = { x,y,z };
	float3 vel = { vx,vy,vz };
	Sphere s = { currentId, pos, vel, r, 0};
	objects.push_back(s);
	currentId++;
	return currentId - 1;
}

int addSmokeSource(float x, float y, float z, float r) {
	float3 pos = { x,y,z };
	float3 vel = { 0,0,0 };
	Sphere s = { currentId, pos, vel, r, 1 };
	objects.push_back(s);
	currentId++;
	return currentId - 1;
}

void updateObjectPos(int id, float x, float y, float z) {
	float3 pos = { x,y,z };
	objects[id].pos = pos;
}

std::vector<Sphere> getObjectsOfType(int type) {
	std::vector<Sphere> object;
	for (int i = 0; i < objects.size(); i++) {
		if (objects[i].type == type) {
			object.push_back(objects[i]);
		}
	}

	return object;
}



void initializeVolume(float* smoke_grid, unsigned int width, unsigned int heigth, unsigned int depth) {
	cudaError_t err; 

	smokeDim = { width, heigth, depth };
	int smokeSize = width * heigth * depth * sizeof(float);

	for (int i = 0; i < 2; i++) {
		err = cudaMalloc(&dev_smoke[i], smokeSize);
		if (err != 0) {
			fprintf(stderr, "Error allocating smoke gird on GPU\n");
			exit(-1);
		}
	}

	err = cudaMemcpy(dev_smoke[0], smoke_grid, smokeSize, cudaMemcpyHostToDevice);
	if (err != cudaSuccess) {
		fprintf(stderr, "Error copying data from host to device: %s\n", cudaGetErrorString(err));
		exit(-1);
	}


	/*
	*	should be different for each velocity axis.
	*	ex u size: x: dim.x+1, y: dim.y, z: dim.z
	*/
	int velSize = (width+1) * (heigth+1) * (depth+1) * sizeof(float); 
	smokeStaggeredDim = { (width + 1) , (heigth + 1) , (depth + 1) };

	for (int i = 0; i < 2; i++) {
		err = cudaMalloc(&dev_u[i], velSize);
		if (err != 0) { 
			fprintf(stderr, "Error allocating U gird on GPU\n"); 
			exit(-1); 
		}
	}

	err = cudaMemset(dev_u[0], 0, velSize);
	if (err != cudaSuccess) {
		fprintf(stderr, "Error copying data from host to device: %s\n", cudaGetErrorString(err));
		exit(-1);
	}

	for (int i = 0; i < 2; i++) {
		err = cudaMalloc(&dev_v[i], velSize);
		if (err != 0) {
			fprintf(stderr, "Error allocating V gird on GPU\n");
			exit(-1);
		}
	}

	err = cudaMemset(dev_v[0], 0, velSize);
	if (err != cudaSuccess) {
		fprintf(stderr, "Error copying data from host to device: %s\n", cudaGetErrorString(err));
		exit(-1);
	}

	for (int i = 0; i < 2; i++) {
		err = cudaMalloc(&dev_w[i], velSize);
		if (err != 0) {
			fprintf(stderr, "Error allocating V gird on GPU\n");
			exit(-1);
		}
	}

	err = cudaMemset(dev_w[0], 0, velSize);
	if (err != cudaSuccess) {
		fprintf(stderr, "Error copying data from host to device: %s\n", cudaGetErrorString(err));
		exit(-1);
	}


	// generate smoke bounds
	int sSize = width * heigth * depth * sizeof(bool);
	err = cudaMalloc(&dev_s, sSize);
	if (err != 0) {
		fprintf(stderr, "Error allocating S gird on GPU\n");
		exit(-1);
	}

	std::vector<unsigned char> s_host(width * heigth * depth, 1);
	int y = 0;
	for (int x = 0; x < width; x++) {
		for (int z = 0; z < depth; z++) {
			
			s_host[x + y * width + z * width * heigth] = 0;
		}
	}

	err = cudaMemcpy(dev_s, s_host.data(), sSize, cudaMemcpyHostToDevice);
	if (err != cudaSuccess) {
		fprintf(stderr, "Error copying data from host to device: %s\n", cudaGetErrorString(err));
		exit(-1);
	}

	s_host.clear();




	// generate arrays for objects
	int obstacleSize = (3 + 3 + 1) * sizeof(float) * max_objects; //pos, vel, radius
	err = cudaMalloc(&dev_obstacles, obstacleSize);
	if (err != 0) {
		fprintf(stderr, "Error allocating dev_obstacles gird on GPU\n");
		exit(-1);
	}

	int smokeSourcesSize = (3 + 1) * sizeof(float) * max_objects; //pos, radius
	err = cudaMalloc(&dev_smokeSources, smokeSourcesSize);
	if (err != 0) {
		fprintf(stderr, "Error allocating dev_smokeSources gird on GPU\n");
		exit(-1);
	}



	float3 spherePosition = { smokeDim.x / 2.0, smokeDim.y / 2.0 , smokeDim.z / 2.0 };
}

void deleteVolume() {
	for (int i = 0; i < 2; i++) {
		cudaFree(dev_smoke[i]);
		cudaFree(dev_u[i]);
		cudaFree(dev_v[i]);
		cudaFree(dev_w[i]);
	}
	cudaFree(dev_s);
}


__global__ void fillSmoke(float* smoke0, float* smoke1, uint3 normalDim, float* dev_smokeSources, int num_sources) {
	int x = threadIdx.x + blockDim.x * blockIdx.x + 1;
	int y = threadIdx.y + blockDim.y * blockIdx.y + 1;
	int z = threadIdx.z + blockDim.z * blockIdx.z + 1;

	if (x < normalDim.x-1 && y < normalDim.y-1 && z < normalDim.z-1) {
		
		for (int i = 0; i < num_sources; i++) {
			float sx = dev_smokeSources[i * 4 + 0];
			float sy = dev_smokeSources[i * 4 + 1];
			float sz = dev_smokeSources[i * 4 + 2];
			float sr = dev_smokeSources[i * 4 + 3];
			
			
			float dist = powf(x - sx, 2) + powf(y - sy, 2) + powf(z - sz, 2);
			if (dist < sr*sr) {
				smoke0[x + y * normalDim.x + z * normalDim.x * normalDim.y] = 1.0f;
				smoke1[x + y * normalDim.x + z * normalDim.x * normalDim.y] = 1.0f;
			}
		}

	}
}

__global__ void fillSphere(float* v0, float* v1, float value,  uint3 staggeredDim, float3 sphereCenter, float sphereDiameter) {
	int x = threadIdx.x + blockDim.x * blockIdx.x + 1;
	int y = threadIdx.y + blockDim.y * blockIdx.y + 1;
	int z = threadIdx.z + blockDim.z * blockIdx.z + 1;

	if (x < staggeredDim.x - 1 && y < staggeredDim.y - 1 && z < staggeredDim.z - 1) {
		float dist = powf(x - sphereCenter.x, 2) + powf(y - sphereCenter.y, 2) + powf(z - sphereCenter.z, 2);
		if (dist < sphereDiameter) {
			v0[x + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] = value;
			v1[x + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] = value;
		}
	}
}

__global__ void fillObstacle(bool* s, uint3 normalDim, float* dev_obstacles, int num_obstacles) {
	int x = threadIdx.x + blockDim.x * blockIdx.x + 1;
	int y = threadIdx.y + blockDim.y * blockIdx.y + 1;
	int z = threadIdx.z + blockDim.z * blockIdx.z + 1;

	if (x < normalDim.x - 1 && y < normalDim.y - 1 && z < normalDim.z - 1) {
		for (int i = 0; i < num_obstacles; i++) {
			float sx = dev_obstacles[i * 7 + 0];
			float sy = dev_obstacles[i * 7 + 1];
			float sz = dev_obstacles[i * 7 + 2];
			float svx = dev_obstacles[i * 7 + 3];
			float svy = dev_obstacles[i * 7 + 4];
			float svz = dev_obstacles[i * 7 + 5];
			float sr = dev_obstacles[i * 7 + 6];
			
			float dist = powf(x - sx, 2) + powf(y - sy, 2) + powf(z - sz, 2);
			if (dist < sr*sr) {
				s[x + y * normalDim.x + z * normalDim.x * normalDim.y] = 0;
			}
			else {
				s[x + y * normalDim.x + z * normalDim.x * normalDim.y] = 1;
			}
		}
	}
}

__global__ void integrate(float *v, float *smoke, bool *s, uint3 normalDim, uint3 staggeredDim, float dt, float gravity, float buoyancy_alpha) {
	int x = threadIdx.x + blockDim.x * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y+1;
	int z = threadIdx.z + blockDim.z * blockIdx.z;

	if (x < normalDim.x && y < normalDim.y && z < normalDim.z) {
		if (s[x + y * normalDim.x + z * normalDim.x * normalDim.y] != 0 && s[x + (y - 1) * normalDim.x + z * normalDim.x * normalDim.y] != 0) {

			float buoyancy = buoyancy_alpha* (smoke[x + y * normalDim.x + z * normalDim.x * normalDim.y] - 0.0f);
			v[x + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] += smoke[x + y * normalDim.x + z * normalDim.x * normalDim.y] * gravity * dt + buoyancy * dt;
			//v[x + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] +=  gravity * dt + buoyancy * dt;

		}
	}
}

__global__ void velocityConfinement(float* u, float* v, float* w, uint3 normalDim, uint3 staggeredDim, float dt) {
	int x = threadIdx.x + blockDim.x * blockIdx.x +1;
	int y = threadIdx.y + blockDim.y * blockIdx.y +1;
	int z = threadIdx.z + blockDim.z * blockIdx.z +1;

	if (x < normalDim.x && y < normalDim.y && z < normalDim.z) {
		int staggeredIndex = x + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y;

		float u_t = u[staggeredIndex];
		float v_t = v[staggeredIndex];
		float w_t = w[staggeredIndex];

		float velLength = u_t * u_t + v_t * v_t + w_t * w_t;

		if (velLength * dt > MAX_VELOCITY_PER_STEP* MAX_VELOCITY_PER_STEP) {
			u[staggeredIndex] = u_t * (MAX_VELOCITY_PER_STEP * MAX_VELOCITY_PER_STEP / (velLength * dt));
			v[staggeredIndex] = v_t * (MAX_VELOCITY_PER_STEP * MAX_VELOCITY_PER_STEP / (velLength * dt));
			w[staggeredIndex] = w_t * (MAX_VELOCITY_PER_STEP * MAX_VELOCITY_PER_STEP /(velLength * dt));
		}

	}
}



__global__ void divergence(float* u, float* v, float* w, bool* s, uint3 normalDim, uint3 staggeredDim, char offset) {

	int x = threadIdx.x + blockDim.x * blockIdx.x + 1;
	int y = threadIdx.y + blockDim.y * blockIdx.y + 1;
	int z = threadIdx.z + blockDim.z * blockIdx.z + 1;

	x = x * 2 - (y + z + offset) % 2;

	if (x < normalDim.x - 1 && y < normalDim.y - 1 && z < normalDim.z - 1) {
		if (s[x + y * normalDim.x + z * normalDim.x * normalDim.y] == 0)
			return;

		int acc_s = 0;
		int sx1 = s[(x + 1) + y * normalDim.x + z * normalDim.x * normalDim.y];
		int sx0 = s[(x - 1) + y * normalDim.x + z * normalDim.x * normalDim.y];
		int sy1 = s[x + (y + 1) * normalDim.x + z * normalDim.x * normalDim.y];
		int sy0 = s[x + (y - 1) * normalDim.x + z * normalDim.x * normalDim.y];
		int sz1 = s[x + y * normalDim.x + (z + 1) * normalDim.x * normalDim.y];
		int sz0 = s[x + y * normalDim.x + (z - 1) * normalDim.x * normalDim.y];
		acc_s = sx0 + sx1 + sy0 + sy1 + sz0 + sz1;

		if (acc_s == 0) return;

		float div = -u[(x)+y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] + u[(x + 1) + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] +
			-v[x + (y)*staggeredDim.x + z * staggeredDim.x * staggeredDim.y] + v[x + (y + 1) * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] +
			-w[x + y * staggeredDim.x + (z)*staggeredDim.x * staggeredDim.y] + w[x + y * staggeredDim.x + (z + 1) * staggeredDim.x * staggeredDim.y];


		float p_t = (-div / acc_s) * OVER_RELAXATION;

		u[(x + 0) + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] -= (p_t * sx0);
		u[(x + 1) + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] += (p_t * sx1);
		v[x + (y + 0) * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] -= (p_t * sy0);
		v[x + (y + 1) * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] += (p_t * sy1);
		w[x + y * staggeredDim.x + (z + 0) * staggeredDim.x * staggeredDim.y] -= (p_t * sz0);
		w[x + y * staggeredDim.x + (z + 1) * staggeredDim.x * staggeredDim.y] += (p_t * sz1);

	}
}

__global__ void extrapolate(float* velFeild, uint3 normalDim, uint3 staggeredDim, uchar3 direction) {
	int x = threadIdx.x + blockDim.x * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y;
	int z = threadIdx.z + blockDim.z * blockIdx.z;

	
	if (x < normalDim.x && y < normalDim.y && z < normalDim.z) {
		velFeild[x + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] = velFeild[(x + direction.x) + (y + direction.y) * staggeredDim.x + (z + direction.z) * staggeredDim.x * staggeredDim.y];
		velFeild[(staggeredDim.x-1) + (staggeredDim.y - 1) * staggeredDim.x + (staggeredDim.z - 1) * staggeredDim.x * staggeredDim.y] = velFeild[((staggeredDim.x - 1) - direction.x) + ((staggeredDim.y - 1) - direction.y) * staggeredDim.x + ((staggeredDim.z - 1) - direction.z) * staggeredDim.x * staggeredDim.y];
	}
}


__device__ float avgU(float* vel, int x, int y, int z, uint3 staggeredDim) {
	float u = 
		vel[x + y * staggeredDim.x + (z-1) * staggeredDim.x * staggeredDim.y] +
		vel[(x + 1) + y * staggeredDim.x + (z-1) * staggeredDim.x * staggeredDim.y] +
		vel[x + (y - 1) * staggeredDim.x + (z-1) * staggeredDim.x * staggeredDim.y] +
		vel[(x + 1) + (y - 1) * staggeredDim.x + (z-1) * staggeredDim.x * staggeredDim.y] +
		vel[x + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] +
		vel[(x + 1) + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] +
		vel[x + (y - 1) * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] +
		vel[(x + 1) + (y - 1) * staggeredDim.x + z * staggeredDim.x * staggeredDim.y];
	return u / 8;
}

__device__ float avgV(float* vel, int x, int y, int z, uint3 staggeredDim) {
	float v = 
		vel[x + y * staggeredDim.x + (z-1) * staggeredDim.x * staggeredDim.y] +
		vel[(x - 1) + y * staggeredDim.x + (z-1) * staggeredDim.x * staggeredDim.y] +
		vel[x + (y + 1) * staggeredDim.x + (z-1) * staggeredDim.x * staggeredDim.y] +
		vel[(x - 1) + (y + 1) * staggeredDim.x + (z-1) * staggeredDim.x * staggeredDim.y] +
		vel[x + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] +
		vel[(x - 1) + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] +
		vel[x + (y + 1) * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] +
		vel[(x - 1) + (y + 1) * staggeredDim.x + z * staggeredDim.x * staggeredDim.y];

	return v / 8;
}

__device__ float avgW(float* vel, int x, int y, int z, uint3 staggeredDim) {
	float w =
		vel[x + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] +
		vel[(x - 1) + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] +
		vel[x + (y - 1) * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] +
		vel[(x - 1) + (y - 1) * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] +
		vel[x + y * staggeredDim.x + (z-1) * staggeredDim.x * staggeredDim.y] +
		vel[(x - 1) + y * staggeredDim.x + (z-1) * staggeredDim.x * staggeredDim.y] +
		vel[x + (y - 1) * staggeredDim.x + (z-1) * staggeredDim.x * staggeredDim.y] +
		vel[(x - 1) + (y - 1) * staggeredDim.x + (z-1) * staggeredDim.x * staggeredDim.y];
	return w / 8;
}



__device__ float sampleSmoke(float* field, uint3 gridDim, float3 pos, float3 delta, uint3 normalDim) {
	
	float x = fmaxf(fminf(pos.x, normalDim.x-1), 1);
	float y = fmaxf(fminf(pos.y, normalDim.y-1), 1);
	float z = fmaxf(fminf(pos.z, normalDim.z-1), 1);

	//interpelation
	int x0 = (int)fminf(floorf(x - delta.x), normalDim.x-1);
	float xw1 = (x - delta.x) - x0;
	float xw0 = 1 - xw1;
	int x1 = (int)fminf(x0 + 1, normalDim.x - 1);

	int y0 = (int)fminf(floorf(y - delta.y), normalDim.y-1);
	float yw1 = (y - delta.y) - y0;
	float yw0 = 1 - yw1;
	int y1 = (int)fminf(y0 + 1, normalDim.y - 1);

	int z0 = (int)fminf(floorf(z - delta.z), normalDim.z-1);
	float zw1 = (z - delta.z) - z0;
	float zw0 = 1 - zw1;
	int z1 = (int)fminf(z0 + 1, normalDim.z - 1);

	return
		xw0 * yw0 * zw0 * field[x0 + y0 * gridDim.x + z0 * gridDim.x * gridDim.y] +
		xw1 * yw0 * zw0 * field[x1 + y0 * gridDim.x + z0 * gridDim.x * gridDim.y] +
		xw0 * yw1 * zw0 * field[x0 + y1 * gridDim.x + z0 * gridDim.x * gridDim.y] +
		xw1 * yw1 * zw0 * field[x1 + y1 * gridDim.x + z0 * gridDim.x * gridDim.y] +
		xw0 * yw0 * zw1 * field[x0 + y0 * gridDim.x + z1 * gridDim.x * gridDim.y] +
		xw1 * yw0 * zw1 * field[x1 + y0 * gridDim.x + z1 * gridDim.x * gridDim.y] +
		xw0 * yw1 * zw1 * field[x0 + y1 * gridDim.x + z1 * gridDim.x * gridDim.y] +
		xw1 * yw1 * zw1 * field[x1 + y1 * gridDim.x + z1 * gridDim.x * gridDim.y];
		

}

__device__ float sampleVelocity(float* field, uint3 gridDim, float3 pos, float3 delta, uint3 normalDim) {

	float x = fmaxf(fminf(pos.x, gridDim.x - 1), 1);
	float y = fmaxf(fminf(pos.y, gridDim.y - 1), 1);
	float z = fmaxf(fminf(pos.z, gridDim.z - 1), 1);

	//interpelation
	int x0 = (int)fminf(floorf(x - delta.x), gridDim.x - 1);
	float xw1 = (x - delta.x) - x0;
	float xw0 = 1 - xw1;
	int x1 = (int)fminf(x0 + 1, gridDim.x - 1);

	int y0 = (int)fminf(floorf(y - delta.y), gridDim.y - 1);
	float yw1 = (y - delta.y) - y0;
	float yw0 = 1 - yw1;
	int y1 = (int)fminf(y0 + 1, gridDim.y - 1);

	int z0 = (int)fminf(floorf(z - delta.z), gridDim.z - 1);
	float zw1 = (z - delta.z) - z0;
	float zw0 = 1 - zw1;
	int z1 = (int)fminf(z0 + 1, gridDim.z - 1);

	return
		xw0 * yw0 * zw0 * field[x0 + y0 * gridDim.x + z0 * gridDim.x * gridDim.y] +
		xw1 * yw0 * zw0 * field[x1 + y0 * gridDim.x + z0 * gridDim.x * gridDim.y] +
		xw0 * yw1 * zw0 * field[x0 + y1 * gridDim.x + z0 * gridDim.x * gridDim.y] +
		xw1 * yw1 * zw0 * field[x1 + y1 * gridDim.x + z0 * gridDim.x * gridDim.y] +
		xw0 * yw0 * zw1 * field[x0 + y0 * gridDim.x + z1 * gridDim.x * gridDim.y] +
		xw1 * yw0 * zw1 * field[x1 + y0 * gridDim.x + z1 * gridDim.x * gridDim.y] +
		xw0 * yw1 * zw1 * field[x0 + y1 * gridDim.x + z1 * gridDim.x * gridDim.y] +
		xw1 * yw1 * zw1 * field[x1 + y1 * gridDim.x + z1 * gridDim.x * gridDim.y];


}







__global__ void velocityAdvectionU(float *u0, float *u1, float *v0, float *w0, bool* s, uint3 normalDim, uint3 staggeredDim, float dt) {
	int x = threadIdx.x + blockDim.x * blockIdx.x + 1;
	int y = threadIdx.y + blockDim.y * blockIdx.y + 1;
	int z = threadIdx.z + blockDim.z * blockIdx.z + 1;

	if (x < normalDim.x && y < normalDim.y-1 && z < normalDim.z-1) {
		
		if (s[x + y * normalDim.x + z * normalDim.x * normalDim.y] != 0 && s[(x - 1) + y * normalDim.x + z * normalDim.x * normalDim.y] != 0) {
			float x_t = x;
			float y_t = y + 0.5;
			float z_t = z + 0.5; 

			float u = u0[x + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y];
			float v = avgV(v0, x, y, z, staggeredDim);
			float w = avgW(w0, x, y, z, staggeredDim);

			x_t = x_t - dt * u;
			y_t = y_t - dt * v;
			z_t = z_t - dt * w;

			float3 pos = { x_t, y_t, z_t };
			float3 delta = { 0, 0.5, 0.5 };

			float newU = sampleSmoke(u0, staggeredDim, pos, delta, normalDim);
			u1[x + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] = newU;

		}
	}
}

__global__ void velocityAdvectionV(float* v0, float* v1, float* u0, float* w0, bool* s, uint3 normalDim, uint3 staggeredDim, float dt) {
	int x = threadIdx.x + blockDim.x * blockIdx.x + 1;
	int y = threadIdx.y + blockDim.y * blockIdx.y + 1;
	int z = threadIdx.z + blockDim.z * blockIdx.z + 1;

	if (x < normalDim.x-1 && y < normalDim.y && z < normalDim.z - 1) {

		if (s[x + y * normalDim.x + z * normalDim.x * normalDim.y] != 0 && s[x + (y-1) * normalDim.x + z * normalDim.x * normalDim.y] != 0) {
			float x_t = x + 0.5;
			float y_t = y;
			float z_t = z + 0.5;

			float u = avgU(u0, x, y, z, staggeredDim);
			float v = v0[x + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y];
			float w = avgW(w0, x, y, z, staggeredDim);

			x_t = x_t - dt * u;
			y_t = y_t - dt * v;
			z_t = z_t - dt * w;

			float3 pos = { x_t, y_t, z_t };
			float3 delta = { 0.5, 0, 0.5 };

			float newU = sampleSmoke(v0, staggeredDim, pos, delta, normalDim);
			v1[x + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] = newU;

		}
	}
}

__global__ void velocityAdvectionW(float* w0, float* w1, float* u0, float* v0, bool* s, uint3 normalDim, uint3 staggeredDim, float dt) {
	int x = threadIdx.x + blockDim.x * blockIdx.x + 1;
	int y = threadIdx.y + blockDim.y * blockIdx.y + 1;
	int z = threadIdx.z + blockDim.z * blockIdx.z + 1;

	if (x < normalDim.x - 1 && y < normalDim.y - 1 && z < normalDim.z) {

		if (s[x + y * normalDim.x + z * normalDim.x * normalDim.y] != 0 && s[x + y * normalDim.x + (z-1) * normalDim.x * normalDim.y] != 0) {
			float x_t = x + 0.5;
			float y_t = y + 0.5;
			float z_t = z;

			float u = avgU(u0, x, y, z, staggeredDim);
			float v = avgV(v0, x, y, z, staggeredDim);
			float w = w0[x + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y];

			x_t = x_t - dt * u;
			y_t = y_t - dt * v;
			z_t = z_t - dt * w;

			float3 pos = { x_t, y_t, z_t };
			float3 delta = { 0.5, 0.5, 0 };

			float newU = sampleSmoke(w0, staggeredDim, pos, delta, normalDim);
			w1[x + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] = newU;

		}
	}
}

__global__ void advectSmoke(float* smoke0, float *smoke1, float* u, float* v, float* w, bool *s, uint3 normalDim, uint3 staggeredDim, float dt) {
	int x = threadIdx.x + blockDim.x * blockIdx.x+1;
	int y = threadIdx.y + blockDim.y * blockIdx.y+1;
	int z = threadIdx.z + blockDim.z * blockIdx.z+1;

	if (x < normalDim.x-1 && y < normalDim.y-1 && z < normalDim.z-1) {
		if (s[x + y * normalDim.x + z * normalDim.x * normalDim.y] != 0) {
			float u_t = (u[x + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] + u[(x + 1) + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y]) / 2;
			float v_t = (v[x + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] + v[x + (y + 1) * staggeredDim.x + z * staggeredDim.x * staggeredDim.y]) / 2;
			float w_t = (w[x + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] + w[x + y * staggeredDim.x + (z + 1) * staggeredDim.x * staggeredDim.y]) / 2;

			float x_t = x + 0.5 - u_t * dt;
			float y_t = y + 0.5 - v_t * dt;
			float z_t = z + 0.5 - w_t * dt;

			float3 pos = { x_t, y_t, z_t };
			float3 delta = { 0.5, 0.5, 0.5 };

			smoke1[x + y * normalDim.x + z * normalDim.x * normalDim.y] = sampleSmoke(smoke0, normalDim, pos, delta, normalDim); 
		}
	}
}

__global__ void copyFeildAToB(float* feildA, float* feildB, uint3 dim) {
	int x = threadIdx.x + blockDim.x * blockIdx.x + 1;
	int y = threadIdx.y + blockDim.y * blockIdx.y + 1;
	int z = threadIdx.z + blockDim.z * blockIdx.z + 1;

	if(x < dim.x && y < dim.y && z < dim.z) {
		feildB[x + y * dim.x + z * dim.x * dim.y] = feildA[x + y * dim.x + z * dim.x * dim.y];
	}
}

__global__ void visualizeV(float* v, float* smoke, uint3 normalDim, uint3 staggeredDim, uint3 dir) {
	int x = threadIdx.x + blockDim.x * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y;
	int z = threadIdx.z + blockDim.z * blockIdx.z;

	if (x < normalDim.x && y < normalDim.y && z < normalDim.z) {
		smoke[x + y * normalDim.x + z * normalDim.x * normalDim.y] =
			(v[x + y * staggeredDim.x + z * staggeredDim.x * staggeredDim.y] + v[(x+dir.x) + (y + dir.y) * staggeredDim.x + (z+dir.z) * staggeredDim.x * staggeredDim.y]) / 2;
	}
}

__global__ void visualizeS(bool *s, float* smoke, uint3 normalDim, uint3 staggeredDim) {
	int x = threadIdx.x + blockDim.x * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y;
	int z = threadIdx.z + blockDim.z * blockIdx.z;

	if (x < normalDim.x && y < normalDim.y && z < normalDim.z) {
		smoke[x + y * normalDim.x + z * normalDim.x * normalDim.y] = (float)s[x + y * normalDim.x + z * normalDim.x * normalDim.y];
	}
}


__global__ void visualizeP(float *p, float* smoke, uint3 normalDim, uint3 staggeredDim) {
	int x = threadIdx.x + blockDim.x * blockIdx.x;
	int y = threadIdx.y + blockDim.y * blockIdx.y;
	int z = threadIdx.z + blockDim.z * blockIdx.z;

	if (x < normalDim.x && y < normalDim.y && z < normalDim.z) {
		smoke[x + y * normalDim.x + z * normalDim.x * normalDim.y] = p[x + y * normalDim.x + z * normalDim.x * normalDim.y];
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

int tempIndexNow = 1;
int tempIndexPast = 0;
int velindex = 0;




void drawObjects() {


	//draw smokeSources
	std::vector<Sphere> smokeSources = getObjectsOfType(1);
	std::vector<float> smokeData;
	for (int i = 0; i < smokeSources.size(); i++) {
		smokeData.push_back(smokeSources[i].pos.x);
		smokeData.push_back(smokeSources[i].pos.y);
		smokeData.push_back(smokeSources[i].pos.z);
		smokeData.push_back(smokeSources[i].radius);
	}

	cudaError_t err;

	err = cudaMemcpy(dev_smokeSources, smokeData.data(), smokeData.size() * sizeof(float), cudaMemcpyHostToDevice);
	if (err != cudaSuccess) {
		fprintf(stderr, "Error copying data from host to device: %s\n", cudaGetErrorString(err));
		exit(-1);
	}

	dim3 dimBlock(8, 8, 8); //512 
	dim3 dimGrid(ceil(smokeDim.x / 8.0), ceil(smokeDim.y / 8.0), ceil(smokeDim.z / 8.0));


	fillSmoke << <dimGrid, dimBlock >> > (dev_smoke[0], dev_smoke[1], smokeDim, dev_smokeSources, smokeSources.size());

	smokeSources.clear();
	smokeData.clear();




	//draw obstacles
	std::vector<Sphere> obstacles = getObjectsOfType(0); 
	std::vector<float> obstaclesData;
	for (int i = 0; i < obstacles.size(); i++) {
		obstaclesData.push_back(obstacles[i].pos.x);
		obstaclesData.push_back(obstacles[i].pos.y);
		obstaclesData.push_back(obstacles[i].pos.z);
		obstaclesData.push_back(obstacles[i].vel.x);
		obstaclesData.push_back(obstacles[i].vel.y);
		obstaclesData.push_back(obstacles[i].vel.z);
		obstaclesData.push_back(obstacles[i].radius);
	}

	err = cudaMemcpy(dev_obstacles, obstaclesData.data(), obstaclesData.size() * sizeof(float), cudaMemcpyHostToDevice);
	if (err != cudaSuccess) {
		fprintf(stderr, "Error copying data from host to device: %s\n", cudaGetErrorString(err));
		exit(-1);
	}

	fillObstacle << <dimGrid, dimBlock >> > (dev_s, smokeDim, dev_obstacles, obstacles.size());

	obstacles.clear();
	obstaclesData.clear();

}


void simulate(float* smoke_grid, float dt) {
	cudaError_t err;

	tempIndexPast = tempIndexNow;
	if (tempIndexNow == 0) tempIndexNow = 1;
	else tempIndexNow = 0;


	drawObjects();


	
	dim3 dimBlock(8, 8, 8); //512 
	dim3 dimGrid(ceil(smokeDim.x / 8.0), ceil(smokeDim.y / 8.0), ceil(smokeDim.z / 8.0));
	
	integrate<<<dimGrid, dimBlock>>>(dev_v[smokeIndex], dev_smoke[tempIndexNow], dev_s, smokeDim, smokeStaggeredDim, dt, gravity, buoyancy_alpha);

	velocityConfinement << <dimGrid, dimBlock >> > (dev_u[tempIndexNow], dev_v[tempIndexNow], dev_w[tempIndexNow], smokeDim, smokeStaggeredDim, dt);



	dim3 divdimBlock(8, 8, 8); //512 
	dim3 divdimGrid(ceil(ceil(smokeDim.x / 2.0) / 8.0), ceil(smokeDim.y / 8.0), ceil(smokeDim.z / 8.0));
	int num_iterations = 30;
	for (int i = 0; i < num_iterations; i++) {
		divergence << <divdimGrid, divdimBlock >> > (dev_u[tempIndexNow], dev_v[tempIndexNow], dev_w[tempIndexNow], dev_s, smokeDim, smokeStaggeredDim, 0);
		divergence << <divdimGrid, divdimBlock >> > (dev_u[tempIndexNow], dev_v[tempIndexNow], dev_w[tempIndexNow], dev_s, smokeDim, smokeStaggeredDim, 1);
	}



	velocityAdvectionU << <dimGrid, dimBlock >> > (dev_u[tempIndexNow], dev_u[tempIndexPast], dev_v[tempIndexNow], dev_w[tempIndexNow], dev_s, smokeDim, smokeStaggeredDim, dt);
	velocityAdvectionV << <dimGrid, dimBlock >> > (dev_v[tempIndexNow], dev_v[tempIndexPast], dev_u[tempIndexNow], dev_w[tempIndexNow], dev_s, smokeDim, smokeStaggeredDim, dt);
	velocityAdvectionW << <dimGrid, dimBlock >> > (dev_w[tempIndexNow], dev_w[tempIndexPast], dev_u[tempIndexNow], dev_v[tempIndexNow], dev_s, smokeDim, smokeStaggeredDim, dt);


	advectSmoke << <dimGrid, dimBlock >> > (dev_smoke[tempIndexNow], dev_smoke[tempIndexPast], dev_u[tempIndexPast], dev_v[tempIndexPast], dev_w[tempIndexPast], dev_s, smokeDim, smokeStaggeredDim, dt);

	uint3 dir = { 0,1,0 };

	err = cudaMemcpy(smoke_grid, dev_smoke[tempIndexPast], smokeDim.x * smokeDim.y * smokeDim.z * sizeof(float), cudaMemcpyDeviceToHost);
	if (err != cudaSuccess) {
		fprintf(stderr, "Error copying data from device to host: %s\n", cudaGetErrorString(err));
		exit(-1);
	}
}

