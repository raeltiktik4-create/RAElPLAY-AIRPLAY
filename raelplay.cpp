#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "json_decoder.h"

#define ID_START        1001
#define ID_STOP         1002
#define ID_DECODER      1003
#define ID_QUALITY      1004
#define ID_AUDIO        1005
#define ID_REFRESH      1006
#define ID_BUFFER       1007
#define ID_OFFSET       1008
#define ID_FULLSCREEN   1009
#define ID_LATENCY      1010
#define ID_STATUS       1011
#define ID_LAT_LABEL    1012
#define ID_FPS_LABEL    1013
#define ID_KBPS_LABEL   1014
#define ID_GRAPH        1015
#define ID_RES          1016
#define ID_FPS          1017
#define ID_BUFVAL       1019
#define ID_OFFVAL       1020
#define ID_GAMER        1021
#define ID_H265         1022

#define COL_BG      RGB(246, 247, 249)
#define COL_HEADER  RGB(18, 18, 21)
#define COL_SUB     RGB(150, 152, 158)
#define COL_SECTION RGB(110, 114, 122)
#define COL_ACCENT  RGB(34, 197, 94)
#define COL_OFF     RGB(200, 202, 206)
#define COL_TEXT    RGB(24, 26, 30)

static HWND hMain, hStart, hStop, hDecoder, hQuality, hAudio, hRefresh, hBuffer, hOffset;
static HWND hFullscreen, hLatency, hStatus, hLat, hFps, hKbps, hGraph, hRes, hFpsSel, hBufVal, hOffVal, hGamer, hH265;
static HFONT hFont, hMono, hBtnFont, hTitleFont, hSubFont, hSecFont;
static HBRUSH hBg;
static HANDLE hProc = NULL, hThread = NULL, hReadPipe = NULL;
static HANDLE hJob = NULL;
static DWORD procId = 0;
static int stopping = 0;
static int hist[240];
static int histPos = 0;
static char exeDir[MAX_PATH];
static char lastLine[512];
static char uxplayPath[MAX_PATH];
static char uxplayDir[MAX_PATH];

struct Quality { const wchar_t *name; int res; int fps; const wchar_t *note; };
static const Quality qualities[] = {
    { L"Mínima 540p 30",       1, 30, L"menor atraso" },
    { L"Baixa 720p 30",        2, 30, L"leve" },
    { L"Recomendada 1080p 60", 3, 60, L"equilibrio" },
    { L"Nítida 1080x1920 60",  4, 60, L"retrato nitido, mais atraso" },
    { L"Nativa 1242x2688 30",  5, 30, L"pesada" },
    { L"Automática 60",        0, 60, L"o iPhone decide" }
};
static const wchar_t *resolutions[] = { L"Automática", L"960x540", L"1280x720",
                                        L"1920x1080", L"1080x1920", L"1242x2688" };
static const int fpsValues[] = { 30, 45, 60 };
static const wchar_t *decoders[] = { L"GPU D3D11", L"GPU NVDEC", L"CPU avdec (padrão)" };
static const wchar_t *audioModes[] = { L"Padrão do Windows", L"Fone Bluetooth", L"Fone + Windows", L"Sem áudio" };

static int cfgQuality = 2, cfgDecoder = 2, cfgAudio = 0, cfgBuffer = 1, cfgOffset = 0, cfgFs = 1, cfgLat = 1, cfgRes = 3, cfgFps = 60, cfgGamer = 0, cfgH265 = 0;

static void cfgPath(char *out, size_t n) { snprintf(out, n, "%s\\config.json", exeDir); }

