// make_rounded_ico.cpp — Load LOGO.png (or use embedded data), output rounded LOGO.ico
// Uses only Windows API + C runtime (no Qt dependency)

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <cmath>

// Load a PNG from file into RGBA buffer
static bool load_png(const char* path, std::vector<uint8_t>& rgba, int& w, int& h) {
    // We'll just use GDI+ to load the PNG
    // But since this is complex, let's take a shortcut:
    // Instead of loading PNG, we read the original LOGO.ico,
    // extract its 256x256 PNG data, round the corners, and write back.
    return false;
}

static uint32_t crc32_table[256];
static void init_crc32() {
    for (int i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c >> 1) ^ (c & 1 ? 0xEDB88320 : 0);
        crc32_table[i] = c;
    }
}
static uint32_t crc32_func(const uint8_t* buf, int len) {
    uint32_t c = 0xFFFFFFFF;
    for (int i = 0; i < len; i++)
        c = crc32_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFF;
}

static void put32(FILE* f, uint32_t v) {
    fputc((v >> 24) & 0xFF, f);
    fputc((v >> 16) & 0xFF, f);
    fputc((v >> 8) & 0xFF, f);
    fputc(v & 0xFF, f);
}

static void write_png(FILE* f, const uint8_t* rgba, int w, int h) {
    static const uint8_t sig[] = {137, 80, 78, 71, 13, 10, 26, 10};
    fwrite(sig, 1, 8, f);

    // IHDR
    uint8_t ihdr[13] = {};
    ihdr[0] = (w >> 24) & 0xFF; ihdr[1] = (w >> 16) & 0xFF;
    ihdr[2] = (w >> 8) & 0xFF; ihdr[3] = w & 0xFF;
    ihdr[4] = (h >> 24) & 0xFF; ihdr[5] = (h >> 16) & 0xFF;
    ihdr[6] = (h >> 8) & 0xFF; ihdr[7] = h & 0xFF;
    ihdr[8] = 8; ihdr[9] = 6; // RGBA
    put32(f, 13);
    fwrite("IHDR", 1, 4, f);
    fwrite(ihdr, 1, 13, f);
    uint8_t ihdr_chunk[17];
    memcpy(ihdr_chunk, "IHDR", 4);
    memcpy(ihdr_chunk+4, ihdr, 13);
    put32(f, crc32_func(ihdr_chunk, 17));

    // IDAT - raw deflate stored blocks
    std::vector<uint8_t> scanlines;
    scanlines.reserve((w*4+1) * h);
    for (int y = 0; y < h; y++) {
        scanlines.push_back(0); // filter none
        for (int x = 0; x < w*4; x++)
            scanlines.push_back(rgba[y*w*4 + x]);
    }

    std::vector<uint8_t> deflated;
    size_t pos = 0;
    while (pos < scanlines.size()) {
        size_t chunk = std::min(scanlines.size() - pos, (size_t)65535);
        uint8_t bfinal = (pos + chunk >= scanlines.size()) ? 1 : 0;
        deflated.push_back(bfinal);
        deflated.push_back(chunk & 0xFF);
        deflated.push_back((chunk >> 8) & 0xFF);
        deflated.push_back((~chunk) & 0xFF);
        deflated.push_back(((~chunk) >> 8) & 0xFF);
        for (size_t i = 0; i < chunk; i++)
            deflated.push_back(scanlines[pos + i]);
        pos += chunk;
    }

    put32(f, (uint32_t)deflated.size());
    fwrite("IDAT", 1, 4, f);
    fwrite(deflated.data(), 1, deflated.size(), f);

    std::vector<uint8_t> idat_full;
    idat_full.insert(idat_full.end(), (uint8_t*)"IDAT", (uint8_t*)"IDAT"+4);
    idat_full.insert(idat_full.end(), deflated.begin(), deflated.end());
    put32(f, crc32_func(idat_full.data(), (int)idat_full.size()));

    // IEND
    put32(f, 0);
    fwrite("IEND", 1, 4, f);
    put32(f, crc32_func((uint8_t*)"IEND", 4));
}

