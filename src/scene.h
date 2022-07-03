#ifndef SCENE_H
#define SCENE_H

#include "includes.h"
#include "utils.h"
#include "prefab.h"
#include "fbo.h"
#include "camera.h"
#include "sphericalharmonics.h"

enum class eType { BASE_NODE, PREFAB, LIGHT };
enum class eLightType { POINT, SPOT, DIRECTIONAL };

class BaseEntity {
public:
	unsigned int id;
	Matrix44 model;
	bool visible;
	eType type;

	BaseEntity(eType typ = eType::BASE_NODE, Matrix44 mod = Matrix44());
	


};

class PrefabEntity : public BaseEntity {
public:
	GTR::Prefab* prefab;
	PrefabEntity(GTR::Prefab* pref);
	void renderInMenu();
};

class Light : public BaseEntity {
public:
	Vector3 color;
	float maxDist;
	eLightType light_type;
	float angle;//spotlight specific
	float cutoff;//spotlight specific
	FBO* shadowMap = nullptr;
	Camera* cam = nullptr;
	float bias;
	float counter;
	Light( eLightType lightType, Vector3 col = Vector3(0.2, 0.2, 0.2), float maxDist = 500, float ang = 30, float cut = 0);
	void readyShadowMap();
	void renderInMenu();

};

class Scene {
public:
	std::vector<sProbe> irradianceProbes;
	std::vector<sReflectionProbe*> reflectionProbes;

	Texture* environment;
static Scene* instance;
	Vector3 ambientLight;
	std::vector<PrefabEntity*> prefabs;
	std::vector<Light*> lights;
	
	

	Scene(Vector3 ambLight = Vector3(0.188, 0.188, 0.188));
	void addPrefab(PrefabEntity* pref);
	void addLight(Light* lig);

};

#endif 