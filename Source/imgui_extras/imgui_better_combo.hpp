#pragma once

namespace ImGui {

  bool BetterCombo(
		const char* label, int* current_item,
		bool(*items_getter)(void*, int, const char**), 
		void* data, int items_count);

}