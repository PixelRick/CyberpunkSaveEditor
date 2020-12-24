// Mini memory editor for Dear ImGui (to embed in your game/tools)
// Get latest version at http://www.github.com/ocornut/imgui_club
//
// Right-click anywhere to access the Options menu!
// You can adjust the keyboard repeat delay/rate in ImGuiIO.
// The code assume a mono-space font for simplicity!
// If you don't use the default font, use ImGui::PushFont()/PopFont() to switch to a mono-space font before caling this.
//
// Usage:
//   // Create a window and draw memory editor inside it:
//   static MemoryEditor mem_edit_1;
//   static char data[0x10000];
//   size_t data_size = 0x10000;
//   mem_edit_1.DrawWindow("Memory Editor", data, data_size);
//
// Usage:
//   // If you already have a window, use DrawContents() instead:
//   static MemoryEditor mem_edit_2;
//   ImGui::Begin("MyWindow")
//   mem_edit_2.DrawContents(this, sizeof(*this), (size_t)this);
//   ImGui::End();
//
// Changelog:
// - v0.10: initial version
// - v0.23 (2017/08/17): added to github. fixed right-arrow triggering a byte write.
// - v0.24 (2018/06/02): changed DragInt("Rows" to use a %d data format (which is desirable since imgui 1.61).
// - v0.25 (2018/07/11): fixed wording: all occurrences of "Rows" renamed to "Columns".
// - v0.26 (2018/08/02): fixed clicking on hex region
// - v0.30 (2018/08/02): added data preview for common data types
// - v0.31 (2018/10/10): added OptUpperCaseHex option to select lower/upper casing display [@samhocevar]
// - v0.32 (2018/10/10): changed signatures to use void* instead of unsigned char*
// - v0.33 (2018/10/10): added OptShowOptions option to hide all the interactive option setting.
// - v0.34 (2019/05/07): binary preview now applies endianness setting [@nicolasnoble]
// - v0.35 (2020/01/29): using ImGuiDataType available since Dear ImGui 1.69.
// - v0.36 (2020/05/05): minor tweaks, minor refactor.
// - v0.40 (2020/10/04): fix misuse of ImGuiListClipper API, broke with Dear ImGui 1.79. made cursor position appears on left-side of edit box. option popup appears on mouse release. fix MSVC warnings where _CRT_SECURE_NO_WARNINGS wasn't working in recent versions.
// - v0.41 (2020/10/05): fix when using with keyboard/gamepad navigation enabled.
// - v0.42 (2020/10/14): fix for . character in ASCII view always being greyed out.
//
// Todo/Bugs:
// - This is generally old code, it should work but please don't use this as reference!
// - Arrows are being sent to the InputText() about to disappear which for LeftArrow makes the text cursor appear at position 1 for one frame.
// - Using InputText() is awkward and maybe overkill here, consider implementing something custom.

#pragma once

#include <stdio.h>      // sprintf, scanf
#include <stdint.h>     // uint8_t, etc.

#include <vector>
#include <algorithm>

#ifdef _MSC_VER
#define _PRISizeT   "I"
#define ImSnprintf  _snprintf
#else
#define _PRISizeT   "z"
#define ImSnprintf  snprintf
#endif

#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable: 4996) // warning C4996: 'sprintf': This function or variable may be unsafe.
#endif

struct MemoryEditor
{
  enum DataFormat
  {
    DataFormat_Bin = 0,
    DataFormat_Dec = 1,
    DataFormat_Hex = 2,
    DataFormat_COUNT
  };

  // Settings
  bool            Open;                                       // = true   // set to false when DrawWindow() was closed. ignore if not using DrawWindow().
  bool            ReadOnly;                                   // = false  // disable any editing.
  int             Cols;                                       // = 16     // number of columns to display.
  bool            OptShowOptions;                             // = true   // display options button/context menu. when disabled, options will be locked unless you provide your own UI for them.
  bool            OptShowDataPreview;                         // = false  // display a footer previewing the decimal/binary/hex/float representation of the currently selected bytes.
  bool            OptShowHexII;                               // = false  // display values in HexII representation instead of regular hexadecimal: hide null/zero bytes, ascii values as ".X".
  bool            OptShowAscii;                               // = true   // display ASCII representation on the right side.
  bool            OptGreyOutZeroes;                           // = true   // display null/zero bytes using the TextDisabled color.
  bool            OptUpperCaseHex;                            // = true   // display hexadecimal values as "FF" instead of "ff".
  int             OptMidColsCount;                            // = 8      // set to 0 to disable extra spacing between every mid-cols.
  int             OptAddrDigitsCount;                         // = 0      // number of addr digits to display (default calculated based on maximum displayed addr).
  ImU32           HighlightColor;                             //          // background color of highlighted bytes.
  ImU32           HighlightFuncColor;                         //          // background color of highlighted bytes.
  ImU32           SelectionColor;                             //          // background color of selected bytes.
  int             HexCellPaddingX;                             // = 2      // padding of hex cells (individual bytes)
  ImU8(*ReadFn)(const ImU8* data, size_t off);    // = 0      // optional handler to read bytes.
  void            (*WriteFn)(ImU8* data, size_t off, ImU8 d); // = 0      // optional handler to overwrite bytes.
  bool            (*HighlightFn)(const ImU8* data, size_t off);//= 0      // optional handler to return Highlight property (to support non-contiguous highlighting).

