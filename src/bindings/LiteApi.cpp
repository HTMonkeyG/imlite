#include "LiteApi.h"

#include <cctype>
#include <cstring>
#include <string>
#include <vector>

#include "imgui.h"

#include "Lite.h"
#include "LiteLua.h"
#include "LitePlatform.h"
#include "LiteRenderer.h"

#define LITE_FONT_METATABLE "Font"


static Lite *GetLite(lua_State *L) {
  lua_getfield(L, LUA_REGISTRYINDEX, LITE_INSTANCE_KEY);
  Lite *lite = (Lite *) lua_touserdata(L, -1);
  lua_pop(L, 1);
  return lite;
}


/* ------------------------------------------------------------------------- */
/* Renderer                                                                  */
/* ------------------------------------------------------------------------- */

static LiteColor CheckColor(lua_State *L, int index, int defaultComponent) {
  if (lua_isnoneornil(L, index)) {
    return LiteColor(
      (unsigned char) defaultComponent,
      (unsigned char) defaultComponent,
      (unsigned char) defaultComponent,
      255);
  }

  lua_rawgeti(L, index, 1);
  lua_rawgeti(L, index, 2);
  lua_rawgeti(L, index, 3);
  lua_rawgeti(L, index, 4);

  LiteColor color;
  color.r = (unsigned char) luaL_checknumber(L, -4);
  color.g = (unsigned char) luaL_checknumber(L, -3);
  color.b = (unsigned char) luaL_checknumber(L, -2);
  color.a = (unsigned char) luaL_optnumber(L, -1, 255);
  lua_pop(L, 4);
  return color;
}


static LiteRenderer *GetRenderer(lua_State *L) {
  Lite *lite = GetLite(L);
  return lite ? lite->GetRenderer() : nullptr;
}


static int f_show_debug(lua_State *L) {
  luaL_checkany(L, 1);
  LiteRenderer *renderer = GetRenderer(L);
  if (renderer) {
    renderer->SetDebug(lua_toboolean(L, 1) != 0);
  }
  return 0;
}


static int f_get_size(lua_State *L) {
  int width = 0;
  int height = 0;
  LiteRenderer *renderer = GetRenderer(L);
  if (renderer) {
    renderer->GetSize(&width, &height);
  }
  lua_pushnumber(L, width);
  lua_pushnumber(L, height);
  return 2;
}


static int f_begin_frame(lua_State *L) {
  (void) L;
  /* The host owns the ImGui frame; nothing to do here. */
  return 0;
}


static int f_end_frame(lua_State *L) {
  (void) L;
  return 0;
}


static int f_set_clip_rect(lua_State *L) {
  LiteRenderer *renderer = GetRenderer(L);
  if (renderer) {
    int x = (int) luaL_checknumber(L, 1);
    int y = (int) luaL_checknumber(L, 2);
    int width = (int) luaL_checknumber(L, 3);
    int height = (int) luaL_checknumber(L, 4);
    renderer->SetClipRect(x, y, width, height);
  }
  return 0;
}


static int f_draw_rect(lua_State *L) {
  LiteRenderer *renderer = GetRenderer(L);
  if (renderer) {
    int x = (int) luaL_checknumber(L, 1);
    int y = (int) luaL_checknumber(L, 2);
    int width = (int) luaL_checknumber(L, 3);
    int height = (int) luaL_checknumber(L, 4);
    LiteColor color = CheckColor(L, 5, 255);
    renderer->DrawRect(x, y, width, height, color);
  }
  return 0;
}


static int f_draw_text(lua_State *L) {
  LiteRenderer *renderer = GetRenderer(L);
  LiteFont **font = (LiteFont **) luaL_checkudata(L, 1, LITE_FONT_METATABLE);
  const char *text = luaL_checkstring(L, 2);
  int x = (int) luaL_checknumber(L, 3);
  int y = (int) luaL_checknumber(L, 4);
  LiteColor color = CheckColor(L, 5, 255);

  if (renderer && font && *font) {
    x = renderer->DrawText(*font, text, x, y, color);
  }
  lua_pushnumber(L, x);
  return 1;
}


