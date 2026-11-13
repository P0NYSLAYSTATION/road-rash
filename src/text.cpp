#include "text.hpp"
#include "assets.hpp"
#include "pe_strings.hpp"
#include "utf8_path.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H
#include FT_DRIVER_H
#include FT_MODULE_H
#include FT_OUTLINE_H
#include FT_TRUETYPE_TABLES_H

namespace rr {
// GDI size for a positive cell height: Wine's calc_ppem_for_height, or for a
// face with VDMX (load_VDMX) the record whose yMax-yMin equals the height, else
// the last one below it; that record also gives tmAscent/tmDescent. Heights
// below the first record fall back to the ppem rule. Wine ppemprobe sweep
// 8..48 (Wingdings, Courier New Bold, Arial, Times New Roman): every height
// the VDMX covers matches.
struct GdiSize {
    int ppem, ascent, descent; // ascent < 0: from the win metrics at the FreeType scale
};

// Wine-GDI renders these faces with its bundled FreeType 2.13.3; the build pins
// that release (cmake/freetype.cmake). Each face owns its library: the renderer
// caches glyphs and never shares FreeType state between instances.
struct TextRenderer::FontData {
    std::vector<uint8_t> bytes;
    FT_Library library = nullptr;
    FT_Face face = nullptr;
    int win_ascent = 0, win_descent = 0; // OS/2 usWinAscent/usWinDescent: GDI cell metrics
    int units_per_em = 0;                // head.unitsPerEm for the GDI ppem rule
    bool symbol = false;                 // Microsoft symbol cmap (Wingdings): bytes map to U+F000+byte
    struct Vdmx {
        int ppem, y_max, y_min;
    };
    std::vector<Vdmx> vdmx; // VDMX group for the 1:1 aspect ratio, by ascending ppem (system faces)
    std::vector<std::pair<int, int>> gasp; // (rangeMaxPPEM, behaviour); empty: grid-fit and gray
#ifdef _WIN32
    HANDLE registration = nullptr;
    std::wstring native_file;
    struct NativeFont {
        HDC dc = nullptr;
        HFONT handle = nullptr;
        HGDIOBJ previous_font = nullptr, previous_bitmap = nullptr;
        HBITMAP mono = nullptr, menu = nullptr;
        void* mono_bits = nullptr;
        void* menu_bits = nullptr;
        int mono_width = 0, mono_height = 0, mono_stride = 0, pad = 0;
        int menu_width = 0, menu_height = 0;
        std::vector<uint16_t> painted;
        TEXTMETRICA metrics{};
        ~NativeFont() {
            if (dc && previous_bitmap) SelectObject(dc, previous_bitmap);
            if (dc && previous_font) SelectObject(dc, previous_font);
            if (mono) DeleteObject(mono);
            if (menu) DeleteObject(menu);
            if (handle) DeleteObject(handle);
            if (dc) DeleteDC(dc);
        }
        void mono_surface(int width, int height) {
            if (mono && mono_width >= width && mono_height >= height) return;
            const bool selected = mono && GetCurrentObject(dc, OBJ_BITMAP) == mono;
            if (selected) SelectObject(dc, previous_bitmap);
            if (mono) {
                DeleteObject(mono);
                mono = nullptr;
            }
            mono_width = std::max(width, mono_width);
            mono_height = std::max(height, mono_height);
            mono_stride = ((mono_width + 31) / 32) * 4;
            struct {
                BITMAPINFOHEADER header;
                RGBQUAD colours[2];
            } info{};
            info.header.biSize = sizeof(BITMAPINFOHEADER);
            info.header.biWidth = mono_width;
            info.header.biHeight = -mono_height;
            info.header.biPlanes = 1;
            info.header.biBitCount = 1;
            info.colours[1].rgbRed = info.colours[1].rgbGreen = info.colours[1].rgbBlue = 255;
            mono = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO*>(&info), DIB_RGB_COLORS, &mono_bits,
                                    nullptr, 0);
            if (!mono) throw std::runtime_error("Cannot allocate native GDI glyph surface");
            if (selected) SelectObject(dc, mono);
        }
        void menu_surface(int width, int height) {
            if (menu && menu_width >= width && menu_height >= height) return;
            const bool selected = menu && GetCurrentObject(dc, OBJ_BITMAP) == menu;
            if (selected) SelectObject(dc, mono ? static_cast<HGDIOBJ>(mono) : previous_bitmap);
            if (menu) {
                DeleteObject(menu);
                menu = nullptr;
            }
            menu_width = std::max(width, menu_width);
            menu_height = std::max(height, menu_height);
            struct {
                BITMAPINFOHEADER header;
                DWORD masks[3];
            } info{};
            info.header.biSize = sizeof(BITMAPINFOHEADER);
            info.header.biWidth = menu_width;
            info.header.biHeight = -menu_height;
            info.header.biPlanes = 1;
            info.header.biBitCount = 16;
            info.header.biCompression = BI_BITFIELDS;
            info.masks[0] = 0xf800;
            info.masks[1] = 0x07e0;
            info.masks[2] = 0x001f;
            menu = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO*>(&info), DIB_RGB_COLORS, &menu_bits,
                                    nullptr, 0);
            if (!menu) throw std::runtime_error("Cannot allocate native GDI menu surface");
            if (selected) SelectObject(dc, menu);
        }
        static uint16_t pack565(uint32_t p) {
            return uint16_t(((p >> 19) << 11) | (((p >> 10) & 63) << 5) | ((p >> 3) & 31));
        }
        static uint32_t unpack565(uint16_t p) {
            const uint32_t r = p >> 11, g = (p >> 5) & 63, b = p & 31;
            return ((r << 3 | r >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (b << 3 | b >> 2);
        }
        template <class Paint>
        int paint565(RgbImage& image, const TextRect& area, uint32_t colour, Paint&& paint) {
            const int w = area.right - area.left, h = area.bottom - area.top;
            menu_surface(w, h);
            auto* bits = static_cast<uint16_t*>(menu_bits);
            const int stride = (menu_width + 1) & ~1;
            bool transparent = false;
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x) {
                    const auto p = image.at(area.left + x, area.top + y);
                    transparent |= p == 0x01000000;
                    bits[size_t(y) * stride + x] = pack565(p);
                }
            const HGDIOBJ previous = SelectObject(dc, menu);
            SetTextColor(dc, RGB((colour >> 16) & 255, (colour >> 8) & 255, colour & 255));
            const int result = paint();
            GdiFlush();
            if (transparent) {
                // The HUD uses a transparency sentinel outside glyph ink.
                // A white-on-black pass identifies ink even for black text
                // whose output is identical to the sentinel's black RGB.
                if (painted.size() < size_t(w) * h) painted.resize(size_t(w) * h);
                for (int y = 0; y < h; ++y) {
                    std::copy_n(bits + size_t(y) * stride, w, painted.data() + size_t(y) * w);
                    std::fill_n(bits + size_t(y) * stride, w, uint16_t(0));
                }
                SetTextColor(dc, RGB(255, 255, 255));
                const int mask_result = paint();
                GdiFlush();
                if (mask_result != result) {
                    SelectObject(dc, previous);
                    throw std::runtime_error("Native GDI text coverage failed");
                }
            }
            SelectObject(dc, previous);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x) {
                    auto& p = image.at(area.left + x, area.top + y);
                    const auto value = bits[size_t(y) * stride + x];
                    if (transparent) {
                        if (value) p = unpack565(painted[size_t(y) * w + x]);
                    } else if (value != pack565(p))
                        p = unpack565(value);
                }
            return result;
        }
    };
    mutable std::map<int, std::unique_ptr<NativeFont>> native_fonts;
    NativeFont& native_font(Face requested, int height, TextSurface surface) const {
        if (height <= 0 || height > 4096) throw std::runtime_error("Invalid text cell height");
        if (const auto it = native_fonts.find(height); it != native_fonts.end()) return *it->second;
        auto n = std::make_unique<NativeFont>();
        n->dc = CreateCompatibleDC(nullptr);
        const bool system = requested == Face::wingdings || requested == Face::courier_new_bold;
        // The Bold face shares its family name with Regular. Request the
        // original full name so a weight-zero LOGFONT cannot select Regular.
        static constexpr const char* requests[] = {
            "Badloc",    "Futura Condensed Regular", "Futura Condensed Bold", "Futura Condensed Demi Regular",
            "Wingdings", "New Courier Bold"};
        const char* name = requests[static_cast<size_t>(requested)];
        if (!n->dc) throw std::runtime_error("Cannot create native GDI font DC");
        n->pad = std::max(96, height * 2);
        n->mono_width = std::max(320, height * 8);
        n->mono_height = std::max(288, height * 8);
        n->mono_stride = ((n->mono_width + 31) / 32) * 4;
        if (surface == TextSurface::rgb565) {
            n->menu_surface(n->mono_width, n->mono_height);
            n->previous_bitmap = SelectObject(n->dc, n->menu);
        } else {
            n->mono_surface(n->mono_width, n->mono_height);
            n->previous_bitmap = SelectObject(n->dc, n->mono);
        }
        n->handle =
            CreateFontA(height, 0, 0, 0, system ? (symbol ? 400 : 700) : 0, 0, 0, 0,
                        symbol ? SYMBOL_CHARSET : ANSI_CHARSET, system ? OUT_TT_PRECIS : OUT_TT_ONLY_PRECIS,
                        0, DEFAULT_QUALITY, system ? (symbol ? 4 : 5) : 0, name);
        if (!n->handle) throw std::runtime_error("Cannot create native GDI font");
        n->previous_font = SelectObject(n->dc, n->handle);
        SetBkMode(n->dc, TRANSPARENT);
        SetTextAlign(n->dc, TA_TOP | TA_LEFT | TA_NOUPDATECP);
        if (!GetTextMetricsA(n->dc, &n->metrics)) throw std::runtime_error("Cannot measure native GDI font");
        return *native_fonts.emplace(height, std::move(n)).first->second;
    }
