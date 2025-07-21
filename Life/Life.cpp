#define _WIN32_WINNT 0x0A00  
#include <windows.h>
#include <dwmapi.h>        
#include <cstdlib>
#include <ctime>
#include <tchar.h>
#include <shellapi.h>  
#include <gdiplus.h>        
using namespace Gdiplus;
#pragma comment(lib, "shell32.lib")  
#pragma comment(lib, "dwmapi.lib")   
#pragma comment(lib, "gdiplus.lib") 
#pragma comment(lib, "winmm.lib")   

#ifndef DWMWA_ACCENT_POLICY
#define DWMWA_ACCENT_POLICY 33  
#endif

#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35  
#endif

// 鼠标光晕效果常量（进一步缩小光晕大小）
#define HALO_RADIUS 30           // 光晕半径从60进一步缩小到30
#define HALO_MAX_ALPHA 70        // 降低最大透明度使光晕更柔和
#define HALO_COLOR RGB(255, 255, 255) // 白色光晕

// 渲染优化常量
#define RENDER_INTERVAL 16       // 渲染间隔16ms (约60FPS)
#define PARTICLE_COUNT 60        // 减少粒子数量提升性能
#define METEOR_CHANCE 12         // 降低流星生成概率

// 控件ID
#define IDC_BILIBILI_BTN   1001  
#define IDC_SETTINGS_BTN   1002  
#define IDC_SETTINGS_PANEL 1003  
#define IDC_THEME_BTN      1005  
#define IDC_GRADIENT_BTN   1006  

// 粒子类型
enum ParticleType { NORMAL, METEOR };
struct Particle {
    int x, y, vx, vy, size;
    COLORREF color;
    float alpha;
    ParticleType type;
    int life;
};

// 全局状态
HINSTANCE   hInst = NULL;
HWND        g_hSplash = NULL;
HWND        g_hSettingsPanel = NULL;
Particle    splashParticles[50] = { 0 };
Particle    mainParticles[PARTICLE_COUNT] = { 0 };
WCHAR       g_timeText[50] = L"00:00:00";
BOOL        g_isTimeColorful = FALSE;
RECT        g_timeRect = { 0 };
HWND        g_hBilibiliBtn = NULL;
HWND        g_hSettingsBtn = NULL;
RECT        g_settingsPanelRect = { 0 };
BOOL        g_isSettingsVisible = FALSE;
BOOL        g_isWhiteBlackTheme = FALSE;
BOOL        g_isGradientBackground = TRUE;
HBITMAP     g_backBuffer = NULL;
HDC         g_backDC = NULL;
int         g_clientWidth = 0;
int         g_clientHeight = 0;
POINT       g_mousePos = { -1, -1 };
BOOL        g_mouseInWindow = FALSE;
ULONG_PTR   g_gdiplusToken = 0;
HBRUSH      g_backgroundBrush = NULL;     // 背景画刷缓存
HFONT       g_timeFont = NULL;            // 时间字体缓存

// 布局参数
const int BUTTON_RIGHT_MARGIN = 20;
const int BUTTON_TOP_MARGIN = 20;
const int BUTTON_VERTICAL_SPACING = 10;
const int SETTINGS_PANEL_LEFT_OFFSET = 40;

// 彩色时间颜色
const COLORREF g_softYellowColors[7] = {
    RGB(255, 255, 153), RGB(255, 255, 102), RGB(255, 230, 102),
    RGB(255, 204, 51),  RGB(255, 220, 100), RGB(255, 240, 120),
    RGB(255, 235, 150)
};

// 时间文本颜色
const COLORREF TIME_BASE_COLOR = RGB(255, 255, 153);
const COLORREF TIME_DARK_COLOR = RGB(50, 50, 50);

// 流星效果参数
const COLORREF METEOR_COLOR = RGB(144, 238, 144);
const int      METEOR_MIN_VY = 7;
const int      METEOR_MAX_VY = 12;
const int      METEOR_MIN_SIZE = 15;
const int      METEOR_MAX_SIZE = 30;
const int      METEOR_WIDTH = 2;
const float    METEOR_MIN_ALPHA = 0.9f;
const float    METEOR_MAX_ALPHA = 1.0f;
const int      METEOR_LIFE = 80;