/* ------------------------------------------------------------------------- */
/* Renderer font                                                             */
/* ------------------------------------------------------------------------- */

static int f_font_load(lua_State *L) {
  LiteRenderer *renderer = GetRenderer(L);
  const char *filename = luaL_checkstring(L, 1);
  float size = (float) luaL_checknumber(L, 2);

  LiteFont *font = renderer ? renderer->LoadFont(filename, size) : nullptr;
  if (!font) {
    return luaL_error(L, "failed to load font");
  }

  LiteFont **self = (LiteFont **) lua_newuserdata(L, sizeof(LiteFont *));
  *self = font;
  luaL_setmetatable(L, LITE_FONT_METATABLE);
  return 1;
}


static int f_font_set_tab_width(lua_State *L) {
  LiteRenderer *renderer = GetRenderer(L);
  LiteFont **font = (LiteFont **) luaL_checkudata(L, 1, LITE_FONT_METATABLE);
  int width = (int) luaL_checknumber(L, 2);
  if (renderer && font && *font) {
    renderer->SetTabWidth(*font, width);
  }
  return 0;
}


static int f_font_gc(lua_State *L) {
  LiteRenderer *renderer = GetRenderer(L);
  LiteFont **font = (LiteFont **) luaL_checkudata(L, 1, LITE_FONT_METATABLE);
  if (renderer && font && *font) {
    renderer->ReleaseFont(*font);
    *font = nullptr;
  }
  return 0;
}


static int f_font_get_width(lua_State *L) {
  LiteRenderer *renderer = GetRenderer(L);
  LiteFont **font = (LiteFont **) luaL_checkudata(L, 1, LITE_FONT_METATABLE);
  const char *text = luaL_checkstring(L, 2);
  int width = 0;
  if (renderer && font && *font) {
    width = renderer->GetFontWidth(*font, text);
  }
  lua_pushnumber(L, width);
  return 1;
}


static int f_font_get_height(lua_State *L) {
  LiteRenderer *renderer = GetRenderer(L);
  LiteFont **font = (LiteFont **) luaL_checkudata(L, 1, LITE_FONT_METATABLE);
  int height = 0;
  if (renderer && font && *font) {
    height = renderer->GetFontHeight(*font);
  }
  lua_pushnumber(L, height);
  return 1;
}


static const luaL_Reg rendererFontLib[] = {
  { "__gc",          f_font_gc            },
  { "load",          f_font_load          },
  { "set_tab_width", f_font_set_tab_width },
  { "get_width",     f_font_get_width     },
  { "get_height",    f_font_get_height    },
  { nullptr,         nullptr              }
};


static int LiteOpenRendererFont(lua_State *L) {
  luaL_newmetatable(L, LITE_FONT_METATABLE);
  luaL_setfuncs(L, rendererFontLib, 0);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  return 1;
}


/* ------------------------------------------------------------------------- */
/* System                                                                    */
/* ------------------------------------------------------------------------- */

