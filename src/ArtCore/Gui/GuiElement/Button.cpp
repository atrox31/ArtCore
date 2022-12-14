#include "Button.h"

#include "ArtCore/Graphic/Render.h"
#include "ArtCore/System/Core.h"

GuiElement::Button::Button()
{
	this->_type = GuiElementTemplate::Type::BUTTON;
}

GuiElement::Button* GuiElement::Button::SetText(const std::string& text)
{
	this->_text = text;
	this->_dimensions = FC_GetBounds(Gui::GlobalFont, 0, 0, FC_ALIGN_LEFT, FC_MakeScale(1.0f, 1.0f), text.c_str());
	this->_dimensions.W += 12;
	this->_dimensions.H += 2;
	return this;
}

void GuiElement::Button::SetVariableFromStringEx(const std::string& name, const std::string& value)
{
	/*
	 new Variable(Variable.VariableType.VTypeString, "Text", "button")
	 */
	if (name == "Text")
	{
		SetText(value); return;
	}
	Console::WriteLine("[GuiElement::Button::SetVariableFromStringEx]: '" + name + "' not found");
}

void GuiElement::Button::Render()
{
	const float line_thickness = GPU_SetLineThickness(2.0f);
	if (_enabled) {
		if (_mouse_hover) {
			Render::DrawRectRoundedFilled(_dimensions.ToGPU_Rect(), 2.0f, _pallet.Active);
			if (_focus) {
				GPU_SetShapeBlending(true);
				Render::DrawRectRoundedFilled(_dimensions.ToGPU_Rect(), 2.0f, { 0,0,0,100 });
				const GPU_Rect frame_border = (_dimensions / 2).ToGPU_Rect();
				Render::DrawRectRounded(frame_border, 2.0f, { 0,0,0,100 });
				GPU_SetShapeBlending(false);
			}
		}
		else {
			Render::DrawRectRoundedFilled(_dimensions.ToGPU_Rect(), 2.0f, _pallet.Background);
		}
	}
	else {
		Render::DrawRectRoundedFilled(_dimensions.ToGPU_Rect(), 2.0f, _pallet.BackgroundDisable);
	}
	Render::DrawRectRounded(_dimensions.ToGPU_Rect(), 2.0f, _pallet.Frame);
	if (_focus) {
		GPU_SetShapeBlending(true);
		GPU_SetLineThickness(4.0f);
		Render::DrawRectRoundedFilled(_dimensions.ToGPU_Rect(), 2.0f, { 0,0,0,100 });
		GPU_SetLineThickness(2.0f);
		GPU_SetShapeBlending(false);
	}
	const GPU_Rect temp_dimensions = {
				_dimensions.X ,
				_dimensions.Y + (_mouse_hover ? 1.0f : 0.0f),
				_dimensions.W ,
				_dimensions.H };
	
	Render::DrawTextBox(_text, _default_font, temp_dimensions, _pallet.Font, FC_ALIGN_CENTER);
	GPU_SetLineThickness(line_thickness);
}
