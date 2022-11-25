#include "Scene.h"
#include "Debug.h"
#include "Event.h"
#include "VaribleMap.h"

Scene::Scene()
{
	_instances = plf::colony<Instance*>();
	_instances_new = std::vector<Instance*>();
	BeginInstances = std::vector<Spawner>();
	_instances_size = 0;
}

Scene::~Scene()
{
	_instances.clear();
	_instances_new.clear();
}
bool Scene::Load(std::string name)
{
	Sint64 len(0);
	const char* data = Func::GetFileBuf(name, &len);
	Func::DataValues dv(data, (int)len);
	if (!dv.IsOk()) {
		return false;
	}
	Scene new_scene;
	new_scene.Width = std::stoi(dv.GetData("setup", "Width"));
	new_scene.Height = std::stoi(dv.GetData("setup", "Height"));
	auto tmp = Func::Split(name, '/');
	new_scene.Name = tmp[tmp.size() - 1].substr(0, tmp[tmp.size() - 1].length() - 3);

	if (dv.GetData("setup", "BackGroundType") == "DrawColor") {
		new_scene.BackGround.type = Scene::BackGround::BType::DrawColor;
		new_scene.BackGround.color = Func::TextToColor(dv.GetData("setup", "BackGroundColor"));
	}else if (dv.GetData("setup", "BackGroundType") == "DrawTexture") {
		new_scene.BackGround.type = Scene::BackGround::BType::DrawTexture;
		if (dv.GetData("setup", "BackGroundWrapMode") == "Tile") {
			new_scene.BackGround.type_wrap = Scene::BackGround::BTypeWrap::Tile;
		}
		else
		if (dv.GetData("setup", "BackGroundWrapMode") == "TileFlipX") {
			new_scene.BackGround.type_wrap = Scene::BackGround::BTypeWrap::TileFlipX;
		}
		else
		if (dv.GetData("setup", "BackGroundWrapMode") == "TileFlipY") {
			new_scene.BackGround.type_wrap = Scene::BackGround::BTypeWrap::TileFlipY;
		}
		else
		if (dv.GetData("setup", "BackGroundWrapMode") == "TileFlipXY") {
			new_scene.BackGround.type_wrap = Scene::BackGround::BTypeWrap::TileFlipXY;
		}
		else {
			Debug::WARNING("new_scene.BackGround.type_wrap unknown");
			new_scene.BackGround.type_wrap = Scene::BackGround::BTypeWrap::Tile;
		}
		GPU_Image* txt = Core::GetInstance()->assetManager->GetTexture(dv.GetData("setup", "BackGroundTexture_name"));
		if (txt == nullptr) {
			Debug::WARNING("Background texture not exists '"+ dv.GetData("setup", "BackGroundTexture_name") + "'");
			new_scene.BackGround.type = Scene::BackGround::BType::DrawColor;
			new_scene.BackGround.color = SDL_Color(0, 0, 0, 255);
		}
		//BackGroundWrapMode   type_wrap
	}
	else {
		Debug::WARNING("new_scene.BackGround.type unknown");
		new_scene.BackGround.type = Scene::BackGround::BType::DrawColor;
		new_scene.BackGround.color = SDL_Color( 0,0,0,255 );
	}

	// regions
	for (std::string region : dv.GetSection("regions")) {

	}

	// triggers
	for (std::string trigger : dv.GetSection("triggers")) {

	}

	// instances
	for (std::string& instance : dv.GetSection("instance")) {
		std::vector<std::string> data = Func::Split(instance, '|');
		if (data.size() != 3) {
			Debug::WARNING("Instance error: '" + instance + "'");
			continue;
		}
		BeginInstances.push_back({ data[0], std::stoi(data[1]), std::stoi(data[2]) });
	}

	return true;
}
void Scene::Start()
{
}
void Scene::Exit()
{
	if (_instances.size() > 0) {
		Instance* pd;
		for (plf::colony<Instance*>::iterator it = _instances.begin(); it != _instances.end(); ++it) {
			pd = *it;
			delete pd;
		}
	}
	_instances.clear();
	if (_instances_new.size() > 0) {
		Instance* pd;
		for (std::vector<Instance*>::iterator it = _instances_new.begin(); it != _instances_new.end(); ++it) {
			pd = *it;
			delete pd;
		}
	}
	_instances_new.clear();
	// TODO: triggers
	// TODO: regions
}
/*
Instance* Scene::CreateInstance(Instance* Instance)
{
	_instances_size++;
	_instances_new.push_back(Instance);
	//Instance->Parrent = this;
	return Instance;
}

void Scene::DeleteInstance(Instance*)
{
	_instances_size--;
}
*/