static int f_poll_event(lua_State *L) {
  Lite *lite = GetLite(L);
  if (!lite) {
    return 0;
  }

  LiteEvent event;
  if (!lite->PollEvent(event)) {
    return 0;
  }

  switch (event.Type) {
    case LiteEventType::Quit:
      lua_pushliteral(L, "quit");
      return 1;

    case LiteEventType::Resized:
      lua_pushliteral(L, "resized");
      lua_pushnumber(L, event.A);
      lua_pushnumber(L, event.B);
      return 3;

    case LiteEventType::Exposed:
      lua_pushliteral(L, "exposed");
      return 1;

    case LiteEventType::KeyPressed:
      lua_pushliteral(L, "keypressed");
      lua_pushlstring(L, event.Text.c_str(), event.Text.size());
      return 2;

    case LiteEventType::KeyReleased:
      lua_pushliteral(L, "keyreleased");
      lua_pushlstring(L, event.Text.c_str(), event.Text.size());
      return 2;

    case LiteEventType::TextInput:
      lua_pushliteral(L, "textinput");
      lua_pushlstring(L, event.Text.c_str(), event.Text.size());
      return 2;

    case LiteEventType::MousePressed:
      lua_pushliteral(L, "mousepressed");
      lua_pushlstring(L, event.Text.c_str(), event.Text.size());
      lua_pushnumber(L, event.A);
      lua_pushnumber(L, event.B);
      lua_pushnumber(L, event.C);
      return 5;

    case LiteEventType::MouseReleased:
      lua_pushliteral(L, "mousereleased");
      lua_pushlstring(L, event.Text.c_str(), event.Text.size());
      lua_pushnumber(L, event.A);
      lua_pushnumber(L, event.B);
      return 4;

    case LiteEventType::MouseMoved:
      lua_pushliteral(L, "mousemoved");
      lua_pushnumber(L, event.A);
      lua_pushnumber(L, event.B);
      lua_pushnumber(L, event.C);
      lua_pushnumber(L, event.D);
      return 5;

    case LiteEventType::MouseWheel:
      lua_pushliteral(L, "mousewheel");
      lua_pushnumber(L, event.A);
      return 2;

    case LiteEventType::FileDropped:
      lua_pushliteral(L, "filedropped");
      lua_pushlstring(L, event.Text.c_str(), event.Text.size());
      lua_pushnumber(L, event.A);
      lua_pushnumber(L, event.B);
      return 4;
  }

  return 0;
}


static int f_wait_event(lua_State *L) {
  double seconds = luaL_checknumber(L, 1);
  LitePlatform::Sleep(seconds);
  lua_pushboolean(L, 0);
  return 1;
}


static const char *cursorOptions[] = {
  "arrow",
  "ibeam",
  "sizeh",
  "sizev",
  "hand",
  nullptr
};

static const ImGuiMouseCursor cursorValues[] = {
  ImGuiMouseCursor_Arrow,
  ImGuiMouseCursor_TextInput,
  ImGuiMouseCursor_ResizeEW,
  ImGuiMouseCursor_ResizeNS,
  ImGuiMouseCursor_Hand
};

static int f_set_cursor(lua_State *L) {
  int option = luaL_checkoption(L, 1, "arrow", cursorOptions);
  ImGui::SetMouseCursor(cursorValues[option]);
  return 0;
}


static int f_set_window_title(lua_State *L) {
  const char *title = luaL_checkstring(L, 1);
  Lite *lite = GetLite(L);
  if (lite) {
    lite->SetWindowTitle(title);
  }
  return 0;
}


static const char *windowOptions[] = { "normal", "maximized", "fullscreen", nullptr };

static int f_set_window_mode(lua_State *L) {
  int option = luaL_checkoption(L, 1, "normal", windowOptions);
  Lite *lite = GetLite(L);
  if (lite) {
    lite->SetWindowMode((LiteWindowMode) option);
  }
  return 0;
}


static int f_window_has_focus(lua_State *L) {
  Lite *lite = GetLite(L);
  lua_pushboolean(L, lite && lite->WindowHasFocus());
  return 1;
}


static int f_show_confirm_dialog(lua_State *L) {
  const char *title = luaL_checkstring(L, 1);
  const char *message = luaL_checkstring(L, 2);
  Lite *lite = GetLite(L);
  lua_pushboolean(L, lite && lite->Confirm(title, message));
  return 1;
}


static int f_chdir(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  std::string error;
  if (!LitePlatform::ChangeDirectory(path, error)) {
    return luaL_error(L, "chdir() failed: %s", error.c_str());
  }
  return 0;
}


static int f_list_dir(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  std::vector<std::string> entries;
  std::string error;
  if (!LitePlatform::ListDirectory(path, entries, error)) {
    lua_pushnil(L);
    lua_pushstring(L, error.c_str());
    return 2;
  }

  lua_newtable(L);
  for (size_t i = 0; i < entries.size(); i++) {
    lua_pushlstring(L, entries[i].c_str(), entries[i].size());
    lua_rawseti(L, -2, (int) i + 1);
  }
  return 1;
}


static int f_absolute_path(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);
  std::string result;
  if (!LitePlatform::GetAbsolutePath(path, result)) {
    return 0;
  }
  lua_pushlstring(L, result.c_str(), result.size());
  return 1;
}