  // [Internal State]
  bool            ContentsWidthChanged;
  size_t          DataPreviewAddr;
  size_t          DataEditingAddr;
  size_t          DataEditingAddrNext;
  bool            DataEditingNeedCursorReset;
  bool            DataSelectionDrag;
  size_t          DataSelectionStart;
  size_t          DataSelectionEnd;
  char            ByteInputBuf[3];
  char            DataInputBuf[32];
  char            AddrInputBuf[32];
  size_t          GotoAddr;
  size_t          HighlightMin, HighlightMax; // used for selection
  int             PreviewEndianess;
  ImGuiDataType   PreviewDataType;

  MemoryEditor()
  {
    // Settings
    Open = true;
    ReadOnly = false;
    Cols = 16;
    OptShowOptions = true;
    OptShowDataPreview = false;
    OptShowHexII = false;
    OptShowAscii = true;
    OptGreyOutZeroes = true;
    OptUpperCaseHex = true;
    OptMidColsCount = 8;
    HexCellPaddingX = 2;
    OptAddrDigitsCount = 0;
    HighlightColor = IM_COL32(255, 255, 150, 60);
    HighlightFuncColor = IM_COL32(255, 200, 50, 100);
    ReadFn = NULL;
    WriteFn = NULL;
    HighlightFn = NULL;

    // State/Internals
    ContentsWidthChanged = false;
    DataPreviewAddr = DataEditingAddr = DataEditingAddrNext = (size_t)-1;
    DataEditingNeedCursorReset = false;
    DataSelectionDrag = false;
    DataSelectionStart = DataSelectionEnd = (size_t)-1;
    memset(ByteInputBuf, 0, sizeof(ByteInputBuf));
    memset(DataInputBuf, 0, sizeof(DataInputBuf));
    memset(AddrInputBuf, 0, sizeof(AddrInputBuf));
    GotoAddr = (size_t)-1;
    HighlightMin = HighlightMax = (size_t)-1;
    PreviewEndianess = 0;
    PreviewDataType = ImGuiDataType_S32;
  }

  void GotoAddrAndHighlight(size_t addr_min, size_t addr_max)
  {
    GotoAddr = addr_min;
    HighlightMin = addr_min;
    HighlightMax = addr_max;
  }

  struct Sizes
  {
    int     AddrDigitsCount;
    float   LineHeight;
    float   GlyphWidth;
    float   HexCellWidth;
    float   SpacingBetweenMidCols;
    float   PosHexStart;
    float   PosHexEnd;
    float   PosAsciiStart;
    float   PosAsciiEnd;
    float   WindowWidth;

    Sizes() { memset(this, 0, sizeof(*this)); }
  };

  void CalcSizes(Sizes& s, size_t mem_size, size_t base_display_addr)
  {
    ImGuiStyle& style = ImGui::GetStyle();
    s.AddrDigitsCount = OptAddrDigitsCount;
    if (s.AddrDigitsCount == 0)
      for (size_t n = base_display_addr + mem_size - 1; n > 0; n >>= 4)
        s.AddrDigitsCount++;
    s.LineHeight = ImGui::GetTextLineHeight();
    s.GlyphWidth = (float)(int)ImGui::CalcTextSize("F").x;                      // We assume the font is mono-space
    s.HexCellWidth = (float)(int)((s.GlyphWidth + HexCellPaddingX) * 2.0f);             // "FF " we include trailing space in the width to easily catch clicks everywhere
    s.SpacingBetweenMidCols = (float)(int)(s.HexCellWidth * 0.25f); // Every OptMidColsCount columns we add a bit of extra spacing
    s.PosHexStart = (s.AddrDigitsCount + 2) * s.GlyphWidth;
    s.PosHexEnd = s.PosHexStart + (s.HexCellWidth * Cols);
    s.PosAsciiStart = s.PosAsciiEnd = s.PosHexEnd;
    if (OptShowAscii)
    {
      s.PosAsciiStart = s.PosHexEnd + s.GlyphWidth * 1;
      if (OptMidColsCount > 0)
        s.PosAsciiStart += (float)((Cols + OptMidColsCount - 1) / OptMidColsCount) * s.SpacingBetweenMidCols;
      s.PosAsciiEnd = s.PosAsciiStart + Cols * s.GlyphWidth;
    }
    s.WindowWidth = s.PosAsciiEnd + style.ScrollbarSize + style.WindowPadding.x * 2 + s.GlyphWidth;
  }

  enum class HighlightType : uint8_t { None, User, Func, Preview };

  HighlightType GetHighlightType(ImU8* mem_data, size_t addr)
  {
    if (addr >= HighlightMin && addr < HighlightMax)
      return HighlightType::User;

    if ((HighlightFn && HighlightFn(mem_data, addr)))
      return HighlightType::Func;

    if (addr >= DataPreviewAddr && addr < DataPreviewAddr + 8)
      if (addr < DataPreviewAddr + (OptShowDataPreview ? DataTypeGetSize(PreviewDataType) : 0))
        return HighlightType::Preview;

    return HighlightType::None;
  }

  size_t SelectionStart() const { return std::min(DataSelectionStart, DataSelectionEnd); }
  size_t SelectionLen() const { // always >= 1
    return DataSelectionStart <= DataSelectionEnd
      ? DataSelectionEnd - DataSelectionStart + 1
      : DataSelectionStart - DataSelectionEnd;
  }

