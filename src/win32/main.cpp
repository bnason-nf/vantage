// Copyright 2019 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

// This began life as Tutorial07 from MSDN.
// MS has copyrights on whatever subset of this code still somewhat looks like theirs.

#define _CRT_SECURE_NO_WARNINGS

#include "vantage.h"

#include <windowsx.h>

#include <algorithm>
#include <codecvt>
#include <locale>
#include <shlwapi.h>
#include <vector>

#include "resource.h"

#include <d3d11_1.h>
#include <directxmath.h>
#include <dxgi1_6.h>
#pragma comment(lib, "d3d11.lib")
using namespace DirectX;

#include "pixelShader.h"
#include "vertexShader.h"

#define VANTAGE_STYLE_WINDOWED (WS_OVERLAPPEDWINDOW | WS_VISIBLE)
#define VANTAGE_STYLE_FULLSCREEN (WS_SYSMENU | WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE)

// Main Vantage instance
static Vantage * vantage_;

// Win32 API
struct WindowPosition
{
    int x;
    int y;
    int w;
    int h;
};
static WindowPosition windowPos_;
static HINSTANCE hInstance_;
static HWND hwnd_;
static HMENU menu_;
static bool fullscreen_;

// D3D
struct ConstantBuffer
{
    XMMATRIX transform;
    XMFLOAT4 color;
    XMFLOAT4 texOffsetScale;
};
struct SimpleVertex
{
    XMFLOAT3 Pos;
    XMFLOAT2 Tex;
};
static D3D_DRIVER_TYPE driverType_;
static D3D_FEATURE_LEVEL featureLevel_;
static ID3D11Device * device_;
static ID3D11Device1 * device1_;
static ID3D11DeviceContext * context_;
static ID3D11DeviceContext1 * context1_;
static IDXGIAdapter1 * defaultAdapter_;
static IDXGISwapChain * swapChain_;
static IDXGISwapChain1 * swapChain1_;
static ID3D11RenderTargetView * renderTarget_;
static ID3D11VertexShader * vertexShader_;
static ID3D11PixelShader * pixelShader_;
static ID3D11InputLayout * vertexLayout_;
static ID3D11Buffer * constantBuffer_;
static ID3D11Buffer * vertexBuffer_;
static ID3D11Buffer * indexBuffer_;
static ID3D11SamplerState * sampler_;
static ID3D11BlendState * blend_;
static ID3D11ShaderResourceView * image_;
static ID3D11ShaderResourceView * font_;
static ID3D11ShaderResourceView * fill_;
static XMVECTORF32 backgroundColor = { { { 0.000000000f, 0.000000000f, 0.000000000f, 1.000000000f } } };

// Forward declarations
static void setMenuVisible(bool visible);
static bool createWindow();
static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
static unsigned int GetMonitorSDRWhiteLevel(HMONITOR monitor);
static unsigned int sdrWhiteLevel();
static void resizeSwapChain(bool initializing);
static HRESULT createDevice();
static void destroyDevice();
static void render();
static void checkHDR();
static void updateWindowPos();
static void loadAdjacentPaths(const char * filename);

static void setMenuVisible(bool visible)
{
    HMENU menu = NULL;
    if (visible) {
        menu = menu_;
    }
    SetMenu(hwnd_, menu);
}

static bool createWindow()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance_;
    wcex.hIcon = LoadIcon(hInstance_, (LPCTSTR)IDI_VANTAGE);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = "vantageWindowClass";
    wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_APPLICATION);
    if (!RegisterClassEx(&wcex))
        return false;

    hwnd_ = CreateWindow("vantageWindowClass",
                         VANTAGE_WINDOW_TITLE,
                         VANTAGE_STYLE_WINDOWED,
                         CW_USEDEFAULT,
                         CW_USEDEFAULT,
                         1280,
                         720,
                         nullptr,
                         nullptr,
                         hInstance_,
                         nullptr);
    if (!hwnd_)
        return false;

    ShowWindow(hwnd_, SW_SHOW);
    updateWindowPos();

    menu_ = LoadMenu(hInstance_, MAKEINTRESOURCE(IDC_VANTAGE));
    setMenuVisible(true);

    DragAcceptFiles(hwnd_, TRUE);
    return true;
}

static void updateWindowPos()
{
    if (fullscreen_) {
        HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi;
        mi.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(monitor, &mi);
        SetWindowLongPtr(hwnd_, GWL_STYLE, VANTAGE_STYLE_FULLSCREEN);
        MoveWindow(hwnd_,
                   mi.rcMonitor.left,
                   mi.rcMonitor.top,
                   mi.rcMonitor.right - mi.rcMonitor.left,
                   mi.rcMonitor.bottom - mi.rcMonitor.top,
                   TRUE);
    } else {
        SetWindowLongPtr(hwnd_, GWL_STYLE, VANTAGE_STYLE_WINDOWED);
        MoveWindow(hwnd_, windowPos_.x, windowPos_.y, windowPos_.w, windowPos_.h, TRUE);
    }
}