static void cfgLoad(void) {
    char p[MAX_PATH]; cfgPath(p, sizeof p);
    cfgDecoder = raelplay_json_decoder_from_file(p);
    FILE *f = fopen(p, "rb");
    if (!f) return;
    char buf[1024] = {0};
    fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    const char *k;
    if ((k = strstr(buf, "\"quality\":"))) cfgQuality = atoi(k + 10);
    if ((k = strstr(buf, "\"audio\":")))   cfgAudio = atoi(k + 8);
    if ((k = strstr(buf, "\"buffer\":")))  cfgBuffer = atoi(k + 9);
    if ((k = strstr(buf, "\"offset\":")))  cfgOffset = atoi(k + 9);
    if ((k = strstr(buf, "\"fs\":")))      cfgFs = atoi(k + 5);
    if ((k = strstr(buf, "\"lat\":")))     cfgLat = atoi(k + 6);
    if ((k = strstr(buf, "\"res\":"))) cfgRes = atoi(k + 6);
    if ((k = strstr(buf, "\"fpsv\":"))) cfgFps = atoi(k + 7);
    if ((k = strstr(buf, "\"gamer\":"))) cfgGamer = atoi(k + 8);
    if ((k = strstr(buf, "\"h265\":"))) cfgH265 = atoi(k + 7);
    if (cfgQuality < 0 || cfgQuality > 5) cfgQuality = 2;
    if (cfgRes < 0 || cfgRes > 5) cfgRes = 3;
    if (cfgFps != 30 && cfgFps != 45 && cfgFps != 60) cfgFps = 60;
    if (cfgDecoder < 0 || cfgDecoder > 2) cfgDecoder = 2;
    if (cfgAudio < 0 || cfgAudio > 3) cfgAudio = 0;
    if (cfgBuffer < 1 || cfgBuffer > 12) cfgBuffer = 1;
    if (cfgOffset < 0 || cfgOffset > 1000) cfgOffset = 0;
    if (cfgH265 != 0 && cfgH265 != 1) cfgH265 = 0;
}

static void cfgSave(void) {
    char p[MAX_PATH]; cfgPath(p, sizeof p);
    FILE *f = fopen(p, "wb");
    if (!f) return;
    fprintf(f, "{\n  \"quality\": %d,\n  \"decoder\": %d,\n  \"audio\": %d,\n"
                    "  \"buffer\": %d,\n  \"offset\": %d,\n  \"fs\": %d,\n  \"lat\": %d,\n"
                    "  \"res\": %d,\n  \"fpsv\": %d,\n  \"gamer\": %d,\n  \"h265\": %d\n}\n",
                cfgQuality, cfgDecoder, cfgAudio, cfgBuffer, cfgOffset, cfgFs, cfgLat, cfgRes, cfgFps, cfgGamer, cfgH265);
    fclose(f);
}

static void setStatus(const wchar_t *txt) { SetWindowTextW(hStatus, txt); }

static int findUxplay(void) {
    char probe[MAX_PATH], full[MAX_PATH];
    const char *suffix[] = { "\\uxplay.exe", "\\..\\uxplay.exe", "\\..\\..\\uxplay.exe",
                             "\\..\\..\\..\\uxplay.exe" };
    for (int i = 0; i < 4; i++) {
        snprintf(probe, sizeof probe, "%s%s", exeDir, suffix[i]);
        if (GetFullPathNameA(probe, MAX_PATH, full, NULL) && GetFileAttributesA(full) != INVALID_FILE_ATTRIBUTES) {
            snprintf(uxplayPath, sizeof uxplayPath, "%s", full);
            snprintf(uxplayDir, sizeof uxplayDir, "%s", full);
            { char *s2 = strrchr(uxplayDir, '\\'); if (s2) *s2 = 0; }
            return 1;
        }
    }
    return 0;
}

static void parseMetrics(char *line) {
    char *p;
    double lat = -1, fps = -1, kbps = -1;
    if ((p = strstr(line, "lat=")))  lat = atof(p + 4);
    if ((p = strstr(line, "fps=")))  fps = atof(p + 4);
    if ((p = strstr(line, "kbps="))) kbps = atof(p + 5);
    if (lat < 0) return;
    wchar_t buf[128];
    swprintf(buf, 128, L"%.0f ms", lat);   SetWindowTextW(hLat, buf);
    swprintf(buf, 128, L"%.0f fps", fps);  SetWindowTextW(hFps, buf);
    swprintf(buf, 128, L"%.0f kbps", kbps); SetWindowTextW(hKbps, buf);
    hist[histPos] = (int) lat;
    histPos = (histPos + 1) % 240;
    InvalidateRect(hGraph, NULL, TRUE);
}