#endif
    GdiSize size(int height) const;
    bool gray(int ppem) const {
        for (const auto& [max_ppem, flags] : gasp)
            if (ppem <= max_ppem) return (flags & 2) != 0; // GASP_DOGRAY
        return gasp.empty();
    }
    ~FontData() {
#ifdef _WIN32
        native_fonts.clear();
        if (registration) RemoveFontMemResourceEx(registration);
        if (!native_file.empty()) RemoveFontResourceExW(native_file.c_str(), 0, nullptr);
#endif
        if (face) FT_Done_Face(face);
        if (library) FT_Done_FreeType(library);
    }
};

// GDI pixel size for a positive lfHeight cell (0x0042f720 table): ppem rounds
// H*upm/cell, then steps down once when the rounded scaled cell would exceed
// H. Probed under Wine-GDI (textprobe sweeps, heights 8..80, all four faces:
// 260/292 match plain rounding; the 32 outliers all take exactly one less).
int gdi_ppem(int height, int upm, int cell) {
    int p = (height * upm + cell / 2) / cell;
    if ((cell * p + upm / 2) / upm > height) --p;
    return p < 1 ? 1 : p;
}

GdiSize TextRenderer::FontData::size(int height) const {
    for (size_t i = 0; i < vdmx.size(); ++i) {
        const int cell = vdmx[i].y_max - vdmx[i].y_min;
        if (cell == height) return {vdmx[i].ppem, vdmx[i].y_max, -vdmx[i].y_min};
        if (cell > height) {
            if (i == 0) break;
            return {vdmx[i - 1].ppem, vdmx[i - 1].y_max, -vdmx[i - 1].y_min};
        }
    }
    return {gdi_ppem(height, units_per_em, win_ascent + win_descent), -1, -1};
}

