#include "application.h"
#include "utils.h"
#include "mesh.h"
#include "texture.h"

#include "fbo.h"
#include "shader.h"
#include "input.h"
#include "includes.h"
#include "prefab.h"
#include "gltf_loader.h"
#include "renderer.h"
#include "scene.h"

#include <cmath>
#include <string>
#include <cstdio>
#include <iostream>  

#include <cstdlib>
#include <time.h>
#include "sphericalharmonics.h"

Application* Application::instance = nullptr;




GTR::Renderer* renderer = nullptr;
FBO* fbo = nullptr;

float cam_speed = 10;

Mesh ground;
GTR::Material ground_material;







Application::Application(int window_width, int window_height, SDL_Window* window)
{
	this->window_width = window_width;
	this->window_height = window_height;
	this->window = window;
	instance = this;
	must_exit = false;
	render_debug = true;
	render_gui = true;

	render_wireframe = false;

	fps = 0;
	frame = 0;
	time = 0.0f;
	elapsed_time = 0.0f;
	mouse_locked = false;

	//loads and compiles several shaders from one single file
    //change to "data/shader_atlas_osx.txt" if you are in XCODE
	if(!Shader::LoadAtlas("data/shader_atlas.txt"))
        exit(1);
    checkGLErrors();

	// Create camera
	camera = new Camera();
	camera->lookAt(Vector3(150.f, 150.0f, 250.f), Vector3(0.f, 0.0f, 0.f), Vector3(0.f, 1.f, 0.f));
	camera->setPerspective( 45.f, window_width/(float)window_height, 1.0f, 10000.f);
	

	//This class will be the one in charge of rendering all 
	renderer = new GTR::Renderer(); //here so we have opengl ready in constructor

	//Lets load some object to render
	//prefab = GTR::Prefab::Get("data/prefabs/gmc/scene.gltf");

	GTR::Prefab* ground_prefab = new GTR::Prefab();
	


	ground.createPlane(500);
	
	ground_material.color_texture = Texture::Get("data/textures/floor.tga");
	

	ground_prefab->root.mesh = &ground;
	ground_prefab->root.material = &ground_material;

	

	new Scene();
	std::vector<PrefabEntity*>* prefs = &Scene::instance->prefabs;
	std::vector<Light*>* lgts = &Scene::instance->lights;

	Scene::instance->addPrefab(new PrefabEntity(ground_prefab));

	Scene::instance->addPrefab(new PrefabEntity(GTR::Prefab::Get("data/prefabs/gmc/scene.gltf")));
	Scene::instance->addPrefab(new PrefabEntity(GTR::Prefab::Get("data/prefabs/gmc/scene.gltf")));
	Scene::instance->addPrefab(new PrefabEntity(GTR::Prefab::Get("data/prefabs/brutalism/scene.gltf")));
	(*prefs)[1]->model.translate(200, 0, 0);
	(*prefs)[3]->model.translate(-350, 0, 0);
	(*prefs)[3]->model.scale(50,50,50);
	Matrix44 model;

	(*prefs)[1]->prefab->root.children[2]->children[0]->visible = false;
	/*(*prefs)[3]->prefab->root.children[0]->material->color.x = 1;
	(*prefs)[3]->prefab->root.children[0]->material->color.y = 0.22;
	(*prefs)[3]->prefab->root.children[0]->material->color.z = 0.22;

	(*prefs)[3]->prefab->root.children[2]->material->color.x = 0.22;
	(*prefs)[3]->prefab->root.children[2]->material->color.y = 0.22;
	(*prefs)[3]->prefab->root.children[2]->material->color.z = 1;*/

	//add lights
	Scene::instance->addLight(new Light(eLightType::DIRECTIONAL));
	(*lgts)[0]->model.translate(1000, 1000, 1000);
	(*lgts)[0]->bias = 0.002f;
	(*lgts)[0]->color = Vector3(0.207, 0.199, 0.148);

	Scene::instance->addLight(new Light(eLightType::POINT, Vector3(1, 0, 0)));
	(*lgts)[1]->model.translate(100, 50, 0);

	/*Scene::instance->addLight(new Light(eLightType::SPOT, Vector3(0, 0, 1)));
	(*lgts)[2]->model.translate(-80, 220, 120);
	(*lgts)[2]->model.rotate(-100.0f * DEG2RAD, Vector3(1, 0, 0));
	(*lgts)[2]->bias = 0.0014f;*/

	Scene::instance->addLight(new Light(eLightType::SPOT, Vector3(0, 1, 0)));
	(*lgts)[2]->model.translate(0, 500, -100);
	(*lgts)[2]->model.rotate(-70.0f * DEG2RAD, Vector3(1, 0, 0));//DEG2RAD
	(*lgts)[2]->angle = 10;
	(*lgts)[2]->bias = 0.0023f;

	/*for (int i = 4; i < 20; i++) {//add point lights to deferred
		Scene::instance->addLight(new Light(eLightType::POINT, Vector3(((float)rand() / (RAND_MAX)), ((float)rand() / (RAND_MAX)), ((float)rand() / (RAND_MAX)))));
		(*lgts)[i]->model.translate(-400 + ((i - 4) * 50), 50, rand() % 1000 - 500);
	}*/

	//Scene::instance->irradianceProbes.sh.coeffs[2].set(0.1,0,0);
	//Scene::instance->irradianceProbes[0].pos = Vector3(-330, 70, 0);

	//renderer->computeIrradiance(Scene::instance);

	//Scene::instance->lights[3]->readyShadowMap();///LOOK AT ROTATION SIGNS 1 WORKS BUT SHOULDN'T
	//camera->lookAt(Scene::instance->lights[0]->model.getTranslation(), Scene::instance->lights[0]->model * Vector3(0, 0, -1), Vector3(0, 1, 0));

	Scene::instance->environment = GTR::CubemapFromHDRE("data/textures/panorama.hdre");

	//init reflection probes

	//create the probes
	sReflectionProbe* probe1 = new sReflectionProbe;
	sReflectionProbe* probe2 = new sReflectionProbe;
	sReflectionProbe* probe3 = new sReflectionProbe;

	//set them up
	probe1->pos.set(300, 75, 30);
	probe1->cubemap = new Texture();
	probe1->cubemap->createCubemap(
		512, 512,
		NULL,
		GL_RGB, GL_UNSIGNED_INT, false);

	probe2->pos.set(-100, 75, 30);
	probe2->cubemap = new Texture();
	probe2->cubemap->createCubemap(
		512, 512,
		NULL,
		GL_RGB, GL_UNSIGNED_INT, false);

	probe3->pos.set(100, 75, 30);
	probe3->cubemap = new Texture();
	probe3->cubemap->createCubemap(
		512, 512,
		NULL,
		GL_RGB, GL_UNSIGNED_INT, false);

	//add them to the list
	Scene::instance->reflectionProbes.push_back(probe1);
	Scene::instance->reflectionProbes.push_back(probe2);
	Scene::instance->reflectionProbes.push_back(probe3);


	//hide the cursor
	SDL_ShowCursor(!mouse_locked); //hide or show the mouse
}