static DWORD WINAPI readerThread(LPVOID) {
    char buf[4096], line[1024];
    DWORD got = 0;
    size_t used = 0;
    while (ReadFile(hReadPipe, buf, sizeof buf, &got, NULL) && got > 0) {
        for (DWORD i = 0; i < got; i++) {
            char c = buf[i];
            if (c == 10 || c == 13) {
                if (used) {
                    line[used] = 0;
                    if (!strncmp(line, "RAELPLAY ", 9)) {
                        parseMetrics(line);
                    } else {
                        strncpy(lastLine, line, sizeof lastLine - 1);
                        lastLine[sizeof lastLine - 1] = 0;
                        PostMessageW(hMain, WM_APP + 1, 0, 0);
                    }
                    used = 0;
                }
            } else if (used < sizeof line - 1) {
                line[used++] = c;
            }
        }
    }
    return 0;
}

static void appendArg(char *dst, size_t n, const char *arg) {
    size_t len = strlen(dst);
    if (len && len + 1 < n) { dst[len] = ' '; dst[++len] = 0; }
    snprintf(dst + len, n - len, "%s", arg);
}

static void startServer(void) {
    if (hProc) return;
    stopping = 0;
    if (!findUxplay()) {
        setStatus(L"uxplay.exe não encontrado. Rode o coletor de DLLs ou use a versão instalada.");
        return;
    }
    if (cfgGamer) {
        cfgRes = 1;
        cfgFps = 30;
        cfgBuffer = 1;
        cfgOffset = 0;
    }
    if (cfgBuffer > 3) {
        cfgBuffer = 3;
    }
    char audioArg[600] = "";
    char cmd[2048];
    char sizeA[32], fpsA[8], bufA[8], offA[16];

    sizeA[0] = 0;
    if (cfgRes > 0 && cfgRes < (int)(sizeof resolutions / sizeof resolutions[0])) {
        WideCharToMultiByte(CP_UTF8, 0, resolutions[cfgRes], -1, sizeA, sizeof sizeA, NULL, NULL);
    }
    snprintf(fpsA, sizeof fpsA, "%d", cfgFps);
    snprintf(bufA, sizeof bufA, "%d", cfgBuffer);
    snprintf(offA, sizeof offA, "%.3f", cfgOffset / 1000.0);

    switch (cfgAudio) {
    case 1: snprintf(audioArg, sizeof audioArg, "wasapisink"); break;
    case 2: snprintf(audioArg, sizeof audioArg,
                     "tee name=t t. ! queue ! wasapisink t. ! queue ! wasapisink"); break;
    case 3: snprintf(audioArg, sizeof audioArg, "fakesink"); break;
    default: audioArg[0] = 0; break;
    }

    snprintf(cmd, sizeof cmd, "\"%s\" -n RaelPlay -fps %s -reset 0 -al %s",
             uxplayPath, fpsA, offA);
    if (sizeA[0]) {
        appendArg(cmd, sizeof cmd, "-s");
        appendArg(cmd, sizeof cmd, sizeA);
    }
    if (cfgDecoder == 0) {
        appendArg(cmd, sizeof cmd, "-hw");
        appendArg(cmd, sizeof cmd, "-vd");
        appendArg(cmd, sizeof cmd, "d3d11h264dec");
    } else if (cfgDecoder == 1) {
        appendArg(cmd, sizeof cmd, "-hw");
        appendArg(cmd, sizeof cmd, "-vd");
        appendArg(cmd, sizeof cmd, "nvh264dec");
    } else {
        appendArg(cmd, sizeof cmd, "-vd");
        appendArg(cmd, sizeof cmd, "avdec_h264");
    }
    if (cfgH265) appendArg(cmd, sizeof cmd, "-h265");
    appendArg(cmd, sizeof cmd, "-vsync no");
    if (cfgFs) appendArg(cmd, sizeof cmd, "-fs");
    if (audioArg[0]) { appendArg(cmd, sizeof cmd, "-as"); appendArg(cmd, sizeof cmd, audioArg); }

    SetEnvironmentVariableA("RAELPLAY_QUEUE", bufA);
    SetEnvironmentVariableA("RAELPLAY_FPS", fpsA);
    SetEnvironmentVariableA("RAELPLAY_LAT", cfgLat ? "1" : "0");
    SetEnvironmentVariableA("RAELPLAY_MAXLAT", cfgGamer ? "120" : "220");
    {
        char gp[MAX_PATH];
        snprintf(gp, sizeof gp, "%s\\gstreamer-1.0", uxplayDir);
        if (GetFileAttributesA(gp) != INVALID_FILE_ATTRIBUTES) {
            SetEnvironmentVariableA("GST_PLUGIN_PATH", gp);
        }
    }

    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE rd = NULL, wr = NULL;
    CreatePipe(&rd, &wr, &sa, 0);
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = wr;
    si.hStdError = wr;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    ZeroMemory(&pi, sizeof pi);

    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, uxplayDir, &si, &pi)) {
        CloseHandle(rd); CloseHandle(wr);
        setStatus(L"Falha ao iniciar o uxplay.exe.");
        return;
    }
    CloseHandle(wr);
    CloseHandle(pi.hThread);
    hProc = pi.hProcess;
    hJob = CreateJobObjectW(NULL, NULL);
    if (hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jl;
        ZeroMemory(&jl, sizeof jl);
        jl.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jl, sizeof jl);
        AssignProcessToJobObject(hJob, hProc);
    }
    hJob = CreateJobObjectW(NULL, NULL);
    if (hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jl;
        ZeroMemory(&jl, sizeof jl);
        jl.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jl, sizeof jl);
        AssignProcessToJobObject(hJob, hProc);
    }
    procId = pi.dwProcessId;
    hReadPipe = rd;
    hThread = CreateThread(NULL, 0, readerThread, NULL, 0, NULL);

    EnableWindow(hStart, FALSE);
    EnableWindow(hStop, TRUE);
    EnableWindow(hQuality, FALSE);
    EnableWindow(hDecoder, FALSE);
    setStatus(L"Rodando. Escolha RaelPlay no espelhamento do iPhone.");
}