static int f_get_file_info(lua_State *L) {
  const char *path = luaL_checkstring(L, 1);

  std::string type;
  long long size = 0;
  double modified = 0.0;
  std::string error;
  if (!LitePlatform::GetFileInfo(path, type, size, modified, error)) {
    lua_pushnil(L);
    lua_pushstring(L, error.c_str());
    return 2;
  }

  lua_newtable(L);
  lua_pushnumber(L, modified);
  lua_setfield(L, -2, "modified");

  lua_pushnumber(L, (lua_Number) size);
  lua_setfield(L, -2, "size");

  if (!type.empty()) {
    lua_pushlstring(L, type.c_str(), type.size());
  } else {
    lua_pushnil(L);
  }
  lua_setfield(L, -2, "type");

  return 1;
}


static int f_get_clipboard(lua_State *L) {
  const char *text = ImGui::GetClipboardText();
  if (!text) {
    return 0;
  }
  lua_pushstring(L, text);
  return 1;
}


static int f_set_clipboard(lua_State *L) {
  size_t length = 0;
  const char *text = luaL_checklstring(L, 1, &length);
  (void) length;
  ImGui::SetClipboardText(text);
  return 0;
}


static int f_get_time(lua_State *L) {
  lua_pushnumber(L, LitePlatform::GetTime());
  return 1;
}


static int f_sleep(lua_State *L) {
  double seconds = luaL_checknumber(L, 1);
  LitePlatform::Sleep(seconds);
  return 0;
}


static int f_exec(lua_State *L) {
  const char *command = luaL_checkstring(L, 1);
  LitePlatform::Execute(command);
  return 0;
}


static int f_fuzzy_match(lua_State *L) {
  const char *str = luaL_checkstring(L, 1);
  const char *pattern = luaL_checkstring(L, 2);
  int score = 0;
  int run = 0;

  while (*str && *pattern) {
    while (*str == ' ') { str++; }
    while (*pattern == ' ') { pattern++; }
    if (tolower((unsigned char) *str) == tolower((unsigned char) *pattern)) {
      score += run * 10 - (*str != *pattern);
      run++;
      pattern++;
    } else {
      score -= 10;
      run = 0;
    }
    str++;
  }
  if (*pattern) {
    return 0;
  }

  lua_pushnumber(L, score - (int) strlen(str));
  return 1;
}


/* ------------------------------------------------------------------------- */
/* Libraries                                                                 */
/* ------------------------------------------------------------------------- */

static const luaL_Reg rendererLib[] = {
  { "show_debug",    f_show_debug    },
  { "get_size",      f_get_size      },
  { "begin_frame",   f_begin_frame   },
  { "end_frame",     f_end_frame     },
  { "set_clip_rect", f_set_clip_rect },
  { "draw_rect",     f_draw_rect     },
  { "draw_text",     f_draw_text     },
  { nullptr,         nullptr         }
};


static const luaL_Reg systemLib[] = {
  { "poll_event",          f_poll_event          },
  { "wait_event",          f_wait_event          },
  { "set_cursor",          f_set_cursor          },
  { "set_window_title",    f_set_window_title    },
  { "set_window_mode",     f_set_window_mode     },
  { "window_has_focus",    f_window_has_focus    },
  { "show_confirm_dialog", f_show_confirm_dialog },
  { "chdir",               f_chdir               },
  { "list_dir",            f_list_dir            },
  { "absolute_path",       f_absolute_path       },
  { "get_file_info",       f_get_file_info       },
  { "get_clipboard",       f_get_clipboard       },
  { "set_clipboard",       f_set_clipboard       },
  { "get_time",            f_get_time            },
  { "sleep",               f_sleep               },
  { "exec",                f_exec                },
  { "fuzzy_match",         f_fuzzy_match         },
  { nullptr,               nullptr               }
};


int LiteOpenRenderer(lua_State *L) {
  luaL_newlib(L, rendererLib);
  lua_pushcfunction(L, LiteOpenRendererFont);
  lua_call(L, 0, 1);
  lua_setfield(L, -2, "font");
  return 1;
}


int LiteOpenSystem(lua_State *L) {
  luaL_newlib(L, systemLib);
  return 1;
}
