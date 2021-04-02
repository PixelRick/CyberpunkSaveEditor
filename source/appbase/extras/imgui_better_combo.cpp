#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <cctype>
#include <vector>
#include <set>
#include <future>

// https://github.com/HasKha/GWToolboxpp

namespace ImGui {

  [[nodiscard]] bool BetterCombo(
    const char* label, int* current_item,
    bool(*items_getter)(void*, int, const char**), 
    void* data, int items_count)
  {
    ImGuiContext& g = *ImGui::GetCurrentContext();
    ImGuiWindow* window = GetCurrentWindow();
    const ImGuiID id = window->GetID(label);

    const char* preview_value = NULL;
    if (*current_item >= 0 && *current_item < items_count) {
      items_getter(data, *current_item, &preview_value);
    }
  
    // this is actually shared between all combos. It's kinda ok because there is
    // only one combo open at any given time, however it causes a problem where
    // if you open combo -> keyboard select (but no enter) and close, the 
    // keyboard_selected will stay as-is when re-opening the combo, or even others.
    static int keyboard_selected = -1;
  
    static std::vector<int> filtered_indices;
    static bool filtered = false;

    static std::atomic<size_t> task_uid = 0;
    static std::future<std::vector<int>> task_future;

    static ImGuiID last_id = 0;

    static char word[64] = "";

    if (!BeginCombo(label, preview_value))
    {
      return false;
    }

    if (last_id != id)
    {
      word[0] = '\0';
      last_id = id;
      task_uid++;
      task_future = std::future<std::vector<int>>();
      filtered_indices.clear();
      filtered = false;
    }

    GetIO().WantTextInput = true;
    

    bool update_keyboard_match = false;

    for (int n = 0; n < g.IO.InputQueueCharacters.size() && g.IO.InputQueueCharacters[n]; n++)
    {
      if (unsigned int c = (unsigned int)g.IO.InputQueueCharacters[n])
      {
        if (c == ' ' || c == '.' || c == '_'
          || (c >= '0' && c <= '9')
          || (c >= 'A' && c <= 'Z')
          || (c >= 'a' && c <= 'z'))
        {
          // build temporary word
          // 
          // append
          const size_t i = strnlen(word, 64);
          if (i + 1 < 64)
          {
            word[i] = static_cast<char>(c);
            word[i + 1] = '\0';
          }
          
          update_keyboard_match = true;
        }
      }
    }

    if (IsKeyPressed(GetKeyIndex(ImGuiKey_Backspace)))
    {
      const size_t i = strnlen(word, 64);
      if (i > 0)
        word[i-1] = '\0';
      update_keyboard_match = true;
    }

    if (IsKeyPressed(GetKeyIndex(ImGuiKey_Delete)))
    {
      word[0] = '\0';
      update_keyboard_match = true;
    }

    if (word[0] != '\0')
      SetTooltip(word);

    // find best keyboard match
    int best = -1;
    bool keyboard_selected_now = false;

    if (update_keyboard_match)
    {
      if (word[0] != '\0')
      {
        std::vector<const char*> in;
        for (int i = 0; i < items_count; ++i)
        {
          const char* item_text;
          if (items_getter(data, i, &item_text))
          {
            in.push_back(item_text);
          }
        }
        task_uid++;
        task_future = std::async(
          [](std::vector<const char*> in, std::string search, size_t self_uid, std::atomic<size_t>& uid) -> std::vector<int>
          {
            std::vector<int> ret;

            const char* search_cstr = search.c_str();

            int i = 0;
            for (auto& s : in)
            {
              if (self_uid != uid.load())
              {
                ret.clear();
                return {};
              }

              if (strstr(s, search_cstr) != nullptr)
              {
                ret.push_back(i);
              }

              i++;
            }

            return ret;
          },
          std::move(in),
          std::string(word),
          task_uid.load(),
          std::ref(task_uid)
        );
      }
      else
      {
        task_uid++;
        task_future = std::future<std::vector<int>>();
        filtered_indices.clear();
        filtered = false;
      }
    }

    if (task_future.valid() && task_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
      filtered = true;
      filtered_indices = task_future.get();
      if (filtered_indices.size())
      {
        keyboard_selected = 0;
        keyboard_selected_now = true;
      }
    }
  
    // Display items
    bool value_changed = false;
    auto it = filtered_indices.begin();
    int last_valid_i = 0;

    ImGuiListClipper clipper;

    clipper.Begin(filtered ? (int)filtered_indices.size() : items_count);

    int last_j = filtered_indices.size() ? filtered_indices[0] : 0;

    while (clipper.Step())
    {
      for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
      {
        int j = filtered ? filtered_indices[i] : i;

        PushID((void*)(intptr_t)j);

        const bool item_selected = (j == *current_item);
        const bool item_keyboard_selected = (j == keyboard_selected);
        const char* item_text;

        if (!items_getter(data, j, &item_text))
        {
          item_text = "*Unknown item*";
        }

        if (Selectable(item_text, item_selected || item_keyboard_selected))
        {
          value_changed = true;
          *current_item = j;
          keyboard_selected = -1;
        }

        if (!item_keyboard_selected && (last_j < *current_item && *current_item < j))
        {
          *current_item = last_j;
        }

        if (item_selected && IsWindowAppearing())
        {
          SetScrollHereY();
        }
        if (item_keyboard_selected && keyboard_selected_now)
        {
          SetScrollHereY();
        }
        PopID();

        last_j = j;
      }
    }
  
    EndCombo();
    return value_changed;
  }

} // namespace ImGui