static DWORD WINAPI stopperThread(LPVOID) {
    if (hJob) {
        TerminateJobObject(hJob, 0);
        CloseHandle(hJob);
        hJob = NULL;
    } else if (procId) {
        HANDLE hp = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, procId);
        if (hp) {
            TerminateProcess(hp, 0);
            CloseHandle(hp);
        }
    }
    if (hProc) TerminateProcess(hProc, 0);
    if (hProc && WaitForSingleObject(hProc, 3000) != WAIT_OBJECT_0) {
        HANDLE hp = OpenProcess(PROCESS_TERMINATE, FALSE, procId);
        if (hp) {
            TerminateProcess(hp, 0);
            CloseHandle(hp);
        }
        WaitForSingleObject(hProc, 2000);
    }
    if (hThread) {
        CancelSynchronousIo(hThread);
        WaitForSingleObject(hThread, 2000);
        CloseHandle(hThread);
        hThread = NULL;
    }
    if (hReadPipe) {
        CloseHandle(hReadPipe);
        hReadPipe = NULL;
    }
    if (hProc) {
        DWORD code = 0;
        GetExitCodeProcess(hProc, &code);
        if (code == STILL_ACTIVE) {
            strncpy(lastLine, "ATENCAO: processo continua vivo", sizeof lastLine - 1);
        }
        CloseHandle(hProc);
        hProc = NULL;
    }
    procId = 0;
    PostMessageW(hMain, WM_APP + 2, 0, 0);
    return 0;
}

static void stopServer(void) {
    if (!hProc || stopping) {
        return;
    }
    stopping = 1;
    setStatus(L"Parando...");
    EnableWindow(hStart, FALSE);
    EnableWindow(hStop, FALSE);
    CreateThread(NULL, 0, stopperThread, NULL, 0, NULL);
}