//what to do when the image has to be draw
void Application::render(void)
{
	//be sure no errors present in opengl before start
	checkGLErrors();

	//set the clear color (the background color)
	glClearColor(bg_color.x, bg_color.y, bg_color.z, bg_color.w );

	// Clear the color and the depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    checkGLErrors();
   
	//set the camera as default (used by some functions in the framework)
	camera->enable();

	//set default flags
	glDisable(GL_BLEND);
    
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	if(render_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	for (int i = 0; i < Scene::instance->lights.size(); i++) {//shadomap pass
		Light* current = Scene::instance->lights[i];
		if (current->light_type == eLightType::DIRECTIONAL && current->counter >= 0.2f) {//render directional shadowmap 
			renderer->generateShadowMap(current);
			current->counter = 0.0f;
		}
		else if (current->light_type == eLightType::SPOT && current->counter >= 0.5f) {// do it one for spot
			renderer->generateShadowMap(current);
			current->counter = 0.0f;
		}

	}
	///lets render something FORWARD or DEFERRED

	switch (renderer->pipelineType) {
	case pipeline::FORWARD:
		renderer->renderSkybox();
		for (int i = 0; i < Scene::instance->prefabs.size(); i++) {//pass with lighting and all
			if (i == 0) {//don't use pbr for ground mesh
				renderer->noMat = true;
			}

			if (Scene::instance->prefabs[i]->visible)
				renderer->renderPrefab(Scene::instance->prefabs[i]->model, Scene::instance->prefabs[i]->prefab, camera);
			if (i == 0)
				renderer->noMat = false;
		}
		break;
	case pipeline::DEFERRED:
		renderer->renderDeferred(camera);
		break;

	}

	if (renderDirectionalShadowMap) {
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glDisable(GL_BLEND);
		glViewport(0, 0, 256, 256);

		Scene::instance->lights[0]->shadowMap->depth_texture->toViewport();


		/**/
		glViewport(0, 0, window_width, window_height);
	}
	if (renderGBuffers) {
		glDisable(GL_DEPTH_TEST);//display gbuffers
		glDisable(GL_CULL_FACE);
		glDisable(GL_BLEND);
		glViewport(0, window_height * 0.5, window_width * 0.5, window_height * 0.5);
		renderer->gBuffers->color_textures[0]->toViewport();
		glViewport(window_width * 0.5, 0, window_width * 0.5, window_height * 0.5);
		renderer->gBuffers->color_textures[1]->toViewport();
		glViewport(window_width * 0.5, window_height * 0.5, window_width * 0.5, window_height * 0.5);
		Shader* zshader = Shader::Get("depth");
		zshader->enable();
		zshader->setUniform("u_camera_nearfar", Vector2(camera->near_plane, camera->far_plane));
		renderer->gBuffers->depth_texture->toViewport(zshader);
		zshader->disable();

		glViewport(0, 0, window_width, window_height);
	}
	if (showSSAO) {
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glDisable(GL_BLEND);
		renderer->ssaoBlur->toViewport();
	}
	if (showIrradianceGrid) {
		for (int i = 0; i < Scene::instance->irradianceProbes.size(); i++) {//for every probe in the scene
			renderer->renderProbe(Scene::instance->irradianceProbes[i].pos, 5, (float*)&Scene::instance->irradianceProbes[i].sh.coeffs);
		}
		
	}
	if (showReflectionProbes) {
		for (int i = 0; i < Scene::instance->reflectionProbes.size(); i++) {
			renderer->renderReflectionProbe(Scene::instance->reflectionProbes[i]->pos, 5, Scene::instance->reflectionProbes[i]->cubemap);
		}

	}
	



	/*Draw the floor grid, helpful to have a reference point
	if(render_debug)*/
	//drawGrid();
	//Scene::instance->prefabs[1]->

	//glBindFramebuffer(GL_FRAMEBUFFER, 0);
	//glDisable(GL_DEPTH_TEST);

	//render anything in the gui after this


	//the swap buffers is done in the main loop after this function
}

void Application::update(double seconds_elapsed)
{
	float speed = seconds_elapsed * cam_speed; //the speed is defined by the seconds_elapsed so it goes constant
	float orbit_speed = seconds_elapsed * 0.5;
	
	//async input to move the camera around
	if (Input::isKeyPressed(SDL_SCANCODE_LSHIFT)) speed *= 10; //move faster with left shift
	if (Input::isKeyPressed(SDL_SCANCODE_W) || Input::isKeyPressed(SDL_SCANCODE_UP)) camera->move(Vector3(0.0f, 0.0f, 1.0f) * speed);
	if (Input::isKeyPressed(SDL_SCANCODE_S) || Input::isKeyPressed(SDL_SCANCODE_DOWN)) camera->move(Vector3(0.0f, 0.0f,-1.0f) * speed);
	if (Input::isKeyPressed(SDL_SCANCODE_A) || Input::isKeyPressed(SDL_SCANCODE_LEFT)) camera->move(Vector3(1.0f, 0.0f, 0.0f) * speed);
	if (Input::isKeyPressed(SDL_SCANCODE_D) || Input::isKeyPressed(SDL_SCANCODE_RIGHT)) camera->move(Vector3(-1.0f, 0.0f, 0.0f) * speed);

	//mouse input to rotate the cam
	#ifndef SKIP_IMGUI
	if (!ImGuizmo::IsUsing())
	#endif
	{
		if (mouse_locked || Input::mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT)) //move in first person view
		{
			camera->rotate(-Input::mouse_delta.x * orbit_speed * 0.5, Vector3(0, 1, 0));
			Vector3 right = camera->getLocalVector(Vector3(1, 0, 0));
			camera->rotate(-Input::mouse_delta.y * orbit_speed * 0.5, right);
		}
		else //orbit around center
		{
			bool mouse_blocked = false;
			#ifndef SKIP_IMGUI
						mouse_blocked = ImGui::IsAnyWindowHovered() || ImGui::IsAnyItemHovered() || ImGui::IsAnyItemActive();
			#endif
			if (Input::mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT) && !mouse_blocked) //is left button pressed?
			{
				camera->orbit(-Input::mouse_delta.x * orbit_speed, Input::mouse_delta.y * orbit_speed);
			}
		}
	}
	
	//move up or down the camera using Q and E
	if (Input::isKeyPressed(SDL_SCANCODE_Q)) camera->moveGlobal(Vector3(0.0f, -1.0f, 0.0f) * speed);
	if (Input::isKeyPressed(SDL_SCANCODE_E)) camera->moveGlobal(Vector3(0.0f, 1.0f, 0.0f) * speed);

	//to navigate with the mouse fixed in the middle
	SDL_ShowCursor(!mouse_locked);
	#ifndef SKIP_IMGUI
		ImGui::SetMouseCursor(mouse_locked ? ImGuiMouseCursor_None : ImGuiMouseCursor_Arrow);
	#endif
	if (mouse_locked)
	{
		Input::centerMouse();
		//ImGui::SetCursorPos(ImVec2(Input::mouse_position.x, Input::mouse_position.y));
	}
	//shadowmap counter
	for (int i = 0; i < Scene::instance->lights.size(); i++) {
		Light* current = Scene::instance->lights[i];
		if (current->light_type == eLightType::DIRECTIONAL || current->light_type == eLightType::SPOT)
			current->counter += seconds_elapsed;
	}
}