static void windowPosChanged(int x, int y, int w, int h)
{
    if (!fullscreen_) {
        windowPos_.x = x;
        windowPos_.y = y;
        windowPos_.w = w;
        windowPos_.h = h;
    }

    resizeSwapChain(false);
    checkHDR();
}

static void toggleFullscreen()
{
    fullscreen_ = !fullscreen_;
    updateWindowPos();
    setMenuVisible(!fullscreen_);
}

static const char * imageFileFilter =
    "All Image Files (*.avif, *.bmp, *.jpg, *.jpeg, *.jp2, *.j2k, *.png, *.tif, *.tiff, *.webp)\0*.avif;*.bmp;*.jpg;*.jpeg;*.jp2;*.j2k;*.png;*.tif;*.tiff;*.webp\0All Files (*.*)\0*.*\0";

static void fileOpen()
{
    char filename[MAX_PATH];

    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFile = filename;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof(filename);
    ofn.lpstrFilter = imageFileFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        loadAdjacentPaths(filename);
    }
}

static void diffOpen()
{
    if (daSize(&vantage_->filenames_) < 1) {
        return;
    }

    char filename[MAX_PATH];

    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFile = filename;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof(filename);
    ofn.lpstrFilter = imageFileFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        char * currentFilename = NULL;
        dsCopy(&currentFilename, vantage_->filenames_[vantage_->imageFileIndex_]);
        vantageLoadDiff(vantage_, currentFilename, filename);
        dsDestroy(&currentFilename);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;
    char filename1[MAX_PATH + 1];
    char filename2[MAX_PATH + 1];

    switch (message) {
        case WM_CHAR: {
            unsigned int key = (unsigned int)wParam;
            switch (key) {
                case 49: // 1
                    vantageSetDiffMode(vantage_, DIFFMODE_SHOW1);
                    break;
                case 50: // 2
                    vantageSetDiffMode(vantage_, DIFFMODE_SHOW2);
                    break;
                case 51: // 3
                    vantageSetDiffMode(vantage_, DIFFMODE_SHOWDIFF);
                    break;
                case 100: // D
                    diffOpen();
                    break;
                case 102: // F
                    toggleFullscreen();
                    break;
                case 111: // O
                    fileOpen();
                    break;
                case 113: // Q
                    PostQuitMessage(0);
                    break;
                case 114: // R
                    vantageResetImagePos(vantage_);
                    break;
                case 115: // S
                    vantageToggleSrgbHighlight(vantage_);
                    break;

                case 122: // Z
                    vantageSetDiffIntensity(vantage_, DIFFINTENSITY_ORIGINAL);
                    break;
                case 120: // X
                    vantageSetDiffIntensity(vantage_, DIFFINTENSITY_BRIGHT);
                    break;
                case 99: // C
                    vantageSetDiffIntensity(vantage_, DIFFINTENSITY_DIFFONLY);
                    break;

                case 32: // Space
                    vantageKickOverlay(vantage_);
                    break;
            }
            break;
        }

        case WM_KEYDOWN: {
            switch (wParam) {
                case VK_ESCAPE:
                    vantageKillOverlay(vantage_);
                    break;
                case VK_LEFT:
                    vantageLoad(vantage_, -1);
                    break;
                case VK_RIGHT:
                    vantageLoad(vantage_, 1);
                    break;
                case VK_F11:
                    toggleFullscreen();
                    break;

                case VK_UP:
                    vantageAdjustThreshold(vantage_, 1);
                    break;
                case VK_DOWN:
                    vantageAdjustThreshold(vantage_, -1);
                    break;
                case VK_DELETE:
                    vantageAdjustThreshold(vantage_, -5);
                    break;
                case VK_END:
                    vantageAdjustThreshold(vantage_, -50);
                    break;
                case VK_NEXT:
                    vantageAdjustThreshold(vantage_, -500);
                    break;
                case VK_INSERT:
                    vantageAdjustThreshold(vantage_, 5);
                    break;
                case VK_HOME:
                    vantageAdjustThreshold(vantage_, 50);
                    break;
                case VK_PRIOR:
                    vantageAdjustThreshold(vantage_, 500);
                    break;
            }
            break;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDM_EXIT:
                    PostQuitMessage(0);
                    break;
                case ID_VIEW_FULLSCREEN:
                    toggleFullscreen();
                    break;
                case ID_VIEW_RESETIMAGEPOSITION:
                    vantageResetImagePos(vantage_);
                    break;
                case ID_FILE_OPEN:
                    fileOpen();
                    break;

                case ID_VIEW_PREVIOUSIMAGE:
                    vantageLoad(vantage_, -1);
                    break;
                case ID_VIEW_NEXTIMAGE:
                    vantageLoad(vantage_, 1);
                    break;
                case ID_DIFF_DIFFCURRENTIMAGEAGAINST:
                    diffOpen();
                    break;

                case ID_VIEW_TOGGLESRGBHIGHLIGHT:
                    vantageToggleSrgbHighlight(vantage_);
                    break;

                case ID_DIFF_SHOWIMAGE1:
                    vantageSetDiffMode(vantage_, DIFFMODE_SHOW1);
                    break;
                case ID_DIFF_SHOWIMAGE2:
                    vantageSetDiffMode(vantage_, DIFFMODE_SHOW2);
                    break;
                case ID_DIFF_SHOWDIFF:
                    vantageSetDiffMode(vantage_, DIFFMODE_SHOWDIFF);
                    break;

                case ID_DIFF_DIFFINTENSITY_ORIGINAL:
                    vantageSetDiffIntensity(vantage_, DIFFINTENSITY_ORIGINAL);
                    break;
                case ID_DIFF_DIFFINTENSITY_BRIGHT:
                    vantageSetDiffIntensity(vantage_, DIFFINTENSITY_BRIGHT);
                    break;
                case ID_DIFF_DIFFINTENSITY_DIFFONLY:
                    vantageSetDiffIntensity(vantage_, DIFFINTENSITY_DIFFONLY);
                    break;

                case ID_DIFF_ADJUSTTHRESHOLDM1:
                    vantageAdjustThreshold(vantage_, -1);
                    break;
                case ID_DIFF_ADJUSTTHRESHOLDM5:
                    vantageAdjustThreshold(vantage_, -5);
                    break;
                case ID_DIFF_ADJUSTTHRESHOLDM50:
                    vantageAdjustThreshold(vantage_, -50);
                    break;
                case ID_DIFF_ADJUSTTHRESHOLDM500:
                    vantageAdjustThreshold(vantage_, -500);
                    break;

                case ID_DIFF_ADJUSTTHRESHOLD1:
                    vantageAdjustThreshold(vantage_, 1);
                    break;
                case ID_DIFF_ADJUSTTHRESHOLD5:
                    vantageAdjustThreshold(vantage_, 5);
                    break;
                case ID_DIFF_ADJUSTTHRESHOLD50:
                    vantageAdjustThreshold(vantage_, 50);
                    break;
                case ID_DIFF_ADJUSTTHRESHOLD500:
                    vantageAdjustThreshold(vantage_, 500);
                    break;
            }
            break;

        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            vantageMouseLeftDown(vantage_, x, y);
        } break;

        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            vantageMouseSetPos(vantage_, x, y);
        } break;

        case WM_LBUTTONDBLCLK: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            vantageMouseLeftDoubleClick(vantage_, x, y);
        } break;

        case WM_LBUTTONUP: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            vantageMouseLeftUp(vantage_, x, y);
        } break;

        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            vantageMouseMove(vantage_, x, y);
            if (wParam & MK_RBUTTON) {
                vantageMouseSetPos(vantage_, x, y);
            }
            if ((wParam & MK_RBUTTON) || vantage_->dragging_) {
                render();
            }
        } break;

        case WM_MOUSEWHEEL: {
            POINT p;
            p.x = GET_X_LPARAM(lParam);
            p.y = GET_Y_LPARAM(lParam);
            ScreenToClient(hwnd, &p);
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            vantageMouseWheel(vantage_, p.x, p.y, (float)delta / 240.0f);
        } break;

        case WM_DROPFILES: {
            filename1[0] = 0;
            filename2[0] = 0;

            HDROP drop = (HDROP)wParam;
            int dropCount = DragQueryFile(drop, 0xFFFFFFFF, filename1, MAX_PATH);
            if (dropCount > 1) {
                DragQueryFile(drop, 0, filename1, MAX_PATH);
                DragQueryFile(drop, 1, filename2, MAX_PATH);
                DragFinish(drop);
                if (filename1[0] && filename2[0]) {
                    vantageLoadDiff(vantage_, filename1, filename2);
                }
            } else {
                DragQueryFile(drop, 0, filename1, MAX_PATH);
                DragFinish(drop);
                if (filename1[0]) {
                    loadAdjacentPaths(filename1);
                }
            }
            break;
        }

        case WM_WINDOWPOSCHANGED: {
            WINDOWPOS * pos = (WINDOWPOS *)lParam;
            windowPosChanged(pos->x, pos->y, pos->cx, pos->cy);
            break;
        }

        case WM_PAINT:
            hdc = BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }

    return 0;
}