static void paintGraph(HWND hw) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hw, &ps);
    RECT rc;
    GetClientRect(hw, &rc);
    HBRUSH bg = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(dc, &rc, bg);
    DeleteObject(bg);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    const int maxMs = 300;
    HPEN pen = CreatePen(PS_SOLID, 2, COL_ACCENT);
    HPEN old = (HPEN) SelectObject(dc, pen);
    for (int i = 0; i < 239; i++) {
        int a = hist[(histPos + i) % 240];
        int b = hist[(histPos + i + 1) % 240];
        if (a > maxMs) a = maxMs;
        if (b > maxMs) b = maxMs;
        MoveToEx(dc, i * w / 240, h - a * h / maxMs, NULL);
        LineTo(dc, (i + 1) * w / 240, h - b * h / maxMs);
    }
    SelectObject(dc, old);
    DeleteObject(pen);
    pen = CreatePen(PS_DOT, 1, RGB(220, 80, 80));
    old = (HPEN) SelectObject(dc, pen);
    MoveToEx(dc, 0, h - 250 * h / maxMs, NULL);
    LineTo(dc, w, h - 250 * h / maxMs);
    SelectObject(dc, old);
    DeleteObject(pen);
    EndPaint(hw, &ps);
}

static HWND mk(HWND parent, const wchar_t *cls, const wchar_t *txt, DWORD style,
               int x, int y, int ww, int hh, int id, HFONT font) {
    HWND hwnd = CreateWindowExW(0, cls, txt, WS_CHILD | WS_VISIBLE | style,
                                x, y, ww, hh, parent, (HMENU)(INT_PTR) id, NULL, NULL);
    SendMessageW(hwnd, WM_SETFONT, (WPARAM) font, TRUE);
    return hwnd;
}