// Opens the face. use_os2 selects OS/2 win metrics (the game faces); without
// it, or without an OS/2 table, the hhea ascender/descender stand in.
std::unique_ptr<TextRenderer::FontData> TextRenderer::open_font(std::vector<uint8_t> bytes, bool use_os2,
                                                                const std::string& what,
                                                                bool register_native) {
    auto f = std::make_unique<TextRenderer::FontData>();
    f->bytes = std::move(bytes);
    if (FT_Init_FreeType(&f->library)) throw std::runtime_error("FreeType initialisation failed");
    // Wine's default: the v40 ("minimal") TrueType interpreter.
    FT_UInt interpreter = TT_INTERPRETER_VERSION_40;
    FT_Property_Set(f->library, "truetype", "interpreter-version", &interpreter);
    if (f->bytes.size() < 12 ||
        FT_New_Memory_Face(f->library, f->bytes.data(), FT_Long(f->bytes.size()), 0, &f->face) ||
        !FT_IS_SCALABLE(f->face))
        throw std::runtime_error("Invalid TrueType font" + what);
    // Symbol fonts carry only a (3,0) cmap; Wine selects it and looks a byte up
    // as U+F000+byte, then as the byte itself (win32u get_glyph_index).
    if ((!f->face->charmap || f->face->charmap->encoding != FT_ENCODING_UNICODE) &&
        !FT_Select_Charmap(f->face, FT_ENCODING_MS_SYMBOL))
        f->symbol = true;
    const auto* os2 = static_cast<const TT_OS2*>(FT_Get_Sfnt_Table(f->face, FT_SFNT_OS2));
    if (use_os2 && os2 && os2->version != 0xffff) {
        f->win_ascent = os2->usWinAscent;
        f->win_descent = os2->usWinDescent;
    } else {
        f->win_ascent = f->face->ascender;
        f->win_descent = -f->face->descender;
    }
    f->units_per_em = f->face->units_per_EM;
    if (f->win_ascent + f->win_descent <= 0) throw std::runtime_error("Font has no vertical metrics");
    if (f->units_per_em <= 0) throw std::runtime_error("Font has no units-per-em");
    // VDMX (Windows' installed faces have one; the disc faces do not): Wine's
    // load_VDMX takes the first ratio record that is the 0:0:0 wildcard or
    // covers a 1:1 device, then that group's (ppem, yMax, yMin) records.
    // gasp: Wine anti-aliases a size only when its range sets GASP_DOGRAY
    // (Wingdings: off for ppem 9..28); faces without the table always do.
    FT_ULong length = 0;
    const FT_ULong gasp_tag = FT_MAKE_TAG('g', 'a', 's', 'p');
    if (!FT_Load_Sfnt_Table(f->face, gasp_tag, 0, nullptr, &length) && length >= 4) {
        std::vector<uint8_t> t(length);
        FT_Load_Sfnt_Table(f->face, gasp_tag, 0, t.data(), &length);
        const size_t ranges = size_t(t[2] << 8 | t[3]);
        for (size_t i = 0; i < ranges && 8 + i * 4 <= t.size(); ++i)
            f->gasp.emplace_back(t[4 + i * 4] << 8 | t[5 + i * 4], t[6 + i * 4] << 8 | t[7 + i * 4]);
    }
    length = 0;
    const FT_ULong tag = FT_MAKE_TAG('V', 'D', 'M', 'X');
    if (!FT_Load_Sfnt_Table(f->face, tag, 0, nullptr, &length) && length >= 6) {
        std::vector<uint8_t> t(length);
        FT_Load_Sfnt_Table(f->face, tag, 0, t.data(), &length);
        auto u16 = [&](size_t o) { return o + 2 <= t.size() ? int(t[o] << 8 | t[o + 1]) : -1; };
        const int ratios = u16(4);
        for (int i = 0; i < ratios; ++i) {
            const size_t r = 6 + size_t(i) * 4;
            if (r + 4 > t.size()) break;
            const int x = t[r + 1], y0 = t[r + 2], y1 = t[r + 3];
            if (!((x == 0 && y0 == 0 && y1 == 0) || (x == 1 && y0 <= 1 && y1 >= 1))) continue;
            const int group = u16(6 + size_t(ratios) * 4 + size_t(i) * 2), recs = u16(size_t(group));
            for (int j = 0; group >= 0 && j < recs; ++j) {
                const size_t e = size_t(group) + 4 + size_t(j) * 6;
                if (e + 6 > t.size()) break;
                f->vdmx.push_back({u16(e), int16_t(u16(e + 2)), int16_t(u16(e + 4))});
            }
            break;
        }
    }
#ifdef _WIN32
    if (register_native) {
        DWORD count = 0;
        f->registration = AddFontMemResourceEx(f->bytes.data(), DWORD(f->bytes.size()), nullptr, &count);
        if (!f->registration || !count)
            throw std::runtime_error("Cannot register original TrueType font" + what);
    }
#else
    (void)register_native;
#endif
    return f;
}

