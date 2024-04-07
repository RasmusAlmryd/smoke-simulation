
#include <glm/vec3.hpp>
#include <GL/glew.h>


using namespace glm;

#ifndef BOUNDINGBOX_H
#define BOUNDINGBOX_H

class BoundingBox {
public:
	GLuint m_vao;
	GLuint m_positionBuffer;
	GLuint m_indexBuffer;
	GLuint m_colorBuffer;
	GLuint m_texBuffer;


	vec3 m_dimensions; // x,y,z dimensions
	vec3 m_num_cells; // number of cells in x,y,z directions
	int m_cell_size; // size of cell in x,y,z (all dims are equal)

	vec3 m_position; // x,y,z position of bounding box center

	BoundingBox(vec3 position, vec3 num_cells, int cell_size);

	// Generate mesh
	void generateMesh(void);

	void submitTriangles(void);
};



#endif // !