static void buildUi(HWND hw) {
    hFont     = CreateFontW(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    hBtnFont  = CreateFontW(-16, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    hTitleFont = CreateFontW(-26, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    hSubFont  = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    hSecFont  = CreateFontW(-11, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    hMono     = CreateFontW(-21, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Consolas");

    mk(hw, L"STATIC", L"VÍDEO", 0, 20, 84, 200, 14, 0, hSecFont);
    hDecoder = mk(hw, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, 20, 100, 320, 200, ID_DECODER, hFont);
    for (int i = 0; i < 3; i++) SendMessageW(hDecoder, CB_ADDSTRING, 0, (LPARAM) decoders[i]);
    SendMessageW(hDecoder, CB_SETCURSEL, cfgDecoder, 0);

    mk(hw, L"STATIC", L"PRESET", 0, 20, 138, 200, 14, 0, hSecFont);
    hQuality = mk(hw, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, 20, 154, 320, 260, ID_QUALITY, hFont);
    for (int i = 0; i < (int)(sizeof qualities / sizeof qualities[0]); i++) {
        SendMessageW(hQuality, CB_ADDSTRING, 0, (LPARAM) qualities[i].name);
    }
    SendMessageW(hQuality, CB_SETCURSEL, cfgQuality, 0);
    hFullscreen = mk(hw, L"BUTTON", L"Tela cheia", BS_AUTOCHECKBOX, 352, 158, 120, 22, ID_FULLSCREEN, hFont);
    SendMessageW(hFullscreen, BM_SETCHECK, cfgFs ? BST_CHECKED : BST_UNCHECKED, 0);

    mk(hw, L"STATIC", L"RESOLUÇÃO", 0, 20, 196, 200, 14, 0, hSecFont);
    hRes = mk(hw, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, 20, 212, 290, 260, ID_RES, hFont);
    for (int i = 0; i < (int)(sizeof resolutions / sizeof resolutions[0]); i++) {
        SendMessageW(hRes, CB_ADDSTRING, 0, (LPARAM) resolutions[i]);
    }
    SendMessageW(hRes, CB_SETCURSEL, cfgRes, 0);

    mk(hw, L"STATIC", L"FPS", 0, 322, 196, 60, 14, 0, hSecFont);
    hFpsSel = mk(hw, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, 322, 212, 150, 260, ID_FPS, hFont);
    for (int i = 0; i < 3; i++) {
        wchar_t fb[8];
        swprintf(fb, 8, L"%d", fpsValues[i]);
        SendMessageW(hFpsSel, CB_ADDSTRING, 0, (LPARAM) fb);
        if (fpsValues[i] == cfgFps) SendMessageW(hFpsSel, CB_SETCURSEL, i, 0);
    }

    mk(hw, L"STATIC", L"ÁUDIO", 0, 20, 254, 200, 14, 0, hSecFont);
    hAudio = mk(hw, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, 20, 270, 320, 200, ID_AUDIO, hFont);
    for (int i = 0; i < 4; i++) SendMessageW(hAudio, CB_ADDSTRING, 0, (LPARAM) audioModes[i]);
    SendMessageW(hAudio, CB_SETCURSEL, cfgAudio, 0);
    hRefresh = mk(hw, L"BUTTON", L"Atualizar", BS_PUSHBUTTON, 352, 270, 120, 26, ID_REFRESH, hFont);

    mk(hw, L"STATIC", L"BUFFER (FRAMES)", 0, 20, 308, 200, 14, 0, hSecFont);
    hBuffer = mk(hw, TRACKBAR_CLASS, L"", TBS_HORZ | TBS_NOTICKS, 20, 324, 412, 26, ID_BUFFER, hFont);
    SendMessageW(hBuffer, TBM_SETRANGE, TRUE, MAKELONG(1, 12));
    SendMessageW(hBuffer, TBM_SETPOS, TRUE, cfgBuffer);
    {
        wchar_t vb[16];
        swprintf(vb, 16, L"%d", cfgBuffer);
        hBufVal = mk(hw, L"STATIC", vb, SS_RIGHT, 440, 328, 32, 20, ID_BUFVAL, hFont);
    }

    mk(hw, L"STATIC", L"OFFSET DE ÁUDIO (MS)", 0, 20, 358, 200, 14, 0, hSecFont);
    hOffset = mk(hw, TRACKBAR_CLASS, L"", TBS_HORZ | TBS_NOTICKS, 20, 374, 412, 26, ID_OFFSET, hFont);
    SendMessageW(hOffset, TBM_SETRANGE, TRUE, MAKELONG(0, 1000));
    SendMessageW(hOffset, TBM_SETPOS, TRUE, cfgOffset);
    {
        wchar_t vo[16];
        swprintf(vo, 16, L"%d", cfgOffset);
        hOffVal = mk(hw, L"STATIC", vo, SS_RIGHT, 440, 378, 32, 20, ID_OFFVAL, hFont);
    }

    hLatency = mk(hw, L"BUTTON", L"Mostrar métricas", BS_AUTOCHECKBOX, 20, 410, 200, 22, ID_LATENCY, hFont);
    SendMessageW(hLatency, BM_SETCHECK, cfgLat ? BST_CHECKED : BST_UNCHECKED, 0);
    hGamer = mk(hw, L"BUTTON", L"Modo gamer", BS_AUTOCHECKBOX, 230, 410, 140, 22, ID_GAMER, hFont);
    SendMessageW(hGamer, BM_SETCHECK, cfgGamer ? BST_CHECKED : BST_UNCHECKED, 0);
    hH265 = mk(hw, L"BUTTON", L"H.265", BS_AUTOCHECKBOX, 380, 410, 92, 22, ID_H265, hFont);
    SendMessageW(hH265, BM_SETCHECK, cfgH265 ? BST_CHECKED : BST_UNCHECKED, 0);

    hStart = mk(hw, L"BUTTON", L"INICIAR", BS_OWNERDRAW, 20, 444, 216, 44, ID_START, hBtnFont);
    hStop  = mk(hw, L"BUTTON", L"PARAR",   BS_OWNERDRAW, 256, 444, 216, 44, ID_STOP, hBtnFont);
    EnableWindow(hStop, FALSE);

    mk(hw, L"STATIC", L"LATÊNCIA", 0, 20, 504, 120, 14, 0, hSecFont);
    hLat = mk(hw, L"STATIC", L"-", SS_LEFT, 20, 518, 140, 30, ID_LAT_LABEL, hMono);
    mk(hw, L"STATIC", L"FPS", 0, 180, 504, 100, 14, 0, hSecFont);
    hFps = mk(hw, L"STATIC", L"-", SS_LEFT, 180, 518, 130, 30, ID_FPS_LABEL, hMono);
    mk(hw, L"STATIC", L"BITRATE", 0, 320, 504, 120, 14, 0, hSecFont);
    hKbps = mk(hw, L"STATIC", L"-", SS_LEFT, 320, 518, 152, 30, ID_KBPS_LABEL, hMono);

    hGraph = mk(hw, L"STATIC", L"", SS_OWNERDRAW, 20, 556, 452, 76, ID_GRAPH, hFont);
    hStatus = mk(hw, L"STATIC", L"Pronto.", SS_LEFT, 20, 642, 452, 34, ID_STATUS, hFont);
}

static LRESULT CALLBACK wndProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        buildUi(hw);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hw, &ps);
        RECT rc;
        GetClientRect(hw, &rc);
        FillRect(dc, &rc, hBg);
        RECT head = { 0, 0, rc.right, 68 };
        HBRUSH hb = CreateSolidBrush(COL_HEADER);
        FillRect(dc, &head, hb);
        DeleteObject(hb);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        HFONT of = (HFONT) SelectObject(dc, hTitleFont);
        TextOutW(dc, 20, 12, L"RaelPlay", 8);
        SelectObject(dc, of);
        SetTextColor(dc, COL_SUB);
        of = (HFONT) SelectObject(dc, hSubFont);
        TextOutW(dc, 21, 42, L"receptor AirPlay", 16);
        SelectObject(dc, of);
        EndPaint(hw, &ps);
        return 0;
    }
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT d = (LPDRAWITEMSTRUCT) lp;
        if (d->CtlID == ID_START || d->CtlID == ID_STOP) {
            int primary = (d->CtlID == ID_START);
            int on = !(d->itemState & ODS_DISABLED);
            COLORREF bg = primary ? (on ? COL_ACCENT : COL_OFF) : (on ? RGB(255, 255, 255) : RGB(238, 239, 241));
            COLORREF fg = primary ? RGB(255, 255, 255) : COL_TEXT;
            HBRUSH b = CreateSolidBrush(bg);
            HPEN p = CreatePen(PS_SOLID, 1, primary ? bg : RGB(214, 216, 220));
            HBRUSH ob = (HBRUSH) SelectObject(d->hDC, b);
            HPEN op = (HPEN) SelectObject(d->hDC, p);
            RoundRect(d->hDC, d->rcItem.left, d->rcItem.top, d->rcItem.right, d->rcItem.bottom, 12, 12);
            SelectObject(d->hDC, ob);
            SelectObject(d->hDC, op);
            DeleteObject(b);
            DeleteObject(p);
            wchar_t txt[64];
            GetWindowTextW(d->hwndItem, txt, 64);
            SetBkMode(d->hDC, TRANSPARENT);
            SetTextColor(d->hDC, fg);
            HFONT of2 = (HFONT) SelectObject(d->hDC, hBtnFont);
            DrawTextW(d->hDC, txt, -1, &d->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(d->hDC, of2);
            return TRUE;
        }
        break;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC) wp;
        SetBkMode(dc, TRANSPARENT);
        HWND ctl = (HWND) lp;
        if (ctl == hStatus) SetTextColor(dc, RGB(80, 84, 92));
        else if (ctl == hLat || ctl == hFps || ctl == hKbps) SetTextColor(dc, COL_TEXT);
        else SetTextColor(dc, COL_SECTION);
        return (LRESULT) hBg;
    }
    case WM_APP + 1: {
        wchar_t w[512];
        MultiByteToWideChar(CP_UTF8, 0, lastLine, -1, w, 512);
        setStatus(w);
        return 0;
    }
    case WM_APP + 2:
        stopping = 0;
        EnableWindow(hStart, TRUE);
        EnableWindow(hStop, FALSE);
        EnableWindow(hQuality, TRUE);
        EnableWindow(hDecoder, TRUE);
        SetWindowTextW(hLat, L"-");
        SetWindowTextW(hFps, L"-");
        SetWindowTextW(hKbps, L"-");
        setStatus(L"Parado.");
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_START: startServer(); return 0;
        case ID_STOP: stopServer(); return 0;
        case ID_REFRESH: setStatus(L"Reinicie o servidor para aplicar a saída de áudio."); return 0;
        case ID_DECODER:
            if (HIWORD(wp) == CBN_SELCHANGE) { cfgDecoder = (int) SendMessageW(hDecoder, CB_GETCURSEL, 0, 0); cfgSave(); }
            return 0;
        case ID_RES:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                cfgRes = (int) SendMessageW(hRes, CB_GETCURSEL, 0, 0);
                cfgSave();
            }
            return 0;
        case ID_FPS:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                int idx = (int) SendMessageW(hFpsSel, CB_GETCURSEL, 0, 0);
                if (idx >= 0 && idx < 3) cfgFps = fpsValues[idx];
                cfgSave();
            }
            return 0;
        case ID_QUALITY:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                cfgQuality = (int) SendMessageW(hQuality, CB_GETCURSEL, 0, 0);
                cfgRes = qualities[cfgQuality].res;
                cfgFps = qualities[cfgQuality].fps;
                SendMessageW(hRes, CB_SETCURSEL, cfgRes, 0);
                for (int i = 0; i < 3; i++) {
                    if (fpsValues[i] == cfgFps) SendMessageW(hFpsSel, CB_SETCURSEL, i, 0);
                }
                cfgSave();
                wchar_t m[256];
                swprintf(m, 256, L"%s — %s", qualities[cfgQuality].name, qualities[cfgQuality].note);
                setStatus(m);
            }
            return 0;
        case ID_AUDIO:
            if (HIWORD(wp) == CBN_SELCHANGE) { cfgAudio = (int) SendMessageW(hAudio, CB_GETCURSEL, 0, 0); cfgSave(); }
            return 0;
        case ID_FULLSCREEN:
            cfgFs = SendMessageW(hFullscreen, BM_GETCHECK, 0, 0) == BST_CHECKED; cfgSave(); return 0;
        case ID_LATENCY:
            cfgLat = SendMessageW(hLatency, BM_GETCHECK, 0, 0) == BST_CHECKED; cfgSave(); return 0;
        case ID_GAMER:
            cfgGamer = SendMessageW(hGamer, BM_GETCHECK, 0, 0) == BST_CHECKED;
            cfgSave();
            return 0;
        case ID_H265:
            cfgH265 = SendMessageW(hH265, BM_GETCHECK, 0, 0) == BST_CHECKED;
            cfgSave();
            return 0;
        }
        return 0;
    case WM_HSCROLL: {
        cfgBuffer = (int) SendMessageW(hBuffer, TBM_GETPOS, 0, 0);
        cfgOffset = (int) SendMessageW(hOffset, TBM_GETPOS, 0, 0);
        wchar_t vb[16], vo[16];
        swprintf(vb, 16, L"%d", cfgBuffer);
        swprintf(vo, 16, L"%d", cfgOffset);
        SetWindowTextW(hBufVal, vb);
        SetWindowTextW(hOffVal, vo);
        cfgSave();
        return 0;
    }
    case WM_DESTROY:
        stopServer();
        for (int i = 0; i < 60 && (stopping || hProc); i++) {
            Sleep(50);
        }
        if (hJob) { CloseHandle(hJob); hJob = NULL; }
        WinExec("taskkill /IM uxplay.exe /F >nul 2>&1", SW_HIDE);
        WinExec("taskkill /IM uxplay.exe /F >nul 2>&1", SW_HIDE);
        cfgSave();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hw, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    HANDLE mtx = CreateMutexW(NULL, TRUE, L"RaelPlaySingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(NULL, L"O RaelPlay já está aberto.", L"RaelPlay", MB_ICONINFORMATION);
        return 0;
    }

    GetModuleFileNameA(NULL, exeDir, MAX_PATH);
    char *slash = strrchr(exeDir, '\\');
    if (slash) *slash = 0;
    cfgLoad();
    hBg = CreateSolidBrush(COL_BG);

    INITCOMMONCONTROLSEX icc = { sizeof icc, ICC_BAR_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);
    WinExec("taskkill /IM uxplay.exe /F >nul 2>&1", SW_HIDE);

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = hBg;
    wc.lpszClassName = L"RaelPlayPanel";
    RegisterClassExW(&wc);

    RECT rc = { 0, 0, 492, 692 };
    AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    hMain = CreateWindowExW(0, L"RaelPlayPanel", L"RaelPlay",
                            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                            CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
                            NULL, NULL, hInst, NULL);
    ShowWindow(hMain, SW_SHOW);
    UpdateWindow(hMain);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    ReleaseMutex(mtx);
    return 0;
}
