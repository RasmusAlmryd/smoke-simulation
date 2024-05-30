
#ifdef _WIN32
extern "C" _declspec(dllexport) unsigned int NvOptimusEnablement = 0x00000001;
#endif

#include <GL/glew.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <vector>

#include <labhelper.h>
#include <imgui.h>

#include <perf.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
using namespace glm;

#include <Model.h>
#include "hdr.h"
#include "fbo.h"

#include <iostream>

#include "boundingBox.h"

#include "smokeSimulation.cuh"


///////////////////////////////////////////////////////////////////////////////
// Various globals
///////////////////////////////////////////////////////////////////////////////
SDL_Window* g_window = nullptr;
float currentTime = 0.0f;
float previousTime = 0.0f;
float deltaTime = 0.0f;
int windowWidth, windowHeight;

// Mouse input
ivec2 g_prevMouseCoords = { -1, -1 };
bool g_isMouseDragging = false;

// Debug 
bool debug = true;

///////////////////////////////////////////////////////////////////////////////
// Shader programs
///////////////////////////////////////////////////////////////////////////////
GLuint shaderProgram;       // Shader for rendering the final image
GLuint simpleShaderProgram; // Shader used to draw the shadow map
GLuint backgroundProgram;
GLuint boundingShaderProgram;
GLuint volumeShaderProgram;
GLuint texVolumeShaderProgram;
GLuint lightVolumeShaderProgram;
GLuint testShaderProgram;

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
float environment_multiplier = 1.5f;
GLuint environmentMap;
const std::string envmap_base_name = "001";

///////////////////////////////////////////////////////////////////////////////
// Light source
///////////////////////////////////////////////////////////////////////////////
vec3 lightPosition;
//vec3 point_light_color = vec3(1.f, 1.f, 1.f);
vec3 point_light_color = vec3(0.9882f, 0.9333f, 0.7549f);

float point_light_intensity_multiplier = 10000.0f;

mat4 lightViewMatrix;
mat4 lightProjMatrix;

FboInfo lightMapFB;
int lightMapResolution = 512;

///////////////////////////////////////////////////////////////////////////////
// BoundingBox
///////////////////////////////////////////////////////////////////////////////
IntVec3 num_cells = { 80,80,80};
BoundingBox boundingBox(vec3(0,0,0), num_cells, 1);

GLuint gridTex;
float simDeltaTime = 0.0f;
bool simRunning = false;
const float simUpdatesPerSecond = 20;

bool proxy_active = true;
bool proxy_update = true;

int proxy_step_count = 100;

int count = 0;

float generalUpdateDt = 0.0f;
const int generalUpdatesPerSecond = 20;

bool drawing_volume = false;

bool animating_smoke = false;

vec3 smokePos;
int smokeSourceId1;
int sourceId2;

vec3 obstaclePos;
float obstacleRadius;
int obstacleId1;
int obstacleId2;

///////////////////////////////////////////////////////////////////////////////
// Floor
///////////////////////////////////////////////////////////////////////////////
int floorWidth = 1000;
int floorDepth = 1000;
vec3 floorPos(0.0f,-40.0f,0.0f);
GLuint floorVAO;
GLuint floorPB;
GLuint floorIB;

 
///////////////////////////////////////////////////////////////////////////////
// Camera parameters.
///////////////////////////////////////////////////////////////////////////////
//vec3 cameraPosition(-70.0f, 50.0f, 70.0f);
vec3 cameraPosition(1.0f, 20.0f, 100.0f);
vec3 cameraDirection = normalize(vec3(0.0f) - cameraPosition);
float cameraSpeed = 13.f;
float cameraSpeedBoost = cameraSpeed * 4;

vec3 worldUp(0.0f, 1.0f, 0.0f);

FboInfo viewFB;

///////////////////////////////////////////////////////////////////////////////
// Models
///////////////////////////////////////////////////////////////////////////////
labhelper::Model* fighterModel = nullptr;
labhelper::Model* landingpadModel = nullptr;
labhelper::Model* sphereModel = nullptr;

labhelper::Texture floorTexture;

mat4 roomModelMatrix;
mat4 landingPadModelMatrix;
mat4 fighterModelMatrix;


///////////////////////////////////////////////////////////////////////////////
// Render Options
///////////////////////////////////////////////////////////////////////////////

