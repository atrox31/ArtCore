#pragma once
#include "ArtCore/Graphic/Sprite.h"
#include "ArtCore/Gui/Gui.h"

namespace GuiElement {
class Grid final :
	public Gui::GuiElementTemplate
{
public:
	struct gh_button {
	public:
		gh_button(Sprite* image, int* enabled, void (*call_back)(std::string), void (*call_back_hover)(std::string, float, SDL_FPoint), const std::string
		          & arg = "") {
			this->image = image;
			this->enabled = enabled;
			this->call_back = call_back;
			this->call_back_hover = call_back_hover;
			this->arg = arg;
			this->_hover_time = .0f;
		};

		Sprite* image;
		int* enabled;
		std::string arg;
		void (*call_back)(std::string);
		void (*call_back_hover)(std::string, float, SDL_FPoint);
		float _hover_time;
	};
	Grid();
	~Grid() override;
	void Render() override;
	bool OnClick() override;
	gh_button* GetButton(int x, int y) const;
	gh_button* GetButton(SDL_Point xy) const;
	gh_button* AddButton(int x, int y, gh_button* button);
	void AddButtons(std::initializer_list<gh_button*> buttons);
	void SetElementSize(int w, int h);
	void SetElementSpacing(int index);
	int GetRows() const { return _rows; };
	int GetColumns() const { return _columns; };
	void Clear() override;

	void SetVariableFromStringEx(const std::string& name, const std::string& value) override;
private:
	SDL_FPoint _focus_xy{};
	SDL_Point _null_focus = { -1, -1 };
	SDL_Point _focus_element{};
	SDL_Point _clicked_element{};
	int _columns{}, _rows{}, _spacing{};
	std::vector< gh_button* > _grid_elements;
	SDL_Point _element_size{};
};

};