std::vector<uint8_t> builtin_wine_wingdings(); // generated from third_party/wine-fonts/wingding.ttf

namespace {
std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
} // namespace

std::vector<std::string> system_font_paths(Face face) {
    // GDI resolves the game's requests to these files: Wingdings 0x0046b200, and
    // "New Courier Bold" (no such face) to Courier New at weight 700, whose
    // bold file Windows and macOS both install.
    const char* file = face == Face::wingdings          ? "Wingdings.ttf"
                       : face == Face::courier_new_bold ? "Courier New Bold.ttf"
                                                        : nullptr;
    if (!file) return {};
    std::vector<std::string> paths;
#ifdef _WIN32
    const char* windir = std::getenv("WINDIR");
    paths.push_back(std::string(windir ? windir : "C:\\Windows") + "\\Fonts\\" +
                    (face == Face::wingdings ? "wingding.ttf" : "courbd.ttf"));
#elif defined(__APPLE__)
    paths.push_back(std::string("/System/Library/Fonts/Supplemental/") + file);
    paths.push_back(std::string("/Library/Fonts/") + file);
#else
    // Linux: the msttcorefonts package (Courier New only; Wingdings is not in it).
    for (const char* dir :
         {"/usr/share/fonts/truetype/msttcorefonts/", "/usr/share/fonts/msttcore/", "/usr/share/fonts/TTF/"})
        paths.push_back(std::string(dir) + (face == Face::wingdings ? "wingding.ttf" : "courbd.ttf"));
    // Without it, fontconfig's Courier New substitute: Liberation Mono Bold (metric-compatible).
    if (face == Face::courier_new_bold)
        for (const char* dir : {"/usr/share/fonts/truetype/liberation/", "/usr/share/fonts/liberation-mono/",
                                "/usr/share/fonts/liberation/", "/usr/share/fonts/TTF/"})
            paths.push_back(std::string(dir) + "LiberationMono-Bold.ttf");
#endif
    return paths;
}

void TextRenderer::load_disc_faces(const Assets& assets) {
    // 0x0042f720 installs Badloc, Futura Bold, Futura Demi and Futura Regular from Text\.
    const std::pair<Face, const char*> files[] = {{Face::badloc, "TEXT/BADLOC.DLL"},
                                                  {Face::futura_bold, "TEXT/FUTB.DLL"},
                                                  {Face::futura_demi, "TEXT/FUTD.DLL"},
                                                  {Face::futura_regular, "TEXT/FUTR.DLL"}};
    for (const auto& [face, path] : files) {
#ifdef _WIN32
        // Match the original's AddFontResource registration. Private memory
        // fonts change GDI's fallback metrics even for installed system faces:
        // Courier New Bold DEL at height 18 becomes 5 rather than 10 on RGB565.
        auto loaded = open_font(assets.read(path), true, std::string(" ") + path, false);
        const auto native_path = path_from_utf8(assets.root() + "/" + path).wstring();
        if (!AddFontResourceExW(native_path.c_str(), 0, nullptr))
            throw std::runtime_error(std::string("Cannot register original font ") + path);
        loaded->native_file = native_path;
        fonts_[face] = std::move(loaded);
#else
        fonts_[face] = open_font(assets.read(path), true, std::string(" ") + path);
#endif
    }
}

