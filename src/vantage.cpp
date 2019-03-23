#include "vantage.h"

Vantage::Vantage(HINSTANCE hInstance)
    : hInstance_(hInstance)
    , fullscreen_(false)
    , hdrActive_(false)
    , hdrImage_(false)
    , coloristContext_(nullptr)
    , coloristImage_(nullptr)

    , driverType_(D3D_DRIVER_TYPE_NULL)
    , featureLevel_(D3D_FEATURE_LEVEL_11_0)
    , device_(nullptr)
    , device1_(nullptr)
    , context_(nullptr)
    , context1_(nullptr)
    , defaultAdapter_(nullptr)
    , swapChain_(nullptr)
    , swapChain1_(nullptr)
    , renderTarget_(nullptr)
    , vertexShader_(nullptr)
    , pixelShader_(nullptr)
    , vertexLayout_(nullptr)
    , constantBuffer_(nullptr)
    , vertexBuffer_(nullptr)
    , indexBuffer_(nullptr)
    , sampler_(nullptr)
    , image_(nullptr)
{
    // TODO: read from / write to disk
    windowPos_.x = 0;
    windowPos_.y = 0;
    windowPos_.w = 1280;
    windowPos_.h = 720;

    coloristContext_ = clContextCreate(nullptr);
}

Vantage::~Vantage()
{
    unloadImage();
    clContextDestroy(coloristContext_);
}

void Vantage::cleanup()
{
    destroyDevice();
}

bool Vantage::init()
{
    if (!createWindow()) {
        return false;
    }
    if (createDevice() != S_OK) {
        return false;
    }
    checkHDR();

    // DEBUG, REMOVE ME
    loadImage("C:\\Users\\jdrago\\Desktop\\newpq\\HDR10.bmp");

    return true;
}
