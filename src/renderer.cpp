#include "renderer.h"

#include "camera.h"
#include "shader.h"
#include "mesh.h"
#include "texture.h"
#include "prefab.h"
#include "material.h"
#include "utils.h"
#include "extra/hdre.h"
#include "scene.h"
#include "application.h"
#include "sphericalharmonics.h"
#include <cmath>

using namespace GTR;

Mesh* cube = nullptr;

void Renderer::renderDeferred(Camera* camera) {
	mode = 2;
	float w = Application::instance->window_width;
	float h = Application::instance->window_height;

	Matrix44 inv_vp = camera->viewprojection_matrix;
	inv_vp.inverse();

	if (!gBuffers) {
		//create and FBO
		gBuffers = new FBO();


		gBuffers->create(w, h,
			2, 			//color and normal
			GL_RGBA, 		//four channels
			GL_UNSIGNED_BYTE, //1 byte
			true);//depth
	}

	gBuffers->bind();
	Vector4 bg = Application::instance->bg_color;
	glClearColor(0.1, 0.1, 0.1, 0.1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//Geometry
	for (int i = 0; i < Scene::instance->prefabs.size(); i++) {
		if (Scene::instance->prefabs[i]->visible) {
			if (i == 0 || i == 3) {//don't use pbr for ground mesh
				noMat = true;
			}
			renderPrefab(Scene::instance->prefabs[i]->model, Scene::instance->prefabs[i]->prefab, camera);

			if (i == 0 || i == 3)
				noMat = false;
		}
	}

	

	gBuffers->unbind();//GBuffers are filled

	//decals
	if (useDecals) {
		if (!tempDepthTexture) {
			tempDepthTexture = new Texture(gBuffers->depth_texture->width, gBuffers->depth_texture->height, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, false);
			std::cout << "\nviewtiful!\n";
		}
		//create cube
		if (!cube) {
			cube = new Mesh();
			cube->createCube();
		}
		
	
		//tempDepthTexture = gBuffers->depth_texture;
		gBuffers->depth_texture->copyTo(tempDepthTexture);

		gBuffers->bind();
		

		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		Matrix44 m;
		m.setTranslation(-200,0,200);
		m.scale(100, 1, 100);
		Matrix44 im;
		im = m;
		im.inverse();

		Shader* dShader = Shader::Get("decals");
		dShader->enable();
		dShader->setUniform("u_inverse_viewprojection", inv_vp);
		dShader->setUniform("u_viewprojection", camera->viewprojection_matrix);
		dShader->setUniform("u_model", m);
		dShader->setUniform("u_imodel", im);
		dShader->setTexture("u_depth_texture", tempDepthTexture, 2);
		dShader->setTexture("u_texture", Texture::Get("data/textures/vj.png"), 1);
		dShader->setUniform("u_camera_position", camera->eye);
		dShader->setUniform("u_iRes", Vector2(1.0 / (float)gBuffers->depth_texture->width, 1.0 / (float)gBuffers->depth_texture->height));
		


		cube->render(GL_TRIANGLES);

		dShader->disable();
		gBuffers->unbind();
	}
	
	glDisable(GL_DEPTH_TEST);

	mode = 0;
	Mesh* quad = Mesh::getQuad();
	

	if (computeSSAO) {

		if (!ssaoBuffer) {
			ssaoBuffer = new FBO();

			ssaoBuffer->create(w, h);
		}
		if (!ssaoBlur) {
			ssaoBlur = new Texture();
			ssaoBlur->create(w * 0.5, h * 0.5);
		}
		ssaoBuffer->bind();

		glClearColor(0.1, 0.1, 0.1, 0.1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//disable using mipmaps
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

		//enable bilinear filtering
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


		Shader* shader = NULL;
		shader = Shader::Get("ssao");
		shader->enable();
		shader->setUniform("u_inverse_viewprojection", inv_vp);
		shader->setUniform("u_viewprojection", camera->viewprojection_matrix);

		shader->setTexture("u_depth_texture", gBuffers->depth_texture, 0);
		shader->setTexture("u_normal_texture", gBuffers->color_textures[1], 1);
		shader->setUniform("u_camera_position", camera->eye);

		//we need the pixel size so we can center the samples 
		shader->setUniform("u_iRes", Vector2(1.0 / (float)gBuffers->depth_texture->width, 1.0 / (float)gBuffers->depth_texture->height));

		//send random points so we can fetch around
		shader->setUniform3Array("u_points", (float*)&randomPoints[0], randomPoints.size());


		quad->render(GL_TRIANGLES);
		shader->disable();

		ssaoBuffer->unbind();
		shader = Shader::Get("blur");
		shader->enable();
		shader->setUniform("u_offset", Vector2(1.0 / (float)ssaoBuffer->color_textures[0]->width, 1.0 / (float)ssaoBuffer->color_textures[0]->height));
		ssaoBuffer->color_textures[0]->copyTo(ssaoBlur, shader);
		shader->setUniform("u_offset", Vector2(4.0 / (float)ssaoBuffer->color_textures[0]->width, 2.0 / (float)ssaoBuffer->color_textures[0]->height) * 2.0);
		ssaoBlur->copyTo(ssaoBuffer->color_textures[0], shader);
		shader->setUniform("u_offset", Vector2(4.0 / (float)ssaoBuffer->color_textures[0]->width, 4.0 / (float)ssaoBuffer->color_textures[0]->height) * 4.0);
		ssaoBuffer->color_textures[0]->copyTo(ssaoBlur, shader);
		shader->disable();

	}

	if (!deferredLight) {
		deferredLight = new FBO();

		deferredLight->create(w, h,
			1, 			//color and normal
			GL_RGBA, 		//four channels
			GL_FLOAT, //1 byte
			false);
	}

	Texture* noise = Texture::Get("data/textures/noise.png");
	noise->bind();
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	//start rendering to the illumination fbo
	deferredLight->bind();

	//we need a fullscreen quad
	renderSkybox();

	//we need a shader specially for this task, lets call it "deferred"
	Shader* sh = NULL;
	if (usepbr)
		sh = Shader::Get("deferred_pbr");
	else
		sh = Shader::Get("deferred");

	sh->enable();

	//pass the gbuffers to the shader
	sh->setUniform("u_color_texture", gBuffers->color_textures[0], 0);
	sh->setUniform("u_normal_texture", gBuffers->color_textures[1], 1);
	sh->setUniform("u_depth_texture", gBuffers->depth_texture, 3);
	if(computeSSAO)
		sh->setUniform("u_ao_texture", ssaoBlur, 2);
	else
		sh->setUniform("u_ao_texture", Texture::getWhiteTexture(), 2);

	//camera eye
	sh->setUniform("u_camera_position", camera->eye);

	//pass the inverse projection of the camera to reconstruct world pos.
	sh->setUniform("u_inverse_viewprojection", inv_vp);
	//pass the inverse window resolution, this may be useful
	sh->setUniform("u_iRes", Vector2(1.0 / (float)w, 1.0 / (float)h));

	//pass all the information about the light and ambient…
	sh->setUniform("u_ambient_light", Scene::instance->ambientLight);
	sh->setUniform("u_light_type", 0);
	//pass irradiance info if available
	if (irradianceAvailable && useIrradiance) {
		sh->setUniform("u_irr_texture", irradianceProbesTexture, 4);
		sh->setUniform("u_irr_start", irr_start_pos);
		sh->setUniform("u_irr_end", irr_end_pos);
		sh->setUniform("u_irr_dim", irr_dim);
		sh->setUniform("u_irr_normal_distance", irr_normal_distance);
		sh->setUniform("u_irr_delta", irr_delta);
		sh->setUniform("u_num_probes", Scene::instance->irradianceProbes.size());
		sh->setUniform("u_use_irradiance",true);
	}
	else {
		sh->setUniform("u_use_irradiance", false);
	}
		

	Light* currentLight = Scene::instance->lights[0];
	if (currentLight->visible) {
		sh->setUniform("u_shadow_map", currentLight->shadowMap->depth_texture, 8);
		sh->setUniform("u_shadow_viewprojection", currentLight->cam->viewprojection_matrix);
		sh->setUniform("u_shadow_bias", currentLight->bias);
		sh->setUniform("u_light_color", currentLight->color);
		sh->setUniform("u_light_position", currentLight->model.getTranslation());
	}

	//disable depth test and blend!!
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	//render a fullscreen quad
	quad->render(GL_TRIANGLES);

	//SPOT AND POINT LIGHT PASS
	//we must accumulate the light contribution of every light
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	//for every other light...

	//we do not need to pass all the general info for every light as we are using the same shader for every light

	//pass light info...
	for (int i = 1; i < Scene::instance->lights.size(); ++i)// one pass for every light source
	{
		currentLight = Scene::instance->lights[i];
		if (currentLight->visible) {

			switch (currentLight->light_type) {
			case eLightType::POINT:
				sh->setUniform("u_light_type", 1);
				sh->setUniform("u_light_maxdist", currentLight->maxDist);
				break;
			case eLightType::SPOT:
				float cutoff = cos(currentLight->angle * (DEG2RAD));
				sh->setUniform("u_light_type", 2);
				sh->setUniform("u_spotDirection", currentLight->model.frontVector());
				sh->setUniform("u_spotCosineCutoff", cutoff);
				sh->setUniform("u_spotExponent", currentLight->cutoff);
				sh->setUniform("u_shadow_map", currentLight->shadowMap->depth_texture, 8);
				sh->setUniform("u_shadow_viewprojection", currentLight->cam->viewprojection_matrix);
				sh->setUniform("u_shadow_bias", currentLight->bias);
				break;
			}
			sh->setUniform("u_light_color", currentLight->color);
			sh->setUniform("u_light_position", currentLight->model.getTranslation());

			//render fullscreen quad
			quad->render(GL_TRIANGLES);
		}
	}


	sh->disable();

	//reflections pass
	if (useReflections) {

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		sh = Shader::Get("deferredReflections");
		sh->enable();
		sh->setUniform("u_inverse_viewprojection",inv_vp);
		sh->setUniform("u_viewprojection", camera->viewprojection_matrix);
		sh->setTexture("u_color_texture", gBuffers->color_textures[0], 0);
		sh->setTexture("u_normal_texture", gBuffers->color_textures[1], 1);
		sh->setTexture("u_depth_texture", gBuffers->depth_texture,2);
		sh->setUniform("u_iRes", Vector2(1.0 / (float)gBuffers->depth_texture->width, 1.0 / (float)gBuffers->depth_texture->height));
		sh->setUniform("u_camera_position", camera->eye);
		sh->setTexture("u_environment_texture",Scene::instance->environment,8);

		bool reflectSkybox = true;

		Texture* nearestCubemap = Scene::instance->environment;
		for (int i = 0; i < Scene::instance->reflectionProbes.size();i++) {
			sReflectionProbe* pr = Scene::instance->reflectionProbes[i];
			float dist = camera->eye.distance(pr->pos);
			if (dist < 150 && pr->cubemap && reflectionsAvailable) {
				nearestCubemap = pr->cubemap;
				reflectSkybox = false;
			}
				
		}

		sh->setUniform("u_reflect_skybox", reflectSkybox);
		sh->setTexture("u_environment_texture", nearestCubemap, 8);

		quad->render(GL_TRIANGLES);
		sh->disable();

	}
	
	//volumetric
	if (useVolumetric) {

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		

		Light* sun = Scene::instance->lights[0];
		sh = Shader::Get("volumetricDirectional");
		sh->enable();
		sh->setUniform("u_inverse_viewprojection", inv_vp);
		sh->setUniform("u_viewprojection", camera->viewprojection_matrix);
		sh->setTexture("u_depth_texture", gBuffers->depth_texture, 2);
		sh->setUniform("u_iRes", Vector2(1.0 / (float)gBuffers->depth_texture->width, 1.0 / (float)gBuffers->depth_texture->height));
		sh->setUniform("u_camera_position", camera->eye);
		sh->setUniform("u_light_color", sun->color);
		sh->setUniform("u_shadow_viewprojection",sun->cam->viewprojection_matrix);
		sh->setUniform("u_shadow_bias",sun->bias);
		sh->setTexture("u_shadow_map", sun->shadowMap->depth_texture, 8);

		sh->setUniform("u_rand", Vector3(random(), random(), random()));
		sh->setTexture("u_noise_texture", noise, 5);

		quad->render(GL_TRIANGLES);
		glDisable(GL_BLEND);
		sh->disable();
		

	}

	deferredLight->unbind();

	//be sure blending is not active
	glDisable(GL_BLEND);

	deferredLight->color_textures[0]->toViewport();
	



}

void Renderer::generateShadowMap(Light* light) {
	mode = 1;
	light->readyShadowMap();

	light->shadowMap->bind();

	glClearColor(0.1, 0.1, 0.1, 1);

	// Clear the color and the depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


	for (int i = 0; i < Scene::instance->prefabs.size(); i++) {
		if (Scene::instance->prefabs[i]->visible)
			renderPrefab(Scene::instance->prefabs[i]->model, Scene::instance->prefabs[i]->prefab, light->cam);
	}


	light->shadowMap->unbind();
	mode = 0;
}

//renders all the prefab
void Renderer::renderPrefab(const Matrix44& model, GTR::Prefab* prefab, Camera* camera)
{
	//assign the model to the root node
	renderNode(model, &prefab->root, camera);
}

//renders a node of the prefab and its children
void Renderer::renderNode(const Matrix44& prefab_model, GTR::Node* node, Camera* camera)
{
	if (!node->visible)
		return;

	//compute global matrix
	Matrix44 node_model = node->getGlobalMatrix(true) * prefab_model;

	//does this node have a mesh? then we must render it
	if (node->mesh && node->material)
	{
		//compute the bounding box of the object in world space (by using the mesh bounding box transformed to world space)
		BoundingBox world_bounding = transformBoundingBox(node_model,node->mesh->box);
		
		//if bounding box is inside the camera frustum then the object is probably visible
		if (camera->testBoxInFrustum(world_bounding.center, world_bounding.halfsize) )
		{
			//render node mesh
			renderMeshWithMaterial( node_model, node->mesh, node->material, camera );
			//node->mesh->renderBounding(node_model, true);
		}
	}

	//iterate recursively with children
	for (int i = 0; i < node->children.size(); ++i)
		renderNode(prefab_model, node->children[i], camera);
}

//renders a mesh given its transform and material
void Renderer::renderMeshWithMaterial(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera)
{
	//in case there is nothing to do
	if (!mesh || !mesh->getNumVertices() || !material )
		return;
    assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	Shader* shader = NULL;
	Texture* texture = NULL;
	Texture* metallicRoughness = NULL;

	texture = material->color_texture;
	//texture = material->emissive_texture;
	if (!noMat)
		metallicRoughness = material->metallic_roughness_texture;
	else
		metallicRoughness = Texture::getBlackTexture();
	//texture = material->normal_texture;
	//texture = material->occlusion_texture;
	if (texture == NULL)
		texture = Texture::getWhiteTexture(); //a 1x1 white texture

	//select the blending
	if (material->alpha_mode == GTR::AlphaMode::BLEND)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
		glDisable(GL_BLEND);

	//select if render both sides of the triangles
	if(material->two_sided)
		glDisable(GL_CULL_FACE);
	else
		glEnable(GL_CULL_FACE);
    assert(glGetError() == GL_NO_ERROR);

	//chose a shader
	switch (mode) {
	case 0:  //lighting
		if (usepbr)
			shader = Shader::Get("pbr");
		else
			shader = Shader::Get("phong");
		break;
	case 1: //shadowmap
		shader = Shader::Get("flat");
		break;
	case 2: //deferred buffers
		shader = Shader::Get("gBuffers");
		break;
	}

    assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	bool first = true;

	Light* currentLight;
	Vector3 colisionRes;
	Vector3 normalRes;


	switch (mode) {
	case 0:  //lighting

	//allow to render pixels that have the same depth as the one in the depth buffer
		glDepthFunc(GL_LEQUAL);


		//set blending mode to additive
		//this will collide with materials with blend...
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);



		for (int i = 0; i < maxLights; ++i)// one pass for every light source
		{
			if (first) {//first pass doesn't use blending
				shader->setUniform("u_ambient_light", Scene::instance->ambientLight);
				first = false;
			}
			else {
				shader->setUniform("u_ambient_light", 0);
				glEnable(GL_BLEND);
			}

			shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
			shader->setUniform("u_camera_position", camera->eye);
			shader->setUniform("u_model", model);



			currentLight = Scene::instance->lights[i];

			if ((currentLight->visible && currentLight->light_type == eLightType::POINT && mesh->testSphereCollision(model, currentLight->model.getTranslation(), currentLight->maxDist, colisionRes, normalRes))
				|| currentLight->visible) {
				//pass the light data to the shader
				switch (currentLight->light_type) {
				case eLightType::DIRECTIONAL:
					shader->setUniform("u_light_type", 0);

					shader->setUniform("u_shadow_map", currentLight->shadowMap->depth_texture, 8);
					shader->setUniform("u_shadow_viewprojection", currentLight->cam->viewprojection_matrix);
					shader->setUniform("u_shadow_bias", currentLight->bias);
					break;
				case eLightType::POINT:
					shader->setUniform("u_light_type", 1);
					shader->setUniform("u_light_maxdist", currentLight->maxDist);
					break;
				case eLightType::SPOT:
					float cutoff = cos(currentLight->angle * (DEG2RAD));
					shader->setUniform("u_light_type", 2);
					shader->setUniform("u_spotDirection", currentLight->model.frontVector());
					shader->setUniform("u_spotCosineCutoff", cutoff);
					shader->setUniform("u_spotExponent", currentLight->cutoff);

					shader->setUniform("u_shadow_map", currentLight->shadowMap->depth_texture, 8);
					shader->setUniform("u_shadow_viewprojection", currentLight->cam->viewprojection_matrix);
					shader->setUniform("u_shadow_bias", currentLight->bias);
					break;

				}

				shader->setUniform("u_light_color", currentLight->color);
				shader->setUniform("u_light_position", currentLight->model.getTranslation());

			}
			else {
				shader->setUniform("u_light_color", Vector3(0, 0, 0));
			}

			shader->setUniform("u_color", material->color);
			if (texture)
				shader->setUniform("u_texture", texture, 0);
			if (metallicRoughness)
				shader->setUniform("u_roughness_metalness", metallicRoughness, 1);


			//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
			shader->setUniform("u_alpha_cutoff", material->alpha_mode == GTR::AlphaMode::MASK ? material->alpha_cutoff : 0);

			//render the mesh
			mesh->render(GL_TRIANGLES);

		}
		break;
	case 1: //shadowmap

		//upload uniforms
		shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
		shader->setUniform("u_camera_position", camera->eye);
		shader->setUniform("u_model", model);


		//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
		//shader->setUniform("u_alpha_cutoff", material->alpha_mode == GTR::AlphaMode::MASK ? material->alpha_cutoff : 0);

		//do the draw call that renders the mesh into the screen
		mesh->render(GL_TRIANGLES);

		//disable shader
		break;
	case 2://fill gbuffers
		glDisable(GL_BLEND);
		shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
		shader->setUniform("u_camera_position", camera->eye);
		shader->setUniform("u_model", model);

		shader->setUniform("u_color", material->color);
		if (texture)
			shader->setUniform("u_texture", texture, 0);
		if (metallicRoughness)
			shader->setUniform("u_rough_metal", metallicRoughness, 1);
		//this is used to say which is the alpha threshold to what we should not paint a pixel on the screen (to cut polygons according to texture alpha)
		shader->setUniform("u_alpha_cutoff", material->alpha_mode == GTR::AlphaMode::MASK ? material->alpha_cutoff : 0);

		//do the draw call that renders the mesh into the screen
		mesh->render(GL_TRIANGLES);

		//disable shader
		break;
	}

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
}


Texture* GTR::CubemapFromHDRE(const char* filename)
{
	HDRE* hdre = new HDRE();
	if (!hdre->load(filename))
	{
		delete hdre;
		return NULL;
	}

	Texture* texture = new Texture();
	texture->createCubemap(hdre->width, hdre->height, (Uint8**)hdre->getFaces(0), hdre->header.numChannels == 3 ? GL_RGB : GL_RGBA, GL_FLOAT );
	for(int i = 1; i < 6; ++i)
		texture->uploadCubemap(texture->format, texture->type, false, (Uint8**)hdre->getFaces(i), GL_RGBA32F, i);
	return texture;
}

std::vector<Vector3> Renderer::generateSpherePoints(int num, float radius, bool hemi)
{
	std::vector<Vector3> points;
	points.resize(num);
	for (int i = 0; i < num; i += 3)
	{
		Vector3& p = points[i];
		float u = random();
		float v = random();
		float theta = u * 2.0 * PI;
		float phi = acos(2.0 * v - 1.0);
		float r = cbrt(random() * 0.9 + 0.1) * radius;
		float sinTheta = sin(theta);
		float cosTheta = cos(theta);
		float sinPhi = sin(phi);
		float cosPhi = cos(phi);
		p.x = r * sinPhi * cosTheta;
		p.y = r * sinPhi * sinTheta;
		p.z = r * cosPhi;
		if (hemi && p.z < 0)
			p.z *= -1.0;
	}
	return points;
}

void Renderer::renderProbe(Vector3 pos, float size, float* coeffs)
{
	Camera* camera = Camera::current;
	Shader* shader = Shader::Get("probe");
	Mesh* mesh = Mesh::Get("data/meshes/sphere.obj");

	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);

	Matrix44 model;
	model.setTranslation(pos.x, pos.y, pos.z);
	model.scale(size, size, size);

	shader->enable();
	shader->setUniform("u_viewprojection",
		camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);
	shader->setUniform("u_model", model);
	shader->setUniform3Array("u_coeffs", coeffs, 9);

	mesh->render(GL_TRIANGLES);
	shader->disable();
}

