
#include "boundingBox.h" 

#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <set>
#include "smokeSimulation.cuh"
#include <algorithm> 



using namespace glm;


BoundingBox::BoundingBox(vec3 position, IntVec3 num_cells, int cell_size)
	: m_vao(UINT32_MAX)
	, m_positionBuffer(UINT32_MAX)
	, m_indexBuffer(UINT32_MAX)
	, m_colorBuffer(UINT32_MAX)
	, m_texBuffer(UINT32_MAX)
	, m_gridTex(UINT32_MAX)
	, m_proxy_vao(UINT32_MAX)
	, m_proxy_indexBuffer(UINT32_MAX)
	, m_proxy_positionBuffer(UINT32_MAX)
{
	m_cell_size = cell_size;
	m_num_cells = { 0,0,0 };
	updateGridDim(num_cells);
	//initGrid();
	initializeVolume(m_grid.data(), m_num_cells.x, m_num_cells.y, m_num_cells.z);

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

void BoundingBox::initGrid() {
	//int y = m_num_cells.y - 2;
	int hx = m_num_cells.x / 2;
	int hz = m_num_cells.z / 2;
	for (int x = 1; x < m_num_cells.x-1; x++) {
		for (int y = m_num_cells.y-6; y < m_num_cells.y-2; y++) {
			for (int z = 1; z < m_num_cells.z - 1; z++) {
				if (abs(x - hx) < 3 && abs(z - hz) < 3) m_grid[x + y * m_num_cells.x + z * m_num_cells.x * m_num_cells.y] = 1.0;
			}
		}
	}
}

float pseudoangle2(float y, float x)
{
	return sign(y) * (1. - x / (abs(x) + abs(y)));
}

float pseudoangle(float y, float x)
{
	float r = x / (abs(x) + abs(y));
	if (y < 0) return 1. + r;
	return 3. - r;
	//return sign(y)* (1. - x / (abs(x) + abs(y)));
	//float p = x / (abs(x) + abs(y)); //  - 1 .. 1 increasing with x
	//if (y < 0) return p - 1;			//  - 2 .. 0 increasing with x
	//return 1 - p;  //  0 .. 2 decreasing with x
	//return copysign(1. - x / (fabs(x) + fabs(y)), y);
}

void BoundingBox::generateMesh(void) {


	/*static const GLfloat positions[] = {
	-1.0, -1.0,  1.0,
	1.0, -1.0,  1.0,
	-1.0,  1.0,  1.0,
	1.0,  1.0,  1.0,
	-1.0, -1.0, -1.0,
	1.0, -1.0, -1.0,
	-1.0,  1.0, -1.0,
	1.0,  1.0, -1.0,
	};*/

	const vec3 box_positions[8] = {
		vec3(-1.0, -1.0,  1.0),
		vec3(1.0, -1.0,  1.0),
		vec3(-1.0,  1.0,  1.0),
		vec3(1.0,  1.0,  1.0),
		vec3(-1.0, -1.0, -1.0),
		vec3(1.0, -1.0, -1.0),
		vec3(-1.0,  1.0, -1.0),
		vec3(1.0,  1.0, -1.0),
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
	glBufferData(GL_ARRAY_BUFFER, sizeof(box_positions), box_positions, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0); 
	glVertexAttribPointer(0, 3, GL_FLOAT, false /*normalized*/, 0 /*stride*/, 0 /*offset*/); 


	static const GLushort indices[] = {
	0, 1, 2, 3, 7, 1, 5, 4, 7, 6, 2, 4, 0, 1
	};
	
	glGenBuffers(1, &m_indexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	

	
}

void BoundingBox::generateProxyGeometry(std::vector<vec3>& vertices, std::vector<short>& indices, vec3 viewOrigin, vec3 viewDirection, int steps) {

	vec3 inRayOrigin = viewOrigin;
	vec3 inRayDirection = -viewDirection;
	inRayDirection = normalize(inRayDirection);

	m_planeIndexing.clear();

	//int steps = 50;
	float minDist;
	float maxDist;

	const vec3 box_positions[8] = {
		vec3(-1.0, -1.0,  1.0) * m_dimensions / 2.0f,
		vec3(1.0, -1.0,  1.0) * m_dimensions / 2.0f,
		vec3(-1.0,  1.0,  1.0) * m_dimensions / 2.0f,
		vec3(1.0,  1.0,  1.0) * m_dimensions / 2.0f,
		vec3(-1.0, -1.0, -1.0) * m_dimensions / 2.0f,
		vec3(1.0, -1.0, -1.0) * m_dimensions / 2.0f,
		vec3(-1.0,  1.0, -1.0) * m_dimensions / 2.0f,
		vec3(1.0,  1.0, -1.0) * m_dimensions / 2.0f,
	};

	getMinMaxPoints(minDist, maxDist, box_positions, m_dimensions, inRayOrigin, inRayDirection);

	vec3 start = inRayOrigin + inRayDirection * minDist;
	vec3 end = inRayOrigin + inRayDirection * maxDist;


	for (float step = 0.5; step < steps; step++) {
		float t = (step / (float)(steps));

		vec3 planePos = start + (end - start) * t;
		std::vector<vec3> newPoints = planeBoxIntersections(m_position, m_dimensions, planePos, inRayDirection);


		sortPoints(newPoints, newPoints.size() - 1, newPoints[newPoints.size() - 1], inRayDirection);

		PlaneIndexValue pv = { newPoints.size() - 1, indices.size() / 3 };
		m_planeIndexing.push_back(pv);

		std::vector<short> newIndices = teselate(newPoints, vertices.size());

		vertices.insert(vertices.end(), newPoints.begin(), newPoints.end());
		newPoints.clear();

		indices.insert(indices.end(), newIndices.begin(), newIndices.end());
		newIndices.clear();

	}
}


void BoundingBox::initProxyGeometry(vec3 viewOrigin, vec3 viewDirection, int steps) {
	
	std::vector<vec3> vertices;
	std::vector<short> triangleIndices;

	generateProxyGeometry(vertices, triangleIndices, viewOrigin, viewDirection, steps);

	m_num_proxy_triangles = triangleIndices.size() / 3;

	glGenVertexArrays(1, &m_proxy_vao);
	glBindVertexArray(m_proxy_vao); 


	glGenBuffers(1, &m_proxy_positionBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, m_proxy_positionBuffer);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vec3), vertices.data(), GL_STATIC_DRAW); 
	glEnableVertexAttribArray(0); 
	glVertexAttribPointer(0, 3, GL_FLOAT, false /*normalized*/, 0 /*stride*/, 0 /*offset*/); 



	glGenBuffers(1, &m_proxy_indexBuffer); 
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_proxy_indexBuffer); 
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, triangleIndices.size() * sizeof(short), triangleIndices.data(), GL_STATIC_DRAW); 
}