void Application::renderDebugGizmo()
{
	//if (!prefab)
		return;

	//example of matrix we want to edit, change this to the matrix of your entity
	//Matrix44& matrix = prefab->root.model;

	#ifndef SKIP_IMGUI

	static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
	static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
	if (ImGui::IsKeyPressed(90))
		mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
	if (ImGui::IsKeyPressed(69))
		mCurrentGizmoOperation = ImGuizmo::ROTATE;
	if (ImGui::IsKeyPressed(82)) // r Key
		mCurrentGizmoOperation = ImGuizmo::SCALE;
	if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
		mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
	ImGui::SameLine();
	if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE))
		mCurrentGizmoOperation = ImGuizmo::ROTATE;
	ImGui::SameLine();
	if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
		mCurrentGizmoOperation = ImGuizmo::SCALE;
	float matrixTranslation[3], matrixRotation[3], matrixScale[3];
	//ImGuizmo::DecomposeMatrixToComponents(matrix.m, matrixTranslation, matrixRotation, matrixScale);
	ImGui::InputFloat3("Tr", matrixTranslation, 3);
	ImGui::InputFloat3("Rt", matrixRotation, 3);
	ImGui::InputFloat3("Sc", matrixScale, 3);
	//ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, matrix.m);

	if (mCurrentGizmoOperation != ImGuizmo::SCALE)
	{
		if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
			mCurrentGizmoMode = ImGuizmo::LOCAL;
		ImGui::SameLine();
		if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
			mCurrentGizmoMode = ImGuizmo::WORLD;
	}
	static bool useSnap(false);
	if (ImGui::IsKeyPressed(83))
		useSnap = !useSnap;
	ImGui::Checkbox("", &useSnap);
	ImGui::SameLine();
	static Vector3 snap;
	switch (mCurrentGizmoOperation)
	{
	case ImGuizmo::TRANSLATE:
		//snap = config.mSnapTranslation;
		ImGui::InputFloat3("Snap", &snap.x);
		break;
	case ImGuizmo::ROTATE:
		//snap = config.mSnapRotation;
		ImGui::InputFloat("Angle Snap", &snap.x);
		break;
	case ImGuizmo::SCALE:
		//snap = config.mSnapScale;
		ImGui::InputFloat("Scale Snap", &snap.x);
		break;
	}
	ImGuiIO& io = ImGui::GetIO();
	ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
	//ImGuizmo::Manipulate(camera->view_matrix.m, camera->projection_matrix.m, mCurrentGizmoOperation, mCurrentGizmoMode, matrix.m, NULL, useSnap ? &snap.x : NULL);
	#endif
}


