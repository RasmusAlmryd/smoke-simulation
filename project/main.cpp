
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
bool debug = false;

///////////////////////////////////////////////////////////////////////////////
// Shader programs
///////////////////////////////////////////////////////////////////////////////
GLuint shaderProgram;       // Shader for rendering the final image
GLuint simpleShaderProgram; // Shader used to draw the shadow map
GLuint backgroundProgram;
GLuint boundingShaderProgram;
GLuint volumeShaderProgram;
GLuint texVolumeShaderProgram;

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
vec3 point_light_color = vec3(1.f, 1.f, 1.f);

float point_light_intensity_multiplier = 10000.0f;

///////////////////////////////////////////////////////////////////////////////
// BoundingBox
///////////////////////////////////////////////////////////////////////////////
IntVec3 num_cells = { 48,48,48};
BoundingBox boundingBox(vec3(0,0,0), num_cells, 1);

GLuint gridTex;
float simDeltaTime = 0.0f;
bool simRunning = false;
const float simUpdatesPerSecond = 20;

bool proxy_active = false;
bool proxy_update = false;

int count = 0;
 
///////////////////////////////////////////////////////////////////////////////
// Camera parameters.
///////////////////////////////////////////////////////////////////////////////
//vec3 cameraPosition(-70.0f, 50.0f, 70.0f);
vec3 cameraPosition(1.0f, 7.0f, 18.0f);
vec3 cameraDirection = normalize(vec3(0.0f) - cameraPosition);
float cameraSpeed = 13.f;
float cameraSpeedBoost = cameraSpeed * 4;

vec3 worldUp(0.0f, 1.0f, 0.0f);

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
	boundingBox.initProxyGeometry(cameraPosition, cameraDirection);
	boundingBox.generateVolumeTex();



	glEnable(GL_DEPTH_TEST); // enable Z-buffering
	glEnable(GL_CULL_FACE);  // enables backface culling
	//glDisable(GL_CULL_FACE);
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

void drawTexVolume(const mat4& viewMatrix, const mat4& projectionMatrix, bool backToFront) {
	glUseProgram(texVolumeShaderProgram);
	glEnable(GL_BLEND);
	glDisable(GL_CULL_FACE); 
	glDisable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	labhelper::setUniformSlow(texVolumeShaderProgram, "modelViewProjectionMatrix", projectionMatrix * viewMatrix);
	labhelper::setUniformSlow(texVolumeShaderProgram, "volume_pos", boundingBox.m_position);

	//labhelper::setUniformSlow(volumeShaderProgram, "modelViewMatrix", viewMatrix * boundingBox.m_modelMatrix);
	//labhelper::setUniformSlow(volumeShaderProgram, "camera_pos", cameraPosition);


	glActiveTexture(GL_TEXTURE8);
	glBindTexture(GL_TEXTURE_3D, boundingBox.m_gridTex);

	boundingBox.submitProxyGeometry();
	//boundingBox.submitProxyPlane(count);
	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE); 
	glEnable(GL_DEPTH_TEST);
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

	vec4 lightStartPosition = vec4(40.0f, 40.0f, 0.0f, 1.0f);
	lightPosition = vec3(rotate(currentTime, worldUp) * lightStartPosition);
	mat4 lightViewMatrix = lookAt(lightPosition, vec3(0.0f), worldUp);
	mat4 lightProjMatrix = perspective(radians(45.0f), 1.0f, 25.0f, 100.0f);

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
		if (proxy_active) {
			vec3 cameraView = (boundingBox.m_position - cameraPosition);
			vec3 lightView = (boundingBox.m_position - lightPosition);

			vec3 halfView = cameraView + lightView;
			if (dot(cameraView, lightView) < 0.0) halfView = -cameraView + lightView;

			
			boundingBox.updateProxyGeometry(vec3(0.0), halfView);
			proxy_update = false;
		}

		drawTexVolume(viewMatrix, projMatrix, true);
	}

	if (debug) {
		drawBoundingBox(viewMatrix, projMatrix);
		
		debugDrawLight(viewMatrix, projMatrix, vec3(lightPosition));
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

	vec3 cameraView = (boundingBox.m_position - cameraPosition);
	vec3 lightView = (boundingBox.m_position - lightPosition);

	vec3 halfView = cameraView + lightView;
	if (dot(cameraView, lightView) < 0.0) halfView = -cameraView + lightView;
	ImGui::Text("halfView | x: %f, y: %f, z: %f, dot: %f", halfView.x, halfView.y, halfView.z, dot(cameraView, lightView));
	////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////

	labhelper::perf::drawEventsWindow();
}

int main(int argc, char* argv[])
{
	g_window = labhelper::init_window_SDL("OpenGL Project");

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