void BoundingBox::updateProxyGeometry(vec3 viewOrigin, vec3 viewDirection, int steps) {
	std::vector<vec3> vertices; 
	std::vector<short> triangleIndices; 

	generateProxyGeometry(vertices, triangleIndices, viewOrigin, viewDirection, steps); 

	m_num_proxy_triangles = triangleIndices.size() / 3; 



	glBindBuffer(GL_ARRAY_BUFFER, m_proxy_positionBuffer); 
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vec3), vertices.data(), GL_STATIC_DRAW); 

 
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_proxy_indexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, triangleIndices.size() * sizeof(short), triangleIndices.data(), GL_STATIC_DRAW); 
}


void getMinMaxPoints(float &min, float &max, const vec3 points[8], vec3 boxSize, vec3 vievOrigin, vec3 viewDir) {
	 
	 viewDir = normalize(viewDir);
	 min = dot(points[0] - vievOrigin, viewDir);
	 max = min;
	for (int i = 0; i < 8; i++) {
		float dist = dot(points[i] - vievOrigin, viewDir);
		if (dist < min) min = dist;
		if (dist > max) max = dist;
	}
}

std::vector<short> teselate(std::vector<vec3> points, int offset) {
	std::vector<short> triangleIndices;

	for (int i = 0; i < points.size() - 2; i++) {
		triangleIndices.push_back(points.size() - 1 + offset);
		triangleIndices.push_back(i + offset);
		triangleIndices.push_back(i + 1 + offset);
	}
	triangleIndices.push_back(points.size() - 1 + offset);
	triangleIndices.push_back(points.size() - 2 + offset);
	triangleIndices.push_back(0 + offset);
	return triangleIndices;
}



