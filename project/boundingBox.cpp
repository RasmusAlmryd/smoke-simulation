
#include "boundingBox.h" 

#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <labhelper.h>
#include <vector>



using namespace glm;


BoundingBox::BoundingBox(vec3 position, IntVec3 num_cells, int cell_size)
	: m_vao(UINT32_MAX)
	, m_positionBuffer(UINT32_MAX)
	, m_indexBuffer(UINT32_MAX)
	, m_colorBuffer(UINT32_MAX)
	, m_texBuffer(UINT32_MAX)
	, m_gridTex(UINT32_MAX)
{
	m_cell_size = cell_size;
	m_num_cells = { 0,0,0 };
	updateGridDim(num_cells);

	//printf(num_cells)

	//m_num_cells = num_cells;
	m_dimensions = float(m_cell_size) * num_cells.getGlmVec();
	m_position = position;
	printf("init done");
	
	m_modelMatrix = scale(mat4(1.0f), 0.5f * m_dimensions);
	//m_modelMatrix = scale()
}

void BoundingBox::updateGridDim(IntVec3 num_cells) {

	//float* temp_grid = (float*) malloc(sizeof(float) * num_cells.x * num_cells.y * num_cells.z);

	std::vector<float> temp_grid(num_cells.x * num_cells.y * num_cells.z);

	for (int x = 0; x < num_cells.x; x++) {
		for (int y = 0; y < num_cells.y; y++) {
			for (int z = 0; z < num_cells.z; z++) {

				if (x < m_num_cells.x &&
					y < m_num_cells.y &&
					z < m_num_cells.z)
					temp_grid[x + y * num_cells.x + z * num_cells.x * num_cells.y ] = m_grid[x + y * m_num_cells.x + z * m_num_cells.x * m_num_cells.y];
				else
					temp_grid[x + y * num_cells.x + z * num_cells.x * num_cells.y] = 0.0f;
			}
		}
	}

	//free(m_grid);
	m_grid.clear();
	m_grid = temp_grid;
	m_num_cells.x = num_cells.x;
	m_num_cells.y = num_cells.y;
	m_num_cells.z = num_cells.z;
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

	//const int pointsPerQuad = 3 * 3 * 2;
	//const int positionLength = pointsPerQuad * 6;
	//float positions[positionLength];

	//static const GLfloat g_vertex_buffer_data[] = {
	//-1.0f,-1.0f,-1.0f, // triangle 1 : begin
	//-1.0f,-1.0f, 1.0f,
	//-1.0f, 1.0f, 1.0f, // triangle 1 : end
	//1.0f, 1.0f,-1.0f, // triangle 2 : begin
	//-1.0f,-1.0f,-1.0f,
	//-1.0f, 1.0f,-1.0f, // triangle 2 : end
	//1.0f,-1.0f, 1.0f,
	//-1.0f,-1.0f,-1.0f,
	//1.0f,-1.0f,-1.0f,
	//1.0f, 1.0f,-1.0f,
	//1.0f,-1.0f,-1.0f,
	//-1.0f,-1.0f,-1.0f,
	//-1.0f,-1.0f,-1.0f,
	//-1.0f, 1.0f, 1.0f,
	//-1.0f, 1.0f,-1.0f,
	//1.0f,-1.0f, 1.0f,
	//-1.0f,-1.0f, 1.0f,
	//-1.0f,-1.0f,-1.0f,
	//-1.0f, 1.0f, 1.0f,
	//-1.0f,-1.0f, 1.0f,
	//1.0f,-1.0f, 1.0f,
	//1.0f, 1.0f, 1.0f,
	//1.0f,-1.0f,-1.0f,
	//1.0f, 1.0f,-1.0f,
	//1.0f,-1.0f,-1.0f,
	//1.0f, 1.0f, 1.0f,
	//1.0f,-1.0f, 1.0f,
	//1.0f, 1.0f, 1.0f,
	//1.0f, 1.0f,-1.0f,
	//-1.0f, 1.0f,-1.0f,
	//1.0f, 1.0f, 1.0f,
	//-1.0f, 1.0f,-1.0f,
	//-1.0f, 1.0f, 1.0f,
	//1.0f, 1.0f, 1.0f,
	//-1.0f, 1.0f, 1.0f,
	//1.0f,-1.0f, 1.0f
	//};

	static const GLfloat positions[] = {
	-1.0, -1.0,  1.0,
	1.0, -1.0,  1.0,
	-1.0,  1.0,  1.0,
	1.0,  1.0,  1.0,
	-1.0, -1.0, -1.0,
	1.0, -1.0, -1.0,
	-1.0,  1.0, -1.0,
	1.0,  1.0, -1.0,
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
	glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);
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

	/*static const GLfloat texcoords[] = {

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

	};*/

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

	//glGenBuffers(1, &m_texBuffer);
	//glBindBuffer(GL_ARRAY_BUFFER, m_texBuffer);
	//glBufferData(GL_ARRAY_BUFFER, sizeof(texcoords), texcoords, GL_STATIC_DRAW);
	//glVertexAttribPointer(2, 2, GL_FLOAT, false /*normalized*/, 0 /*stride*/, 0 /*offset*/);
	//glEnableVertexAttribArray(2);
	


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
	};*/

	static const GLushort indices[] = {
	0, 1, 2, 3, 7, 1, 5, 4, 7, 6, 2, 4, 0, 1
	};
	
	glGenBuffers(1, &m_indexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);



	
	
	
	
}