TextRenderer::TextRenderer(const Assets& assets, TextSurface surface) : surface_(surface) {
    load_disc_faces(assets);
    for (Face face : {Face::wingdings, Face::courier_new_bold}) {
        for (const std::string& path : system_font_paths(face)) {
            std::vector<uint8_t> bytes = read_file(path);
            if (bytes.empty()) continue;
            try {
                // Installed system faces retain Windows' font linking. A
                // second private registration changes fallback advances.
                fonts_[face] = open_font(std::move(bytes), true, " " + path, false);
                break;
            } catch (const std::runtime_error&) {
            }
        }
    }
    // Without a host Wingdings, Wine draws its own (fonts/wingding.sfd, LGPL-2.1).
    if (!fonts_.count(Face::wingdings))
        fonts_[Face::wingdings] = open_font(builtin_wine_wingdings(), true, " (built-in Wine Wingdings)");
}

TextRenderer::TextRenderer(const Assets& assets, TextSurface surface,
                           const std::map<Face, std::string>& system_fonts)
    : surface_(surface) {
    load_disc_faces(assets);
    for (const auto& [face, path] : system_fonts)
        fonts_[face] = path.empty() ? open_font(builtin_wine_wingdings(), true, " (built-in Wine Wingdings)")
                                    : open_font(read_file(path), true, " " + path, false);
}

TextRenderer::TextRenderer(Face face, std::vector<uint8_t> ttf) {
    fonts_[face] = open_font(std::move(ttf), false, "");
}

TextRenderer::~TextRenderer() = default;

bool TextRenderer::has(Face f) const {
    return fonts_.count(f) != 0;
}

namespace {
FT_UInt glyph_index(FT_Face face, bool symbol, uint8_t ch) {
    if (!symbol) return FT_Get_Char_Index(face, cp1252_codepoint(ch));
    const FT_UInt index = FT_Get_Char_Index(face, 0xf000u + ch);
    return index ? index : FT_Get_Char_Index(face, ch);
}
} // namespace

bool TextRenderer::covers(Face f, uint8_t ch) const {
    const FontData& d = font(f);
    return glyph_index(d.face, d.symbol, ch) != 0;
}

const TextRenderer::FontData& TextRenderer::font(Face f) const {
    const auto it = fonts_.find(f);
    if (it == fonts_.end()) throw std::runtime_error("Font face not loaded");
    return *it->second;
}