void sortPoints(std::vector<vec3> &points, int numPoints, vec3 planeOrigin, vec3 planeNormal) {
	planeNormal = normalize(planeNormal);
	vec3 ax = normalize(points[0]- planeOrigin);
	vec3 ay = cross(planeNormal, ax);//vec3(0, 1, 0) - planeNormal * dot(vec3(0, 1, 0), planeNormal);
	std::sort(points.begin(), points.begin() + numPoints, [&](const vec3& p1, const vec3& p2) {
		vec3 v1 = p1 - planeOrigin;
		vec3 v2 = p2 - planeOrigin;
		vec2 pos1(dot(ax, v1), dot(ay, v1));
		vec2 pos2(dot(ax, v2), dot(ay, v2));
		float a1 = pseudoangle(pos1.y, pos1.x);
		float a2 = pseudoangle(pos2.y, pos2.x);
		return a1 > a2;
	});
}

bool rayPlaneIntersection(vec3 &intersectionPoint, vec3 rayOrigin, vec3 rayDir, float max_t, vec3 planeOrigin, vec3 planeNormal) {

	//if (dot(rayDir, planeNormal) < 0.001) return false;
	 
	float t = dot((planeOrigin - rayOrigin), planeNormal) / dot(rayDir, planeNormal);
	if (t > max_t || t < 0) return false;

	intersectionPoint = rayOrigin + rayDir * t;

	return true;
}

std::vector<vec3> planeBoxIntersections(vec3 boxPosition, vec3 boxSize, vec3 planeOrigin, vec3 planeNormal) {
	
	std::vector<vec3> points;
	vec3 halfBoxSize = boxSize / 2.0f;

	vec3 avgPoint(0, 0, 0);

	
	//X-axis
	for (int y = -1; y < 2; y+=2) {
		for (int z = -1; z < 2; z+=2) {
			vec3 point;
			if (rayPlaneIntersection(
				point,
				boxPosition - vec3(halfBoxSize.x, halfBoxSize.y*y, halfBoxSize.z*z),
				vec3(1, 0, 0),
				boxSize.x,
				planeOrigin,
				planeNormal
			)) {
				if (duplicate(point, points)) continue;
				points.push_back(point);
				avgPoint += point;
			}
		}
	}

	//Y-axis
	for (int x = -1; x < 2; x += 2) {
		for (int z = -1; z < 2; z += 2) { 
			vec3 point; 
			if (rayPlaneIntersection(  
				point,  
				boxPosition - vec3(halfBoxSize.x*x, halfBoxSize.y, halfBoxSize.z * z), 
				vec3(0, 1, 0),
				boxSize.y,  
				planeOrigin,  
				planeNormal  
			)) { 
				if (duplicate(point, points)) continue;
				points.push_back(point);
				avgPoint += point; 
			}
		}
	}

	//Z-axis
	for (int y = -1; y < 2; y += 2) {
		for (int x = -1; x < 2; x += 2) {
			vec3 point;
			if (rayPlaneIntersection(
				point,
				boxPosition - vec3(halfBoxSize.x*x, halfBoxSize.y * y, halfBoxSize.z),
				vec3(0, 0, 1),
				boxSize.z,
				planeOrigin,
				planeNormal
			)) {
				if (duplicate(point, points)) continue;
				points.push_back(point);
				avgPoint += point; 
			}
		}
	}

	
	avgPoint /= points.size();
	points.push_back(avgPoint);
	return points;
}

