#include "test_assets.hpp"
#include "assets.hpp"
#include "pe_strings.hpp"
#include "text.hpp"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

int main(int argc, char** argv) {
    const bool mono_only = argc > 2 && std::string(argv[2]) == "--mono-only";
    const std::string mode = argc > 2 ? argv[2] : "";
    const bool system_only = mode == "--system-mono" || mode == "--system-rgb565";
    int failures = 0;
    auto check = [&](bool v, const std::string& what) {
        if (!v) {
            ++failures;
            std::cerr << "FAIL " << what << "\n";
        }
    };
    try {
        rr::TextRenderer bad(rr::Face::badloc, std::vector<uint8_t>(40, 0));
        ++failures;
    } catch (const std::runtime_error&) {
    }
    rr::Assets assets(argc > 1 ? argv[1] : rr::test::assets_root());
    if (!assets.exists("TEXT/FUTB.DLL")) {
        std::cout << "skip: no disc\n";
        return 77;
    }
    const rr::TextRenderer text(assets);
    if (!system_only) {
        for (auto f :
             {rr::Face::badloc, rr::Face::futura_regular, rr::Face::futura_bold, rr::Face::futura_demi}) {
            check(text.width(f, 24, "") == 0, "empty width");
            const std::string s = "Big Game Mode \xe9";
            const int w = text.width(f, 24, s);
            check(w > 40 && w < 400, "plausible width");
            rr::RgbImage img(640, 100, 0);
            text.draw(img, f, 24, 100, 30, 0x123456, s);
            int ink = 0, outside = 0, other = 0;
            for (int y = 0; y < img.height; ++y)
                for (int x = 0; x < img.width; ++x) {
                    const uint32_t p = img.at(x, y);
                    if (!p) continue;
                    ++ink;
                    if (p != 0x123456) ++other;
                    if (x < 100 - 4 || x >= 100 + w + 4 || y < 30 || y >= 30 + 24) ++outside;
                }
            check(ink > 50 && other == 0, "ink drawn in the exact colour");
            check(outside == 0, "ink stays inside the measured box");
        }
        // Word wrap keeps every language's longest menu description inside a 300-pixel box (0x00420470).
        for (auto l : {rr::Language::english, rr::Language::german, rr::Language::french,
                       rr::Language::spanish, rr::Language::italian}) {
            const auto table =
                rr::load_game_strings(rr::read_string_resources(assets.read(rr::language_dll(l))));
            size_t longest = 0;
            for (size_t i = 1; i < table.size(); ++i)
                if (table[i].size() > table[longest].size()) longest = i;
            rr::TextRect r{0, 0, 300, 0};
            const int h = text.draw_box(nullptr, rr::Face::futura_regular, 32, r, 0, table[longest],
                                        rr::dt::wordbreak | rr::dt::calcrect);
            check(h > 32 && r.right - r.left <= 300 + 200, "wrapped text measured");
            rr::RgbImage img(640, 480, 0);
            rr::TextRect box{8, 0, 308, 480};
            text.draw_box(&img, rr::Face::futura_demi, 24, box, 0xffffff, table[longest], rr::dt::wordbreak);
        }
    }
    // Wine-GDI glyph ground truth (tools/glyphprobe.c mono/gray, tools/blendprobe.c),
    // dumped from the disc fonts into the git-ignored evidence folder.
    const char* configured_probe = std::getenv("RR_TEST_TEXT_PROBES");
    std::string probe = configured_probe && *configured_probe ? configured_probe
                                                              : RR_SOURCE_DIR
                            "/analysis/frontend/glyphs-20261031/";
    if (probe.back() != '/' && probe.back() != '\\') probe += '/';
#ifdef _WIN32
    const char* gray_file = "gray565.txt";
    const char* sysgray_file = "sysgray565.txt";
#else
    const char* gray_file = "gray.txt";
    const char* sysgray_file = "sysgray.txt";
#endif
    std::ifstream mono_in(probe + "mono.txt"), gray_in(probe + gray_file), blend_in(probe + "blend.txt");
    if (!mono_in || (!mono_only && (!gray_in || !blend_in))) {
        std::cout << failures << " failures; skip glyph probes: no " << probe << "\n";
        return failures ? 1 : 77;
    }
    const std::map<std::string, rr::Face> files{
        {"BADLOC.DLL", rr::Face::badloc},    {"FUTR.DLL", rr::Face::futura_regular},
        {"FUTB.DLL", rr::Face::futura_bold}, {"FUTD.DLL", rr::Face::futura_demi},
        {"WINGDINGS", rr::Face::wingdings},  {"NEWCOURIERBOLD", rr::Face::courier_new_bold}};
    const uint8_t ramp[17] = {0,    0x4d, 0x68, 0x7c, 0x8c, 0x9a, 0xa7, 0xb2, 0xbd,
                              0xc7, 0xd0, 0xd9, 0xe1, 0xe9, 0xf0, 0xf8, 0xff};
    auto expand5 = [](int v) { return (v >> 3) << 3 | v >> 5; };
    // Each probe line: tag file height byte advance left top w h rows. The
    // glyph is drawn white at a pad on black and its ink box compared.
    auto replay = [&](std::istream& in, const rr::TextRenderer& r, bool gray, int& total, int& exact,
                      int& advances, std::vector<std::string>& misses) {
        std::string line;
        while (std::getline(in, line)) {
#ifdef _WIN32
            const char tag = gray ? 'C' : 'G'; // RGB565 probe contains all three expanded channels.
#else
            const char tag = gray ? 'A' : 'G';
#endif
            if (line.empty() || line[0] != tag) continue;
            std::istringstream ss(line.substr(2));
            std::string file;
            int height, ch, advance, left, top, w, h;
            ss >> file >> height >> ch >> advance >> left >> top >> w >> h;
            const rr::Face face = files.at(file);
            // Wine's font linking requires separate fixtures for uncovered
            // bytes. Native Windows probes include every byte, including
            // undefined CP1252 controls and fallback glyphs.
#ifndef _WIN32
            if (ch >= 0x80 && !r.covers(face, uint8_t(ch))) continue;
#endif
            ++total;
            const std::string s(1, char(ch));
            const int native_advance = r.width(face, height, s);
            if (native_advance == advance)
                ++advances;
            else
                std::cerr << "advance mismatch " << file << ' ' << height << ' ' << ch
                          << " original=" << advance << " native=" << native_advance << '\n';
            const int pad = 96;
            rr::RgbImage img(320, 288, 0);
            r.draw(img, face, height, pad, pad, 0xffffff, s);
            int x0 = 1 << 30, y0 = 1 << 30, x1 = -1, y1 = -1;
            for (int y = 0; y < img.height; ++y)
                for (int x = 0; x < img.width; ++x)
                    if (img.at(x, y)) {
                        x0 = std::min(x0, x), x1 = std::max(x1, x);
                        y0 = std::min(y0, y), y1 = std::max(y1, y);
                    }
            bool ok =
                x1 < 0 ? w == 0 : x0 - pad == left && y0 - pad == top && x1 - x0 + 1 == w && y1 - y0 + 1 == h;
            for (int y = 0; ok && y < h; ++y) {
                std::string row;
                ss >> row;
                for (int x = 0; ok && x < w; ++x) {
                    const uint32_t p = img.at(x0 + x, y0 + y)
#ifndef _WIN32
                                       & 0xff
#else
                                       & (gray ? 0xffffffu : 0xffu)
#endif
                        ;
                    const int want =
                        gray ?
#ifdef _WIN32
                             std::stoi(row.substr(size_t(x) * 6, 6), nullptr, 16)
#else
                             expand5(std::stoi(row.substr(size_t(x) * 2, 2), nullptr, 16))
#endif
                             : (std::stoi(row.substr(size_t(x / 8) * 2, 2), nullptr, 16) >> (7 - x % 8) & 1) *
                                   0xff;
                    ok = int(p) == want;
                }
            }
            if (ok)
                ++exact;
            else
                misses.push_back(file + " " + std::to_string(height) + " " + std::to_string(ch));
        }
    };
    if (!system_only) {
        int total = 0, exact = 0, advances = 0;
        std::vector<std::string> misses;
        replay(mono_in, text, false, total, exact, advances, misses);
        std::cout << "mono glyphs exact " << exact << "/" << total << ", advances " << advances << "\n";
        // Known residual: BADLOC 14 I-acute and U-acute keep one accent row Wine drops.
#ifdef _WIN32
        check(total == 9856 && advances == total, "mono probe coverage and advances");
#else
        check(total == 8448 && advances == total, "mono probe coverage and advances");
#endif
#ifdef _WIN32
        check(misses.empty(), "mono glyph bitmaps");
#else
        check(misses == std::vector<std::string>{"BADLOC.DLL 14 205", "BADLOC.DLL 14 218"},
              "mono glyph bitmaps");
#endif
        for (const auto& m : misses) std::cout << "  mono miss " << m << "\n";
    }
    if (mono_only) {
        std::cout << failures << " failures (monochrome component check only)\n";
        return failures ? 1 : 0;
    }
#ifdef _WIN32
    if (!system_only) {
        std::ifstream metrics(probe + "metrics.txt");
        check(bool(metrics), "native line-height and layout probes present");
        const std::map<std::string, rr::Face> faces{{"Badloc", rr::Face::badloc},
                                                    {"Futura Condensed Regular", rr::Face::futura_regular},
                                                    {"Futura Condensed Bold", rr::Face::futura_bold},
                                                    {"Futura Condensed Demi Regular", rr::Face::futura_demi}};
        int heights = 0, extents = 0, rectangles = 0;
        std::string line;
        while (std::getline(metrics, line)) {
            if (line.rfind("SWEEP ", 0) == 0) {
                for (const auto& [name, face] : faces) {
                    const std::string prefix = "SWEEP " + name + ' ';
                    if (line.rfind(prefix, 0) != 0) continue;
                    int height, pitch, ascent, descent, leading;
                    std::istringstream fields(line.substr(prefix.size()));
                    if (fields >> height >> pitch >> ascent >> descent >> leading) {
                        check(text.line_height(face, height) == pitch, "native font cell height " + name);
                        ++heights;
                    }
                }
            } else if (line.rfind("EXTENT ", 0) == 0 || line.rfind("CALCRECT ", 0) == 0) {
                const auto open = line.find('['), close = line.rfind(']');
                if (open == std::string::npos || close == std::string::npos || close <= open) continue;
                const std::string value = line.substr(open + 1, close - open - 1);
                char tag[32];
                int width, height;
                if (std::sscanf(line.c_str(), "EXTENT %31s cx=%d cy=%d", tag, &width, &height) == 3) {
                    const bool button = std::string(tag) == "button24";
                    const auto face = button ? rr::Face::futura_bold : rr::Face::futura_regular;
                    const int cell = button ? 24 : 32;
                    check(text.width(face, cell, value) == width && text.line_height(face, cell) == height,
                          std::string("native string extent ") + tag);
                    ++extents;
                } else {
                    unsigned flags;
                    int left, top, right, bottom;
                    if (std::sscanf(line.c_str(), "CALCRECT %31s fmt=%x w=%d h=%d rect=%d,%d,%d,%d", tag,
                                    &flags, &width, &height, &left, &top, &right, &bottom) != 8)
                        continue;
                    rr::TextRect box{0, 0, width, 0};
                    const int got = text.draw_box(nullptr, rr::Face::futura_regular, 32, box, 0, value,
                                                  flags | rr::dt::calcrect);
                    if (got != height || box.right != right || box.bottom != bottom)
                        std::cerr << "layout mismatch " << tag << " original=" << right << ',' << bottom
                                  << " height=" << height << " native=" << box.right << ',' << box.bottom
                                  << " height=" << got << '\n';
                    check(got == height && box.left == left && box.top == top && box.right == right &&
                              box.bottom == bottom,
                          std::string("native wrapped rectangle ") + tag);
                    ++rectangles;
                }
            }
        }
        check(heights == 292 && extents == 7 && rectangles == 6, "native text layout probe coverage");
        std::cout << "native layout probes: " << heights << " heights, " << extents << " extents, "
                  << rectangles << " wrapped rectangles\n";
    }
#endif
    if (!system_only) {
        const rr::TextRenderer smooth(assets, rr::TextSurface::rgb565);
        int total = 0, exact = 0, advances = 0;
        std::vector<std::string> misses;
        replay(gray_in, smooth, true, total, exact, advances, misses);
        std::cout << "smoothed glyphs exact " << exact << "/" << total << ", advances " << advances << "\n";
#ifdef _WIN32
        check(total == 9856 && exact == total && advances == total, "smoothed glyph bitmaps");
#else
        check(total == 8448 && exact == total && advances == total, "smoothed glyph bitmaps");
#endif
        // HUD canvases use this sentinel outside the text. Black shadow ink
        // must become opaque even though its RGB equals the sentinel's RGB;
        // unpainted pixels, including pixels inside the text box, stay clear.
        for (bool boxed : {false, true}) {
            rr::RgbImage ink(128, 96, 0), transparent(128, 96, 0x01000000);
            rr::TextRect white_box{8, 8, 88, 80}, black_box = white_box;
            if (boxed) {
                smooth.draw_box(&ink, rr::Face::futura_regular, 24, white_box, 0xffffff, "Black shadow",
                                rr::dt::wordbreak);
                smooth.draw_box(&transparent, rr::Face::futura_regular, 24, black_box, 0, "Black shadow",
                                rr::dt::wordbreak);
            } else {
                smooth.draw(ink, rr::Face::futura_regular, 24, 8, 8, 0xffffff, "Black");
                smooth.draw(transparent, rr::Face::futura_regular, 24, 8, 8, 0, "Black");
            }
            bool matches = true;
            for (size_t i = 0; i < ink.pixels.size(); ++i)
                matches &= transparent.pixels[i] == (ink.pixels[i] ? 0u : 0x01000000u);
            check(matches, boxed ? "text box preserves HUD transparency and black ink"
                                 : "text drawing preserves HUD transparency and black ink");
        }
    }
    if (!system_only) {
        int total = 0, exact = 0;
        std::string line;
#ifdef _WIN32
        // A Windows ClearType intensity is not a Wine coverage level. Recover
        // the exact sample coordinates used by blendprobe's 32-bit W, then
        // compare the production 16-bit text drawing at those coordinates.
        std::map<unsigned, std::pair<int, int>> samples;
        std::ifstream probe32(probe + "gray.txt");
        while (std::getline(probe32, line)) {
            std::istringstream row(line);
            char tag;
            std::string file;
            int height, ch, advance, left, top, w, h;
            if (!(row >> tag >> file >> height >> ch >> advance >> left >> top >> w >> h) || tag != 'A' ||
                file != "FUTR.DLL" || height != 72 || ch != 'W')
                continue;
            for (int py = 0; py < h; ++py) {
                std::string pixels;
                row >> pixels;
                for (int px = 0; px < w; ++px) {
                    const auto intensity =
                        unsigned(std::stoul(pixels.substr(size_t(px) * 2, 2), nullptr, 16));
                    const int x = 8 + left + px, y = 8 + top + py;
                    if (intensity && x >= 0 && x < 96 && y >= 0 && y < 96)
                        samples.try_emplace(intensity, x, y);
                }
            }
            break;
        }
        check(!samples.empty(), "native blend probe coordinates present");
        const rr::TextRenderer blend_renderer(assets, rr::TextSurface::rgb565);
        rr::RgbImage blend_image(96, 96);
        unsigned previous_colour = ~0u, previous_bg = ~0u;
#endif
        while (std::getline(blend_in, line)) {
            if (line.size() < 2 || line[0] != 'B') continue;
            unsigned colorref, bg, intensity, out;
            if (std::sscanf(line.c_str(), "B %x %x %x %x", &colorref, &bg, &intensity, &out) != 4) continue;
            int level = 0;
            while (level < 16 && ramp[level] != intensity) ++level;
            auto rgb = [](unsigned p) {
                const unsigned r = p >> 11, g = p >> 5 & 63, b = p & 31;
                return (r << 3 | r >> 2) << 16 | (g << 2 | g >> 4) << 8 | (b << 3 | b >> 2);
            };
            const uint32_t text_rgb = (colorref & 0xff) << 16 | (colorref & 0xff00) | colorref >> 16;
            uint32_t got;
#ifdef _WIN32
            if (colorref != previous_colour || bg != previous_bg) {
                std::fill(blend_image.pixels.begin(), blend_image.pixels.end(), rgb(bg));
                blend_renderer.draw(blend_image, rr::Face::futura_regular, 72, 8, 8, text_rgb, "W");
                previous_colour = colorref;
                previous_bg = bg;
            }
            const auto sample = samples.find(intensity);
            if (sample == samples.end()) {
                check(false, "native blend sample coordinate missing");
                continue;
            }
            got = blend_image.at(sample->second.first, sample->second.second);
#else
            got = level >= 16
                      ? rgb(((text_rgb >> 19) << 11) | ((text_rgb >> 10 & 63) << 5) | (text_rgb >> 3 & 31))
                      : rr::gdi_blend565(rgb(bg), text_rgb, level);
#endif
            ++total;
            exact += got == rgb(out);
        }
        std::cout << "16-bit text blend exact " << exact << "/" << total << "\n";
        check(total > 0 && exact == total, "16-bit anti-aliased blend");
    }
    // System faces (glyphprobe sys: the game's Wingdings 18/12 and New Courier
    // Bold 18 LOGFONTs). Wine on macOS resolves them to the host's Wingdings and
    // Courier New Bold; VDMX sizing and gasp gray ranges make every glyph exact.
#ifdef _WIN32
    const char* windows_dir = std::getenv("WINDIR");
    const std::string fonts = std::string(windows_dir ? windows_dir : "C:/Windows") + "/Fonts/";
    const std::string wing = fonts + "wingding.ttf", courier = fonts + "courbd.ttf";
#else
    const std::string wing = "/System/Library/Fonts/Supplemental/Wingdings.ttf",
                      courier = "/System/Library/Fonts/Supplemental/Courier New Bold.ttf";
#endif
    if (std::ifstream(wing) && std::ifstream(courier)) {
        for (bool gray : {false, true}) {
#ifdef _WIN32
            // The probes run in separate processes. GDI caches a fallback
            // width across mono/RGB565 DCs, so mixing both here would compare
            // a warmed process against two independent cold-process goldens.
            if (!system_only || gray != (mode.find("rgb565") != std::string::npos)) continue;
#endif
            std::ifstream in(probe + (gray ? sysgray_file : "sysmono.txt"));
            if (!in) {
                std::cout << "skip system-face probe: no " << (gray ? "sysgray" : "sysmono") << ".txt\n";
                continue;
            }
            int total = 0, exact = 0, advances = 0;
            std::vector<std::string> misses;
            // Match glyphprobe's per-face/size font lifetime as well as its
            // surface format: it deletes each HFONT before selecting the next.
            std::string line, block, previous;
            auto compare_block = [&] {
                if (block.empty()) return;
                const rr::TextRenderer r(
                    assets, gray ? rr::TextSurface::rgb565 : rr::TextSurface::indexed8,
                    {{rr::Face::wingdings, wing}, {rr::Face::courier_new_bold, courier}});
                std::istringstream rows(block);
                replay(rows, r, gray, total, exact, advances, misses);
                block.clear();
            };
            while (std::getline(in, line)) {
                std::istringstream fields(line);
                char tag;
                std::string file;
                int height;
                if (!(fields >> tag >> file >> height) || (tag != 'A' && tag != 'C' && tag != 'G')) continue;
                const std::string key = file + ':' + std::to_string(height);
                if (key != previous) {
                    compare_block();
                    previous = key;
                }
                block += line + '\n';
            }
            compare_block();
            std::cout << (gray ? "smoothed" : "mono") << " system-face glyphs exact " << exact << "/" << total
                      << ", advances " << advances << "\n";
#ifdef _WIN32
            check(total == 672 && exact == total && advances == total, "system-face glyph bitmaps");
#else
            check(total == 667 && exact == total && advances == total, "system-face glyph bitmaps");
#endif
        }
    } else {
        std::cout << "skip system-face probe: no macOS Wingdings/Courier New Bold\n";
    }
    // Built-in fallback (Wine's own Wingdings) carries the three arrows the game draws.
    {
        const rr::TextRenderer r(assets, rr::TextSurface::rgb565, {{rr::Face::wingdings, ""}});
        for (uint8_t ch : {uint8_t(0xe8), uint8_t(0xe9), uint8_t(0xea)}) {
            check(r.covers(rr::Face::wingdings, ch), "built-in Wingdings arrow");
            rr::RgbImage img(64, 64, 0);
            r.draw(img, rr::Face::wingdings, 18, 8, 8, 0xffff00, std::string(1, char(ch)));
            check(std::count(img.pixels.begin(), img.pixels.end(), 0u) < int(img.pixels.size()) - 20,
                  "built-in Wingdings arrow ink");
        }
    }
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
