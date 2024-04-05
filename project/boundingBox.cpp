
#include "boundingBox.h" 

#include <iostream>
#include <glm/glm.hpp>
#include <labhelper.h>

using namespace glm;


BoundingBox::BoundingBox(vec3 position, vec3 num_cells, int cell_size)
	: m_vao(UINT32_MAX)
	, m_positionBuffer(UINT32_MAX)
	, m_indexBuffer(UINT32_MAX)
{
	m_cell_size = cell_size;
	m_num_cells = num_cells;
	m_dimensions = float(m_cell_size) * m_num_cells;
	m_position = position;
	
}


void BoundingBox::generateMesh(void) {

	float positions[3*4] = {
		1,1,1,
		1,-1,1,
		1,-1,-1,
		1,1,-1
	};

	// Create a handle for the vertex array object
	glGenVertexArrays(1, &m_vao); 
	// Set it as current, i.e., related calls will affect this object
	glBindVertexArray(m_vao); 


	// Create a handle for the vertex position buffer
	glGenBuffers(1, &m_positionBuffer);
	// Set the newly created buffer as the current one
	glBindBuffer(GL_ARRAY_BUFFER, m_positionBuffer);
	// Send the vetex position data to the current buffer
	glBufferData(GL_ARRAY_BUFFER, 3*4 * sizeof(float), positions, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0); 
	glVertexAttribPointer(0, 3, GL_FLOAT, false /*normalized*/, 0 /*stride*/, 0 /*offset*/); 


	int indices[6] = {
		0, 1, 2,
		0, 2, 3
	};

	glGenBuffers(1, &m_indexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6*sizeof(int), indices, GL_STATIC_DRAW);
}


void BoundingBox::submitTriangles(void)
{
	if (m_vao == UINT32_MAX)
	{
		std::cout << "No vertex array is generated, cannot draw anything.\n";
		return;
	}

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	glBindVertexArray(m_vao);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

}