const TextRenderer::Glyph& TextRenderer::glyph(Face face, int height, uint8_t ch) const {
    const auto key = std::make_tuple(face, height, ch);
    const auto it = cache_.find(key);
    if (it != cache_.end()) return it->second;
    const FontData& f = font(face);
#ifdef _WIN32
    // TextOut on the original indexed race surface uses Windows' monochrome
    // rasterizer, including its byte mapping, fallback glyphs and cell metrics.
    // Keep FreeType for other hosts; a Wine bitmap is not a Windows golden.
    auto& n = f.native_font(face, height, surface_);
    Glyph native;
    const char byte = char(ch);
    SIZE extent{};
    // Windows' ANSI width cache can retain a monochrome fallback advance on
    // a colour DC (Courier New Bold DEL: 5 versus the original RGB565 probe's
    // 10). For fixed-pitch colour text, explicit CP1252-to-Unicode measurement
    // preserves the original cell advance. Proportional and symbol faces keep
    // their ANSI fallback/byte behavior, which the probes also distinguish.
    const wchar_t code = wchar_t(cp1252_codepoint(ch));
    const bool measured =
        (surface_ == TextSurface::rgb565 && !f.symbol && !(n.metrics.tmPitchAndFamily & TMPF_FIXED_PITCH))
            ? GetTextExtentPoint32W(n.dc, &code, 1, &extent) != 0
            : GetTextExtentPoint32A(n.dc, &byte, 1, &extent) != 0;
    if (!measured) throw std::runtime_error("Cannot measure native GDI glyph");
    native.advance = extent.cx;
    if (surface_ == TextSurface::rgb565) return cache_.emplace(key, std::move(native)).first->second;
    memset(n.mono_bits, 0, size_t(n.mono_stride) * n.mono_height);
    SetTextColor(n.dc, RGB(255, 255, 255));
    if (!TextOutA(n.dc, n.pad, n.pad, &byte, 1)) throw std::runtime_error("Cannot draw native GDI glyph");
    GdiFlush();
    auto ink = [&](int x, int y) {
        return (static_cast<const uint8_t*>(n.mono_bits)[size_t(y) * n.mono_stride + x / 8] &
                (0x80u >> (x & 7))) != 0;
    };
    int left = n.mono_width, top = n.mono_height, right = -1, bottom = -1;
    for (int y = 0; y < n.mono_height; ++y)
        for (int x = 0; x < n.mono_width; ++x)
            if (ink(x, y)) {
                left = std::min(left, x);
                right = std::max(right, x);
                top = std::min(top, y);
                bottom = std::max(bottom, y);
            }
    if (right >= left) {
        native.x0 = left - n.pad;
        native.y0 = top - n.pad;
        native.w = right - left + 1;
        native.h = bottom - top + 1;
        native.ink.resize(size_t(native.w) * native.h);
        for (int y = 0; y < native.h; ++y)
            for (int x = 0; x < native.w; ++x)
                native.ink[size_t(y) * native.w + x] = ink(left + x, top + y) ? 16 : 0;
    }
    return cache_.emplace(key, std::move(native)).first->second;
#else
    // Wine-GDI TextOutA glyphs (tools/glyphprobe.c, all four faces at 11 heights):
    // FreeType at the integer GDI ppem with v40 hinting; the hinted metric box
    // floored and ceiled to whole pixels; top at tmAscent - ceil(bearingY), rows
    // above the cell top dropped (accented capitals at small sizes). 1-bit
    // surfaces hint for the mono target and rasterise mono (8446/8448); 16-bit
    // ones hint for the normal target and quantise 8-bit coverage to 17 levels
    // by truncation, levels 0 and 1 drawing nothing (8448/8448).
    const GdiSize size = f.size(height);
    const int ppem = size.ppem;
    const bool smooth = surface_ == TextSurface::rgb565 && f.gray(ppem);
    Glyph g;
    if (FT_Set_Pixel_Sizes(f.face, 0, FT_UInt(ppem))) return cache_.emplace(key, std::move(g)).first->second;
    const FT_UInt index = glyph_index(f.face, f.symbol, ch); // 0: .notdef
    // Advance: design units scaled to 26.6, then rounded (8448/8448; ties near
    // .5 px go up, which the hinted advance does not reproduce).
    FT_Fixed units = 0;
    FT_Get_Advance(f.face, index, FT_LOAD_NO_SCALE, &units);
    g.advance = int((FT_MulFix(units, f.face->size->metrics.x_scale) + 32) >> 6);
    const FT_Int32 target = smooth ? FT_LOAD_TARGET_NORMAL : FT_LOAD_TARGET_MONO;
    if (FT_Load_Glyph(f.face, index, FT_LOAD_NO_BITMAP | FT_LOAD_IGNORE_GLOBAL_ADVANCE_WIDTH | target))
        return cache_.emplace(key, std::move(g)).first->second;
    FT_GlyphSlot slot = f.face->glyph;
    const FT_Glyph_Metrics& m = slot->metrics;
    const FT_Pos left = m.horiBearingX & -64, right = (m.horiBearingX + m.width + 63) & -64;
    const FT_Pos top = (m.horiBearingY + 63) & -64, bottom = (m.horiBearingY - m.height) & -64;
    const int w = int((right - left) >> 6), h = int((top - bottom) >> 6);
    const int ascent = size.ascent >= 0
                           ? size.ascent
                           : int((FT_MulFix(f.win_ascent, f.face->size->metrics.y_scale) + 32) >> 6);
    if (w > 0 && h > 0 && slot->format == FT_GLYPH_FORMAT_OUTLINE) {
        const int pitch = smooth ? w : (w + 7) / 8;
        std::vector<uint8_t> bits(size_t(pitch) * size_t(h));
        FT_Bitmap bitmap{};
        bitmap.width = unsigned(w);
        bitmap.rows = unsigned(h);
        bitmap.pitch = pitch;
        bitmap.pixel_mode = smooth ? FT_PIXEL_MODE_GRAY : FT_PIXEL_MODE_MONO;
        bitmap.num_grays = smooth ? 256 : 2;
        bitmap.buffer = bits.data();
        FT_Outline_Translate(&slot->outline, -left, -bottom);
        FT_Outline_Get_Bitmap(f.library, &slot->outline, &bitmap);
        const int y0 = ascent - int(top >> 6);
        const int skip = std::min(h, std::max(0, -y0));
        g.x0 = int(left >> 6);
        g.y0 = y0 + skip;
        g.w = w;
        g.h = h - skip;
        g.ink.resize(size_t(g.w) * size_t(g.h));
        for (int y = 0; y < g.h; ++y) {
            const uint8_t* row = bits.data() + size_t(y + skip) * size_t(pitch);
            for (int x = 0; x < w; ++x) {
                const int level = smooth ? row[x] * 17 / 256 : (row[x / 8] >> (7 - x % 8) & 1) * 16;
                g.ink[size_t(y) * size_t(w) + size_t(x)] = uint8_t(level > 1 ? level : 0);
            }
        }
    }
    return cache_.emplace(key, std::move(g)).first->second;
#endif
}

int TextRenderer::width(Face face, int height, const std::string& text) const {
    int w = 0;
    for (unsigned char c : text) w += glyph(face, height, c).advance;
    return w;
}

int TextRenderer::line_height(Face face, int height) const {
#ifdef _WIN32
    return font(face).native_font(face, height, surface_).metrics.tmHeight;
#else
    // GDI multiline pitch: tmAscent + tmDescent at the ppem scale (textprobe
    // TEXTMETRIC sweeps; e.g. 32px Futura Regular advances 33, not 32).
    const FontData& f = font(face);
    const GdiSize size = f.size(height);
    if (size.ascent >= 0) return size.ascent + size.descent;
    const int ppem = size.ppem;
    return (f.win_ascent * ppem + f.units_per_em / 2) / f.units_per_em +
           (f.win_descent * ppem + f.units_per_em / 2) / f.units_per_em;
#endif
}

