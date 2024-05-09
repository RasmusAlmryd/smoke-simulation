
#include <glm/vec3.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GL/glew.h>
#include <vector>


using namespace glm;

#ifndef BOUNDINGBOX_H
#define BOUNDINGBOX_H

struct IntVec3
{
	int  x, y, z;

	vec3 getGlmVec(void) {
		return vec3(x, y, z);
	}
};

class BoundingBox {
public:
	GLuint m_vao;
	GLuint m_positionBuffer;
	GLuint m_indexBuffer;
	GLuint m_colorBuffer;
	GLuint m_texBuffer;
	GLuint m_gridTex;
	
	
	std::vector<float> m_grid;
	//float* m_grid;

	vec3 m_dimensions; // x,y,z dimensions
	IntVec3 m_num_cells; // number of cells in x,y,z directions
	int m_cell_size; // size of cell in x,y,z (all dims are equal)

	vec3 m_position; // x,y,z position of bounding box center

	mat4 m_modelMatrix;

	BoundingBox(vec3 position, IntVec3 num_cells, int cell_size);

	// Update volume grid dimensions and generate new grid
	void updateGridDim(IntVec3 num_cells);

	void initGrid();

	// Generate mesh
	void generateMesh(void);

	// Generate volume texture
	void generateVolumeTex(void);

	void uppdateVolumeTexture(void);

	void updateVolume(float dt);


	void submitTriangles(void);

	// Free allocated memory by grid
	void freeGrid(void);
};



#endif // !