void Renderer::computeIrradiance(Scene* sc) {

	FloatImage images[6]; //here we will store the six views

	//set the fov to 90 and the aspect to 1
	Camera cam;
	cam.setPerspective(90, 1, 0.1, 1000);
	sc->irradianceProbes.clear();

	//define the corners of the axis aligned grid
	//this can be done using the boundings of our scene
	irr_start_pos = Vector3(-200, 10, -170);
	irr_end_pos = Vector3(-400, 100, 80);

	//define how many probes you want per dimension
	irr_dim = Vector3(6, 4, 10);

	//compute the vector from one corner to the other
	irr_delta = (irr_end_pos - irr_start_pos);

	//and scale it down according to the subdivisions
	//we substract one to be sure the last probe is at end pos
	irr_delta.x /= (irr_dim.x - 1);
	irr_delta.y /= (irr_dim.y - 1);
	irr_delta.z /= (irr_dim.z - 1);

	//now delta give us the distance between probes in every axis

	// lets compute the centers
	//pay attention at the order at which we add them
	for (int z = 0; z < irr_dim.z; ++z)
		for (int y = 0; y < irr_dim.y; ++y)
			for (int x = 0; x < irr_dim.x; ++x)
			{
				sProbe p;
				p.local.set(x, y, z);

				//index in the linear array
				p.index = x + y * irr_dim.x + z * irr_dim.x * irr_dim.y;

				//and its position
				p.pos = irr_start_pos + irr_delta * Vector3(x, y, z);
				sc->irradianceProbes.push_back(p);
			}


	
	for (int j = 0; j < sc->irradianceProbes.size(); j++) {//for every probe in the scene
		for (int i = 0; i < 6; ++i) //for every cubemap face
		{

			//compute camera orientation using defined vectors
			Vector3 eye = sc->irradianceProbes[j].pos;
			Vector3 front = cubemapFaceNormals[i][2];
			Vector3 center = sc->irradianceProbes[j].pos + front;
			Vector3 up = cubemapFaceNormals[i][1];
			cam.lookAt(eye, center, up);
			cam.enable();

			//render the scene from this point of view
			if (!irrFBO) {

				irrFBO = new FBO();
				irrFBO->create(64, 64, 1, GL_RGB, GL_FLOAT);
			}
			irrFBO->bind();
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			renderForward(&cam);
			irrFBO->unbind();

			//read the pixels back and store in a FloatImage
			images[i].fromTexture(irrFBO->color_textures[0]);
		}
		

		//compute the coefficients given the six images
		sc->irradianceProbes[j].sh = computeSH(images);

	}

	if (!irradianceAvailable)
		irradianceAvailable = true;

	if (!irradianceProbesTexture){
		//create the texture to store the probes (do this ONCE!!!)
		irradianceProbesTexture = new Texture(
			9, //9 coefficients per probe
			sc->irradianceProbes.size(), //as many rows as probes
			GL_RGB, //3 channels per coefficient
			GL_FLOAT); //they require a high range
	}
	
	//we must create the color information for the texture. because every SH are 27 floats in the RGB,RGB,... order, we can create an array of SphericalHarmonics and use it as pixels of the texture
	SphericalHarmonics* sh_data = NULL;
	sh_data = new SphericalHarmonics[irr_dim.x * irr_dim.y * irr_dim.z];

	//here we fill the data of the array with our probes in x,y,z order...
	for (int i = 0; i < sc->irradianceProbes.size(); i++) {

		sProbe& probe = sc->irradianceProbes[i];
		sh_data[probe.index] = probe.sh;

	}

	//now upload the data to the GPU
	irradianceProbesTexture->upload(GL_RGB, GL_FLOAT, false, (uint8*)sh_data);

	//disable any texture filtering when reading
	irradianceProbesTexture->bind();
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	irradianceProbesTexture->unbind();


	//always free memory after allocating it!!!
	delete[] sh_data;

	if (!irradianceAvailable)
		irradianceAvailable = true;
	
}