namespace {
// Wine's 16-bit DIB text blend (dibdrv draw_glyph_16 / aa_color / get_range),
// over a 17-step gamma ramp. Channels are 8-bit expansions of RGB565 fields;
// the result is truncated back to 565.
constexpr uint8_t aa_ramp[17] = {0,    0x4d, 0x68, 0x7c, 0x8c, 0x9a, 0xa7, 0xb2, 0xbd,
                                 0xc7, 0xd0, 0xd9, 0xe1, 0xe9, 0xf0, 0xf8, 0xff};

uint32_t expand565(uint32_t rgb) {
    const uint32_t r = rgb >> 19 & 0x1f, g = rgb >> 10 & 0x3f, b = rgb >> 3 & 0x1f;
    return (r << 3 | r >> 2) << 16 | (g << 2 | g >> 4) << 8 | (b << 3 | b >> 2);
}

int aa_channel(int dst, int text, int level) {
    if (dst == text) return dst;
    if (dst > text) {
        const int max = aa_ramp[16 - level] + (0xff - aa_ramp[16 - level]) * text / 0xff;
        return text + (dst - text) * (max - text) / (0xff - text);
    }
    const int min = aa_ramp[level] * text / aa_ramp[16];
    return text - (text - dst) * (text - min) / text;
}

} // namespace

uint32_t gdi_blend565(uint32_t dst, uint32_t text, int level) {
    dst = expand565(dst);
    text = expand565(text);
    uint32_t out = 0;
    for (int shift : {16, 8, 0})
        out |= uint32_t(aa_channel(int(dst >> shift & 0xff), int(text >> shift & 0xff), level)) << shift;
    return expand565(out);
}

void TextRenderer::draw(RgbImage& image, Face face, int height, int x, int y, uint32_t rgb,
                        const std::string& text) const {
    draw_clipped(image, nullptr, face, height, x, y, rgb, text);
}

void TextRenderer::draw_clipped(RgbImage& image, const TextRect* clip, Face face, int height, int x, int y,
                                uint32_t rgb, const std::string& text) const {
#ifdef _WIN32
    if (surface_ == TextSurface::rgb565 && !text.empty()) {
        // Let Windows blend into the actual 16-bit surface. Coverage from a
        // 32-bit ClearType DIB cannot be reduced to Wine's 17-level gray ramp.
        auto& n = font(face).native_font(face, height, surface_);
        int left = std::max(0, x - height * 2), top = std::max(0, y - height * 2);
        int right = std::min(image.width, x + width(face, height, text) + height * 2);
        int bottom = std::min(image.height, y + height * 3);
        if (clip) {
            left = std::max(left, clip->left);
            right = std::min(right, clip->right);
            top = std::max(top, clip->top);
            bottom = std::min(bottom, clip->bottom);
        }
        if (right <= left || bottom <= top) return;
        const int result = n.paint565(image, {left, top, right, bottom}, rgb, [&] {
            int pen = x;
            bool ok = true;
            for (unsigned char ch : text) {
                const char byte = char(ch);
                ok &= TextOutA(n.dc, pen - left, y - top, &byte, 1) != 0;
                pen += glyph(face, height, ch).advance;
            }
            return ok ? 1 : 0;
        });
        if (!result) throw std::runtime_error("Cannot draw native GDI menu text");
        return;
    }
#endif
    const bool smooth = surface_ == TextSurface::rgb565;
    if (smooth) rgb = expand565(rgb);
    for (unsigned char c : text) {
        const Glyph& g = glyph(face, height, c);
        for (int gy = 0; gy < g.h; ++gy)
            for (int gx = 0; gx < g.w; ++gx) {
                const int level = g.ink[size_t(gy) * size_t(g.w) + size_t(gx)];
                if (!level) continue;
                const int px = x + g.x0 + gx, py = y + g.y0 + gy;
                if (!image.contains(px, py)) continue;
                if (clip && (px < clip->left || px >= clip->right || py < clip->top || py >= clip->bottom))
                    continue;
                uint32_t& p = image.at(px, py);
                p = level >= 16 ? rgb : gdi_blend565(p, rgb, level);
            }
        x += g.advance;
    }
}

