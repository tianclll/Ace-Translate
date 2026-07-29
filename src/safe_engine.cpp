// safe_engine.cpp — 在子线程中执行引擎调用，崩溃不影响主进程
#include <string>
#include <cstring>
#include <windows.h>
#include <QFile>
#include <QTextStream>
#include <QByteArray>

extern std::string extract_file_text(
    const std::string& file_path,
    const std::string& output_path,
    const std::string& base_dir,
    float layout_threshold,
    int pdf_dpi,
    bool enable_warp,
    bool enable_enhance);

struct Ctx { std::string fp; std::string bd; int isPdf; std::string out; bool ok; };

static DWORD WINAPI run(LPVOID p) {
    Ctx* c = (Ctx*)p;
    c->ok = false;
    if (c->isPdf) {
        c->out = extract_file_text(c->fp, "", c->bd, 0.5f, 200, true, false);
    } else {
        c->out = extract_file_text(c->fp, "", c->bd, 0.5f, 200, true, false);
    }
    c->ok = !c->out.empty();
    return 0;
}

extern "C" bool safe_extract_file(
    const char* filePath, const char* baseDir, const char* ext,
    char* outBuf, int outBufSize, int* outLen) {

    Ctx c;
    c.fp = filePath;
    c.bd = baseDir;
    c.isPdf = (strcmp(ext, "pdf") == 0);

    HANDLE h = CreateThread(NULL, 0, run, &c, 0, NULL);
    if (!h) return false;

    DWORD r = WaitForSingleObject(h, 120000);
    CloseHandle(h);

    if (r != WAIT_OBJECT_0 || !c.ok || c.out.empty()) return false;

    QFile f(QString::fromUtf8(c.out.c_str()));
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = f.readAll();
        f.close();
        int len = qMin(data.size(), outBufSize - 1);
        if (len > 0) memcpy(outBuf, data.constData(), len);
        outBuf[len] = '\0';
        if (outLen) *outLen = len;
    }
    QFile::remove(QString::fromUtf8(c.out.c_str()));
    return outLen && *outLen > 0;
}