void Renderer::renderForward(Camera* cam) {
	renderSkybox();
	for (int i = 0; i < Scene::instance->prefabs.size(); i++) {//pass with lighting and all
		if (i == 0) {//don't use pbr for ground mesh
			noMat = true;
		}

		if (Scene::instance->prefabs[i]->visible)
			renderPrefab(Scene::instance->prefabs[i]->model, Scene::instance->prefabs[i]->prefab, cam);
		if (i == 0)
			noMat = false;
	}

	

}

void Renderer::renderSkybox() {

	if (!Scene::instance->environment)
		return;

	Camera* cam = Camera::current;
	Shader* sh = Shader::Get("skybox");
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	Matrix44 model;
	model.setTranslation(cam->eye.x, cam->eye.y, cam->eye.z);
	model.scale(10, 10, 10);
	sh->enable();
	sh->setUniform("u_viewprojection", cam->viewprojection_matrix);
	sh->setUniform("u_camera_pos", cam->eye);
	sh->setUniform("u_model", model);
	sh->setUniform("u_texture", Scene::instance->environment,0);
	Mesh::Get("data/meshes/sphere.obj")->render(GL_TRIANGLES);
	sh->disable();
	glEnable(GL_CULL_FACE);
	//glEnable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);

}

void Renderer::renderReflectionProbe(Vector3 pos, float size, Texture* cMap)
{
	Camera* camera = Camera::current;
	Shader* shader = Shader::Get("reflectionProbe");
	Mesh* mesh = Mesh::Get("data/meshes/sphere.obj");

	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);

	Matrix44 model;
	model.setTranslation(pos.x, pos.y, pos.z);
	model.scale(size, size, size);

	shader->enable();
	shader->setUniform("u_viewprojection",
		camera->viewprojection_matrix);
	shader->setUniform("u_camera_pos", camera->eye);
	shader->setUniform("u_model", model);
	shader->setUniform("u_texture", cMap, 0);
	

	mesh->render(GL_TRIANGLES);
	shader->disable();
}