bool add_shadows = false;
float shadowAlpha = 0.4f;
float imGuiTempColor[3] = { point_light_color.x, point_light_color.y, point_light_color.z };


void loadShaders(bool is_reload)
{
	GLuint shader = labhelper::loadShaderProgram("../project/simple.vert", "../project/simple.frag", is_reload);
	if(shader != 0)
	{
		simpleShaderProgram = shader;
	}

	shader = labhelper::loadShaderProgram("../project/background.vert", "../project/background.frag", is_reload);
	if(shader != 0)
	{
		backgroundProgram = shader;
	}

	shader = labhelper::loadShaderProgram("../project/shading.vert", "../project/shading.frag", is_reload);
	if(shader != 0)
	{
		shaderProgram = shader;
	}

	shader = labhelper::loadShaderProgram("../project/boundingBox.vert", "../project/boundingBox.frag" , is_reload);
	if (shader != 0)
	{
		std::cout << "shader" << std::endl;
		boundingShaderProgram = shader;
	}

	shader = labhelper::loadShaderProgram("../project/volume.vert", "../project/volume.frag", is_reload);
	if (shader != 0)
	{
		std::cout << "shader" << std::endl;
		volumeShaderProgram = shader;
	}

	shader = labhelper::loadShaderProgram("../project/texVolume.vert", "../project/texVolume.frag", is_reload);
	if (shader != 0)
	{
		std::cout << "shader" << std::endl;
		texVolumeShaderProgram = shader;
	}

	shader = labhelper::loadShaderProgram("../project/light.vert", "../project/light.frag", is_reload);
	if (shader != 0)
	{
		std::cout << "shader" << std::endl;
		lightVolumeShaderProgram = shader;
	}

	shader = labhelper::loadShaderProgram("../project/test.vert", "../project/test.frag", is_reload);
	if (shader != 0)
	{
		std::cout << "shader" << std::endl;
		testShaderProgram = shader;
	}
}

void initFloor() {

	float halfWidth = floorWidth / 2.0f;
	float halfDepth = floorDepth / 2.0f;

	float vertices[] = {
		halfWidth + floorPos.x, floorPos.y, halfDepth + floorPos.z,
		halfWidth + floorPos.x, floorPos.y, -halfDepth + floorPos.z,
		-halfWidth + floorPos.x, floorPos.y, -halfDepth + floorPos.z,
		-halfWidth + floorPos.x, floorPos.y, halfDepth + floorPos.z,
	};


	glGenVertexArrays(1, &floorVAO);
	glBindVertexArray(floorVAO);


	glGenBuffers(1, &floorPB);
	glBindBuffer(GL_ARRAY_BUFFER, floorPB);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3*4, vertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, false /*normalized*/, 0 /*stride*/, 0 /*offset*/);



	int indices[] = {
		0, 1, 2,
		2, 3, 0  
	};

	glGenBuffers(1, &floorIB);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, floorIB);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(int) * 3* 2 , indices, GL_STATIC_DRAW);
}


///////////////////////////////////////////////////////////////////////////////
/// This function is called once at the start of the program and never again
///////////////////////////////////////////////////////////////////////////////
void initialize()
{
	ENSURE_INITIALIZE_ONLY_ONCE();

	getGPUProperties();

	///////////////////////////////////////////////////////////////////////
	//		Load Shaders
	///////////////////////////////////////////////////////////////////////
	loadShaders(false);

	///////////////////////////////////////////////////////////////////////
	// Load models and set up model matrices
	///////////////////////////////////////////////////////////////////////
	fighterModel = labhelper::loadModelFromOBJ("../scenes/NewShip.obj");
	landingpadModel = labhelper::loadModelFromOBJ("../scenes/landingpad.obj");
	sphereModel = labhelper::loadModelFromOBJ("../scenes/sphere.obj");

	floorTexture.load("../scenes/", "floor.png", 3);
	initFloor();




	printf("sphere: %d", sphereModel->m_vaob);

	roomModelMatrix = mat4(1.0f);
	fighterModelMatrix = translate(15.0f * worldUp);
	landingPadModelMatrix = mat4(1.0f);

	///////////////////////////////////////////////////////////////////////
	// Load environment map
	///////////////////////////////////////////////////////////////////////
	environmentMap = labhelper::loadHdrTexture("../scenes/envmaps/" + envmap_base_name + ".hdr");

	boundingBox.generateMesh();
	boundingBox.initProxyGeometry(cameraPosition, cameraDirection, proxy_step_count);
	boundingBox.generateVolumeTex();


	smokePos = vec3(boundingBox.m_num_cells.getGlmVec()/2.0f);
	smokeSourceId1 = addSmokeSource(smokePos.x, smokePos.y, smokePos.z, 5);

	obstacleId2 = addObstacle(60,10,60, 0, 0, 0, 13);

	boundingBox.updateVolume(0.01f);

	glEnable(GL_DEPTH_TEST); // enable Z-buffering
	glEnable(GL_CULL_FACE);  // enables backface culling


	lightMapFB.resize(lightMapResolution, lightMapResolution);
	

	glBindTexture(GL_TEXTURE_2D, lightMapFB.depthBuffer);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); 

	viewFB.resize(lightMapResolution, lightMapResolution);

	glBindTexture(GL_TEXTURE_2D, viewFB.depthBuffer);

	printf("light id: %d, view id: %d\n", viewFB.framebufferId, lightMapFB.framebufferId);
}