//called to render the GUI from
void Application::renderDebugGUI(void)
{
#ifndef SKIP_IMGUI //to block this code from compiling if we want
	std::string temp;

	//System stats
	ImGui::Text(getGPUStats().c_str());					   // Display some text (you can use a format strings too)

	ImGui::Checkbox("Wireframe", &render_wireframe);
	ImGui::Checkbox("Directional ShadowMap", &renderDirectionalShadowMap);
	if (renderer->pipelineType == pipeline::DEFERRED) {
		ImGui::Checkbox("Show GBuffers", &renderGBuffers);
		ImGui::Checkbox("SSAO+", &renderer->computeSSAO);
		if(renderer->computeSSAO)
			ImGui::Checkbox("Show SSAO+", &showSSAO);
		//ImGui::Checkbox("Irradiance", &renderer->useIrradiance);
		//ImGui::Checkbox("Show Irradiance Grid", &showIrradianceGrid);
		ImGui::Checkbox("Reflections", &renderer->useReflections);
		ImGui::Checkbox("Show ReflectionProbes", &showReflectionProbes);
		ImGui::Checkbox("Volumetric Directional Light", &renderer->useVolumetric);
		ImGui::Checkbox("Decals", &renderer->useDecals);
		


	}
	ImGui::Checkbox("Use pbr", &renderer->usepbr);
	//if (ImGui::Button("Calculate Irradiance"))
		//renderer->computeIrradiance(Scene::instance);
	if (ImGui::Button("Calculate Reflections"))
		renderer->computeReflections(Scene::instance);
	ImGui::ColorEdit4("BG color", bg_color.v);


	ImGui::ColorEdit3("Ambient Light", Scene::instance->ambientLight.v);
	
	

	//add info to the debug panel about the camera
	if (ImGui::TreeNode(camera, "Camera")) {
		camera->renderInMenu();
		ImGui::TreePop();
	}

	//show prefab
	for (int i = 0; i < Scene::instance->prefabs.size(); i++) {
		temp = "Prefab_" + std::to_string(Scene::instance->prefabs[i]->id);

		if (ImGui::TreeNode(Scene::instance->prefabs[i], temp.c_str())) {
			Scene::instance->prefabs[i]->renderInMenu();
			ImGui::TreePop();
		}
	}
	//show lights
	for (int i = 0; i < Scene::instance->lights.size(); i++) {
		switch (Scene::instance->lights[i]->light_type) {
		case eLightType::DIRECTIONAL:
			temp = "Directional_Light_" + std::to_string(Scene::instance->lights[i]->id);
			break;
		case eLightType::SPOT:
			temp = "Spot_Light_" + std::to_string(Scene::instance->lights[i]->id);
			break;
		case eLightType::POINT:
			temp = "Point_Light_" + std::to_string(Scene::instance->lights[i]->id);
			break;
		}

		if (ImGui::TreeNode(Scene::instance->lights[i], temp.c_str())) {
			Scene::instance->lights[i]->renderInMenu();
			ImGui::TreePop();
		}
	}

#endif
}

