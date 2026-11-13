/* GDI glyph-bitmap ground truth (plan 13 Task 6 Step 2, exact menu glyphs).
 * Builds with i686-w64-mingw32-gcc; runs on Windows or Wine (memory DC only).
 * Installs the disc fonts and creates the game's menu LOGFONTs (0x004239e5:
 * positive cell heights, weight 0, OUT_TT_ONLY_PRECIS, DEFAULT_QUALITY), then
 * draws every byte 0x20..0xff with TextOutA (TA_TOP|TA_LEFT, transparent) into
 * a 1-bit DIB and prints the ink box relative to the pen origin:
 *   G <file> <height> <byte> <advance> <left> <top> <w> <h> <rows as hex>
 * Each row is ceil(w/8) bytes, MSB first. The game draws glyph-at-a-time on a
 * transparent background, so per-byte ink composes the drawn string.
 * With "gray" as the second argument it draws white on black into a 32-bit DIB,
 * and prints one byte (blue channel) per pixel instead of bits:
 *   A <file> <height> <byte> <advance> <left> <top> <w> <h> <rows as hex>
 * Windows ClearType on a 32-bit DIB differs from the game's RGB565 target.
 * "gray565" draws directly on RGB565 and records all three expanded channels:
 *   C <file> <height> <byte> <advance> <left> <top> <w> <h> <RRGGBB rows>
 * With "sys" as the third argument it creates the game's system-font LOGFONTs
 * instead (0x00423b25 menu Wingdings 18, 0x00423afd standings New Courier Bold
 * 18, 0x00442990 HUD Wingdings 12/18), prints the face Wine substitutes as
 *   F <requested> <height> <selected face> <tmHeight> <tmAscent> <tmDescent>
 * and labels glyph lines with WINGDINGS or NEWCOURIERBOLD instead of a file. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr,
                "usage: glyphprobe.exe <TEXT-dir-with-trailing-backslash> [mono|gray|gray565] [sys]\n");
        return 1;
    }
    struct {
        const char* file;
        const char* face;
    } faces[] = {{"BADLOC.DLL", "Badloc"},
                 {"FUTR.DLL", "Futura Condensed Regular"},
                 {"FUTB.DLL", "Futura Condensed Bold"},
                 {"FUTD.DLL", "Futura Condensed Demi Regular"}};
    const int heights[] = {14, 15, 16, 18, 20, 24, 28, 32, 36, 42, 72};
    char path[MAX_PATH];
    for (unsigned i = 0; i < 4; ++i) {
        snprintf(path, sizeof path, "%s%s", argv[1], faces[i].file);
        if (!AddFontResourceA(path)) {
            printf("ADDFONT %s FAILED\n", path);
            return 1;
        }
    }
    HDC screen = GetDC(NULL);
    HDC dc = CreateCompatibleDC(screen);
    ReleaseDC(NULL, screen);
    const int gray565 = argc > 2 && !strcmp(argv[2], "gray565");
    const int gray = gray565 || (argc > 2 && !strcmp(argv[2], "gray"));
    const int sys = argc > 3 && !strcmp(argv[3], "sys");
    struct {
        const char *face, *tag;
        int height, weight, charset, pitch;
    } sysfonts[] = {{"Wingdings", "WINGDINGS", 18, 400, SYMBOL_CHARSET, 4},
                    {"Wingdings", "WINGDINGS", 12, 400, SYMBOL_CHARSET, 4},
                    {"New Courier Bold", "NEWCOURIERBOLD", 18, 700, 0, 5}};
    const int pad = 96, cw = 320, ch = 288;
    const int stride = gray565 ? cw * 2 : gray ? cw * 4 : ((cw + 31) / 32) * 4;
    BITMAPINFO* bi = calloc(1, sizeof(BITMAPINFOHEADER) + 3 * sizeof(RGBQUAD));
    bi->bmiHeader.biSize = sizeof bi->bmiHeader;
    bi->bmiHeader.biWidth = cw;
    bi->bmiHeader.biHeight = -ch;
    bi->bmiHeader.biPlanes = 1;
    bi->bmiHeader.biBitCount = gray565 ? 16 : gray ? 32 : 1;
    bi->bmiColors[1].rgbRed = bi->bmiColors[1].rgbGreen = bi->bmiColors[1].rgbBlue = 255;
    if (gray565) {
        bi->bmiHeader.biCompression = BI_BITFIELDS;
        DWORD* masks = (DWORD*)bi->bmiColors;
        masks[0] = 0xf800;
        masks[1] = 0x07e0;
        masks[2] = 0x001f;
    }
    void* bits = NULL;
    HBITMAP bmp = CreateDIBSection(dc, bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!bmp || !bits) return 1;
    SelectObject(dc, bmp);
    SetTextColor(dc, RGB(255, 255, 255));
    SetBkMode(dc, TRANSPARENT);
    SetTextAlign(dc, TA_TOP | TA_LEFT | TA_NOUPDATECP);
    const unsigned char* b = bits;
    const unsigned nf = sys ? sizeof sysfonts / sizeof sysfonts[0] : 4;
    const unsigned nh = sys ? 1 : sizeof heights / sizeof heights[0];
    for (unsigned f = 0; f < nf; ++f)
        for (unsigned hi = 0; hi < nh; ++hi) {
            const int height = sys ? sysfonts[f].height : heights[hi];
            const char* label = sys ? sysfonts[f].tag : faces[f].file;
            HFONT font =
                sys ? CreateFontA(height, 0, 0, 0, sysfonts[f].weight, 0, 0, 0, sysfonts[f].charset,
                                  OUT_TT_PRECIS, 0, DEFAULT_QUALITY, sysfonts[f].pitch, sysfonts[f].face)
                    : CreateFontA(height, 0, 0, 0, 0, 0, 0, 0, 0, OUT_TT_ONLY_PRECIS, 0, DEFAULT_QUALITY, 0,
                                  faces[f].face);
            HGDIOBJ old = SelectObject(dc, font);
            if (sys) {
                char name[LF_FACESIZE];
                TEXTMETRICA tm;
                GetTextFaceA(dc, sizeof name, name);
                GetTextMetricsA(dc, &tm);
                printf("F %s %d %s %ld %ld %ld\n", label, height, name, tm.tmHeight, tm.tmAscent,
                       tm.tmDescent);
            }
            for (int c = 0x20; c <= 0xff; ++c) {
                memset(bits, 0, (size_t)stride * ch);
                char s = (char)c;
                TextOutA(dc, pad, pad, &s, 1);
                GdiFlush();
                SIZE sz;
                GetTextExtentPoint32A(dc, &s, 1, &sz);
                int x0 = cw, y0 = ch, x1 = -1, y1 = -1;
                for (int y = 0; y < ch; ++y)
                    for (int x = 0; x < cw; ++x)
                        if (gray565 ? ((const unsigned short*)bits)[y * cw + x] != 0
                            : gray  ? b[y * stride + x * 4] != 0
                                    : (b[y * stride + x / 8] & (0x80 >> (x & 7)))) {
                            if (x < x0) x0 = x;
                            if (x > x1) x1 = x;
                            if (y < y0) y0 = y;
                            if (y > y1) y1 = y;
                        }
                const char tag = gray565 ? 'C' : gray ? 'A' : 'G';
                if (x1 < 0) {
                    printf("%c %s %d %d %ld 0 0 0 0\n", tag, label, height, c, sz.cx);
                    continue;
                }
                const int w = x1 - x0 + 1, h = y1 - y0 + 1;
                printf("%c %s %d %d %ld %d %d %d %d", tag, label, height, c, sz.cx, x0 - pad, y0 - pad, w, h);
                for (int y = y0; y <= y1; ++y) {
                    putchar(' ');
                    if (gray565) {
                        for (int x = x0; x <= x1; ++x) {
                            const unsigned p = ((const unsigned short*)bits)[y * cw + x];
                            const unsigned r = p >> 11, g = (p >> 5) & 63, blue = p & 31;
                            printf("%02x%02x%02x", (r << 3) | (r >> 2), (g << 2) | (g >> 4),
                                   (blue << 3) | (blue >> 2));
                        }
                        continue;
                    }
                    if (gray) {
                        for (int x = x0; x <= x1; ++x) printf("%02x", b[y * stride + x * 4]);
                        continue;
                    }
                    for (int bx = 0; bx < (w + 7) / 8; ++bx) {
                        unsigned v = 0;
                        for (int k = 0; k < 8; ++k) {
                            const int x = x0 + bx * 8 + k;
                            if (x <= x1 && (b[y * stride + x / 8] & (0x80 >> (x & 7)))) v |= 0x80u >> k;
                        }
                        printf("%02x", v);
                    }
                }
                putchar('\n');
            }
            SelectObject(dc, old);
            DeleteObject(font);
        }
    for (unsigned i = 0; i < 4; ++i) {
        snprintf(path, sizeof path, "%s%s", argv[1], faces[i].file);
        RemoveFontResourceA(path);
    }
    printf("DONE\n");
    return 0;
}