void debugDrawLight(const glm::mat4& viewMatrix,
                    const glm::mat4& projectionMatrix,
                    const glm::vec3& worldSpaceLightPos)
{
	mat4 modelMatrix = glm::translate(worldSpaceLightPos);
	glUseProgram(shaderProgram);
	labhelper::setUniformSlow(shaderProgram, "modelViewProjectionMatrix",
	                          projectionMatrix * viewMatrix * modelMatrix);
	
	labhelper::render(sphereModel);
}

void drawObstacle(const glm::mat4& viewMatrix,
	const glm::mat4& projectionMatrix,
	const glm::vec3& worldSpaceObstaclePos,
	float radius) {
	float width = radius * 2;
	mat4 modelMatrix = glm::scale(translate(worldSpaceObstaclePos),vec3(width) * 0.2f);

	glUseProgram(shaderProgram);
	labhelper::setUniformSlow(shaderProgram, "modelViewProjectionMatrix",
		projectionMatrix * viewMatrix * modelMatrix);
	labhelper::setUniformSlow(shaderProgram, "material_color", vec3(1.0f));
	labhelper::render(sphereModel);
}


void drawBackground(const mat4& viewMatrix, const mat4& projectionMatrix)
{
	glUseProgram(backgroundProgram);
	labhelper::setUniformSlow(backgroundProgram, "environment_multiplier", environment_multiplier);
	labhelper::setUniformSlow(backgroundProgram, "inv_PV", inverse(projectionMatrix * viewMatrix));
	labhelper::setUniformSlow(backgroundProgram, "camera_pos", cameraPosition);
	labhelper::drawFullScreenQuad();
}

void drawVolume(const mat4& viewMatrix, const mat4& projectionMatrix) {
	glUseProgram(volumeShaderProgram);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	labhelper::setUniformSlow(volumeShaderProgram, "modelViewProjectionMatrix", projectionMatrix * viewMatrix * boundingBox.m_modelMatrix);

	labhelper::setUniformSlow(volumeShaderProgram, "modelViewMatrix", viewMatrix * boundingBox.m_modelMatrix);
	labhelper::setUniformSlow(volumeShaderProgram, "camera_pos", cameraPosition);


	glActiveTexture(GL_TEXTURE8); 
	glBindTexture(GL_TEXTURE_3D, boundingBox.m_gridTex);

	boundingBox.submitTriangles(); 
	glDisable(GL_BLEND);
}


void twoPassDraw(const mat4& viewMatrix, const mat4& projectionMatrix, bool backToFront, int planeIndex) {


	///////////////////////////////////////////////////////////////////////
	// Cmaera view pass
	///////////////////////////////////////////////////////////////////////
	glBindFramebuffer(GL_FRAMEBUFFER, viewFB.framebufferId);
	glViewport(0, 0, viewFB.width, viewFB.height);

	glUseProgram(texVolumeShaderProgram);
	glEnable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	if (backToFront)
	{
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	}
	else {
		glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);
	}

	glActiveTexture(GL_TEXTURE8);
	glBindTexture(GL_TEXTURE_3D, boundingBox.m_gridTex);

	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_2D, lightMapFB.colorTextureTargets[0]);

	boundingBox.submitProxyPlane(planeIndex);



	///////////////////////////////////////////////////////////////////////
	// Light view pass
	///////////////////////////////////////////////////////////////////////
	glBindFramebuffer(GL_FRAMEBUFFER, lightMapFB.framebufferId); 
	glViewport(0, 0, lightMapFB.width, lightMapFB.height);

	glUseProgram(lightVolumeShaderProgram);
	glEnable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	boundingBox.submitProxyPlane(planeIndex);
}