static bool GetPathInfo(HMONITOR monitor, DISPLAYCONFIG_PATH_INFO * path_info)
{
    LONG result;
    uint32_t num_path_array_elements = 0;
    uint32_t num_mode_info_array_elements = 0;
    std::vector<DISPLAYCONFIG_PATH_INFO> path_infos;
    std::vector<DISPLAYCONFIG_MODE_INFO> mode_infos;

    // Get the monitor name.
    MONITORINFOEXW view_info;
    view_info.cbSize = sizeof(view_info);
    if (!GetMonitorInfoW(monitor, &view_info))
        return false;

    // Get all path infos.
    do {
        if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &num_path_array_elements, &num_mode_info_array_elements) != ERROR_SUCCESS) {
            return false;
        }
        path_infos.resize(num_path_array_elements);
        mode_infos.resize(num_mode_info_array_elements);
        result = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,
                                    &num_path_array_elements,
                                    path_infos.data(),
                                    &num_mode_info_array_elements,
                                    mode_infos.data(),
                                    nullptr);
    } while (result == ERROR_INSUFFICIENT_BUFFER);

    // Iterate of the path infos and see if we find one with a matching name.
    if (result == ERROR_SUCCESS) {
        for (uint32_t p = 0; p < num_path_array_elements; p++) {
            DISPLAYCONFIG_SOURCE_DEVICE_NAME device_name;
            device_name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
            device_name.header.size = sizeof(device_name);
            device_name.header.adapterId = path_infos[p].sourceInfo.adapterId;
            device_name.header.id = path_infos[p].sourceInfo.id;
            if (DisplayConfigGetDeviceInfo(&device_name.header) == ERROR_SUCCESS) {
                if (wcscmp(view_info.szDevice, device_name.viewGdiDeviceName) == 0) {
                    *path_info = path_infos[p];
                    return true;
                }
            }
        }
    }
    return false;
}