bool duplicate(vec3 point, std::vector<vec3> points) {
	for (int i = 0; i < points.size(); i++) {
		if (point.x == points[i].x && point.y == points[i].y && point.z == points[i].z) return true;
	} 
	return false; 
}


void BoundingBox::generateVolumeTex(void) {


	//printf("%d %d %d", m_num_cells.x, m_num_cells.y, m_num_cells.z);
	//printf("tot len: %d", m_num_cells.x * m_num_cells.y * m_num_cells.z);

	/*for (int x = 0; x < m_num_cells.x; x++) {
		for (int y = 0; y < m_num_cells.y; y++) {
			for (int z = 0; z < m_num_cells.z; z++) {
				if ((x == 0 || x == m_num_cells.x - 1) && (y == 0 || y == m_num_cells.y - 1) && (z == 0 || z == m_num_cells.z - 1)) {
					m_grid[x + y * m_num_cells.x + z * m_num_cells.x * m_num_cells.y] = 1.0f;
				}
				else if(x==y)
					m_grid[x + y * m_num_cells.x + z * m_num_cells.x * m_num_cells.y] = 1.0f;

			}
		}
	}*/



	glGenTextures(1, &m_gridTex);
	glBindTexture(GL_TEXTURE_3D, m_gridTex);


	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, m_num_cells.x, m_num_cells.y, m_num_cells.z, 0, GL_RED, GL_FLOAT, m_grid.data());

}

void BoundingBox::uppdateVolumeTexture() {

	glBindTexture(GL_TEXTURE_3D, m_gridTex);
	glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, m_num_cells.x, m_num_cells.y, m_num_cells.z, GL_RED, GL_FLOAT, m_grid.data());

}

void BoundingBox::updateVolume(float dt) {
	int x = 4;
	int y = 4; 
	int z = 4;
	simulate(m_grid.data(), dt);

	uppdateVolumeTexture();
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
	deleteVolume();
	printf("volume cleared");
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

void BoundingBox::submitProxyGeometry(void)
{
	if (m_proxy_vao == UINT32_MAX)
	{
		std::cout << "No vertex array is generated, cannot draw anything.\n";
		return;
	}

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	glBindVertexArray(m_proxy_vao);
	//glDrawElements(GL_TRIANGLES, 108, GL_UNSIGNED_INT, 0);
	//glDrawArrays(GL_TRIANGLES, 0, 12 * 3);
	//glDrawArrays(GL_TRIANGLES, 0, m_num_proxy_triangles * 3);
	//glDrawElements(GL_TRIANGLES, m_num_proxy_triangles, GL_UNSIGNED_SHORT, 0);
	glDrawElements(GL_TRIANGLES, m_num_proxy_triangles*3, GL_UNSIGNED_SHORT, 0);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

}

void BoundingBox::submitProxyPlane(int planeIndex) {
	if (m_proxy_vao == UINT32_MAX)
	{
		std::cout << "No vertex array is generated, cannot draw anything.\n";
		return;
	}

	
	if (planeIndex >= m_planeIndexing.size() || planeIndex < 0) {
		std::cout << "plane index out of range \n";
		return;
	}


	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	glBindVertexArray(m_proxy_vao);

	PlaneIndexValue iv = m_planeIndexing[planeIndex];

	glDrawElements(GL_TRIANGLES, iv.numTriangles * 3, GL_UNSIGNED_SHORT, (void*)(iv.triangleOffset * 3 * sizeof(GLushort))); 

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}