void drawTexVolume(const mat4& viewMatrix, const mat4& projectionMatrix, bool backToFront) {


	///////////////////////////////////////////////////////////////////////
	// Configuration for light view pass
	///////////////////////////////////////////////////////////////////////
	glBindTexture(GL_TEXTURE_2D, lightMapFB.depthBuffer);

	glBindFramebuffer(GL_FRAMEBUFFER, lightMapFB.framebufferId); 
	glViewport(0, 0, lightMapFB.width, lightMapFB.height); 
	glClearColor(point_light_color.x, point_light_color.y, point_light_color.z, 1.0); 
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	glActiveTexture(GL_TEXTURE8);
	glBindTexture(GL_TEXTURE_3D, boundingBox.m_gridTex);

	glUseProgram(lightVolumeShaderProgram);
	labhelper::setUniformSlow(lightVolumeShaderProgram, "lightViewProjectionMatrix", lightProjMatrix * lightViewMatrix);
	labhelper::setUniformSlow(lightVolumeShaderProgram, "lightViewMatrix", lightViewMatrix);
	labhelper::setUniformSlow(lightVolumeShaderProgram, "volume_pos", boundingBox.m_position);




	///////////////////////////////////////////////////////////////////////
	// Configuration for camera view pass
	///////////////////////////////////////////////////////////////////////

	glBindTexture(GL_TEXTURE_2D, viewFB.depthBuffer);

	glBindFramebuffer(GL_FRAMEBUFFER, viewFB.framebufferId);
	glViewport(0, 0, viewFB.width, viewFB.height);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	

	glUseProgram(texVolumeShaderProgram);

	
	mat4 modelMatrix(1.0f);
	labhelper::setUniformSlow(texVolumeShaderProgram, "modelViewProjectionMatrix", projectionMatrix * viewMatrix * modelMatrix);
	labhelper::setUniformSlow(texVolumeShaderProgram, "modelViewMatrix", viewMatrix * modelMatrix);


	labhelper::setUniformSlow(texVolumeShaderProgram, "lightViewProjectionMatrix", lightProjMatrix * lightViewMatrix);
	mat4 lightMatrix = translate(vec3(0.5f)) * scale(vec3(0.5f)) * lightProjMatrix * lightViewMatrix *inverse(viewMatrix);
	labhelper::setUniformSlow(texVolumeShaderProgram, "lightMatrix", lightMatrix);

	labhelper::setUniformSlow(texVolumeShaderProgram, "volume_pos", boundingBox.m_position);
	labhelper::setUniformSlow(texVolumeShaderProgram, "backToFront", backToFront);


	glActiveTexture(GL_TEXTURE8);
	glBindTexture(GL_TEXTURE_3D, boundingBox.m_gridTex);


	for (int i = 0; i < boundingBox.m_planeIndexing.size(); i++) {
		twoPassDraw(viewMatrix, projectionMatrix, backToFront, i);
	}




	///////////////////////////////////////////////////////////////////////
	// Rendering Two-Pass resoult on screen quad
	///////////////////////////////////////////////////////////////////////

	glUseProgram(testShaderProgram);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, windowWidth, windowHeight);

	labhelper::setUniformSlow(testShaderProgram, "backToFront", backToFront);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glActiveTexture(GL_TEXTURE10);
	glBindTexture(GL_TEXTURE_2D, viewFB.colorTextureTargets[0]);

	labhelper::drawFullScreenQuad();

	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
}



void drawBoundingBox(const mat4& viewMatrix, const mat4& projectionMatrix) {
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
	glUseProgram(boundingShaderProgram);



	labhelper::setUniformSlow(boundingShaderProgram, "modelViewProjectionMatrix", projectionMatrix * viewMatrix * boundingBox.m_modelMatrix);
	
	glActiveTexture(GL_TEXTURE8);
	glBindTexture(GL_TEXTURE_3D, boundingBox.m_gridTex);
	
	boundingBox.submitTriangles();
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
}