void Renderer::computeReflections(Scene* sc) {

	//set the fov to 90 and the aspect to 1
	Camera cam;
	cam.setPerspective(90, 1, 0.1, 1000);
	glEnable(GL_DEPTH_TEST);
	

	//assign cubemap face to FBO
	if (!reflectionFBO)
		reflectionFBO = new FBO();

	for (int j = 0; j < sc->reflectionProbes.size(); j++) {//for every probe in the scene
		//for every reflection probe...
		//sReflectionProbe& probe = sc->reflectionProbes[j];
		//render the view from every side
		for (int i = 0; i < 6; ++i)
		{
			//assign cubemap face to FBO
			reflectionFBO->setTexture(sc->reflectionProbes[j]->cubemap, i);

			//bind FBO
			reflectionFBO->bind();
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			//render view
			Vector3 eye = sc->reflectionProbes[j]->pos;
			Vector3 center = sc->reflectionProbes[j]->pos + cubemapFaceNormals[i][2];
			Vector3 up = cubemapFaceNormals[i][1];
			cam.lookAt(eye, center, up);
			cam.enable();
			renderForward(&cam);
			reflectionFBO->unbind();
		}
		//generate the mipmaps
		sc->reflectionProbes[j]->cubemap->generateMipmaps();

	}

	if (!reflectionsAvailable)
		reflectionsAvailable = true;

}


