#ifndef __LITE_RENDERER_HPP__
#define __LITE_RENDERER_HPP__

#include <string>
#include <vector>

#include "Lite.h"

/* A font loaded into the host's ImGui font atlas. Text is drawn through
** ImDrawList, so the renderer never needs to know which graphics API the host
** uses. */
class LiteFont {
public:
  LiteFont() = default;
  ~LiteFont() = default;

private:
  std::string m_filename;
  ImFont *m_imFont = nullptr;
  float m_size = 0.0f;
  int m_height = 0;
  int m_tabWidth = 0;
  bool m_created = false;
  bool m_failed = false;
  bool m_destroy = false;

  friend class LiteRenderer;
};

/* Turns the editor's draw commands into Dear ImGui draw list commands. The
** dirty rectangle cache of the software renderer is not needed here: the host
** redraws the whole ImGui frame every time. */
class LiteRenderer {
public:
  void Initialize(ImFontAtlas *atlas);
  void Terminate();

  /* Creates and destroys fonts queued by the Lua GC. Must run outside of an
  ** ImGui frame. */
  void Prepare();

  void BeginFrame(ImDrawList *drawList, const ImVec2 &origin, const ImVec2 &size);
  void EndFrame();
  void GetSize(int *width, int *height) const;

  void SetClipRect(int x, int y, int width, int height);
  void DrawRect(int x, int y, int width, int height, LiteColor color);
  int DrawText(LiteFont *font, const char *text, int x, int y, LiteColor color);

  LiteFont *LoadFont(const char *filename, float size);
  void ReleaseFont(LiteFont *font);
  void SetTabWidth(LiteFont *font, int width);
  int GetFontWidth(LiteFont *font, const char *text) const;
  int GetFontHeight(LiteFont *font) const;

  /* Accepted for compatibility with the original software renderer, which
  ** visualized the dirty rectangles. Every Dear ImGui frame is redrawn from
  ** scratch, so there is nothing to visualize here. */
  void SetDebug(bool debug) { m_debug = debug; }

private:
  bool CreateFont(LiteFont *font);
  void DestroyFont(LiteFont *font);
  void WarmFont(LiteFont *font);
  float MeasureText(LiteFont *font, const char *begin, const char *end) const;

  ImFontAtlas *m_atlas = nullptr;
  ImDrawList *m_drawList = nullptr;
  ImVec2 m_origin = ImVec2(0.0f, 0.0f);
  ImVec2 m_size = ImVec2(0.0f, 0.0f);
  bool m_basePushed = false;
  bool m_clipPushed = false;
  bool m_debug = false;
  std::vector<LiteFont *> m_fonts;
};

#endif
