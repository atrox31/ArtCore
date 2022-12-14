#pragma once
#include <string>

#include "ArtCore/Enums/Event.h"
#include "ArtCore/Graphic/Sprite.h"
#include "FC_Fontcache/SDL_FontCache.h"

class Instance final
{
public:
	explicit Instance(int instance_definition_id);
	// do not use, this give unique id for instance
	Instance* GiveId();

	[[nodiscard]] Uint64 GetId() const { return _id; }
	void Delete();
	[[nodiscard]] int GetInstanceDefinitionId() const {	return _instance_definition_id; }

	void DrawSelf();
	bool CheckMaskClick(SDL_FPoint&) const;

	std::string Tag;
	std::string Name;

	bool InView;
	bool Alive;
	bool IsCollider;

	float PosX;
	float PosY;
	float Direction;

	Sprite* SelfSprite;
	float SpriteScaleX;
	float SpriteScaleY;
	int SpriteCenterX;
	int SpriteCenterY;
	float SpriteAngle;

	float SpriteAnimationFrame;
	float SpriteAnimationSpeed;
	bool SpriteAnimationLoop;

	std::vector<int> Variables_int;
	std::vector<float> Variables_float;
	std::vector<bool> Variables_bool;
	std::vector<Instance*> Variables_instance;
	std::vector<int> Variables_object;
	std::vector<int> Variables_sprite;
	std::vector<int> Variables_texture;
	std::vector<int> Variables_sound;
	std::vector<int> Variables_music;
	std::vector<int> Variables_font;
	std::vector<Rect> Variables_rect;
	std::vector<SDL_FPoint> Variables_point;
	std::vector<SDL_Color> Variables_color;
	std::vector<std::string> Variables_string;
	event_bit EventFlag;

	bool SuspendedCodeAdd();
	bool SuspendedCodePop();
	// get count of suspended code
	[[nodiscard]] Uint8 SuspendedCodeStateCount() const { return _have_suspended_code;  }
	// get info if have at least one suspended code
	[[nodiscard]] bool SuspendedCodeStateHave() const { return _have_suspended_code > (uint8_t)0;  }
public:
	struct BodyType {
	public:
		ENUM_WITH_STRING_CONVERSION(Body,(None)(Sprite)(Rect)(Circle))
		Body Type;
		float Value;
		BodyType() {
			Type = Body::None;
			Value = 0.f;
		}

	private:
	};
	BodyType Body;
	[[nodiscard]] Rect GetBodyMask() const;
private:
	Uint64 _id = 0;
	static Uint64 _cid;
	int _instance_definition_id;
	uint8_t _have_suspended_code;
};
