#include "scene.h"
#include "application.h"


Scene* Scene::instance = nullptr;

BaseEntity::BaseEntity(eType typ, Matrix44 mod)
{
	id = Application::instance->idCounter;
	Application::instance->idCounter++;
	model = mod;
	visible = true;
	type = typ;
}

PrefabEntity::PrefabEntity( GTR::Prefab* pref) : BaseEntity( eType::PREFAB) {
	prefab = pref;
}

void PrefabEntity::renderInMenu() {
	#ifndef SKIP_IMGUI
		ImGui::Text("ID: %d", id); // Edit 3 floats representing a color
		ImGui::Checkbox("Visible", &visible);

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));

		//Model edit
		ImGuiMatrix44(model, "Model");

		//Material
		if (prefab->root.material && ImGui::TreeNode(prefab->root.material, "Material"))
		{
			prefab->root.material->renderInMenu();
			ImGui::TreePop();
		}
		prefab->root.renderInMenu();
		ImGui::TreePop();


		ImGui::PopStyleColor();

#endif

}

Light::Light(eLightType lightType, Vector3 col, float mDist, float ang, float cut) : BaseEntity(eType::LIGHT) {
	color = col;
	maxDist = mDist;
	light_type = lightType;
	angle = ang;
	cutoff = cut;
	bias = 0.3000;
	counter = 10000.0f;
	


}
void Light::readyShadowMap() {
	if (light_type == eLightType::DIRECTIONAL || light_type == eLightType::SPOT) {
		if (!shadowMap) {
			shadowMap = new FBO();
			shadowMap->setDepthOnly(1024, 1024);
		}
		
		if(!cam){
			cam = new Camera();
			if (light_type == eLightType::DIRECTIONAL) {
				cam->setOrthographic(-300, 300, -300, 300, 1.0f, 10000.f);
			}
			else {
				cam->setPerspective(angle * 2, 1, 1.0f, 10000.f);
			}
		}
			
		
		
		if (light_type == eLightType::DIRECTIONAL) {//update camera position
			cam->lookAt(model.getTranslation(), ((Application::instance->camera->eye)-model.getTranslation()), Vector3(0, 1, 0));
			
		}
		else {//spot
			cam->lookAt(model.getTranslation(), model * Vector3(0, 0, 1), Vector3(0, 1, 0));
		}


	}
}

void Light::renderInMenu() {
#ifndef SKIP_IMGUI
	ImGui::Text("ID: %d", id); // Edit 3 floats representing a color
	ImGui::Checkbox("Visible", &visible);
	

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));

	//Model edit
	ImGuiMatrix44(model, "Model");
	ImGui::ColorEdit3("Color", color.v);
	ImGui::DragFloat("Max Distance", &maxDist);
	ImGui::DragFloat("Angle", &angle);
	ImGui::DragFloat("Cutoff", &cutoff);
	ImGui::DragFloat("ShadowBias", &bias,0.001f);

	if (light_type == eLightType::DIRECTIONAL || light_type == eLightType::SPOT) {
		if (ImGui::TreeNode(cam, "Camera")) {
			cam->renderInMenu();

		}
	}
	
	
	ImGui::PopStyleColor();

#endif

}

Scene::Scene(Vector3 ambLight) {
	instance = this;
	ambientLight = ambLight;
}

void Scene::addPrefab(PrefabEntity* pref) {
	prefabs.push_back(pref);
}

void Scene::addLight(Light* lig) {
	lights.push_back(lig);


}