static unsigned int GetMonitorSDRWhiteLevel(HMONITOR monitor)
{
    unsigned int ret = 80;
    DISPLAYCONFIG_PATH_INFO path_info = {};
    if (!GetPathInfo(monitor, &path_info))
        return ret;

    DISPLAYCONFIG_SDR_WHITE_LEVEL white_level = {};
    white_level.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL;
    white_level.header.size = sizeof(white_level);
    white_level.header.adapterId = path_info.targetInfo.adapterId;
    white_level.header.id = path_info.targetInfo.id;
    if (DisplayConfigGetDeviceInfo(&white_level.header) != ERROR_SUCCESS)
        return ret;
    ret = (unsigned int)white_level.SDRWhiteLevel * 80 / 1000;
    return ret;
}

static unsigned int sdrWhiteLevel()
{
    HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
    return GetMonitorSDRWhiteLevel(monitor);
}

static void resizeSwapChain(bool initializing)
{
    if (!swapChain_) {
        return;
    }

    if (renderTarget_) {
        renderTarget_->Release();
        renderTarget_ = nullptr;
    }
    context_->OMSetRenderTargets(1, &renderTarget_, nullptr);

    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    unsigned int width = max(clientRect.right, 1);
    unsigned int height = max(clientRect.bottom, 1);
    vantagePlatformSetSize(vantage_, (int)width, (int)height);

    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    swapChain_->GetDesc(&swapChainDesc);
    bool resizeSwapChain = (swapChainDesc.BufferDesc.Width != width) || (swapChainDesc.BufferDesc.Height != height);
    if (resizeSwapChain) {
        // char debugString[128];
        // sprintf(debugString, "ResizeBuffers: %dx%d\n", width, height);
        // OutputDebugString(debugString);

        swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    }

    // Create a render target view
    ID3D11Texture2D * pBackBuffer = nullptr;
    HRESULT hr = swapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&pBackBuffer));
    if (FAILED(hr)) {
        return;
    }

    device_->CreateRenderTargetView(pBackBuffer, nullptr, &renderTarget_);
    pBackBuffer->Release();

    // Setup the viewport
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    context_->RSSetViewports(1, &vp);

    if (!initializing && resizeSwapChain) {
        vantageResetImagePos(vantage_);
        render();
    }
}