void BoundingBox::generateVolumeTex(void) {


	//printf("%d %d %d", m_num_cells.x, m_num_cells.y, m_num_cells.z);
	//printf("tot len: %d", m_num_cells.x * m_num_cells.y * m_num_cells.z);

	for (int x = 0; x < m_num_cells.x; x++) {
		for (int y = 0; y < m_num_cells.y; y++) {
			for (int z = 0; z < m_num_cells.z; z++) {
				if ((x == 0 || x == m_num_cells.x - 1) && (y == 0 || y == m_num_cells.y - 1) && (z == 0 || z == m_num_cells.z - 1)) {
					m_grid[x + y * m_num_cells.x + z * m_num_cells.x * m_num_cells.y] = 1.0f;
				}
				if(x==y)
					m_grid[x + y * m_num_cells.x + z * m_num_cells.x * m_num_cells.y] = 1.0f;
			}
		}
	}



	glGenTextures(1, &m_gridTex);
	glBindTexture(GL_TEXTURE_3D, m_gridTex);


	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, m_num_cells.x, m_num_cells.y, m_num_cells.z, 0, GL_RED, GL_FLOAT, &m_grid[0]);

}

void BoundingBox::uppdateVolumeTexture() {
	for (int x = 0; x < m_num_cells.x; x++) {
		for (int y = m_num_cells.y-1; y >= 0; y--) {
			for (int z = 0; z < m_num_cells.z; z++) {
				if (y != 0) {
					if (m_grid[x + (y - 1) * m_num_cells.x + z * m_num_cells.x * m_num_cells.y] > 0.0f) {
						m_grid[x + y * m_num_cells.x + z * m_num_cells.x * m_num_cells.y] = 1.0f;
					}
					else {
						m_grid[x + y * m_num_cells.x + z * m_num_cells.x * m_num_cells.y] = 0.0f;
					}
				}
				else {
					m_grid[x + y * m_num_cells.x + z * m_num_cells.x * m_num_cells.y] = 0.0f;
				}
			}
		}
	}

	glBindTexture(GL_TEXTURE_3D, m_gridTex);
	glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, m_num_cells.x, m_num_cells.y, m_num_cells.z, GL_RED, GL_FLOAT, &m_grid[0]);

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

void BoundingBox::freeGrid(void) {
	//free(m_grid);
	m_grid.clear();
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
	//glDrawArrays(GL_TRIANGLES, 0, 12 * 3);
	glDrawElements(GL_TRIANGLE_STRIP, 14, GL_UNSIGNED_SHORT, 0);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

}