int TextRenderer::draw_box(RgbImage* image, Face face, int height, TextRect& rect, uint32_t rgb,
                           const std::string& text, unsigned flags) const {
#ifdef _WIN32
    auto& n = font(face).native_font(face, height, surface_);
    std::vector<char> buffer(text.begin(), text.end());
    buffer.resize(text.size() + 5, 0); // DrawTextEx permits four appended characters.
    RECT measured{rect.left, rect.top, rect.right, rect.bottom};
    const int measured_height =
        DrawTextExA(n.dc, buffer.data(), int(text.size()), &measured, flags | DT_CALCRECT, nullptr);
    if (flags & dt::calcrect) {
        rect = {measured.left, measured.top, measured.right, measured.bottom};
        return measured_height;
    }
    if (!image || text.empty()) return measured_height;
    TextRect area{0, 0, image->width, image->height};
    if (!(flags & dt::noclip)) {
        area.left = std::max(area.left, rect.left);
        area.top = std::max(area.top, rect.top);
        area.right = std::min(area.right, rect.right);
        area.bottom = std::min(area.bottom, rect.bottom);
    }
    if (area.right <= area.left || area.bottom <= area.top) return measured_height;
    auto paint = [&] {
        RECT box{rect.left - area.left, rect.top - area.top, rect.right - area.left, rect.bottom - area.top};
        std::copy(text.begin(), text.end(), buffer.begin());
        std::fill(buffer.begin() + text.size(), buffer.end(), 0);
        return DrawTextExA(n.dc, buffer.data(), int(text.size()), &box, flags, nullptr);
    };
    if (surface_ == TextSurface::rgb565) return n.paint565(*image, area, rgb, paint);
    const int w = area.right - area.left, h = area.bottom - area.top;
    n.mono_surface(w, h);
    memset(n.mono_bits, 0, size_t(n.mono_stride) * n.mono_height);
    SetTextColor(n.dc, RGB(255, 255, 255));
    const int result = paint();
    GdiFlush();
    const auto* bits = static_cast<const uint8_t*>(n.mono_bits);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (bits[size_t(y) * n.mono_stride + x / 8] & (0x80u >> (x & 7)))
                image->at(area.left + x, area.top + y) = rgb;
    return result;
#else
    // Split into lines: hard breaks on '\n' ("\r" ignored); word wrap at spaces when requested.
    std::vector<std::string> lines;
    const int max_width = rect.right - rect.left;
    std::string paragraph;
    // Wine user32 TEXT_NextLineW/TEXT_WordBreak: fit whole characters, then
    // break at the word boundary. When the first character that does not fit
    // starts a word, a left-aligned line keeps the space before it (it counts
    // toward CALCRECT width); otherwise the break works back to the previous
    // space and drops it. Centre/right lines never keep the space. Breaks are
    // at spaces only (ScriptBreak soft breaks in the game's strings).
    // textprobe CALCRECT: 300/296/293/300/269/156 reproduced.
    auto flush = [&](const std::string& p) {
        if (!(flags & dt::wordbreak) || (flags & dt::singleline)) {
            lines.push_back(p);
            return;
        }
        std::string rest = p;
        while (true) {
            size_t fit = 0;
            for (int w = 0; fit < rest.size(); ++fit) {
                w += glyph(face, height, uint8_t(rest[fit])).advance;
                if (w > max_width) break;
            }
            if (fit >= rest.size()) {
                lines.push_back(rest);
                return;
            }
            const bool aligned = (flags & (dt::center | dt::right)) != 0;
            size_t len, used;
            bool word_fits = fit > 0;
            long q = long(fit); // Wine's p
            if (word_fits) {
                if (rest[fit - 1] == ' ' && rest[fit] != ' ')
                    --q; // soft break: the word just fitted
                else {
                    long i = long(fit);
                    while (i > 0 && rest[size_t(--i + 1)] != ' ') --q;
                    --q;
                    word_fits = i != 0 || rest[size_t(i + 1)] == ' ';
                }
            }
            if (word_fits) {
                if (!aligned || rest[size_t(q)] != ' ') ++q;
                len = size_t(q);
                used = len + (len < rest.size() && rest[len] == ' ' ? 1 : 0);
            } else {
                // No word fits: take the whole word, discard one trailing space.
                len = rest.find(' ', fit);
                if (len == std::string::npos) len = rest.size();
                used = len + (len < rest.size() ? 1 : 0);
            }
            lines.push_back(rest.substr(0, len));
            if (used >= rest.size()) return;
            rest = rest.substr(used);
        }
    };
    for (char c : text) {
        if (c == '\r') continue;
        if (c == '\n' && !(flags & dt::singleline)) {
            flush(paragraph);
            paragraph.clear();
        } else
            paragraph.push_back(c == '\n' ? ' ' : c);
    }
    flush(paragraph);
    const int pitch = line_height(face, height);
    const int total = int(lines.size()) * pitch;
    if (flags & dt::calcrect) {
        int w = 0;
        for (const auto& l : lines) w = std::max(w, width(face, height, l));
        // GDI returns the measured width even under maxw (textprobe
        // CALCRECT rights 296/293/269/156 vs input 300); the old
        // wordbreak carve-out pinned boxes to maxw and shifted
        // right-aligned text.
        if (max_width > 0) rect.right = rect.left + w;
        rect.bottom = rect.top + total;
        return total;
    }
    int y = rect.top;
    if (flags & dt::singleline) {
        if (flags & dt::vcenter)
            y = rect.top + (rect.bottom - rect.top - height) / 2;
        else if (flags & dt::bottom)
            y = rect.bottom - height;
    }
    for (const auto& l : lines) {
        const int w = width(face, height, l);
        int x = rect.left;
        if (flags & dt::center)
            x = rect.left + (max_width - w) / 2;
        else if (flags & dt::right)
            x = rect.right - w;
        if (image) draw_clipped(*image, (flags & dt::noclip) ? nullptr : &rect, face, height, x, y, rgb, l);
        y += pitch;
    }
    return total;
#endif
}
} // namespace rr