void submitFloor(const mat4& viewMatrix,
	const mat4& projectionMatrix) {
	//glDisable(GL_CULL_FACE);

	glUseProgram(simpleShaderProgram);
	vec3 color = vec3(1, 1, 1);

	mat4 modelMatrix(1.0f);
	labhelper::setUniformSlow(simpleShaderProgram, "modelViewProjectionMatrix", projectionMatrix * viewMatrix * modelMatrix);
	labhelper::setUniformSlow(simpleShaderProgram, "modelViewMatrix", viewMatrix * modelMatrix);
	labhelper::setUniformSlow(simpleShaderProgram, "material_color", color);
	mat4 lightMatrix = translate(vec3(0.5f)) * scale(vec3(0.5f)) * lightProjMatrix * lightViewMatrix * inverse(viewMatrix); 
	labhelper::setUniformSlow(simpleShaderProgram, "lightMatrix", lightMatrix);
	labhelper::setUniformSlow(simpleShaderProgram, "point_light_color", point_light_color);
	labhelper::setUniformSlow(simpleShaderProgram, "add_shadow", add_shadows);
	labhelper::setUniformSlow(simpleShaderProgram, "shadowAlpha", shadowAlpha);

	glActiveTexture(GL_TEXTURE0); 
	glBindTexture(GL_TEXTURE_2D, floorTexture.gl_id); 

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_2D, lightMapFB.colorTextureTargets[0]);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glBindVertexArray(floorVAO);
	glDrawElements(GL_TRIANGLES, 2 * 3, GL_UNSIGNED_INT, 0);

	//glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	//glEnable(GL_CULL_FACE);
}




///////////////////////////////////////////////////////////////////////////////
/// This function will be called once per frame, so the code to set up
/// the scene for rendering should go here
///////////////////////////////////////////////////////////////////////////////
void display(void)
{
	labhelper::perf::Scope s( "Display" );

	//Set light color;
	point_light_color = vec3(imGuiTempColor[0], imGuiTempColor[1], imGuiTempColor[2]);

	///////////////////////////////////////////////////////////////////////////
	// Check if window size has changed and resize buffers as needed
	///////////////////////////////////////////////////////////////////////////
	{
		int w, h;
		SDL_GetWindowSize(g_window, &w, &h);
		if(w != windowWidth || h != windowHeight)
		{
			windowWidth = w;
			windowHeight = h;
		}
	}

	

	///////////////////////////////////////////////////////////////////////////
	// setup matrices
	///////////////////////////////////////////////////////////////////////////
	mat4 projMatrix = perspective(radians(45.0f), float(windowWidth) / float(windowHeight), 5.0f, 2000.0f);
	mat4 viewMatrix = lookAt(cameraPosition, cameraPosition + cameraDirection, worldUp);

	vec4 lightStartPosition = vec4(120.0f, 80.0f, 0.0f, 1.0f);
	lightPosition = vec3(lightStartPosition);
	//lightPosition = vec3(rotate(currentTime, worldUp) * lightStartPosition);
	lightViewMatrix = lookAt(lightPosition, boundingBox.m_position, worldUp);
	//lightProjMatrix = perspective(radians(45.0f), 1.0f, 25.0f, 100.0f);
	lightProjMatrix = perspective(radians(45.0f), 1.0f, 5.0f, 2000.0f);



	if (animating_smoke) {
		vec4 sphereStartPos = vec4(20, 0, 0, 1);
		vec3 newSpherePos;
		newSpherePos = smokePos + vec3(rotate(currentTime*2, worldUp) * sphereStartPos );
		updateObjectPos(smokeSourceId1, newSpherePos.x, newSpherePos.y, newSpherePos.z);
	}
	else {
		vec3 newSpherePos = boundingBox.m_num_cells.getGlmVec()/2.0f;
		updateObjectPos(smokeSourceId1, newSpherePos.x, newSpherePos.y, newSpherePos.z);
	}


	///////////////////////////////////////////////////////////////////////////
	// Bind the environment map(s) to unused texture units
	///////////////////////////////////////////////////////////////////////////
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, environmentMap);
	glActiveTexture(GL_TEXTURE0);


	///////////////////////////////////////////////////////////////////////////
	// Draw from camera
	///////////////////////////////////////////////////////////////////////////
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, windowWidth, windowHeight);
	vec3 bg_color = vec3(40.0f / 255.0f);
	glClearColor(bg_color.x, bg_color.y, bg_color.z, 1.0f);
	//glClearColor(0.2f, 0.2f, 0.8f, 1.0f);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	{
		labhelper::perf::Scope s( "Background" );
		drawBackground(viewMatrix, projMatrix);
	}
	{
		labhelper::perf::Scope s( "Scene" );
		submitFloor(viewMatrix, projMatrix);
	}


	{
		labhelper::perf::Scope s("volume");

		if (viewFB.width != windowWidth || viewFB.height != windowHeight)
		{
			viewFB.resize(windowWidth, windowHeight);
		}
		
		vec3 cameraView = (boundingBox.m_position - cameraPosition);
		vec3 lightView = (boundingBox.m_position - lightPosition);
		bool backToFront = dot(cameraView, lightView) < 0.0;

		drawTexVolume(viewMatrix, projMatrix, backToFront);

	}

	if (debug) {
		drawBoundingBox(viewMatrix, projMatrix);
		debugDrawLight(viewMatrix, projMatrix, vec3(lightPosition));
	}

}

