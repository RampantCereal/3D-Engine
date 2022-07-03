#pragma once
#include "prefab.h"
#include "scene.h"
#include "fbo.h"

//forward declarations
class Camera;

enum class pipeline { FORWARD, DEFERRED };

namespace GTR {

	class Prefab;
	class Material;
	
	// This class is in charge of rendering anything in our system.
	// Separating the render from anything else makes the code cleaner
	class Renderer
	{

	public:
		pipeline pipelineType = pipeline::DEFERRED;
		//pipeline pipelineType = pipeline::FORWARD;
		int mode = 0;//
		bool usepbr = true;
		bool noMat = false;
		bool computeSSAO = true;
		bool irradianceAvailable = false;
		bool useIrradiance = true;
		bool reflectionsAvailable = false;
		bool useReflections = true;
		bool useVolumetric = false;
		bool useDecals = true;

		//irradiance probes
		Vector3 irr_start_pos;
		Vector3 irr_end_pos;
		Vector3 irr_dim;
		Vector3 irr_delta;
		float irr_normal_distance = 5.0;


		FBO* gBuffers = nullptr;
		FBO* deferredLight = nullptr;
		FBO* ssaoBuffer = nullptr;
		FBO* irrFBO = nullptr;
		FBO* reflectionFBO = nullptr;
		FBO* volumetricFBO = nullptr;
		Texture* ssaoBlur = nullptr;
		Texture* irradianceProbesTexture = nullptr;
		Texture* tempDepthTexture = nullptr;
		std::vector<Vector3> randomPoints = generateSpherePoints(32, 1.0, true);

		int maxLights = 3;//maximum lights for forward pipeline

		//add here your functions
		//...
		void generateShadowMap(Light* light);
		void renderDeferred(Camera* camera);
		std::vector<Vector3> generateSpherePoints(int num, float radius, bool hemi);
		void renderProbe(Vector3 pos, float size, float* coeffs);
		void renderReflectionProbe(Vector3 pos, float size, Texture* cMap);
		void computeIrradiance(Scene* scene);
		void computeReflections(Scene* scene);
		void renderForward(Camera* cam);
		void renderSkybox();
	
		//to render a whole prefab (with all its nodes)
		void renderPrefab(const Matrix44& model, GTR::Prefab* prefab, Camera* camera);

		//to render one node from the prefab and its children
		void renderNode(const Matrix44& model, GTR::Node* node, Camera* camera);

		//to render one mesh given its material and transformation matrix
		void renderMeshWithMaterial(const Matrix44 model, Mesh* mesh, GTR::Material* material, Camera* camera);
	};

	Texture* CubemapFromHDRE(const char* filename);

};