// RGB渐变参数
float g_gradientPos = 0.0f;
const float GRADIENT_SPEED = 0.0015f; // 降低渐变速度提升性能

// 函数声明
ATOM        RegisterWindowClass(HINSTANCE);
BOOL        CreateMainWindow(HINSTANCE, int);
LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK SplashWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK SettingsPanelProc(HWND, UINT, WPARAM, LPARAM);
void        InitParticles(Particle*, int, int, int);
void        UpdateParticles(Particle*, int, int, int);
void        DrawParticles(Graphics*, Particle*, int, bool isSplash);
void        UpdateTimeText();
void        OpenUrl(const wchar_t* url);
void        CreateControls(HWND);
void        ToggleSettingsPanel(HWND);
void        UpdateSettingsPanelPos(HWND);
void        PlayStartupSound();
Color       GetRainbowColor(float position);
void        UpdateGradientPosition();
void        InitBackBuffer(HWND hWnd);
void        DestroyBackBuffer();
void        Render(HWND hWnd);
void        PaintTime(HDC hdc);
void        DrawMouseHalo(Graphics* graphics);
void        OptimizeRendering(HWND hWnd); // 新增渲染优化函数

// 程序入口
int APIENTRY wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE,
    _In_ LPWSTR,
    _In_ int nCmdShow
) {
    hInst = hInstance;
    GdiplusStartupInput gdiplusInput;
    GdiplusStartup(&g_gdiplusToken, &gdiplusInput, NULL);

    WNDCLASSW settingsClass = { 0 };
    settingsClass.lpfnWndProc = SettingsPanelProc;
    settingsClass.hInstance = hInstance;
    settingsClass.lpszClassName = L"SettingsPanelClass";
    RegisterClassW(&settingsClass);

    srand(static_cast<unsigned int>(time(0)));

    if (!RegisterWindowClass(hInstance) || !CreateMainWindow(hInstance, nCmdShow)) {
        GdiplusShutdown(g_gdiplusToken);
        return 1;
    }

    WNDCLASSW splashClass = { 0 };
    splashClass.lpfnWndProc = SplashWndProc;
    splashClass.hInstance = hInstance;
    splashClass.lpszClassName = L"Splash";
    RegisterClassW(&splashClass);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    g_hSplash = CreateWindowW(L"Splash", L"", WS_POPUP | WS_VISIBLE,
        (screenW - 600) / 2, (screenH - 400) / 2, 600, 400,
        nullptr, nullptr, hInstance, nullptr);

    PlayStartupSound();
    InitParticles(splashParticles, 50, 600, 400);
    SetTimer(g_hSplash, 1, 3000, nullptr);
    SetTimer(g_hSplash, 2, 33, nullptr);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DestroyBackBuffer();
    if (g_backgroundBrush) DeleteObject(g_backgroundBrush);
    if (g_timeFont) DeleteObject(g_timeFont);
    GdiplusShutdown(g_gdiplusToken);
    return static_cast<int>(msg.wParam);
}

// 播放启动音效
void PlayStartupSound() {
    PlaySound(TEXT("MailBeep"), NULL, SND_ALIAS | SND_ASYNC);
}