void update(float dt) {

	if (proxy_active) {
		vec3 cameraView = (boundingBox.m_position - cameraPosition);
		vec3 lightView = (boundingBox.m_position - lightPosition);

		cameraView = normalize(cameraView);
		lightView = normalize(lightView);

		vec3 halfView = cameraView + lightView;
		if (dot(cameraView, lightView) < 0.0) halfView = -cameraView + lightView;

		
		boundingBox.updateProxyGeometry(vec3(0.0), -halfView, proxy_step_count);
		proxy_update = false;
	}
}


///////////////////////////////////////////////////////////////////////////////
/// This function is used to update the scene according to user input
///////////////////////////////////////////////////////////////////////////////
bool handleEvents(void)
{
	labhelper::perf::Scope s("Events");
	// check events (keyboard among other)
	SDL_Event event;
	bool quitEvent = false;
	while(SDL_PollEvent(&event))
	{
		labhelper::processEvent( &event );

		if(event.type == SDL_QUIT || (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE))
		{
			quitEvent = true;
		}
		if(event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_g)
		{
			if ( labhelper::isGUIvisible() )
			{
				labhelper::hideGUI(); 
			}
			else
			{
				labhelper::showGUI();
			}
		}
		if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_b) {
			debug = !debug;
		}
		if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_u) {
			float dt = 0.01f;
			boundingBox.updateVolume(dt);

		}
		if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_c) {
			proxy_active = !proxy_active;
			//boundingBox.updateProxyGeometry(cameraPosition, cameraDirection);
		}
		if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_r) {
			simRunning = !simRunning;
			printf("start sim: %d\n", simRunning);
		}
		if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
		   && (!labhelper::isGUIvisible() || !ImGui::GetIO().WantCaptureMouse))
		{
			g_isMouseDragging = true;
			int x;
			int y;
			SDL_GetMouseState(&x, &y);
			g_prevMouseCoords.x = x;
			g_prevMouseCoords.y = y;
		}

		if(!(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)))
		{
			g_isMouseDragging = false;
		}

		if(event.type == SDL_MOUSEMOTION && g_isMouseDragging)
		{
			// More info at https://wiki.libsdl.org/SDL_MouseMotionEvent
			int delta_x = event.motion.x - g_prevMouseCoords.x;
			int delta_y = event.motion.y - g_prevMouseCoords.y;
			float rotationSpeed = 0.2f;
			mat4 yaw = rotate(rotationSpeed * deltaTime * -delta_x, worldUp);
			mat4 pitch = rotate(rotationSpeed * deltaTime * -delta_y,
			                    normalize(cross(cameraDirection, worldUp)));
			cameraDirection = vec3(pitch * yaw * vec4(cameraDirection, 0.0f));
			g_prevMouseCoords.x = event.motion.x;
			g_prevMouseCoords.y = event.motion.y;

			/*proxy_update = true;*/
		}
	}

	// check keyboard state (which keys are still pressed)
	const uint8_t* state = SDL_GetKeyboardState(nullptr);
	vec3 cameraRight = cross(cameraDirection, worldUp);
	float currentCameraSpeed = cameraSpeed;
	if (state[SDL_SCANCODE_LSHIFT])
	{
		currentCameraSpeed = cameraSpeedBoost;
	}
	if(state[SDL_SCANCODE_W])
	{
		cameraPosition += currentCameraSpeed * deltaTime * cameraDirection;
		proxy_update = true;
	}
	if(state[SDL_SCANCODE_S])
	{
		cameraPosition -= currentCameraSpeed * deltaTime * cameraDirection;
		proxy_update = true;
	}
	if(state[SDL_SCANCODE_A])
	{
		cameraPosition -= currentCameraSpeed * deltaTime * cameraRight;
		proxy_update = true;
	}
	if(state[SDL_SCANCODE_D])
	{
		cameraPosition += currentCameraSpeed * deltaTime * cameraRight;
		proxy_update = true;
	}
	if(state[SDL_SCANCODE_Q])
	{
		cameraPosition -= currentCameraSpeed * deltaTime * worldUp;
		proxy_update = true;
	}
	if(state[SDL_SCANCODE_E])
	{
		cameraPosition += currentCameraSpeed * deltaTime * worldUp;
		proxy_update = true;
	}

	return quitEvent;
}