static void make_rounded_image(const uint8_t* src, uint8_t* dst, int s, double radius_ratio) {
    double r = s * radius_ratio;
    memset(dst, 0, s*s*4);

    for (int y = 0; y < s; y++) {
        for (int x = 0; x < s; x++) {
            // Distance to nearest corner
            double dx = 0, dy = 0;
            if (x < r) dx = r - x;
            else if (x > s - r) dx = x - (s - r);
            if (y < r) dy = r - y;
            else if (y > s - r) dy = y - (s - r);

            bool inside = false;
            if (dx <= 0 && dy <= 0) {
                // Center area or edge
                inside = true;
            } else {
                double d = sqrt(dx*dx + dy*dy);
                inside = (d <= r);
                // Anti-alias at the edge
                if (d > r && d < r + 1.5) {
                    double alpha = (r + 1.5 - d) / 1.5;
                    // Copy src with alpha multiplied
                    int si = (y * s + x) * 4;
                    int di = (y * s + x) * 4;
                    dst[di+0] = (uint8_t)(src[si+0] * alpha);
                    dst[di+1] = (uint8_t)(src[si+1] * alpha);
                    dst[di+2] = (uint8_t)(src[si+2] * alpha);
                    dst[di+3] = (uint8_t)(src[si+3] * alpha);
                    continue;
                }
            }

            if (inside) {
                int idx = (y * s + x) * 4;
                dst[idx+0] = src[idx+0];
                dst[idx+1] = src[idx+1];
                dst[idx+2] = src[idx+2];
                dst[idx+3] = src[idx+3];
            }
        }
    }
}

