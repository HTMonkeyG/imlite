#include "LiteRenderer.h"

#include <cfloat>
#include <cmath>
#include <cstring>

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
  /* windows.h maps these to their A variants. */
  #ifdef CreateFont
    #undef CreateFont
  #endif
  #ifdef DrawText
    #undef DrawText
  #endif
#endif


namespace {
/* Dear ImGui expects file names to be UTF-8; Lua and the rest of the binding
** use the native narrow encoding. */
std::string ToUtf8(const std::string &text) {
#if defined(_WIN32)
  if (text.empty()) {
    return std::string();
  }

  int wideLength = MultiByteToWideChar(
    CP_ACP, 0, text.c_str(), -1, nullptr, 0);
  if (wideLength <= 0) {
    return text;
  }
  std::wstring wide((size_t) wideLength, L'\0');
  MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, &wide[0], wideLength);

  int utf8Length = WideCharToMultiByte(
    CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
  if (utf8Length <= 0) {
    return text;
  }
  std::string utf8((size_t) utf8Length, '\0');
  WideCharToMultiByte(
    CP_UTF8, 0, wide.c_str(), -1, &utf8[0], utf8Length, nullptr, nullptr);
  utf8.resize((size_t) utf8Length - 1);
  return utf8;
#else
  return text;
#endif
}
}


void LiteRenderer::Initialize(ImFontAtlas *atlas) {
  m_atlas = atlas;
  m_drawList = nullptr;
  m_origin = ImVec2(0.0f, 0.0f);
  m_size = ImVec2(0.0f, 0.0f);
  m_basePushed = false;
  m_clipPushed = false;
  m_debug = false;
}


void LiteRenderer::Terminate() {
  for (LiteFont *font : m_fonts) {
    if (font->m_created && font->m_imFont && m_atlas && !m_atlas->Locked) {
      m_atlas->RemoveFont(font->m_imFont);
    }
    delete font;
  }
  m_fonts.clear();
  m_atlas = nullptr;
  m_drawList = nullptr;
  m_basePushed = false;
  m_clipPushed = false;
}


void LiteRenderer::Prepare() {
  if (!m_atlas || m_atlas->Locked) {
    return;
  }

  for (size_t i = 0; i < m_fonts.size(); ) {
    LiteFont *font = m_fonts[i];
    if (font->m_destroy) {
      DestroyFont(font);
      delete font;
      m_fonts.erase(m_fonts.begin() + (ptrdiff_t) i);
      continue;
    }
    if (!font->m_created && !font->m_failed) {
      CreateFont(font);
    }
    i++;
  }
}


bool LiteRenderer::CreateFont(LiteFont *font) {
  if (!m_atlas) {
    font->m_failed = true;
    return false;
  }

  ImFontConfig config;
  config.PixelSnapH = true;
  config.Flags = ImFontFlags_NoLoadError;

  std::string filename = ToUtf8(font->m_filename);
  ImFont *imFont = m_atlas->AddFontFromFileTTF(
    filename.c_str(), font->m_size, &config);
  if (!imFont) {
    font->m_failed = true;
    return false;
  }

  font->m_imFont = imFont;
  font->m_created = true;

  ImFontBaked *baked = imFont->GetFontBaked(font->m_size);
  if (baked) {
    font->m_height = (int) (baked->Ascent - baked->Descent + 0.5f);
  }
  if (font->m_height <= 0) {
    font->m_height = (int) (font->m_size + 0.5f);
  }

  /* Default tab width, until a document view overrides it. */
  font->m_tabWidth = (int) (MeasureText(font, "    ", nullptr) + 0.5f);

  WarmFont(font);
  return true;
}


void LiteRenderer::DestroyFont(LiteFont *font) {
  if (font->m_created && font->m_imFont && m_atlas && !m_atlas->Locked) {
    m_atlas->RemoveFont(font->m_imFont);
  }
  font->m_imFont = nullptr;
  font->m_created = false;
}


void LiteRenderer::WarmFont(LiteFont *font) {
  if (!font->m_imFont) {
    return;
  }

  /* Rasterize the common glyphs while the atlas is still unlocked, so the
  ** first frames do not grow the font texture in the middle of a frame. */
  ImFontBaked *baked = font->m_imFont->GetFontBaked(font->m_size);
  if (!baked) {
    return;
  }
  for (unsigned int codepoint = 32; codepoint < 127; codepoint++) {
    baked->FindGlyph((ImWchar) codepoint);
  }
  for (unsigned int codepoint = 160; codepoint < 256; codepoint++) {
    baked->FindGlyph((ImWchar) codepoint);
  }
}


LiteFont *LiteRenderer::LoadFont(const char *filename, float size) {
  if (!filename || size <= 0.0f) {
    return nullptr;
  }

  LiteFont *font = new LiteFont();
  font->m_filename = filename;
  font->m_size = size;

  /* When the host backend does not support dynamic font textures the atlas is
  ** locked during a frame; the font is created on the next Prepare(). */
  if (m_atlas && !m_atlas->Locked) {
    if (!CreateFont(font)) {
      delete font;
      return nullptr;
    }
  }

  m_fonts.push_back(font);
  return font;
}


void LiteRenderer::ReleaseFont(LiteFont *font) {
  if (font) {
    font->m_destroy = true;
  }
}


