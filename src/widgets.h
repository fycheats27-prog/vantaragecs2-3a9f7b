// language: C++, file: widgets.h

#pragma once
#include <imgui.h>

namespace Widgets {

	// ── toggles / checks ──
	bool Toggle(const char* label, bool* value);
	bool MultiToggle(const char* label, bool* value, float width);

	// ── sliders ──
	bool SliderInt(const char* label, int* value, int min, int max, const char* fmt = "%d");
	bool SliderFloat(const char* label, float* value, float min, float max, const char* fmt = "%.1f");

	// ── input ──
	bool Keybind(const char* label, int* key);
	bool Combo(const char* label, int* current, const char* const items[], int count);
	bool InputText(const char* label, char* buf, size_t buf_size);

	// ── color ──
	bool ColorEdit(const char* label, float col[4]);

	// ── layout helpers ──
	void Section(const char* label);
	bool Button(const char* label, const ImVec2& size = ImVec2(0, 0));
	bool ButtonPill(const char* label, bool active, const ImVec2& size = ImVec2(0, 0));
	bool DangerButton(const char* label, const ImVec2& size = ImVec2(0, 0));

} // namespace Widgets