int main() {
    init_crc32();

    // Read the PNG data embedded in the source LOGO.ico (256x256)
    // Or: we read the LOGO.png directly using LoadImage + GDI
    // Simplest: Load the existing LOGO.ico, find the 256x256 entry, decode it

    FILE* fsrc = fopen("D:/AceTranslatePro/AceTranslatePro/ui/icons/LOGO.ico", "rb");
    if (!fsrc) {
        fprintf(stderr, "Cannot read LOGO.ico\n");
        return 1;
    }

    fseek(fsrc, 0, SEEK_END);
    long fsize = ftell(fsrc);
    fseek(fsrc, 0, SEEK_SET);
    std::vector<uint8_t> ico_data(fsize);
    fread(ico_data.data(), 1, fsize, fsrc);
    fclose(fsrc);

    // Parse ICO directory
    if (ico_data.size() < 6) { fprintf(stderr, "ICO too small\n"); return 1; }
    int entry_count = ico_data[4] | (ico_data[5] << 8);

    // Find the largest PNG entry (256x256 = width 0)
    int best_idx = -1, best_size = 0;
    for (int i = 0; i < entry_count; i++) {
        int off = 6 + i * 16;
        if (off + 16 > (int)ico_data.size()) break;
        int w = ico_data[off];
        int h = ico_data[off+1];
        int sz = ico_data[off+8] | (ico_data[off+9]<<8) | (ico_data[off+10]<<16) | (ico_data[off+11]<<24);
        if ((w == 0 || w >= 128) && sz > best_size) {
            best_size = sz;
            best_idx = i;
        }
    }

    if (best_idx < 0) { fprintf(stderr, "No large icon found\n"); return 1; }

    int off = 6 + best_idx * 16;
    int img_w = ico_data[off];
    int img_h = ico_data[off+1];
    int data_size = ico_data[off+8] | (ico_data[off+9]<<8) | (ico_data[off+10]<<16) | (ico_data[off+11]<<24);
    int data_off = ico_data[off+12] | (ico_data[off+13]<<8) | (ico_data[off+14]<<16) | (ico_data[off+15]<<24);

    if (img_w == 0) img_w = 256;
    if (img_h == 0) img_h = 256;
    int s = std::max(img_w, img_h);

    fprintf(stderr, "Found %dx%d PNG icon at offset %d, size %d\n", img_w, img_h, data_off, data_size);

    // Save the embedded PNG to temp and load it via GDI+
    // Actually simpler: we output the embedded PNG directly as the largest icon,
    // and generate smaller sizes ourselves

    // For smaller sizes, create rounded versions
    int out_sizes[] = {256, 128, 64, 48, 32, 24, 16};
    int num_out = 7;

    // Read the largest PNG and use as source for all sizes
    // Since we can't decode PNG in pure C easily, let's cheat:
    // We'll keep the original 256x256 PNG untouched (it was already reasonably rounded?),
    // and just generate smaller sizes from scratch with rounded corners

    // Actually, the simplest approach: output the original 256x256 PNG as-is for large,
    // and generate new small sizes with rounding

    // But we can't decode PNG... Let's take a different approach entirely:
    // Just use LoadImage to load the PNG and GetDIBits to read pixels

    // Use GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    // Load LOGO.png
    Gdiplus::Bitmap* bmp = Gdiplus::Bitmap::FromFile(L"D:\\AceTranslatePro\\AceTranslatePro\\ui\\icons\\LOGO.png");
    if (bmp->GetLastStatus() != Gdiplus::Ok) {
        fprintf(stderr, "Cannot load LOGO.png via GDI+\n");
        delete bmp;
        Gdiplus::GdiplusShutdown(gdiplusToken);
        return 1;
    }

    int src_w = bmp->GetWidth();
    int src_h = bmp->GetHeight();
    fprintf(stderr, "Loaded LOGO.png: %dx%d\n", src_w, src_h);

    // Read pixels into RGBA buffer (flattened)
    std::vector<uint8_t> src_rgba(src_w * src_h * 4);
    Gdiplus::Rect rect(0, 0, src_w, src_h);
    Gdiplus::BitmapData bmd;
    bmp->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bmd);
    memcpy(src_rgba.data(), bmd.Scan0, src_w * src_h * 4);
    bmp->UnlockBits(&bmd);
    delete bmp;
    Gdiplus::GdiplusShutdown(gdiplusToken);

    // Write ICO
    FILE* f = fopen("D:/AceTranslatePro/AceTranslatePro/ui/icons/LOGO.ico", "wb");
    if (!f) { fprintf(stderr, "Cannot write LOGO.ico\n"); return 1; }

    // Header
    fputc(0, f); fputc(0, f);
    fputc(1, f); fputc(0, f);
    fputc(num_out, f); fputc(0, f);

    // Pre-compute PNG data for each size
    std::vector<std::vector<uint8_t>> out_pngs(num_out);
    uint32_t data_offset = 6 + num_out * 16;

    for (int i = 0; i < num_out; i++) {
        int sz = out_sizes[i];
        // Scale source to this size
        std::vector<uint8_t> scaled(sz * sz * 4, 0);
        for (int y = 0; y < sz; y++) {
            for (int x = 0; x < sz; x++) {
                int sx = (int)((double)x / sz * src_w);
                int sy = (int)((double)y / sz * src_h);
                if (sx >= src_w) sx = src_w-1;
                if (sy >= src_h) sy = src_h-1;
                int si = (sy * src_w + sx) * 4;
                int di = (y * sz + x) * 4;
                scaled[di+0] = src_rgba[si+0];
                scaled[di+1] = src_rgba[si+1];
                scaled[di+2] = src_rgba[si+2];
                scaled[di+3] = src_rgba[si+3];
            }
        }

        // Apply rounded corners
        std::vector<uint8_t> rounded(sz * sz * 4);
        double radius_ratio = 0.2;
        make_rounded_image(scaled.data(), rounded.data(), sz, radius_ratio);

        // Write PNG to memory
        // Write PNG to a temp file then read back
        char tmpname[64];
        snprintf(tmpname, sizeof(tmpname), "d:/tmp_ico_%d.png", sz);
        FILE* pf = fopen(tmpname, "wb");
        if (pf) {
            write_png(pf, rounded.data(), sz, sz);
            fclose(pf);
            // Read back
            FILE* pr = fopen(tmpname, "rb");
            if (pr) {
                fseek(pr, 0, SEEK_END);
                long len = ftell(pr);
                fseek(pr, 0, SEEK_SET);
                out_pngs[i].resize(len);
                fread(out_pngs[i].data(), 1, len, pr);
                fclose(pr);
            }
            remove(tmpname);
        }

        // ICO directory entry
        uint8_t wb = sz >= 256 ? 0 : (uint8_t)sz;
        fputc(wb, f); fputc(wb, f); // width, height
        fputc(0, f);                // colors
        fputc(0, f);                // reserved
        fputc(1, f); fputc(0, f);   // planes
        fputc(32, f); fputc(0, f);  // bpp
        uint32_t png_sz = (uint32_t)out_pngs[i].size();
        put32(f, png_sz);           // size
        put32(f, data_offset);      // offset
        data_offset += png_sz;
    }

    for (int i = 0; i < num_out; i++) {
        fwrite(out_pngs[i].data(), 1, out_pngs[i].size(), f);
    }

    fclose(f);
    printf("Done! Generated rounded LOGO.ico from LOGO.png\n");
    return 0;
}