void LiteRenderer::SetTabWidth(LiteFont *font, int width) {
  if (font && width >= 0) {
    font->m_tabWidth = width;
  }
}


int LiteRenderer::GetFontHeight(LiteFont *font) const {
  return font ? font->m_height : 0;
}


float LiteRenderer::MeasureText(
  LiteFont *font,
  const char *begin,
  const char *end
) const {
  if (!font || !font->m_imFont || !begin) {
    return 0.0f;
  }
  if (!end) {
    end = begin + strlen(begin);
  }
  if (begin >= end) {
    return 0.0f;
  }
  return font->m_imFont->CalcTextSizeA(
    font->m_size, FLT_MAX, 0.0f, begin, end).x;
}


int LiteRenderer::GetFontWidth(LiteFont *font, const char *text) const {
  if (!font || !text) {
    return 0;
  }

  float width = 0.0f;
  const char *cursor = text;
  while (*cursor) {
    if (*cursor == '\t') {
      width += (float) font->m_tabWidth;
      cursor++;
      continue;
    }
    if (*cursor == '\n' || *cursor == '\r') {
      cursor++;
      continue;
    }

    const char *begin = cursor;
    while (*cursor && *cursor != '\t' && *cursor != '\n' && *cursor != '\r') {
      cursor++;
    }
    width += MeasureText(font, begin, cursor);
  }
  return (int) std::floor(width + 0.5f);
}


void LiteRenderer::BeginFrame(
  ImDrawList *drawList,
  const ImVec2 &origin,
  const ImVec2 &size
) {
  m_drawList = drawList;
  m_origin = origin;
  m_size = size;
  m_basePushed = false;
  m_clipPushed = false;

  if (m_drawList) {
    ImVec2 minimum = origin;
    ImVec2 maximum = ImVec2(origin.x + size.x, origin.y + size.y);
    if (maximum.x < minimum.x) {
      maximum.x = minimum.x;
    }
    if (maximum.y < minimum.y) {
      maximum.y = minimum.y;
    }
    m_drawList->PushClipRect(minimum, maximum, true);
    m_basePushed = true;
  }
}


void LiteRenderer::EndFrame() {
  if (m_drawList) {
    if (m_clipPushed) {
      m_drawList->PopClipRect();
      m_clipPushed = false;
    }
    if (m_basePushed) {
      m_drawList->PopClipRect();
      m_basePushed = false;
    }
  }
  m_drawList = nullptr;
}


void LiteRenderer::GetSize(int *width, int *height) const {
  if (width) {
    *width = (int) m_size.x;
  }
  if (height) {
    *height = (int) m_size.y;
  }
}


void LiteRenderer::SetClipRect(int x, int y, int width, int height) {
  if (!m_drawList) {
    return;
  }

  float x1 = (float) x;
  float y1 = (float) y;
  float x2 = (float) x + (width > 0 ? (float) width : 0.0f);
  float y2 = (float) y + (height > 0 ? (float) height : 0.0f);

  /* Intersect with the editor area. */
  if (x1 < 0.0f) x1 = 0.0f;
  if (y1 < 0.0f) y1 = 0.0f;
  if (x2 > m_size.x) x2 = m_size.x;
  if (y2 > m_size.y) y2 = m_size.y;
  if (x1 > x2) x1 = x2;
  if (y1 > y2) y1 = y2;

  if (m_clipPushed) {
    m_drawList->PopClipRect();
    m_clipPushed = false;
  }

  ImVec2 minimum = ImVec2(m_origin.x + x1, m_origin.y + y1);
  ImVec2 maximum = ImVec2(m_origin.x + x2, m_origin.y + y2);
  m_drawList->PushClipRect(minimum, maximum, true);
  m_clipPushed = true;
}


void LiteRenderer::DrawRect(
  int x,
  int y,
  int width,
  int height,
  LiteColor color
) {
  if (!m_drawList || color.a == 0 || width <= 0 || height <= 0) {
    return;
  }

  ImVec2 minimum = ImVec2(m_origin.x + (float) x, m_origin.y + (float) y);
  ImVec2 maximum = ImVec2(minimum.x + (float) width, minimum.y + (float) height);
  m_drawList->AddRectFilled(minimum, maximum, IM_COL32(color.r, color.g, color.b, color.a));
}


int LiteRenderer::DrawText(
  LiteFont *font,
  const char *text,
  int x,
  int y,
  LiteColor color
) {
  if (!m_drawList || !font || !font->m_imFont || !text || color.a == 0) {
    return x;
  }

  ImU32 packedColor = IM_COL32(color.r, color.g, color.b, color.a);
  float positionX = (float) x;

  const char *cursor = text;
  while (*cursor) {
    if (*cursor == '\t') {
      positionX += (float) font->m_tabWidth;
      cursor++;
      continue;
    }
    if (*cursor == '\n' || *cursor == '\r') {
      cursor++;
      continue;
    }

    const char *begin = cursor;
    while (*cursor && *cursor != '\t' && *cursor != '\n' && *cursor != '\r') {
      cursor++;
    }

    m_drawList->AddText(
      font->m_imFont,
      font->m_size,
      ImVec2(m_origin.x + positionX, m_origin.y + (float) y),
      packedColor,
      begin,
      cursor
    );
    positionX += MeasureText(font, begin, cursor);
  }

  return (int) std::floor(positionX + 0.5f);
}