//Keyboard event handler (sync input)
void Application::onKeyDown( SDL_KeyboardEvent event )
{
	switch(event.keysym.sym)
	{
		case SDLK_ESCAPE: must_exit = true; break; //ESC key, kill the app
		case SDLK_F1: render_debug = !render_debug; break;
		case SDLK_f: camera->center.set(0, 0, 0); camera->updateViewMatrix(); break;
		case SDLK_F5: Shader::ReloadAll(); break;
		/*case SDLK_p:
			pipeline currentType = renderer->pipelineType;
			if (currentType == pipeline::DEFERRED) {
				renderer->pipelineType = pipeline::FORWARD;
				renderGBuffers = false;
				showSSAO = false;
			}
			else {
				renderer->pipelineType = pipeline::DEFERRED;
			}
			break;*/
	}
}

void Application::onKeyUp(SDL_KeyboardEvent event)
{
}

void Application::onGamepadButtonDown(SDL_JoyButtonEvent event)
{

}

void Application::onGamepadButtonUp(SDL_JoyButtonEvent event)
{

}

void Application::onMouseButtonDown( SDL_MouseButtonEvent event )
{
	if (event.button == SDL_BUTTON_MIDDLE) //middle mouse
	{
		//Input::centerMouse();
		mouse_locked = !mouse_locked;
		SDL_ShowCursor(!mouse_locked);
	}
}

void Application::onMouseButtonUp(SDL_MouseButtonEvent event)
{
}

void Application::onMouseWheel(SDL_MouseWheelEvent event)
{
	bool mouse_blocked = false;

	#ifndef SKIP_IMGUI
		ImGuiIO& io = ImGui::GetIO();
		if(!mouse_locked)
		switch (event.type)
		{
			case SDL_MOUSEWHEEL:
			{
				if (event.x > 0) io.MouseWheelH += 1;
				if (event.x < 0) io.MouseWheelH -= 1;
				if (event.y > 0) io.MouseWheel += 1;
				if (event.y < 0) io.MouseWheel -= 1;
			}
		}
		mouse_blocked = ImGui::IsAnyWindowHovered();
	#endif

	if (!mouse_blocked && event.y)
	{
		if (mouse_locked)
			cam_speed *= 1 + (event.y * 0.1);
		else
			camera->changeDistance(event.y * 0.5);
	}
}

void Application::onResize(int width, int height)
{
    std::cout << "window resized: " << width << "," << height << std::endl;
	glViewport( 0,0, width, height );
	camera->aspect =  width / (float)height;
	window_width = width;
	window_height = height;
}