///////////////////////////////////////////////////////////////////////////////
/// This function is to hold the general GUI logic
///////////////////////////////////////////////////////////////////////////////
void gui()
{

	// ----------------- Set variables --------------------------
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS) , ACTUAL: %.3f", 1000.0f / ImGui::GetIO().Framerate,
	            ImGui::GetIO().Framerate, deltaTime);

	ImGui::Text("Simulation dimensions: x: %d, y: %d, z: %d", boundingBox.m_num_cells.x, boundingBox.m_num_cells.y, boundingBox.m_num_cells.z);

	ImGui::Checkbox("Add shadow to environment: ", &add_shadows);
	ImGui::SliderFloat("Added shadow alpha:", &shadowAlpha, 0.0f, 1.0f);

	ImGui::SliderFloat("Gravity:", getGravity(), -15.0f, 15.0f);
	ImGui::SliderFloat("Buoyancy:", getBuoyancy(), 0.0f, 20.0f);

	ImGui::Checkbox("Animate smoke:", &animating_smoke);

	ImGui::ColorPicker3("Light color: ", &imGuiTempColor[0]);

	ImGui::SliderInt("Total number proxy planes:", &proxy_step_count, 20, 200);
	// ----------------------------------------------------------

	////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////

	labhelper::perf::drawEventsWindow();
}

int main(int argc, char* argv[])
{
	g_window = labhelper::init_window_SDL("Smoke simulation [CUDA,OpenGL]");

	initialize();

	bool stopRendering = false;
	auto startTime = std::chrono::system_clock::now();

	labhelper::hideGUI();

	while(!stopRendering)
	{
		//update currentTime
		std::chrono::duration<float> timeSinceStart = std::chrono::system_clock::now() - startTime;
		previousTime = currentTime;
		currentTime = timeSinceStart.count();
		deltaTime = currentTime - previousTime;

		if (simRunning) simDeltaTime += deltaTime;
		else simDeltaTime = 0.0f;
		generalUpdateDt += deltaTime;


		// check events (keyboard among other)
		stopRendering = handleEvents();

		// Inform imgui of new frame
		labhelper::newFrame( g_window );

		// render to window
		display();
		

		// Render overlay GUI.
		gui();


		// Update simulation
		{
			labhelper::perf::Scope s("Simulation");
			if (simDeltaTime > (1.0f / simUpdatesPerSecond) && simRunning) {
				
				boundingBox.updateVolume(simDeltaTime);
				simDeltaTime = 0.0f;
			}
		}

		// General update call
		{
			labhelper::perf::Scope s("proxy update");
			if (generalUpdateDt > (1.0f / generalUpdatesPerSecond)) {

				update(generalUpdateDt);
				generalUpdateDt = 0.0f;
			}
		}


		// Finish the frame and render the GUI
		labhelper::finishFrame();

		// Swap front and back buffer. This frame will now been displayed.
		SDL_GL_SwapWindow(g_window);
	}
	// Free Models
	labhelper::freeModel(fighterModel);
	labhelper::freeModel(landingpadModel);
	labhelper::freeModel(sphereModel);

	boundingBox.freeGrid();

	// Shut down everything. This includes the window and all other subsystems.
	labhelper::shutDown(g_window);
	return 0;
}
