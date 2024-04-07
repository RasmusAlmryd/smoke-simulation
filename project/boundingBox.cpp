
#include "boundingBox.h" 

#include <iostream>
#include <glm/glm.hpp>
#include <labhelper.h>

using namespace glm;


BoundingBox::BoundingBox(vec3 position, vec3 num_cells, int cell_size)
	: m_vao(UINT32_MAX)
	, m_positionBuffer(UINT32_MAX)
	, m_indexBuffer(UINT32_MAX)
	, m_colorBuffer(UINT32_MAX)
	, m_texBuffer(UINT32_MAX)
{
	m_cell_size = cell_size;
	m_num_cells = num_cells;
	m_dimensions = float(m_cell_size) * m_num_cells;
	m_position = position;
	
}


void BoundingBox::generateMesh(void) {

	

	/*float positions[3 * 8] = {
		1,1,1,
		1,-1,1,
		1,-1,-1,
		1,1,-1,
		-1,1,1,
		-1,-1,1,
		-1,-1,-1,
		-1,1,-1,
	};*/

	const int pointsPerQuad = 3 * 3 * 2;
	const int positionLength = pointsPerQuad * 6;
	float positions[positionLength];

	static const GLfloat g_vertex_buffer_data[] = {
	-1.0f,-1.0f,-1.0f, // triangle 1 : begin
	-1.0f,-1.0f, 1.0f,
	-1.0f, 1.0f, 1.0f, // triangle 1 : end
	1.0f, 1.0f,-1.0f, // triangle 2 : begin
	-1.0f,-1.0f,-1.0f,
	-1.0f, 1.0f,-1.0f, // triangle 2 : end
	1.0f,-1.0f, 1.0f,
	-1.0f,-1.0f,-1.0f,
	1.0f,-1.0f,-1.0f,
	1.0f, 1.0f,-1.0f,
	1.0f,-1.0f,-1.0f,
	-1.0f,-1.0f,-1.0f,
	-1.0f,-1.0f,-1.0f,
	-1.0f, 1.0f, 1.0f,
	-1.0f, 1.0f,-1.0f,
	1.0f,-1.0f, 1.0f,
	-1.0f,-1.0f, 1.0f,
	-1.0f,-1.0f,-1.0f,
	-1.0f, 1.0f, 1.0f,
	-1.0f,-1.0f, 1.0f,
	1.0f,-1.0f, 1.0f,
	1.0f, 1.0f, 1.0f,
	1.0f,-1.0f,-1.0f,
	1.0f, 1.0f,-1.0f,
	1.0f,-1.0f,-1.0f,
	1.0f, 1.0f, 1.0f,
	1.0f,-1.0f, 1.0f,
	1.0f, 1.0f, 1.0f,
	1.0f, 1.0f,-1.0f,
	-1.0f, 1.0f,-1.0f,
	1.0f, 1.0f, 1.0f,
	-1.0f, 1.0f,-1.0f,
	-1.0f, 1.0f, 1.0f,
	1.0f, 1.0f, 1.0f,
	-1.0f, 1.0f, 1.0f,
	1.0f,-1.0f, 1.0f
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
	printf("%i", sizeof(g_vertex_buffer_data)/sizeof(float));
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0); 
	glVertexAttribPointer(0, 3, GL_FLOAT, false /*normalized*/, 0 /*stride*/, 0 /*offset*/); 

	//const float colors[8 * 3] = {
	//	//   R     G     B
	//	1.0f, 0.0f, 0.0f, 
	//	0.0f, 1.0f, 0.0f, 
	//	0.0f, 0.0f, 1.0f,
	//	1.0f, 0.0f, 0.0f,
	//	0.0f, 1.0f, 0.0f,
	//	0.0f, 0.0f, 1.0f,
	//	1.0f, 0.0f, 0.0f,
	//	0.0f, 1.0f, 0.0f
	//};

	//glGenBuffers(1, &m_colorBuffer);
	//glBindBuffer(GL_ARRAY_BUFFER, m_colorBuffer);
	//glBufferData(GL_ARRAY_BUFFER, sizeof(colors), colors, GL_STATIC_DRAW);
	//glVertexAttribPointer(1, 3, GL_FLOAT, false /*normalized*/, 0 /*stride*/, 0 /*offset*/); 
	//glEnableVertexAttribArray(1);

	static const GLfloat texcoords[] = {

		0.5f, 0.5f,
		0.5f, 0.5f,
		0.5f, 0.5f,

		0.0f, 0.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,

		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,

		0.0f, 0.0f,
		0.0f, 1.0f,
		1.0f, 1.0f,

		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,

		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,

		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,

		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,

		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,

		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,

		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,

		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,

	};

	//const float texcoords[8 * 3] = {
	//	//   R     G     B
	//	0.0f, 0.0f,
	//	0.0f, 1.0f,
	//	1.0f, 1.0f,
	//	1.0f, 0.0f,
	//	0.0f, 0.0f,
	//	0.0f, 1.0f,
	//	1.0f, 1.0f,
	//	1.0f, 0.0f,
	//};

	glGenBuffers(1, &m_texBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, m_texBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(texcoords), texcoords, GL_STATIC_DRAW);
	glVertexAttribPointer(2, 2, GL_FLOAT, false /*normalized*/, 0 /*stride*/, 0 /*offset*/);
	glEnableVertexAttribArray(2);
	


	/*int indices[3 * 2 * 6] = {
		0, 1, 2,
		0, 2, 3,
		4, 6, 5,
		4, 7, 6,
		1, 6, 2,
		1, 5, 6,
		2, 7, 3,
		2, 6, 7,
		0, 7, 4,
		0, 3, 7,
		0, 4, 5,
		0, 5, 1,
	};

	glGenBuffers(1, &m_indexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);*/
}

void addQuadPoints(float positions[], int offset, vec3 p0, vec3 p1, vec3 p2, vec3 p3) {
	float points[3 * 3 * 2] = {
		//first triangle
		p0.x, p0.y, p0.z,
		p1.x, p1.y, p1.z,
		p2.x, p2.y, p2.z,

		// second triangle
		p0.x, p0.y, p0.z,
		p2.x, p2.y, p2.z,
		p3.x, p3.y, p3.z,
	};

	for (int i = 0; i < 3 * 3 * 2; i++) {
		positions[offset + i] = points[i];
	}
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
	//glDrawElements(GL_TRIANGLES, 108, GL_UNSIGNED_INT, 0);
	glDrawArrays(GL_TRIANGLES, 0, 12 * 3);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

}