static HRESULT createDevice()
{
    HRESULT hr = S_OK;

    RECT rc;
    GetClientRect(hwnd_, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_DRIVER_TYPE driverTypes[] = {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT numDriverTypes = ARRAYSIZE(driverTypes);

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    UINT numFeatureLevels = ARRAYSIZE(featureLevels);

    for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++) {
        driverType_ = driverTypes[driverTypeIndex];
        hr = D3D11CreateDevice(nullptr,
                               driverType_,
                               nullptr,
                               createDeviceFlags,
                               featureLevels,
                               numFeatureLevels,
                               D3D11_SDK_VERSION,
                               &device_,
                               &featureLevel_,
                               &context_);

        if (hr == E_INVALIDARG) {
            // DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
            hr = D3D11CreateDevice(nullptr,
                                   driverType_,
                                   nullptr,
                                   createDeviceFlags,
                                   &featureLevels[1],
                                   numFeatureLevels - 1,
                                   D3D11_SDK_VERSION,
                                   &device_,
                                   &featureLevel_,
                                   &context_);
        }

        if (SUCCEEDED(hr))
            break;
    }
    if (FAILED(hr)) {
        return hr;
    }
    // Obtain DXGI factory from device (since we used nullptr for pAdapter above)
    IDXGIFactory1 * dxgiFactory = nullptr;
    {
        IDXGIDevice * dxgiDevice = nullptr;
        hr = device_->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void **>(&dxgiDevice));
        if (SUCCEEDED(hr)) {
            IDXGIAdapter * adapter = nullptr;
            hr = dxgiDevice->GetAdapter(&adapter);
            if (SUCCEEDED(hr)) {
                hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&dxgiFactory));
                adapter->Release();
            }
            dxgiDevice->Release();
        }
    }
    if (FAILED(hr))
        return hr;

    if (FAILED(dxgiFactory->EnumAdapters1(0, &defaultAdapter_))) {
        return hr;
    }

    // Create swap chain
    IDXGIFactory2 * dxgiFactory2 = nullptr;
    hr = dxgiFactory->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void **>(&dxgiFactory2));
    if (dxgiFactory2) {
        // DirectX 11.1 or later
        hr = device_->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void **>(&device1_));
        if (SUCCEEDED(hr)) {
            (void)context_->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void **>(&context1_));
        }

        DXGI_SWAP_CHAIN_DESC1 sd;
        ZeroMemory(&sd, sizeof(sd));
        sd.Width = width;
        sd.Height = height;
        sd.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 2;

        hr = dxgiFactory2->CreateSwapChainForHwnd(device_, hwnd_, &sd, nullptr, nullptr, &swapChain1_);
        if (SUCCEEDED(hr)) {
            hr = swapChain1_->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void **>(&swapChain_));
        }

        dxgiFactory2->Release();
    } else {
        // DirectX 11.0 systems
        DXGI_SWAP_CHAIN_DESC sd;
        ZeroMemory(&sd, sizeof(sd));
        sd.BufferCount = 1;
        sd.BufferDesc.Width = width;
        sd.BufferDesc.Height = height;
        sd.BufferDesc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hwnd_;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;
        sd.BufferCount = 2;

        hr = dxgiFactory->CreateSwapChain(device_, &sd, &swapChain_);
    }

    if (FAILED(hr))
        return hr;

    // Note this tutorial doesn't handle full-screen swapchains so we block the ALT+ENTER shortcut
    dxgiFactory->MakeWindowAssociation(hwnd_, DXGI_MWA_NO_ALT_ENTER);
    dxgiFactory->Release();

    resizeSwapChain(true);

    // Create the vertex shader
    hr = device_->CreateVertexShader(g_VS, sizeof(g_VS), nullptr, &vertexShader_);
    if (FAILED(hr)) {
        return hr;
    }

    // Define the input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = ARRAYSIZE(layout);

    // Create the input layout
    hr = device_->CreateInputLayout(layout, numElements, g_VS, sizeof(g_VS), &vertexLayout_);
    if (FAILED(hr)) {
        return hr;
    }

    // Create constant buffer
    {
        D3D11_BUFFER_DESC bd;
        ZeroMemory(&bd, sizeof(bd));
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(ConstantBuffer);
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = 0;
        hr = device_->CreateBuffer(&bd, nullptr, &constantBuffer_);
        if (FAILED(hr))
            return hr;
    }

    // Create the pixel shader
    hr = device_->CreatePixelShader(g_PS, sizeof(g_PS), nullptr, &pixelShader_);
    if (FAILED(hr)) {
        return hr;
    }

    // Create vertex buffer
    SimpleVertex vertices[] = {
        { XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
        { XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3(1.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
    };

    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(SimpleVertex) * 4;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory(&InitData, sizeof(InitData));
    InitData.pSysMem = vertices;
    hr = device_->CreateBuffer(&bd, &InitData, &vertexBuffer_);
    if (FAILED(hr)) {
        return hr;
    }

    // Create index buffer
    // Create vertex buffer
    WORD indices[] = {
        0, 1, 2, 0, 2, 3,
    };

    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(WORD) * 6;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    InitData.pSysMem = indices;
    hr = device_->CreateBuffer(&bd, &InitData, &indexBuffer_);
    if (FAILED(hr)) {
        return hr;
    }

    D3D11_SAMPLER_DESC sampDesc;
    ZeroMemory(&sampDesc, sizeof(sampDesc));
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = device_->CreateSamplerState(&sampDesc, &sampler_);
    if (FAILED(hr)) {
        return hr;
    }

    D3D11_BLEND_DESC blendDesc;
    memset(&blendDesc, 0, sizeof(D3D11_BLEND_DESC));
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = device_->CreateBlendState(&blendDesc, &blend_);
    if (FAILED(hr)) {
        return hr;
    }

    {
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Width = 1;
        desc.Height = 1;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;

        uint32_t whitePixel = 0xffffffff;

        D3D11_SUBRESOURCE_DATA initData;
        ZeroMemory(&initData, sizeof(initData));
        initData.pSysMem = (const void *)&whitePixel;
        initData.SysMemPitch = 4;
        initData.SysMemSlicePitch = 4;

        ID3D11Texture2D * tex = nullptr;
        hr = device_->CreateTexture2D(&desc, &initData, &tex);
        if (SUCCEEDED(hr) && (tex != nullptr)) {
            D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
            memset(&SRVDesc, 0, sizeof(SRVDesc));
            SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            SRVDesc.Texture2D.MipLevels = 1;

            hr = device_->CreateShaderResourceView(tex, &SRVDesc, &fill_);
            if (FAILED(hr)) {
                fill_ = nullptr;
            }
            tex->Release();
        }
    }

    {
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Width = static_cast<UINT>(vantage_->imageFont_->width);
        desc.Height = static_cast<UINT>(vantage_->imageFont_->height);
        desc.MipLevels = static_cast<UINT>(1);
        desc.ArraySize = static_cast<UINT>(1);
        desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;

        D3D11_SUBRESOURCE_DATA initData;
        ZeroMemory(&initData, sizeof(initData));
        initData.pSysMem = (const void *)vantage_->imageFont_->pixels;
        initData.SysMemPitch = vantage_->imageFont_->width * 4 * 2;
        initData.SysMemSlicePitch = static_cast<UINT>(vantage_->imageFont_->width * vantage_->imageFont_->height * 4 * 2);

        ID3D11Texture2D * tex = NULL;
        hr = device_->CreateTexture2D(&desc, &initData, &tex);
        if (SUCCEEDED(hr) && (tex != NULL)) {
            D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
            memset(&SRVDesc, 0, sizeof(SRVDesc));
            SRVDesc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            SRVDesc.Texture2D.MipLevels = 1;

            hr = device_->CreateShaderResourceView(tex, &SRVDesc, &font_);
            if (FAILED(hr)) {
                font_ = nullptr;
            }
        }
        tex->Release();
    }

    return S_OK;
}

static void destroyDevice()
{
    if (context_) {
        context_->ClearState();
        context_ = nullptr;
    }

    if (image_) {
        image_->Release();
        image_ = nullptr;
    }

    if (font_) {
        font_->Release();
        font_ = nullptr;
    }

    if (fill_) {
        fill_->Release();
        fill_ = nullptr;
    }

    if (sampler_) {
        sampler_->Release();
        sampler_ = nullptr;
    }
    if (blend_) {
        blend_->Release();
        blend_ = nullptr;
    }

    if (vertexBuffer_) {
        vertexBuffer_->Release();
        vertexBuffer_ = nullptr;
    }
    if (indexBuffer_) {
        indexBuffer_->Release();
        indexBuffer_ = nullptr;
    }
    if (vertexLayout_) {
        vertexLayout_->Release();
        vertexLayout_ = nullptr;
    }
    if (vertexShader_) {
        vertexShader_->Release();
        vertexShader_ = nullptr;
    }
    if (pixelShader_) {
        pixelShader_->Release();
        pixelShader_ = nullptr;
    }
    if (renderTarget_) {
        renderTarget_->Release();
        renderTarget_ = nullptr;
    }
    if (swapChain1_) {
        swapChain1_->Release();
        swapChain1_ = nullptr;
    }
    if (swapChain_) {
        swapChain_->Release();
        swapChain_ = nullptr;
    }
    if (defaultAdapter_) {
        defaultAdapter_->Release();
        defaultAdapter_ = nullptr;
    }
    if (context1_) {
        context1_->Release();
        context1_ = nullptr;
    }
    if (context_) {
        context_->Release();
        context_ = nullptr;
    }
    if (device1_) {
        device1_->Release();
        device1_ = nullptr;
    }
    if (device_) {
        device_->Release();
        device_ = nullptr;
    }
}

static void render()
{
    vantagePlatformSetWhiteLevel(vantage_, (int)sdrWhiteLevel());
    vantageRender(vantage_);

    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    float clientW = (float)clientRect.right;
    float clientH = (float)clientRect.bottom;
    bool showHLG = false;

    if (vantage_->imageDirty_) {
        if (image_) {
            image_->Release();
            image_ = NULL;
        }

        if (vantage_->preparedImage_) {
            D3D11_TEXTURE2D_DESC desc;
            ZeroMemory(&desc, sizeof(desc));
            desc.Width = static_cast<UINT>(vantage_->preparedImage_->width);
            desc.Height = static_cast<UINT>(vantage_->preparedImage_->height);
            desc.MipLevels = static_cast<UINT>(1);
            desc.ArraySize = static_cast<UINT>(1);
            desc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            desc.CPUAccessFlags = 0;

            D3D11_SUBRESOURCE_DATA initData;
            ZeroMemory(&initData, sizeof(initData));
            initData.pSysMem = (const void *)vantage_->preparedImage_->pixels;
            initData.SysMemPitch = vantage_->preparedImage_->width * 4 * 2;
            initData.SysMemSlicePitch = static_cast<UINT>(vantage_->preparedImage_->width * vantage_->preparedImage_->height * 4 * 2);

            ID3D11Texture2D * tex = NULL;
            HRESULT hr = device_->CreateTexture2D(&desc, &initData, &tex);
            if (SUCCEEDED(hr) && (tex != NULL)) {
                D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
                memset(&SRVDesc, 0, sizeof(SRVDesc));
                SRVDesc.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
                SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                SRVDesc.Texture2D.MipLevels = 1;

                hr = device_->CreateShaderResourceView(tex, &SRVDesc, &image_);
                if (FAILED(hr)) {
                    tex->Release();
                    image_ = NULL;
                    vantageUnload(vantage_);
                    return;
                }
            }
            tex->Release();
        }
    }

    context_->OMSetRenderTargets(1, &renderTarget_, nullptr);
    context_->ClearRenderTargetView(renderTarget_, backgroundColor);

    int blitCount = daSize(&vantage_->blits_);
    for (int blitIndex = 0; blitIndex < blitCount; ++blitIndex) {
        Blit * blit = &vantage_->blits_[blitIndex];
        if (blit->mode == BM_IMAGE) {
            if (!vantage_->preparedImage_ || !image_ || (clientW <= 0.0f) || (clientH <= 0.0f)) {
                continue;
            }
        }

        ConstantBuffer cb;
        cb.transform = XMMatrixIdentity();
        cb.transform *= XMMatrixScaling(blit->dw, blit->dh, 1.0f);
        cb.transform *= XMMatrixTranslation(blit->dx, blit->dy, 0.0f);
        cb.transform *= XMMatrixOrthographicOffCenterRH(0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 1.0f);
        cb.color = XMFLOAT4(blit->color.r, blit->color.g, blit->color.b, blit->color.a);
        cb.texOffsetScale = XMFLOAT4(blit->sx, blit->sy, blit->sw, blit->sh);
        context_->UpdateSubresource(constantBuffer_, 0, nullptr, &cb, 0, 0);

        UINT stride = sizeof(SimpleVertex);
        UINT offset = 0;
        context_->IASetVertexBuffers(0, 1, &vertexBuffer_, &stride, &offset);
        context_->IASetInputLayout(vertexLayout_);
        context_->IASetIndexBuffer(indexBuffer_, DXGI_FORMAT_R16_UINT, 0);
        context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        context_->VSSetShader(vertexShader_, nullptr, 0);
        context_->VSSetConstantBuffers(0, 1, &constantBuffer_);
        context_->PSSetShader(pixelShader_, nullptr, 0);
        context_->PSSetConstantBuffers(0, 1, &constantBuffer_);
        switch (blit->mode) {
            case BM_IMAGE:
                context_->PSSetShaderResources(0, 1, &image_);
                break;
            case BM_TEXT:
                context_->PSSetShaderResources(0, 1, &font_);
                break;
            case BM_FILL:
                context_->PSSetShaderResources(0, 1, &fill_);
                break;
        }
        context_->PSSetSamplers(0, 1, &sampler_);
        context_->OMSetBlendState(blend_, NULL, 0xffffffff);
        context_->DrawIndexed(6, 0, 0);
    }
    swapChain_->Present(1, 0);
}

static void checkHDR()
{
    int hdrWasActive = vantage_->platformHDRActive_;

    int hdrActive = 0;
    if (defaultAdapter_) {
        // Find out which monitor we're mostly overlapping
        HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi;
        mi.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(monitor, &mi);

        // Now find that monitor's rect in our outputs list and check against its color space
        IDXGIOutput * output;
        for (UINT outputIndex = 0; defaultAdapter_->EnumOutputs(outputIndex, &output) != DXGI_ERROR_NOT_FOUND; ++outputIndex) {
            IDXGIOutput6 * output6;
            output->QueryInterface(&output6);
            DXGI_OUTPUT_DESC1 desc;
            output6->GetDesc1(&desc);
            output6->Release();
            output->Release();

            if (!memcmp(&mi.rcMonitor, &desc.DesktopCoordinates, sizeof(mi.rcMonitor))) {
                if (desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020) {
                    hdrActive = 1;
                }
            }
        }

        IDXGISwapChain3 * swapChain3 = nullptr;
        HRESULT hr = swapChain_->QueryInterface(IID_PPV_ARGS(&swapChain3));
        if (SUCCEEDED(hr)) {
            DXGI_COLOR_SPACE_TYPE colorSpaceType = (vantage_->platformHDRActive_) ? DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020
                                                                                  : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
            swapChain3->SetColorSpace1(colorSpaceType);
            swapChain3->Release();
        }
    }

    vantagePlatformSetHDRActive(vantage_, hdrActive);
}

static bool sortAlphabeticallyIgnoringCase(const std::string & a, const std::string & b)
{
    return _stricmp(a.c_str(), b.c_str()) < 0;
}

static void loadAdjacentPaths(const char * filename)
{
    std::vector<std::string> imageList;

    char fullFilename[MAX_PATH];
    GetFullPathNameA(filename, MAX_PATH, fullFilename, nullptr);
    char directory[MAX_PATH];
    memcpy(directory, fullFilename, MAX_PATH);
    PathRemoveFileSpec(directory);
    std::string wildcard = directory;
    wildcard += "\\*";

    WIN32_FIND_DATA wfd;
    HANDLE hFind = FindFirstFile(wildcard.c_str(), &wfd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (wfd.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE)) {
                continue;
            }
            std::string foundFilenameStr = directory;
            foundFilenameStr += "\\";
            foundFilenameStr += wfd.cFileName;
            char foundFilename[MAX_PATH];
            GetFullPathNameA(foundFilenameStr.c_str(), MAX_PATH, foundFilename, nullptr);
            if (vantageIsImageFile(foundFilename)) {
                imageList.push_back(foundFilename);
            }
        } while (FindNextFile(hFind, &wfd));
    }

    std::sort(imageList.begin(), imageList.end(), sortAlphabeticallyIgnoringCase);

    vantageFileListClear(vantage_);

    int requestedFilenameIndex = -1;
    int index = 0;
    for (auto it = imageList.begin(); it != imageList.end(); ++it, ++index) {
        vantageFileListAppend(vantage_, it->c_str());
        if (!strcmp(it->c_str(), fullFilename)) {
            requestedFilenameIndex = index;
        }
    }

    if (requestedFilenameIndex == -1) {
        // Somehow the original file wasn't found, tack it onto the end of the list
        imageList.push_back(fullFilename);
        requestedFilenameIndex = (int)imageList.size() - 1;
    }

    vantage_->imageFileIndex_ = requestedFilenameIndex;
    vantageLoad(vantage_, 0);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    hInstance_ = hInstance;

    int numArgs = 0;
    LPWSTR * argv = NULL;
    if (lpCmdLine[0]) {
        argv = CommandLineToArgvW(lpCmdLine, &numArgs);
    }

    std::wstring cmdLine = lpCmdLine;
    std::string filename1, filename2;
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;

    vantage_ = vantageCreate();

    if (numArgs > 1) {
        // Diff!
        filename1 = converter.to_bytes(argv[0]);
        filename2 = converter.to_bytes(argv[1]);
        vantageLoadDiff(vantage_, filename1.c_str(), filename2.c_str());
    } else if (numArgs > 0) {
        // Single filename
        filename1 = converter.to_bytes(argv[0]);
        loadAdjacentPaths(filename1.c_str());
    }

    if (!createWindow()) {
        return 0;
    }
    if (createDevice() != S_OK) {
        return 0;
    }

    MSG msg = { 0 };
    while (WM_QUIT != msg.message) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            render();
        }
    }

    destroyDevice();
    vantageDestroy(vantage_);
    return 0;
}