// 注册主窗口类
ATOM RegisterWindowClass(HINSTANCE hInstance) {
    WNDCLASSEXW wcex = { sizeof(WNDCLASSEXW) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = MainWndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"MainWindow";
    return RegisterClassExW(&wcex);
}

// 创建主窗口
BOOL CreateMainWindow(HINSTANCE hInstance, int nCmdShow) {
    COLORREF deepBlue = RGB(0, 82, 204);
    COLORREF lightYellow = RGB(255, 255, 224);

    // 程序标题
    HWND hWnd = CreateWindowW(L"MainWindow", L"Life App | Beta 2.0.1",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        CW_USEDEFAULT, 0, 800, 600,
        nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return FALSE;

    DwmSetWindowAttribute(hWnd, DWMWA_TEXT_COLOR, &deepBlue, sizeof(deepBlue));
    DwmSetWindowAttribute(hWnd, DWMWA_CAPTION_COLOR, &lightYellow, sizeof(lightYellow));

    TRACKMOUSEEVENT tme = { 0 };
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE | TME_HOVER;
    tme.hwndTrack = hWnd;
    tme.dwHoverTime = HOVER_DEFAULT;
    TrackMouseEvent(&tme);

    ShowWindow(hWnd, SW_HIDE);
    UpdateWindow(hWnd);
    return TRUE;
}

// 创建控件
void CreateControls(HWND hWnd) {
    g_hBilibiliBtn = CreateWindowW(L"BUTTON", L"关注Life的B站账号",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_CLIPSIBLINGS,
        0, 0, 180, 40,
        hWnd, (HMENU)IDC_BILIBILI_BTN, hInst, nullptr);
    SendMessageW(g_hBilibiliBtn, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    g_hSettingsBtn = CreateWindowW(L"BUTTON", L"设置",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_CLIPSIBLINGS,
        0, 0, 80, 30,
        hWnd, (HMENU)IDC_SETTINGS_BTN, hInst, nullptr);
    SendMessageW(g_hSettingsBtn, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
}

// 切换设置面板
void ToggleSettingsPanel(HWND hWnd) {
    g_isSettingsVisible = !g_isSettingsVisible;
    if (g_isSettingsVisible) {
        if (!g_hSettingsPanel) {
            g_hSettingsPanel = CreateWindowW(L"SettingsPanelClass", L"",
                WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                0, 0, 220, 200,
                hWnd, (HMENU)IDC_SETTINGS_PANEL, hInst, nullptr);

            CreateWindowW(L"BUTTON", L"彩色时间切换",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                10, 10, 200, 30,
                g_hSettingsPanel, (HMENU)IDC_THEME_BTN, hInst, nullptr);
            CreateWindowW(L"BUTTON", L"渐变背景切换",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                10, 50, 200, 30,
                g_hSettingsPanel, (HMENU)IDC_GRADIENT_BTN, hInst, nullptr);
        }
        UpdateSettingsPanelPos(hWnd);
    }
    else {
        if (g_hSettingsPanel) {
            DestroyWindow(g_hSettingsPanel);
            g_hSettingsPanel = NULL;
        }
    }
    InvalidateRect(hWnd, &g_settingsPanelRect, FALSE);
}

// 调整设置面板位置
void UpdateSettingsPanelPos(HWND hMainWnd) {
    if (!g_hSettingsPanel || !g_hSettingsBtn) return;

    RECT btnRect;
    GetWindowRect(g_hSettingsBtn, &btnRect);
    MapWindowPoints(NULL, hMainWnd, (POINT*)&btnRect, 2);

    g_settingsPanelRect.left = max(btnRect.left - SETTINGS_PANEL_LEFT_OFFSET, 0);
    g_settingsPanelRect.top = btnRect.bottom + 10;
    g_settingsPanelRect.right = g_settingsPanelRect.left + 220;
    g_settingsPanelRect.bottom = g_settingsPanelRect.top + 200;

    RECT mainRect;
    GetClientRect(hMainWnd, &mainRect);
    if (g_settingsPanelRect.right > mainRect.right) {
        g_settingsPanelRect.right = mainRect.right;
        g_settingsPanelRect.left = g_settingsPanelRect.right - 220;
    }
    if (g_settingsPanelRect.bottom > mainRect.bottom) {
        g_settingsPanelRect.bottom = mainRect.bottom;
    }

    MoveWindow(g_hSettingsPanel,
        g_settingsPanelRect.left, g_settingsPanelRect.top,
        220, 200, TRUE);
}

// 初始化粒子
void InitParticles(Particle* particles, int count, int maxX, int maxY) {
    for (int i = 0; i < count; ++i) {
        particles[i].x = rand() % maxX;
        particles[i].y = rand() % maxY;
        particles[i].vx = (rand() % 7) - 3;
        particles[i].vy = (rand() % 7) - 3;
        particles[i].size = 4 + rand() % 8;  // 略微减小粒子大小
        particles[i].color = RGB(rand() % 256, rand() % 256, rand() % 256);
        particles[i].alpha = 0.5f + (rand() % 50) / 100.0f;
        particles[i].type = NORMAL;
        particles[i].life = 100 + rand() % 100;
    }
}

// 更新粒子状态
void UpdateParticles(Particle* particles, int count, int maxX, int maxY) {
    for (int i = 0; i < count; ++i) {
        if (particles[i].type == METEOR) {
            particles[i].y += particles[i].vy;
            particles[i].x += particles[i].vx;
            particles[i].life--;

            if (particles[i].y > maxY || particles[i].life <= 0) {
                particles[i].x = rand() % maxX;
                particles[i].y = -particles[i].size;
                particles[i].vx = (rand() % 5) - 2;
                particles[i].vy = METEOR_MIN_VY + rand() % (METEOR_MAX_VY - METEOR_MIN_VY);
                particles[i].size = METEOR_MIN_SIZE + rand() % (METEOR_MAX_SIZE - METEOR_MIN_SIZE);
                particles[i].alpha = METEOR_MIN_ALPHA + (rand() % 10) / 100.0f;
                particles[i].life = METEOR_LIFE;
            }
        }
        else {
            particles[i].x += particles[i].vx;
            particles[i].y += particles[i].vy;

            // 鼠标交互优化 - 只在光晕范围内计算斥力
            if (g_mouseInWindow) {
                int dx = g_mousePos.x - particles[i].x;
                int dy = g_mousePos.y - particles[i].y;
                if (abs(dx) < HALO_RADIUS * 1.5 && abs(dy) < HALO_RADIUS * 1.5) {
                    float dxFloat = static_cast<float>(dx);
                    float dyFloat = static_cast<float>(dy);
                    float distance = sqrtf(dxFloat * dxFloat + dyFloat * dyFloat);

                    if (distance < HALO_RADIUS) {
                        float force = static_cast<float>((HALO_RADIUS - distance) * 0.012f); // 添加f后缀
                        float vxForce = (dxFloat / distance) * force;
                        float vyForce = (dyFloat / distance) * force;
                        particles[i].vx -= static_cast<int>(vxForce);
                        particles[i].vy -= static_cast<int>(vyForce);
                    }
                }
            }

            if (particles[i].x < 0) particles[i].x = maxX;
            if (particles[i].x > maxX) particles[i].x = 0;
            if (particles[i].y < 0) particles[i].y = maxY;
            if (particles[i].y > maxY) particles[i].y = 0;

            // 随机生成流星
            if (rand() % METEOR_CHANCE == 0 && particles[i].type == NORMAL) {
                particles[i].type = METEOR;
                particles[i].x = rand() % maxX;
                particles[i].y = -particles[i].size;
                particles[i].vx = (rand() % 5) - 2;
                particles[i].vy = METEOR_MIN_VY + rand() % (METEOR_MAX_VY - METEOR_MIN_VY);
                particles[i].size = METEOR_MIN_SIZE + rand() % (METEOR_MAX_SIZE - METEOR_MIN_SIZE);
                particles[i].color = METEOR_COLOR;
                particles[i].alpha = METEOR_MIN_ALPHA + (rand() % 10) / 100.0f;
                particles[i].life = METEOR_LIFE;
            }

            if (particles[i].life > 0) particles[i].life--;
            else {
                particles[i].x = rand() % maxX;
                particles[i].y = rand() % maxY;
                particles[i].life = 100 + rand() % 100;
            }
        }
    }
}

// 绘制粒子
void DrawParticles(Graphics* graphics, Particle* particles, int count, bool isSplash) {
    if (isSplash) {
        graphics->SetSmoothingMode(SmoothingModeDefault);
    }
    else {
        graphics->SetSmoothingMode(SmoothingModeAntiAlias);
    }

    for (int i = 0; i < count; ++i) {
        if (particles[i].life <= 0) continue;

        if (particles[i].type == METEOR) {
            Pen meteorPen(Color(
                (BYTE)(particles[i].alpha * 255),
                GetRValue(particles[i].color),
                GetGValue(particles[i].color),
                GetBValue(particles[i].color)
            ), METEOR_WIDTH);

            graphics->DrawLine(&meteorPen,
                particles[i].x,
                particles[i].y,
                particles[i].x - particles[i].vx * 3,
                particles[i].y - particles[i].vy * 3);
        }
        else {
            SolidBrush brush(Color(
                (BYTE)(particles[i].alpha * 255),
                GetRValue(particles[i].color),
                GetGValue(particles[i].color),
                GetBValue(particles[i].color)
            ));

            graphics->FillEllipse(&brush,
                particles[i].x - particles[i].size / 2,
                particles[i].y - particles[i].size / 2,
                particles[i].size,
                particles[i].size);
        }
    }
}

// 绘制鼠标白色光晕（进一步优化性能）
void DrawMouseHalo(Graphics* graphics) {
    if (!g_mouseInWindow) return;

    // 光晕参数
    int r = GetRValue(HALO_COLOR);
    int g_val = GetGValue(HALO_COLOR);
    int b = GetBValue(HALO_COLOR);

    // 简化光晕渲染 - 只使用一个渐变路径
    GraphicsPath haloPath;
    haloPath.AddEllipse(
        g_mousePos.x - HALO_RADIUS,
        g_mousePos.y - HALO_RADIUS,
        HALO_RADIUS * 2,
        HALO_RADIUS * 2
    );

    // 光晕渐变
    PathGradientBrush haloBrush(&haloPath);
    Color centerColor = Color(HALO_MAX_ALPHA, r, g_val, b);
    Color surroundColor = Color(0, r, g_val, b);
    haloBrush.SetCenterColor(centerColor);
    haloBrush.SetSurroundColors(&surroundColor, NULL);

    // 优化渲染质量
    graphics->SetSmoothingMode(SmoothingModeAntiAlias);
    graphics->FillPath(&haloBrush, &haloPath);
}

// 更新时间文本
void UpdateTimeText() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    swprintf_s(g_timeText, L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
}

// 打开URL
void OpenUrl(const wchar_t* url) {
    ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOW);
}

// 获取彩虹渐变颜色
Color GetRainbowColor(float position) {
    position = fmod(position, 1.0f);

    float r = 0.0f, g = 0.0f, b = 0.0f;
    if (position < 1.0f / 6.0f) {
        r = 1.0f;
        g = position * 6.0f;
        b = 0.0f;
    }
    else if (position < 2.0f / 6.0f) {
        r = 1.0f - (position - 1.0f / 6.0f) * 6.0f;
        g = 1.0f;
        b = 0.0f;
    }
    else if (position < 3.0f / 6.0f) {
        r = 0.0f;
        g = 1.0f;
        b = (position - 2.0f / 6.0f) * 6.0f;
    }
    else if (position < 4.0f / 6.0f) {
        r = 0.0f;
        g = 1.0f - (position - 3.0f / 6.0f) * 6.0f;
        b = 1.0f;
    }
    else if (position < 5.0f / 6.0f) {
        r = (position - 4.0f / 6.0f) * 6.0f;
        g = 0.0f;
        b = 1.0f;
    }
    else {
        r = 1.0f;
        g = 0.0f;
        b = 1.0f - (position - 5.0f / 6.0f) * 6.0f;
    }

    return Color(255, (BYTE)(r * 255), (BYTE)(g * 255), (BYTE)(b * 255));
}

// 更新渐变位置
void UpdateGradientPosition() {
    g_gradientPos += GRADIENT_SPEED;
    if (g_gradientPos > 1.0f) {
        g_gradientPos -= 1.0f;
    }
}

// 初始化双缓冲
void InitBackBuffer(HWND hWnd) {
    DestroyBackBuffer();

    RECT clientRect;
    GetClientRect(hWnd, &clientRect);
    g_clientWidth = clientRect.right;
    g_clientHeight = clientRect.bottom;

    HDC hdc = GetDC(hWnd);
    g_backDC = CreateCompatibleDC(hdc);
    g_backBuffer = CreateCompatibleBitmap(hdc, g_clientWidth, g_clientHeight);
    SelectObject(g_backDC, g_backBuffer);
    ReleaseDC(hWnd, hdc);
}

// 销毁双缓冲
void DestroyBackBuffer() {
    if (g_backBuffer) {
        DeleteObject(g_backBuffer);
        g_backBuffer = NULL;
    }
    if (g_backDC) {
        DeleteDC(g_backDC);
        g_backDC = NULL;
    }
}

// 渲染优化函数
void OptimizeRendering(HWND hWnd) {
    // 预创建背景画刷
    if (g_backgroundBrush) {
        DeleteObject(g_backgroundBrush);
        g_backgroundBrush = NULL;
    }

    if (g_isGradientBackground) {
        // 渐变背景使用GDI+绘制
    }
    else {
        // 纯色背景使用GDI画刷
        g_backgroundBrush = CreateSolidBrush(
            g_isWhiteBlackTheme ? RGB(255, 255, 255) : RGB(30, 30, 30)
        );
    }

    // 预创建时间字体
    if (g_timeFont) {
        DeleteObject(g_timeFont);
        g_timeFont = NULL;
    }

    g_timeFont = CreateFontW(
        80, 0, 0, 0, FW_BOLD,
        FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"微软雅黑"
    );
}

// 渲染到双缓冲（修复临时对象取地址问题）
void Render(HWND hWnd) {
    if (!g_backDC || !g_backBuffer) return;

    RECT clientRect;
    GetClientRect(hWnd, &clientRect);

    // 创建Graphics对象（左值）
    Graphics graphics(g_backDC);

    // 优化背景绘制
    if (g_backgroundBrush && !g_isGradientBackground) {
        FillRect(g_backDC, &clientRect, g_backgroundBrush);
    }
    else {
        if (g_isGradientBackground) {
            LinearGradientBrush gradientBrush(
                Point(0, 0),
                Point(clientRect.right, clientRect.bottom),
                GetRainbowColor(g_gradientPos),
                GetRainbowColor(g_gradientPos + 0.5f)
            );
            graphics.FillRectangle(&gradientBrush, 0, 0, clientRect.right, clientRect.bottom);
        }
        else {
            SolidBrush solidBrush(g_isWhiteBlackTheme ?
                Color(255, 255, 255) :
                Color(30, 30, 30));
            graphics.FillRectangle(&solidBrush, 0, 0, clientRect.right, clientRect.bottom);
        }
    }

    // 优化粒子渲染
    UpdateParticles(mainParticles, PARTICLE_COUNT, clientRect.right, clientRect.bottom);
    DrawParticles(&graphics, mainParticles, PARTICLE_COUNT, false);

    // 优化光晕渲染 - 只在需要时绘制
    if (g_mouseInWindow) {
        DrawMouseHalo(&graphics);
    }
}

// 绘制时间文本
void PaintTime(HDC hdc) {
    if (!g_timeFont) return;

    HFONT hOldFont = (HFONT)SelectObject(hdc, g_timeFont);

    SetTextColor(hdc, g_isTimeColorful ?
        g_softYellowColors[rand() % 7] :
        (g_isWhiteBlackTheme ? TIME_DARK_COLOR : TIME_BASE_COLOR));
    SetBkMode(hdc, TRANSPARENT);
    DrawTextW(hdc, g_timeText, -1, &g_timeRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, hOldFont);
}

// 主窗口过程
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        CreateControls(hWnd);
        UpdateTimeText();
        SetTimer(hWnd, 1, 1000, nullptr);  // 时间更新计时器
        SetTimer(hWnd, 2, RENDER_INTERVAL, nullptr);  // 渲染计时器
        InitParticles(mainParticles, PARTICLE_COUNT, 800, 600);
        OptimizeRendering(hWnd);  // 初始化渲染优化
        break;

    case WM_SIZE: {
        RECT clientRect;
        GetClientRect(hWnd, &clientRect);

        g_timeRect.left = clientRect.right / 2 - 150;
        g_timeRect.top = clientRect.bottom / 2 - 50;
        g_timeRect.right = clientRect.right / 2 + 150;
        g_timeRect.bottom = clientRect.bottom / 2 + 50;

        if (g_hBilibiliBtn && g_hSettingsBtn) {
            int btnX = clientRect.right - BUTTON_RIGHT_MARGIN - 180;
            int btnY = BUTTON_TOP_MARGIN;
            MoveWindow(g_hBilibiliBtn, btnX, btnY, 180, 40, TRUE);

            btnX = clientRect.right - BUTTON_RIGHT_MARGIN - 80;
            btnY = BUTTON_TOP_MARGIN + 40 + BUTTON_VERTICAL_SPACING;
            MoveWindow(g_hSettingsBtn, btnX, btnY, 80, 30, TRUE);

            if (g_isSettingsVisible) {
                UpdateSettingsPanelPos(hWnd);
            }
        }

        InitBackBuffer(hWnd);
        Render(hWnd);
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        if (ps.fErase) {
            Render(hWnd);
        }

        if (g_backBuffer && g_backDC) {
            BitBlt(hdc, 0, 0, g_clientWidth, g_clientHeight, g_backDC, 0, 0, SRCCOPY);
        }

        PaintTime(hdc);

        EndPaint(hWnd, &ps);
        break;
    }

    case WM_MOUSEMOVE: {
        g_mousePos.x = LOWORD(lParam);
        g_mousePos.y = HIWORD(lParam);

        if (!g_mouseInWindow) {
            g_mouseInWindow = TRUE;
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        else {
            // 只刷新光晕区域（已缩小）
            RECT haloRect = {
                g_mousePos.x - HALO_RADIUS - 10,
                g_mousePos.y - HALO_RADIUS - 10,
                g_mousePos.x + HALO_RADIUS + 10,
                g_mousePos.y + HALO_RADIUS + 10
            };
            InvalidateRect(hWnd, &haloRect, FALSE);
        }

        TRACKMOUSEEVENT tme = { 0 };
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hWnd;
        TrackMouseEvent(&tme);
        break;
    }

    case WM_MOUSELEAVE: {
        g_mouseInWindow = FALSE;
        InvalidateRect(hWnd, nullptr, FALSE);
        break;
    }

    case WM_TIMER:
        if (wParam == 1) {
            UpdateTimeText();
            InvalidateRect(hWnd, &g_timeRect, FALSE);
        }
        else if (wParam == 2) {
            UpdateGradientPosition();
            RECT clientRect;
            GetClientRect(hWnd, &clientRect);
            ExcludeClipRect(GetDC(hWnd), g_timeRect.left, g_timeRect.top,
                g_timeRect.right, g_timeRect.bottom);
            Render(hWnd);
            InvalidateRect(hWnd, &clientRect, FALSE);
            ReleaseDC(hWnd, GetDC(hWnd));
        }
        break;

    case WM_COMMAND: {
        int wmId = LOWORD(wParam);
        switch (wmId) {
        case IDC_BILIBILI_BTN:
            OpenUrl(L"https://space.bilibili.com/3493139876678450?spm_id_from=333.1007.0.0");
            break;
        case IDC_SETTINGS_BTN:
            ToggleSettingsPanel(hWnd);
            break;
        case IDC_THEME_BTN:
            g_isTimeColorful = !g_isTimeColorful;
            g_isWhiteBlackTheme = !g_isWhiteBlackTheme;
            if (g_backgroundBrush) {
                DeleteObject(g_backgroundBrush);
                g_backgroundBrush = NULL;
            }
            InvalidateRect(hWnd, nullptr, FALSE);
            break;
        case IDC_GRADIENT_BTN:
            g_isGradientBackground = !g_isGradientBackground;
            if (g_backgroundBrush) {
                DeleteObject(g_backgroundBrush);
                g_backgroundBrush = NULL;
            }
            InvalidateRect(hWnd, nullptr, FALSE);
            break;
        }
        break;
    }

    case WM_DESTROY:
        KillTimer(hWnd, 1);
        KillTimer(hWnd, 2);
        DestroyBackBuffer();
        if (g_backgroundBrush) DeleteObject(g_backgroundBrush);
        if (g_timeFont) DeleteObject(g_timeFont);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}

// 启动屏窗口过程
LRESULT CALLBACK SplashWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_TIMER:
        if (wParam == 1) {
            DestroyWindow(hWnd);
            HWND hMainWnd = FindWindowW(L"MainWindow", nullptr);
            if (hMainWnd) ShowWindow(hMainWnd, SW_SHOW);
        }
        else if (wParam == 2) {
            RECT rc;
            GetClientRect(hWnd, &rc);
            UpdateParticles(splashParticles, 50, rc.right, rc.bottom);
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        Graphics graphics(hdc);
        DrawParticles(&graphics, splashParticles, 50, true);
        EndPaint(hWnd, &ps);
    } break;

    case WM_DESTROY:
        KillTimer(hWnd, 1);
        KillTimer(hWnd, 2);
        break;

    default:
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}

// 设置面板窗口过程
LRESULT CALLBACK SettingsPanelProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT clientRect;
        GetClientRect(hWnd, &clientRect);

        HBRUSH bgBrush = CreateSolidBrush(g_isWhiteBlackTheme ? RGB(240, 240, 240) : RGB(50, 50, 50));
        FillRect(hdc, &clientRect, bgBrush);
        DeleteObject(bgBrush);

        EndPaint(hWnd, &ps);
    } break;

    case WM_COMMAND:
        SendMessageW(GetParent(hWnd), WM_COMMAND, wParam, lParam);
        break;

    default:
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}