
#ifdef _WIN32
extern "C" _declspec(dllexport) unsigned int NvOptimusEnablement = 0x00000001;
#endif

#include <GL/glew.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <chrono>

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
vec3 point_light_color = vec3(0.9882f, 0.9333f, 0.6549f);

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

int count = 0;

float generalUpdateDt = 0.0f;
const int generalUpdatesPerSecond = 20;

bool drawing_volume = false;

vec3 spherePos;
 
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

mat4 roomModelMatrix;
mat4 landingPadModelMatrix;
mat4 fighterModelMatrix;

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

	roomModelMatrix = mat4(1.0f);
	fighterModelMatrix = translate(15.0f * worldUp);
	landingPadModelMatrix = mat4(1.0f);

	///////////////////////////////////////////////////////////////////////
	// Load environment map
	///////////////////////////////////////////////////////////////////////
	environmentMap = labhelper::loadHdrTexture("../scenes/envmaps/" + envmap_base_name + ".hdr");

	boundingBox.generateMesh();
	boundingBox.initProxyGeometry(cameraPosition, cameraDirection, 100);
	boundingBox.generateVolumeTex();

	boundingBox.updateVolume(0.01f);

	spherePos = vec3(boundingBox.m_num_cells.getGlmVec()/2.0f);


	glEnable(GL_DEPTH_TEST); // enable Z-buffering
	glEnable(GL_CULL_FACE);  // enables backface culling
	//glDisable(GL_CULL_FACE);



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
	labhelper::setUniformSlow(lightVolumeShaderProgram, "lightViewProjectionMatrix", lightProjMatrix * lightViewMatrix);
	labhelper::setUniformSlow(lightVolumeShaderProgram, "lightViewMatrix", lightViewMatrix);
	labhelper::setUniformSlow(lightVolumeShaderProgram, "volume_pos", boundingBox.m_position);
	glEnable(GL_BLEND);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	boundingBox.submitProxyPlane(planeIndex);
}


void drawTexVolume(const mat4& viewMatrix, const mat4& projectionMatrix, bool backToFront) {

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



	glBindTexture(GL_TEXTURE_2D, viewFB.depthBuffer);

	glBindFramebuffer(GL_FRAMEBUFFER, viewFB.framebufferId);
	glViewport(0, 0, viewFB.width, viewFB.height);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	

	//if (backToFront)
	//{
	//	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	//}
	//else {
	//	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	//	glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);
	//}

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	

	/*glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, windowWidth, windowHeight);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);*/

	glUseProgram(texVolumeShaderProgram);
	//glEnable(GL_BLEND);
	//glDisable(GL_CULL_FACE); 
	//glDisable(GL_DEPTH_TEST);
	//glDepthMask(GL_FALSE);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//

	
	mat4 modelMatrix(1.0f);
	labhelper::setUniformSlow(texVolumeShaderProgram, "modelViewProjectionMatrix", projectionMatrix * viewMatrix * modelMatrix);
	labhelper::setUniformSlow(texVolumeShaderProgram, "modelViewMatrix", viewMatrix * modelMatrix);


	labhelper::setUniformSlow(texVolumeShaderProgram, "lightViewProjectionMatrix", lightProjMatrix * lightViewMatrix);
	mat4 lightMatrix = translate(vec3(0.5f)) * scale(vec3(0.5f)) * lightProjMatrix * lightViewMatrix *inverse(viewMatrix);
	labhelper::setUniformSlow(texVolumeShaderProgram, "lightMatrix", lightMatrix);

	labhelper::setUniformSlow(texVolumeShaderProgram, "volume_pos", boundingBox.m_position);
	labhelper::setUniformSlow(texVolumeShaderProgram, "backToFront", backToFront);


	////
	////labhelper::setUniformSlow(volumeShaderProgram, "camera_pos", cameraPosition);

	//

	glActiveTexture(GL_TEXTURE8);
	glBindTexture(GL_TEXTURE_3D, boundingBox.m_gridTex);

	/*twoPassDraw(viewMatrix, projectionMatrix, backToFront, 5);
	twoPassDraw(viewMatrix, projectionMatrix, backToFront, 6);
	twoPassDraw(viewMatrix, projectionMatrix, backToFront, 7);
	twoPassDraw(viewMatrix, projectionMatrix, backToFront, 8);*/


	for (int i = 0; i < boundingBox.m_planeIndexing.size(); i++) {
		twoPassDraw(viewMatrix, projectionMatrix, backToFront, i);
	}

	//boundingBox.submitProxyGeometry();
	//
	//printf("size : %d\n", boundingBox.m_planeIndexing.size());
	/*for (int i = 0; i < boundingBox.m_planeIndexing.size(); i++) {
		boundingBox.submitProxyPlane(i);
	}*/



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


///////////////////////////////////////////////////////////////////////////////
/// This function is used to draw the main objects on the scene
///////////////////////////////////////////////////////////////////////////////
void drawScene(GLuint currentShaderProgram,
               const mat4& viewMatrix,
               const mat4& projectionMatrix,
               const mat4& lightViewMatrix,
               const mat4& lightProjectionMatrix)
{
	glUseProgram(currentShaderProgram);
	// Light source
	vec4 viewSpaceLightPosition = viewMatrix * vec4(lightPosition, 1.0f);
	labhelper::setUniformSlow(currentShaderProgram, "point_light_color", point_light_color);
	labhelper::setUniformSlow(currentShaderProgram, "point_light_intensity_multiplier",
	                          point_light_intensity_multiplier);
	labhelper::setUniformSlow(currentShaderProgram, "viewSpaceLightPosition", vec3(viewSpaceLightPosition));
	labhelper::setUniformSlow(currentShaderProgram, "viewSpaceLightDir",
	                          normalize(vec3(viewMatrix * vec4(-lightPosition, 0.0f))));


	// Environment
	labhelper::setUniformSlow(currentShaderProgram, "environment_multiplier", environment_multiplier);

	// camera
	labhelper::setUniformSlow(currentShaderProgram, "viewInverse", inverse(viewMatrix));

	// landing pad
	labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
	                          projectionMatrix * viewMatrix * landingPadModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix", viewMatrix * landingPadModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "normalMatrix",
	                          inverse(transpose(viewMatrix * landingPadModelMatrix)));

	labhelper::render(landingpadModel);

	// Fighter
	labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
	                          projectionMatrix * viewMatrix * fighterModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix", viewMatrix * fighterModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "normalMatrix",
	                          inverse(transpose(viewMatrix * fighterModelMatrix)));

	labhelper::render(fighterModel);
}