  // Standalone Memory Editor window
  void DrawWindow(const char* title, void* mem_data, size_t mem_size, size_t base_display_addr = 0x0000)
  {
    Sizes s;
    CalcSizes(s, mem_size, base_display_addr);
    ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(s.WindowWidth, FLT_MAX));

    Open = true;
    if (ImGui::Begin(title, &Open, ImGuiWindowFlags_NoScrollbar))
    {
      if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
        ImGui::OpenPopup("context");
      DrawContents(mem_data, mem_size, base_display_addr);
      if (ContentsWidthChanged)
      {
        CalcSizes(s, mem_size, base_display_addr);
        ImGui::SetWindowSize(ImVec2(s.WindowWidth, ImGui::GetWindowSize().y));
      }
    }
    ImGui::End();
  }

  // Memory Editor contents only
  void DrawContents(void* mem_data_void, size_t mem_size, size_t base_display_addr = 0x0000)
  {
    if (Cols < 1)
      Cols = 1;

    ImU8* mem_data = (ImU8*)mem_data_void;
    Sizes s;
    CalcSizes(s, mem_size, base_display_addr);
    const ImGuiContext& g = *ImGui::GetCurrentContext();
    ImGuiStyle& style = ImGui::GetStyle();
    const ImGuiIO& io = ImGui::GetIO();
    ImGuiWindow* window = ImGui::GetCurrentWindow();

    const ImU32 color_text = ImGui::GetColorU32(ImGuiCol_Text);
    const ImU32 color_disabled = OptGreyOutZeroes ? ImGui::GetColorU32(ImGuiCol_TextDisabled) : color_text;
    const ImU32 frame_bg_color = ImGui::GetColorU32(ImGuiCol_FrameBg);
    const ImU32 text_selected_color = ImGui::GetColorU32(ImGuiCol_TextSelectedBg);

    const char* format_address = OptUpperCaseHex ? "%0*" _PRISizeT "X: " : "%0*" _PRISizeT "x: ";
    const char* format_data = OptUpperCaseHex ? "%0*" _PRISizeT "X" : "%0*" _PRISizeT "x";
    const char* format_byte = OptUpperCaseHex ? "%02X" : "%02x";
    const char* format_byte_space = OptUpperCaseHex ? "%02X " : "%02x ";

    // We begin into our scrolling region with the 'ImGuiWindowFlags_NoMove' in order to prevent click from moving the window.
    // This is used as a facility since our main click detection code doesn't assign an ActiveId so the click would normally be caught as a window-move.
    const float height_separator = style.ItemSpacing.y;
    float footer_height = 0;
    if (OptShowOptions)
      footer_height += height_separator + ImGui::GetFrameHeightWithSpacing() * 1;
    if (OptShowDataPreview)
      footer_height += height_separator + ImGui::GetFrameHeightWithSpacing() * 1 + ImGui::GetTextLineHeightWithSpacing() * 3;
    ImGui::BeginChild("##scrolling", ImVec2(0, -footer_height), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    // We are not really using the clipper API correctly here, because we rely on visible_start_addr/visible_end_addr for our scrolling function.
    const int line_total_count = (int)((mem_size + Cols - 1) / Cols);
    ImGuiListClipper clipper;
    clipper.Begin(line_total_count, s.LineHeight);
    clipper.Step();
    const size_t visible_start_addr = clipper.DisplayStart * Cols;
    const size_t visible_end_addr = clipper.DisplayEnd * Cols;

    if (ReadOnly || DataEditingAddr >= mem_size)
      DataEditingAddr = (size_t)-1; // cancel edit

    if (DataEditingAddr != (size_t)-1)
    {
      if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow)) && DataEditingAddr >= (size_t)Cols) { DataEditingAddrNext = DataEditingAddr - Cols; }
      else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow)) && DataEditingAddr < mem_size - Cols) { DataEditingAddrNext = DataEditingAddr + Cols; }
      else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow)) && DataEditingAddr > 0) { DataEditingAddrNext = DataEditingAddr - 1; }
      else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow)) && DataEditingAddr < mem_size - 1) { DataEditingAddrNext = DataEditingAddr + 1; }
    }

    if (DataEditingAddrNext >= mem_size)
      DataEditingAddrNext = (size_t)-1;

    if (DataPreviewAddr >= mem_size)
      DataPreviewAddr = (size_t)-1;

    bool take_focus = false;

    // Focus switch
    if (DataEditingAddrNext != DataEditingAddr)
    {
      // Save edit
      if (DataEditingAddr)
      {
        unsigned int data_input_value = 0;
        if (sscanf(ByteInputBuf, "%X", &data_input_value) == 1) {
          if (WriteFn)
            WriteFn(mem_data, DataEditingAddr, (ImU8)data_input_value);
          else
            mem_data[DataEditingAddr] = (ImU8)data_input_value;
        }
      }
      // Load edit
      if (DataEditingAddrNext != (size_t)-1)
      {
          take_focus = true;

          sprintf(AddrInputBuf, format_data, s.AddrDigitsCount, base_display_addr + DataEditingAddrNext);
          sprintf(ByteInputBuf, format_byte, ReadFn ? ReadFn(mem_data, DataEditingAddrNext) : mem_data[DataEditingAddrNext]);

          if (DataEditingAddr != (size_t)-1 && (DataEditingAddr / Cols) != (DataEditingAddrNext / Cols))
          {
            // Track cursor movements
            const int scroll_offset = ((int)(DataEditingAddrNext / Cols) - (int)(DataEditingAddr / Cols));
            const bool scroll_desired = (scroll_offset < 0 && DataEditingAddrNext < visible_start_addr + Cols * 2) || (scroll_offset > 0 && DataEditingAddrNext > visible_end_addr - Cols * 2);
            if (scroll_desired)
              ImGui::SetScrollY(ImGui::GetScrollY() + scroll_offset * s.LineHeight);
          };
      }
      else if (!DataSelectionDrag)
      {
        DataSelectionStart = DataSelectionEnd = DataPreviewAddr;
      }

      DataEditingAddr = DataEditingAddrNext;
    }

    // Draw vertical separator
    ImVec2 window_pos = ImGui::GetWindowPos();
    if (OptShowAscii)
      draw_list->AddLine(ImVec2(window_pos.x + s.PosAsciiStart - s.GlyphWidth, window_pos.y), ImVec2(window_pos.x + s.PosAsciiStart - s.GlyphWidth, window_pos.y + 9999), ImGui::GetColorU32(ImGuiCol_Border));

    const float start_x = ImGui::GetCursorScreenPos().x;
    const float asc_x = start_x + s.PosAsciiStart;

    // Precompute hex columns' x positions (screen)
    std::vector<float> hex_col_xs(Cols);
    {
      float hex_cur = hex_col_xs[0] = s.PosHexStart;
      for (int n = 1; n < Cols; n++)
      {
        hex_cur += s.HexCellWidth;
        if (OptMidColsCount and n % OptMidColsCount == 0)
          hex_cur += s.SpacingBetweenMidCols;
        hex_col_xs[n] = hex_cur;
      }
    }

    // Prepare selection draw
    size_t seladdr_min = SelectionStart();
    size_t seladdr_max = seladdr_min + SelectionLen() - 1;

    // Draw Lines
    std::vector<HighlightType> col_hls(Cols);
    float line_y = ImGui::GetCursorScreenPos().y;
    for (int line_idx = clipper.DisplayStart; line_idx < clipper.DisplayEnd; line_idx++, line_y += s.LineHeight) // display only visible lines
    {
      const size_t line_addr = (size_t)(line_idx * Cols);
      const ImVec2 line_pos(start_x, line_y);

      // Draw Address
      ImGui::Text(format_address, s.AddrDigitsCount, base_display_addr + line_addr);

      // Draw Highlights
      size_t addr = line_addr;
      size_t hl_start_idx = 0;
      HighlightType hl_last_type = HighlightType::None;
      for (int n = 0; n <= Cols; n++, addr++)
      {
        HighlightType hltype = HighlightType::None;

        bool is_end = !(addr < mem_size && n < Cols); // beware: n and addr are valid only if !is_end
        if (!is_end) {
          hltype = GetHighlightType(mem_data, addr);
          col_hls[n] = hltype;
        }

        if (hltype != hl_last_type)
        {
          if (hl_last_type != HighlightType::None)
          {
            ImU32 c = hl_last_type == HighlightType::Func ? HighlightFuncColor : HighlightColor;

            // highlight in hex view
            ImVec2 c1(start_x + hex_col_xs[hl_start_idx]          , line_y               );
            ImVec2 c2(start_x + hex_col_xs[n - 1] + s.HexCellWidth, line_y + s.LineHeight);
            draw_list->AddRectFilled(c1, c2, c);

            // highlight in asc view
            if (OptShowAscii)
            {
                ImVec2 c1(asc_x + s.GlyphWidth * hl_start_idx, line_y               );
                ImVec2 c2(asc_x + s.GlyphWidth * n           , line_y + s.LineHeight);
                draw_list->AddRectFilled(c1, c2, c);
            }
          }

          hl_last_type = hltype;
          hl_start_idx = n;
        }

        if (is_end)
          break;
      }

      // Draw Selection (over highlights)
      const size_t last_addr = std::min(line_addr + Cols, mem_size) - 1;
      if (seladdr_min != (size_t)-1 && last_addr >= seladdr_min && line_addr <= seladdr_max)
      {
        size_t sel_col_start = seladdr_min > line_addr ? seladdr_min - line_addr : 0;
        size_t sel_col_last = seladdr_max < last_addr ? seladdr_max - line_addr : last_addr - line_addr;

        // selection in hex view
        ImVec2 c1(start_x + hex_col_xs[sel_col_start], line_y);
        ImVec2 c2(start_x + hex_col_xs[sel_col_last] + s.HexCellWidth, line_y + s.LineHeight);
        draw_list->AddRectFilled(c1, c2, frame_bg_color);
        draw_list->AddRectFilled(c1, c2, text_selected_color);

        // selection in asc view
        if (OptShowAscii)
        {
          ImVec2 c1(asc_x + s.GlyphWidth * sel_col_start     , line_y);
          ImVec2 c2(asc_x + s.GlyphWidth * (sel_col_last + 1), line_y + s.LineHeight);
          draw_list->AddRectFilled(c1, c2, frame_bg_color);
          draw_list->AddRectFilled(c1, c2, text_selected_color);
        }
      }

      // Draw Bytes
      addr = line_addr;
      for (int n = 0; n < Cols && addr < mem_size; n++, addr++)
      {
        const ImVec2 cell_pos(start_x + hex_col_xs[n], line_y);
        const ImVec2 char_pos(asc_x + s.GlyphWidth * n, line_y);

        const bool is_being_edited = addr == DataEditingAddr;
        const bool is_selected = addr >= seladdr_min && addr <= seladdr_max;
        
        ImU8 b = ReadFn ? ReadFn(mem_data, addr) : mem_data[addr];

        ImU32 color_cell = is_selected ? color_text ^ 0xFFFFFF : color_text;
        ImU32 color_cell_grey = is_selected ? color_disabled ^ 0xFFFFFF : color_disabled;

        if (!is_being_edited)
        {
          static char tmp[4];
          *(ImU32*)tmp = 0;
          size_t len = 2;

          // Draw Hex
          const ImVec2 hex_pos(cell_pos.x + HexCellPaddingX, cell_pos.y);
          if (OptShowHexII)
          {
            if ((b >= 32 && b < 128))
            {
              ImFormatString(tmp, 4, ".%c", b);
              draw_list->AddText(hex_pos, color_cell, tmp, tmp + 2);
            }
            else if (b == 0xFF && OptGreyOutZeroes)
            {
              *(ImU32*)tmp = '####';
              draw_list->AddText(hex_pos, color_cell_grey, tmp, tmp + 2);
            }
            else if (b != 0x00)
            {
              ImFormatString(tmp, 4, format_byte, b);
              draw_list->AddText(hex_pos, color_cell, tmp, tmp + 2);
            }
          }
          else
          {
            if (b == 0 && OptGreyOutZeroes)
            {
              *(ImU32*)tmp = '0000';
              draw_list->AddText(hex_pos, color_cell_grey, tmp, tmp + 2);
            }
            else
            {
              ImFormatString(tmp, 4, format_byte, b);
              draw_list->AddText(hex_pos, color_cell, tmp, tmp + 2);
            }
          }
        }
        // Draw Ascii edit bg
        else if (OptShowAscii) // && is_being_edited
        {
          const ImVec2 corner(char_pos.x + s.GlyphWidth, char_pos.y + s.LineHeight);
          draw_list->AddRectFilled(char_pos, corner, frame_bg_color);
          draw_list->AddRectFilled(char_pos, corner, text_selected_color);
        }

        // Draw Ascii
        if (OptShowAscii)
        {
          const ImU8 display_b = (b < 32 || b >= 128) ? '.' : b;
          draw_list->AddText(char_pos, (display_b == b) ? color_cell : color_cell_grey, (char*)&display_b, (char*)&display_b + 1);
        }

        if (DataEditingAddr == addr)
        {
          ImGui::SetCursorScreenPos(ImVec2(cell_pos.x + HexCellPaddingX, cell_pos.y));

          // Display text input on current byte

          ImVec2 c2 = cell_pos + ImVec2(s.HexCellWidth, s.LineHeight);
          draw_list->AddRectFilled(cell_pos, c2, frame_bg_color);
          draw_list->AddRectFilled(cell_pos, c2, text_selected_color);

          ImGui::PushID((void*)addr);
          
          if (take_focus) {
            ImGui::SetKeyboardFocusHere();
            ImGui::CaptureKeyboardFromApp(true);
            DataEditingNeedCursorReset = true;
          }

          ImGui::PushItemWidth(s.GlyphWidth * 2);

          static const ImGuiInputTextFlags flags = ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_NoHorizontalScroll | ImGuiInputTextFlags_AlwaysInsertMode;
          if (ImGui::InputText("##cursordata", ByteInputBuf, 3, flags))
          {
            DataEditingAddrNext = DataEditingAddr + 1;
          }
          else if (ImGui::IsItemActive())
          {
            const ImGuiID edit_id = ImGui::GetItemID();
            if (g.InputTextState.ID == edit_id) {
              if (DataEditingNeedCursorReset)
                const_cast<int&>(g.InputTextState.Stb.cursor) = 0;
              else if (g.InputTextState.Stb.cursor >= 2)
                DataEditingAddrNext = DataEditingAddr + 1;
              DataEditingNeedCursorReset = false;
            }
          }
          else if (!take_focus)
            DataEditingAddrNext = (size_t)-1;


          ImGui::PopItemWidth();

          ImGui::PopID();
        }
      } // 

      // Handle inputs
      {
        ImGui::PushID(line_idx);

        //bool pressed = ButtonBehavior(bb, id, &hovered, &held, flags);

        size_t hovered_addr = (size_t)-1;
        bool hovered_is_hex = false;

        const ImGuiID hexline_id = window->GetID("hexline");
        const ImVec2 hexline_pos(start_x + hex_col_xs[0], line_y);
        const ImRect hexline_bb(hexline_pos, hexline_pos + ImVec2(hex_col_xs.back() - hex_col_xs.front() + s.HexCellWidth, s.LineHeight));
        if (ImGui::ItemAdd(hexline_bb, hexline_id) && ImGui::IsItemHovered())
        {
          for (int i = 0; i < hex_col_xs.size(); ++i)
          {
            const float x = start_x + hex_col_xs[i];
            if (io.MousePos.x < x + s.HexCellWidth) {
              if (io.MousePos.x < x)
                break;
              hovered_addr = line_addr + i;
              hovered_is_hex = true;
            }
          }
        }
        else if (OptShowAscii)
        {
          const ImGuiID ascline_id = window->GetID("ascline");
          const ImVec2 ascline_pos(start_x + s.PosAsciiStart, line_y);
          const ImRect ascline_bb(ascline_pos, ascline_pos +  ImVec2(s.PosAsciiEnd - s.PosAsciiStart, s.LineHeight));
          if (ImGui::ItemAdd(ascline_bb, ascline_id) && ImGui::IsItemHovered())
          {
            hovered_addr = line_addr + (size_t)((io.MousePos.x - asc_x) / s.GlyphWidth);
          }
        }

        const bool draging = ImGui::IsMouseDragging(0);
        const bool clicked = ImGui::IsMouseClicked(0);
        const bool clickup = !ImGui::IsMouseDown(0);

        if (DataSelectionDrag && !draging && clickup) {
          DataSelectionDrag = false;
        }

        //ImGui::Text(draging ? "drag" : "!drag");
        //ImGui::Text(hovered_addr != (size_t)-1 ? "hov" : "!hov");
        //ImGui::Text(ImGui::IsMouseClicked(0) ? "clkd" : "!clkd");

        if (hovered_addr != (size_t)-1)
        {
          if (ImGui::IsMouseDoubleClicked(0))
          {
            DataEditingAddrNext = DataPreviewAddr = hovered_addr;
            DataSelectionStart = DataSelectionEnd = (size_t)-1;
          }
          else if (ImGui::IsMouseClicked(0))
          {
            if (DataEditingAddr != (size_t)-1 && io.KeyShift) // if shift-click with alive edit box
            {
              DataSelectionStart = DataEditingAddr;
            }
            else if (DataSelectionStart == (size_t)-1 || !io.KeyShift) // else if there is no current selection or normal click
              DataSelectionStart = hovered_addr;

            DataSelectionEnd = hovered_addr;
            DataSelectionDrag = true;
            DataEditingAddrNext = (size_t)-1;
            DataPreviewAddr = DataSelectionStart;
          }
          else if (DataSelectionDrag) // drag continue
          {
            DataSelectionEnd = hovered_addr;
          }
        }

        ImGui::PopID();
      }
    }

    IM_ASSERT(clipper.Step() == false);
    clipper.End();
    ImGui::PopStyleVar(2);
    ImGui::EndChild();

    const bool lock_show_data_preview = OptShowDataPreview;
    if (OptShowOptions)
    {
      ImGui::Separator();
      DrawOptionsLine(s, mem_data, mem_size, base_display_addr);
    }

    if (lock_show_data_preview)
    {
      ImGui::Separator();
      DrawPreviewLine(s, mem_data, mem_size, base_display_addr);
    }

    // Notify the main window of our ideal child content size (FIXME: we are missing an API to get the contents size from the child)
    ImGui::SetCursorPosX(s.WindowWidth);
  }

  void DrawOptionsLine(const Sizes& s, void* mem_data, size_t mem_size, size_t base_display_addr)
  {
    IM_UNUSED(mem_data);
    ImGuiStyle& style = ImGui::GetStyle();
    const char* format_range = OptUpperCaseHex ? "Range %0*" _PRISizeT "X..%0*" _PRISizeT "X" : "Range %0*" _PRISizeT "x..%0*" _PRISizeT "x";

    // Options menu
    if (ImGui::Button("Options"))
      ImGui::OpenPopup("context");
    if (ImGui::BeginPopup("context"))
    {
      ImGui::PushItemWidth(56);
      if (ImGui::DragInt("##cols", &Cols, 0.2f, 4, 32, "%d cols")) { ContentsWidthChanged = true; if (Cols < 1) Cols = 1; }
      ImGui::PopItemWidth();
      ImGui::Checkbox("Show Data Preview", &OptShowDataPreview);
      ImGui::Checkbox("Show HexII", &OptShowHexII);
      if (ImGui::Checkbox("Show Ascii", &OptShowAscii)) { ContentsWidthChanged = true; }
      ImGui::Checkbox("Grey out zeroes", &OptGreyOutZeroes);
      ImGui::Checkbox("Uppercase Hex", &OptUpperCaseHex);

      ImGui::EndPopup();
    }

    ImGui::SameLine();
    ImGui::Text(format_range, s.AddrDigitsCount, base_display_addr, s.AddrDigitsCount, base_display_addr + mem_size - 1);
    ImGui::SameLine();
    ImGui::PushItemWidth((s.AddrDigitsCount + 1) * s.GlyphWidth + style.FramePadding.x * 2.0f);
    if (ImGui::InputText("##addr", AddrInputBuf, 32, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue))
    {
      size_t goto_addr;
      if (sscanf(AddrInputBuf, "%" _PRISizeT "X", &goto_addr) == 1)
      {
        GotoAddr = goto_addr - base_display_addr;
        HighlightMin = HighlightMax = (size_t)-1;
      }
    }
    ImGui::PopItemWidth();

    if (GotoAddr != (size_t)-1)
    {
      if (GotoAddr < mem_size)
      {
        ImGui::BeginChild("##scrolling");
        ImGui::SetScrollFromPosY(ImGui::GetCursorStartPos().y + (GotoAddr / Cols) * ImGui::GetTextLineHeight());
        ImGui::EndChild();
        DataEditingAddrNext = DataPreviewAddr = GotoAddr;
      }
      GotoAddr = (size_t)-1;
    }
  }

  void DrawPreviewLine(const Sizes& s, void* mem_data_void, size_t mem_size, size_t base_display_addr)
  {
    IM_UNUSED(base_display_addr);
    ImU8* mem_data = (ImU8*)mem_data_void;
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Preview as:");
    ImGui::SameLine();
    ImGui::PushItemWidth((s.GlyphWidth * 10.0f) + style.FramePadding.x * 2.0f + style.ItemInnerSpacing.x);
    if (ImGui::BeginCombo("##combo_type", DataTypeGetDesc(PreviewDataType), ImGuiComboFlags_HeightLargest))
    {
      for (int n = 0; n < ImGuiDataType_COUNT; n++)
        if (ImGui::Selectable(DataTypeGetDesc((ImGuiDataType)n), PreviewDataType == n))
          PreviewDataType = (ImGuiDataType)n;
      ImGui::EndCombo();
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::PushItemWidth((s.GlyphWidth * 6.0f) + style.FramePadding.x * 2.0f + style.ItemInnerSpacing.x);
    ImGui::Combo("##combo_endianess", &PreviewEndianess, "LE\0BE\0\0");
    ImGui::PopItemWidth();

    char buf[128] = "";
    float x = s.GlyphWidth * 6.0f;
    bool has_value = DataPreviewAddr != (size_t)-1;
    if (has_value)
      DrawPreviewData(DataPreviewAddr, mem_data, mem_size, PreviewDataType, DataFormat_Dec, buf, (size_t)IM_ARRAYSIZE(buf));
    ImGui::Text("Dec"); ImGui::SameLine(x); ImGui::TextUnformatted(has_value ? buf : "N/A");
    if (has_value)
      DrawPreviewData(DataPreviewAddr, mem_data, mem_size, PreviewDataType, DataFormat_Hex, buf, (size_t)IM_ARRAYSIZE(buf));
    ImGui::Text("Hex"); ImGui::SameLine(x); ImGui::TextUnformatted(has_value ? buf : "N/A");
    if (has_value)
      DrawPreviewData(DataPreviewAddr, mem_data, mem_size, PreviewDataType, DataFormat_Bin, buf, (size_t)IM_ARRAYSIZE(buf));
    buf[IM_ARRAYSIZE(buf) - 1] = 0;
    ImGui::Text("Bin"); ImGui::SameLine(x); ImGui::TextUnformatted(has_value ? buf : "N/A");
  }

  // Utilities for Data Preview
  const char* DataTypeGetDesc(ImGuiDataType data_type) const
  {
    const char* descs[] = { "Int8", "Uint8", "Int16", "Uint16", "Int32", "Uint32", "Int64", "Uint64", "Float", "Double" };
    IM_ASSERT(data_type >= 0 && data_type < ImGuiDataType_COUNT);
    return descs[data_type];
  }

  size_t DataTypeGetSize(ImGuiDataType data_type) const
  {
    const size_t sizes[] = { 1, 1, 2, 2, 4, 4, 8, 8, sizeof(float), sizeof(double) };
    IM_ASSERT(data_type >= 0 && data_type < ImGuiDataType_COUNT);
    return sizes[data_type];
  }

  const char* DataFormatGetDesc(DataFormat data_format) const
  {
    const char* descs[] = { "Bin", "Dec", "Hex" };
    IM_ASSERT(data_format >= 0 && data_format < DataFormat_COUNT);
    return descs[data_format];
  }

  bool IsBigEndian() const
  {
    uint16_t x = 1;
    char c[2];
    memcpy(c, &x, 2);
    return c[0] != 0;
  }

  static void* EndianessCopyBigEndian(void* _dst, void* _src, size_t s, int is_little_endian)
  {
    if (is_little_endian)
    {
      uint8_t* dst = (uint8_t*)_dst;
      uint8_t* src = (uint8_t*)_src + s - 1;
      for (int i = 0, n = (int)s; i < n; ++i)
        memcpy(dst++, src--, 1);
      return _dst;
    }
    else
    {
      return memcpy(_dst, _src, s);
    }
  }

  static void* EndianessCopyLittleEndian(void* _dst, void* _src, size_t s, int is_little_endian)
  {
    if (is_little_endian)
    {
      return memcpy(_dst, _src, s);
    }
    else
    {
      uint8_t* dst = (uint8_t*)_dst;
      uint8_t* src = (uint8_t*)_src + s - 1;
      for (int i = 0, n = (int)s; i < n; ++i)
        memcpy(dst++, src--, 1);
      return _dst;
    }
  }

  void* EndianessCopy(void* dst, void* src, size_t size) const
  {
    static void* (*fp)(void*, void*, size_t, int) = NULL;
    if (fp == NULL)
      fp = IsBigEndian() ? EndianessCopyBigEndian : EndianessCopyLittleEndian;
    return fp(dst, src, size, PreviewEndianess);
  }

  const char* FormatBinary(const uint8_t* buf, int width) const
  {
    IM_ASSERT(width <= 64);
    size_t out_n = 0;
    static char out_buf[64 + 8 + 1];
    int n = width / 8;
    for (int j = n - 1; j >= 0; --j)
    {
      for (int i = 0; i < 8; ++i)
        out_buf[out_n++] = (buf[j] & (1 << (7 - i))) ? '1' : '0';
      out_buf[out_n++] = ' ';
    }
    IM_ASSERT(out_n < IM_ARRAYSIZE(out_buf));
    out_buf[out_n] = 0;
    return out_buf;
  }

  // [Internal]
  void DrawPreviewData(size_t addr, const ImU8* mem_data, size_t mem_size, ImGuiDataType data_type, DataFormat data_format, char* out_buf, size_t out_buf_size) const
  {
    uint8_t buf[8];
    size_t elem_size = DataTypeGetSize(data_type);
    size_t size = addr + elem_size > mem_size ? mem_size - addr : elem_size;
    if (ReadFn)
      for (int i = 0, n = (int)size; i < n; ++i)
        buf[i] = ReadFn(mem_data, addr + i);
    else
      memcpy(buf, mem_data + addr, size);

    if (data_format == DataFormat_Bin)
    {
      uint8_t binbuf[8];
      EndianessCopy(binbuf, buf, size);
      ImSnprintf(out_buf, out_buf_size, "%s", FormatBinary(binbuf, (int)size * 8));
      return;
    }

    out_buf[0] = 0;
    switch (data_type)
    {
    case ImGuiDataType_S8:
    {
      int8_t int8 = 0;
      EndianessCopy(&int8, buf, size);
      if (data_format == DataFormat_Dec) { ImSnprintf(out_buf, out_buf_size, "%hhd", int8); return; }
      if (data_format == DataFormat_Hex) { ImSnprintf(out_buf, out_buf_size, "0x%02x", int8 & 0xFF); return; }
      break;
    }
    case ImGuiDataType_U8:
    {
      uint8_t uint8 = 0;
      EndianessCopy(&uint8, buf, size);
      if (data_format == DataFormat_Dec) { ImSnprintf(out_buf, out_buf_size, "%hhu", uint8); return; }
      if (data_format == DataFormat_Hex) { ImSnprintf(out_buf, out_buf_size, "0x%02x", uint8 & 0XFF); return; }
      break;
    }
    case ImGuiDataType_S16:
    {
      int16_t int16 = 0;
      EndianessCopy(&int16, buf, size);
      if (data_format == DataFormat_Dec) { ImSnprintf(out_buf, out_buf_size, "%hd", int16); return; }
      if (data_format == DataFormat_Hex) { ImSnprintf(out_buf, out_buf_size, "0x%04x", int16 & 0xFFFF); return; }
      break;
    }
    case ImGuiDataType_U16:
    {
      uint16_t uint16 = 0;
      EndianessCopy(&uint16, buf, size);
      if (data_format == DataFormat_Dec) { ImSnprintf(out_buf, out_buf_size, "%hu", uint16); return; }
      if (data_format == DataFormat_Hex) { ImSnprintf(out_buf, out_buf_size, "0x%04x", uint16 & 0xFFFF); return; }
      break;
    }
    case ImGuiDataType_S32:
    {
      int32_t int32 = 0;
      EndianessCopy(&int32, buf, size);
      if (data_format == DataFormat_Dec) { ImSnprintf(out_buf, out_buf_size, "%d", int32); return; }
      if (data_format == DataFormat_Hex) { ImSnprintf(out_buf, out_buf_size, "0x%08x", int32); return; }
      break;
    }
    case ImGuiDataType_U32:
    {
      uint32_t uint32 = 0;
      EndianessCopy(&uint32, buf, size);
      if (data_format == DataFormat_Dec) { ImSnprintf(out_buf, out_buf_size, "%u", uint32); return; }
      if (data_format == DataFormat_Hex) { ImSnprintf(out_buf, out_buf_size, "0x%08x", uint32); return; }
      break;
    }
    case ImGuiDataType_S64:
    {
      int64_t int64 = 0;
      EndianessCopy(&int64, buf, size);
      if (data_format == DataFormat_Dec) { ImSnprintf(out_buf, out_buf_size, "%lld", (long long)int64); return; }
      if (data_format == DataFormat_Hex) { ImSnprintf(out_buf, out_buf_size, "0x%016llx", (long long)int64); return; }
      break;
    }
    case ImGuiDataType_U64:
    {
      uint64_t uint64 = 0;
      EndianessCopy(&uint64, buf, size);
      if (data_format == DataFormat_Dec) { ImSnprintf(out_buf, out_buf_size, "%llu", (long long)uint64); return; }
      if (data_format == DataFormat_Hex) { ImSnprintf(out_buf, out_buf_size, "0x%016llx", (long long)uint64); return; }
      break;
    }
    case ImGuiDataType_Float:
    {
      float float32 = 0.0f;
      EndianessCopy(&float32, buf, size);
      if (data_format == DataFormat_Dec) { ImSnprintf(out_buf, out_buf_size, "%f", float32); return; }
      if (data_format == DataFormat_Hex) { ImSnprintf(out_buf, out_buf_size, "%a", float32); return; }
      break;
    }
    case ImGuiDataType_Double:
    {
      double float64 = 0.0;
      EndianessCopy(&float64, buf, size);
      if (data_format == DataFormat_Dec) { ImSnprintf(out_buf, out_buf_size, "%f", float64); return; }
      if (data_format == DataFormat_Hex) { ImSnprintf(out_buf, out_buf_size, "%a", float64); return; }
      break;
    }
    case ImGuiDataType_COUNT:
      break;
    } // Switch
    IM_ASSERT(0); // Shouldn't reach
  }
};

#undef _PRISizeT
#undef ImSnprintf

#ifdef _MSC_VER
#pragma warning (pop)
#endif