///////////////////////////////////////////////////////////////////////////////
/// This function will be called once per frame, so the code to set up
/// the scene for rendering should go here
///////////////////////////////////////////////////////////////////////////////
void display(void)
{
	labhelper::perf::Scope s( "Display" );

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

	vec4 lightStartPosition = vec4(80.0f, 40.0f, 0.0f, 1.0f);
	lightPosition = vec3(lightStartPosition);
	//lightPosition = vec3(rotate(currentTime, worldUp) * lightStartPosition);
	lightViewMatrix = lookAt(lightPosition, boundingBox.m_position, worldUp);
	//lightProjMatrix = perspective(radians(45.0f), 1.0f, 25.0f, 100.0f);
	lightProjMatrix = perspective(radians(45.0f), 1.0f, 5.0f, 2000.0f);

	if (simRunning) {
		vec4 sphereStartPos = vec4(7, 0, 0, 1);
		vec3 newSpherePos = spherePos + vec3(rotate(currentTime*5, worldUp) * sphereStartPos );
		//printf("x:%f, y:%f, z:%f\n", newSpherePos.x, newSpherePos.y, newSpherePos.z);
		setSpherePos(newSpherePos.x, newSpherePos.y, newSpherePos.z);

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
	glClearColor(0.2f, 0.2f, 0.8f, 1.0f);
	//glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	{
		labhelper::perf::Scope s( "Background" );
		drawBackground(viewMatrix, projMatrix);
	}
	{
		labhelper::perf::Scope s( "Scene" );
		//drawScene( shaderProgram, viewMatrix, projMatrix, lightViewMatrix, lightProjMatrix );
	}
	{
		labhelper::perf::Scope s("volume");
		//drawVolume(viewMatrix, projMatrix); 
	}

	{
		labhelper::perf::Scope s("proxy"); 

		if (viewFB.width != windowWidth || viewFB.height != windowHeight)
		{
			viewFB.resize(windowWidth, windowHeight);
		}

		//glBindTexture(GL_TEXTURE_2D, viewFB.depthBuffer);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		//glBindTexture(GL_TEXTURE_2D, 0);

		//glBindTexture(GL_TEXTURE_2D, viewFB.depthBuffer);

		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); 
		
		vec3 cameraView = (boundingBox.m_position - cameraPosition);
		vec3 lightView = (boundingBox.m_position - lightPosition);
		bool backToFront = dot(cameraView, lightView) < 0.0;

		//drawing_volume = true;
		drawTexVolume(viewMatrix, projMatrix, backToFront);
		//drawing_volume = false;
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

		//printf("drwaing %d \n", drawing_volume);
		boundingBox.updateProxyGeometry(vec3(0.0), -halfView, 100);
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
