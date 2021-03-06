// Extensions
#include "Engine/Extensions/Default.h"
#include "Engine/Input/Gamepad.h"

#pragma region Engine includes
// Core engine includes
#include "Engine Includes/Core.h"
#include "Engine Includes/Models.h"
#include "Engine Includes/RenderBuffers.h"
#include "Engine Includes/Textures.h"

// Audio engine using XAudio2
#include "Engine Includes/Audio.h"

// Text engine
#include "Engine Includes/Text.h"

// External
#include "Engine Includes/ImGui.h"

// Global game instances
static _DirectX      *gDirectX     = 0;
static Window        *gWindow      = 0;
static Input         *gInput       = 0;
static Mouse         *gMouse       = 0;
static Keyboard      *gKeyboard    = 0;
static AudioDevice   *gAudioDevice = 0;

#if USE_GAMEPADS
static Gamepad* gGamepad[NUM_GAMEPAD] = {};
#endif
#pragma endregion

#pragma region Heap allocated instances
// Text
Text *tTest;
Font *fRegular;
TextFactory *gTextFactory;
TextController *gTextController;

// Cameras
Camera *cPlayer, *c2DScreen;

// Shaders
Shader *shGUI, *shTexturedQuad, *shPostProcess, *shParticles3D,
       *shTextSimple, *shTextEffects, *shTextSimpleSDF, *shTextEffectsSDF;

// Models


// Textures and materials
Texture *tDefault, *tBlueNoiseRG, *tClearPixel, *tOpacityDefault, *tSpecularDefault, *tGradient;
DiffuseMap *mDefaultDiffuse;
Sampler *sPoint, *sMipLinear, *sPointClamp, *sMipLinearOpacity, *sMipLinearRougness;

Material *mDefault;

// Render Targets


// States
BlendState *pBlendState0, *pBlendStateAdditive;
RasterState *rsFrontCull;
//ID3D11RasterizerState *rsFrontCull;

// Viewports
D3D11_VIEWPORT vpMain;

// 
struct Particle {
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Velocity;
};

struct DataBuffer {
    INT _GroupDim;
    UINT _ParticleNum;
    FLOAT _DeltaTime;
    FLOAT _Dummy;
};

ConstantBuffer *cbDataBuffer;
StructuredBuffer<Particle> *sbParticles;

const int _Num = 100000;
#pragma endregion

float fAspect = 1024.f / 540.f;
bool bIsWireframe = false, bDebugGUI = false, bLookMouse = true, bPause = false;
int SceneID = 0;
int sfxWalkIndex;

// Define Frame Function
bool _DirectX::FrameFunction() {
    // Resize event
    Resize();
    
#pragma region Scene rendering
    auto RenderScene = [&](Camera *cam, uint32_t flags=RendererFlags::None) {
        // 
        
    };
#pragma endregion

    // Bind and clear RTV
    gContext->OMSetRenderTargets(1, &gRTV, gDSV);

    float Clear[4] = { .2f, .2f, .2f, 1.f }; // RGBA
    float Clear0[4] = { 0.f, 0.f, 0.f, 1.f }; // RGBA black
    gContext->ClearRenderTargetView(gRTV, Clear0);
    gContext->ClearDepthStencilView(gDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);

    // Render scene and every thing else here
    {
        cPlayer->BuildView();
    }

    // Reset to defaults
    if( bIsWireframe ) {
        gContext->RSSetState(gRSDefaultWriteframe);
    } else {
        gContext->RSSetState(gRSDefault);
    }

    // Particle test
    shParticles3D->Bind();
    pBlendStateAdditive->Bind();
    gContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

    cPlayer->BuildConstantBuffer();
    cPlayer->BindBuffer(Shader::Vertex, 0);
    cPlayer->BindBuffer(Shader::Geometry, 0);
    
    sbParticles->Bind(Shader::Vertex, 0, false);

    tGradient->Bind(Shader::Pixel, 0);
    sPoint->Bind(Shader::Pixel, 0);

    // Draw call for particles
    gContext->Draw(sbParticles->GetNumber(), 0);

    // Set defaults
    pBlendState0->Bind();
    gContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    gContext->RSSetState(gRSDefault);

    // 2D Rendering
    ComposeUI();

    // End of frame
    gSwapchain->Present(1, 0);
    return false;
}

float fSpeed = 20.f, fRotSpeed = 100.f, fSensetivityX = 2.f, fSensetivityY = 3.f;
float fDir = 0.f, fPitch = 0.f;
void _DirectX::Tick(float fDeltaTime) {
    const WindowConfig& winCFG = gWindow->GetCFG();

    // Integrate particles
    if( !bPause ) {
        shParticles3D->Bind();

        const UINT dimX = 32;
        const UINT dimY = 24;
        const UINT dim = dimX * dimY;

        const UINT NumGroups = (_Num % dim != 0) ? ((_Num / dim) + 1) : (_Num / dim);
        double sqroot = ceil(sqrt(double(NumGroups)));

        UINT gx = sqroot;
        UINT gy = sqroot;

        DataBuffer *dbInst = (DataBuffer*)cbDataBuffer->Map();
            dbInst->_DeltaTime = fDeltaTime * (2.f + 18.f * gKeyboard->IsDown(VK_F));
            dbInst->_GroupDim = gx;
            dbInst->_ParticleNum = _Num;
        cbDataBuffer->Unmap();

        cbDataBuffer->Bind(Shader::Compute, 0);
        sbParticles->Bind(Shader::Compute, 0, true);

        // Dispatch
        shParticles3D->Dispatch(gx, gy, 1);

        // Unbind UAVs
        ID3D11UnorderedAccessView *pEmpty = nullptr;
        gContext->CSSetUnorderedAccessViews(0, 1, &pEmpty, 0);
    }

    // Update GUI elements
    tTest = gTextFactory->Build(tTest, (std::string("FPS: ") + std::to_string(1.f / fDeltaTime)).c_str());

    // Toggle debug
    bIsWireframe ^= gKeyboard->IsPressed(VK_F1); // Toggle wireframe mode
    bDebugGUI    ^= gKeyboard->IsPressed(VK_F3); // Toggle debug gui mode
    bLookMouse   ^= gKeyboard->IsPressed(VK_F2); // Toggle release mouse
    bPause       ^= gKeyboard->IsPressed(VK_F4); // Toggle world pause

    // Update camera
    DirectX::XMFLOAT3 f3Move(0.f, 0.f, 0.f); // Movement vector
    static DirectX::XMFLOAT2 pLastPos = {0, 0}; // Last mouse pos

    // Camera
    if( gKeyboard->IsDown(VK_W) ) f3Move.x = +fSpeed * fDeltaTime;  // Forward / Backward
    if( gKeyboard->IsDown(VK_S) ) f3Move.x = -fSpeed * fDeltaTime;
    if( gKeyboard->IsDown(VK_D) ) f3Move.z = +fSpeed * fDeltaTime;  // Strafe
    if( gKeyboard->IsDown(VK_A) ) f3Move.z = -fSpeed * fDeltaTime;

    if( !bLookMouse ) { return; }
    float dx = 0.f, dy = 0.f;
    
#if USE_GAMEPADS
    // Use gamepad if we can and it's connected
    if( gGamepad[0]->IsConnected() ) {
        // Move around
        if( !gGamepad[0]->IsDeadZoneL() ) {
            f3Move.x = gGamepad[0]->LeftY() * fSpeed * fDeltaTime;
            f3Move.z = gGamepad[0]->LeftX() * fSpeed * fDeltaTime;
        }
    }
#endif
    {
        // Use mouse
        bool b = false;
        if( abs(dx) <= .1 ) {
            fDir += (float(gMouse->GetX() - winCFG.CurrentWidth * .5f) * fSensetivityX * fDeltaTime);
            b = true;
        }

        if( abs(dy) <= .1 ) {
            fPitch += (float(gMouse->GetY() - winCFG.CurrentHeight * .5f) * fSensetivityY * fDeltaTime);
            b = true;
            //std::cout << winCFG.CurrentHeight2 * .5f << " " << gMouse->GetY() << std::endl;
        }

        if( b ) gMouse->SetAt(int(winCFG.CurrentWidth * .5f), int(winCFG.CurrentHeight * .5f));
    }

    if( gKeyboard->IsDown(VK_LEFT) ) fDir -= fRotSpeed * fDeltaTime; // Right / Left 
    if( gKeyboard->IsDown(VK_RIGHT) ) fDir += fRotSpeed * fDeltaTime;

    // I got used to KSP and other avia/space sims
    // So i flipped them
    if( gKeyboard->IsDown(VK_UP) ) fPitch -= fRotSpeed * fDeltaTime; // Look Up / Down
    if( gKeyboard->IsDown(VK_DOWN) ) fPitch += fRotSpeed * fDeltaTime;

    // Limit pitch
    fPitch = std::min(std::max(fPitch, -84.f), 84.f);

    // Look around
    cPlayer->TranslateLookAt(f3Move);
    cPlayer->RotateAbs(DirectX::XMFLOAT3(fPitch, fDir, 0.));
}

void _DirectX::ComposeUI() {
#if _DEBUG_BUILD
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();

    ImGui::NewFrame();

    // Create debug window
    ImGui::Begin("Debug");
    
    ImGui::Button("Test button");

    // Console
    static std::string buf;
    if( buf.size() == 0 ) {
        buf.resize(50, ' ');
    }

    ImGui::InputText("", (char*)buf.data(), buf.size());
    ImGui::SameLine();
    if( ImGui::Button("Submit") ) {
        std::cout << buf << std::endl;
        // gDebugObject->ConsoleParse(buf);
    }

    bIsWireframe ^= ImGui::Button("Wireframe");
    bDebugGUI    ^= ImGui::Button("Debug buffers");



    ImGui::End();
    ImGui::Render();

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
#endif

    // Set blendmode
    pBlendState0->Bind();

    // Draw text
    gTextController->Draw(tTest);
}

void _DirectX::Resize() {
    const WindowConfig& cfg = gWindow->GetCFG();
    if( !cfg.Resized ) { return; }                                     // Window isn't resized
    if( cfg.CurrentWidth <= 0 && cfg.CurrentHeight2 <= 0 ) { return;  } // Window was minimazed

    std::cout << "Window/DirectX resize event (w=" << cfg.CurrentWidth << ", h=" << cfg.CurrentHeight << ")" << std::endl;

    // Release targets
    gContext->OMSetRenderTargets(0, 0, 0);
    gRTV->Release();

    // Resize buffers


    // Resize text port
    gTextController->SetSize(cfg.CurrentWidth, cfg.CurrentHeight);

    // Resize swapchain
    scd.Width = (UINT)cfg.CurrentWidth;
    scd.Height = (UINT)cfg.CurrentHeight;

    DXGI_MODE_DESC pModeDesc = {};
    pModeDesc.Format = scd.Format;
    pModeDesc.Width = scd.Width;
    pModeDesc.Height = scd.Height;
    pModeDesc.RefreshRate = pSCFDesc.RefreshRate;
    pModeDesc.Scaling = pSCFDesc.Scaling;
    pModeDesc.ScanlineOrdering = pSCFDesc.ScanlineOrdering;
    //pModeDesc.Stereo = scd.Stereo;

    gSwapchain->ResizeTarget(&pModeDesc);

    std::cout << (gSwapchain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0) == S_OK) << std::endl;

    // Create RTV
    ID3D11Texture2D *BackBufferColor;
    gSwapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&BackBufferColor);

    std::cout << (gDevice->CreateRenderTargetView(BackBufferColor, NULL, &gRTV) == S_OK) << std::endl;
    BackBufferColor->Release(); // ?

    // Create depth texture
    pTex2DDesc.Width  = cfg.CurrentWidth;
    pTex2DDesc.Height = cfg.CurrentHeight;
    gDSVTex->Release();
    gDSV->Release();
    std::cout << (gDevice->CreateTexture2D(&pTex2DDesc, NULL, &gDSVTex) == S_OK) << std::endl;

    // Create depth stencil view
    std::cout << (gDevice->CreateDepthStencilView(gDSVTex, &pDesc2, &gDSV) == S_OK) << std::endl;

    // Bind default targets
    gContext->OMSetRenderTargets(1, &gRTV, gDSV);

    // Recalculate camer's aspect ratio and projection matrix
    CameraConfig cfg2 = cPlayer->GetParams();
    cfg2.fAspect = float(cfg.CurrentWidth) / float(cfg.CurrentHeight);
    cPlayer->BuildProj();

    // Save aspect
    fAspect = cfg2.fAspect;

    cfg2 = c2DScreen->GetParams();
    cfg2.fAspect = float(cfg.CurrentWidth) / float(cfg.CurrentHeight);
    c2DScreen->BuildProj();

    // Set up the viewport
    D3D11_VIEWPORT vp;
    vp.Width = static_cast<float>(cfg.CurrentWidth);
    vp.Height = static_cast<float>(cfg.CurrentHeight);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    gContext->RSSetViewports(1, &vp);

    vpMain.MinDepth = 0.f;
    vpMain.MaxDepth = 1.f;
    vpMain.TopLeftX = 0;
    vpMain.TopLeftY = 0;
    vpMain.Width    = vp.Width;
    vpMain.Height   = vp.Height;
}

void _DirectX::Load() {
    // Temp Bug fix
    gKeyboard->SetState(VK_W, false);
    gKeyboard->SetState(VK_S, false);
    gKeyboard->SetState(VK_UP, false);
    gKeyboard->SetState(VK_DOWN, false);
    gKeyboard->SetState(VK_LEFT, false);
    gKeyboard->SetState(VK_RIGHT, false);

#pragma region Create instances
    shGUI                    = new Shader();
    shTexturedQuad           = new Shader();
    shPostProcess            = new Shader();
    shTextSimple             = new Shader();
    shTextEffects            = new Shader();
    shTextSimpleSDF          = new Shader();
    shTextEffectsSDF         = new Shader();
    shParticles3D            = new Shader();

    cPlayer   = new Camera();
    c2DScreen = new Camera();

    tDefault         = new Texture();
    tBlueNoiseRG     = new Texture();
    tClearPixel      = new Texture();
    tOpacityDefault  = new Texture();
    tSpecularDefault = new Texture();
    tGradient        = new Texture();

    sPoint             = new Sampler();
    sMipLinear         = new Sampler();
    sPointClamp        = new Sampler();
    sMipLinearOpacity  = new Sampler();
    sMipLinearRougness = new Sampler();

    mDefaultDiffuse = new DiffuseMap();

    mDefault = new Material();

    sbParticles = new StructuredBuffer<Particle>();
#pragma endregion

    const WindowConfig& cfg = gWindow->GetCFG();

#pragma region Create states
    rsFrontCull = new RasterState();
    D3D11_RASTERIZER_DESC rDesc2;
    ZeroMemory(&rDesc2, sizeof(D3D11_RASTERIZER_DESC));
    rDesc2.AntialiasedLineEnable = false;
    rDesc2.CullMode = D3D11_CULL_NONE;
    rDesc2.DepthBias = 0;
    rDesc2.DepthBiasClamp = 0.0f;
    rDesc2.DepthClipEnable = true;
    rDesc2.FillMode = D3D11_FILL_SOLID;
    rDesc2.FrontCounterClockwise = false;
    rDesc2.MultisampleEnable = false;
    rDesc2.ScissorEnable = false;
    rDesc2.SlopeScaledDepthBias = 0.0f;
    
    rsFrontCull->Create(rDesc2);
    
    // Create blend state with no color write
    pBlendState0 = new BlendState();

    D3D11_BLEND_DESC pDesc_;
    pDesc_.RenderTarget[0].BlendEnable = true;
    pDesc_.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    pDesc_.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    pDesc_.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    pDesc_.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    pDesc_.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    pDesc_.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
    pDesc_.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    pDesc_.AlphaToCoverageEnable = false;
    pDesc_.IndependentBlendEnable = false;

    pBlendState0->Create(pDesc_, { 1.f, 1.f, 1.f, 1.f }, 0xFFFFFFFF);

    pBlendStateAdditive = new BlendState();
    pDesc_.RenderTarget[0].BlendEnable = true;
    pDesc_.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    pDesc_.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    pDesc_.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    pDesc_.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    pDesc_.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    pDesc_.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    pDesc_.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    pDesc_.AlphaToCoverageEnable = false;
    pDesc_.IndependentBlendEnable = false;

    pBlendStateAdditive->Create(pDesc_, { 1.f, 1.f, 1.f, 1.f }, 0xFFFFFFFF);
#pragma endregion

#pragma region Load textures
    // Create default texture
    tDefault->Load("../Textures/TileInverse.png", DXGI_FORMAT_R8G8B8A8_UNORM);
    tDefault->SetName("Default tile texture");
    //gContext->GenerateMips(tDefault->GetSRV());

    // Load more textures
    tBlueNoiseRG->Load("../Textures/Noise/Blue/LDR_RG01_0.png", DXGI_FORMAT_R16G16_UNORM);

    //tClearPixel->Load("../Textures/ClearPixel.png", DXGI_FORMAT_R8G8B8A8_UNORM);
    tOpacityDefault->Load("../Textures/ClearPixel.png", DXGI_FORMAT_R8G8B8A8_UNORM);
    tSpecularDefault->Load("../Textures/DarkPixel.png", DXGI_FORMAT_R8G8B8A8_UNORM);

    tGradient->Load("../Textures/Gradient.png", DXGI_FORMAT_R8G8B8A8_UNORM);
#pragma endregion 

    // Set default textures
    Model::SetDefaultTexture(tDefault);
    Model::SetDefaultTextureOpacity(tOpacityDefault);
    Model::SetDefaultTextureSpecular(tSpecularDefault);

#pragma region Samplers, Viewports, some constant buffers
    // Create samplers
    D3D11_SAMPLER_DESC pDesc;
    ZeroMemory(&pDesc, sizeof(D3D11_SAMPLER_DESC));
    pDesc.Filter = D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
    pDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    pDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    pDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    pDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    pDesc.MaxLOD = D3D11_FLOAT32_MAX;
    pDesc.MinLOD = 0;
    pDesc.MipLODBias = 0;
    pDesc.MaxAnisotropy = 16;

    // Point sampler
    sPoint->Create(pDesc);

    // Linear mip opacity sampler
    pDesc.BorderColor[0] = pDesc.BorderColor[1] = pDesc.BorderColor[2] = pDesc.BorderColor[3] = 1.;
    sMipLinearOpacity->Create(pDesc);

    // Linear mip rougness sampler
    pDesc.BorderColor[0] = pDesc.BorderColor[1] = pDesc.BorderColor[2] = 0.;
    sMipLinearRougness->Create(pDesc);

    // Anisotropic mip sampler
    pDesc.Filter = D3D11_FILTER_MAXIMUM_ANISOTROPIC;
    sMipLinear->Create(pDesc);

    // Clamped point sampler
    pDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    pDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    pDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    pDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    pDesc.ComparisonFunc = D3D11_COMPARISON_LESS;
    pDesc.BorderColor[0] = pDesc.BorderColor[1] = pDesc.BorderColor[2] = pDesc.BorderColor[3] = 1.;
    sPointClamp->Create(pDesc);

    // Create maps
    mDefaultDiffuse->mTexture = tDefault;

    // Create materials
    mDefault->SetDiffuse(mDefaultDiffuse);
    mDefault->SetSampler(sMipLinear);

    // Setup cameras
    CameraConfig cfg2;
    cfg2.fAspect = float(cfg.CurrentWidth) / float(cfg.CurrentHeight2);
    cfg2.FOV = 100.f;
    cfg2.fNear = .1f;
    cfg2.fFar = 10000.f;
    cPlayer->SetParams(cfg2);
    cPlayer->BuildProj();
    cPlayer->BuildView();

    /*vpMain.MinDepth = cfg2.fNear;
    vpMain.MaxDepth = cfg2.fFar;*/
    vpMain.MinDepth = 0.f;
    vpMain.MaxDepth = 1.f;
    vpMain.TopLeftX = 0;
    vpMain.TopLeftY = 0;
    vpMain.Width    = cfg.CurrentWidth;
    vpMain.Height   = cfg.CurrentHeight2;

    // Save aspect for further use
    fAspect = cfg2.fAspect;

    // 
    cfg2.FOV = 90.f;
    cfg2.fNear = .1f;
    cfg2.fFar = 1.f;
    c2DScreen->SetParams(cfg2);
    c2DScreen->BuildProj();
    c2DScreen->BuildView();
#pragma endregion

#pragma region Load Shaders
    // Simple textured quad
    shTexturedQuad->LoadFile("shTexturedQuadVS.cso", Shader::Vertex);
    shTexturedQuad->LoadFile("shTexturedQuadPS.cso", Shader::Pixel);

    // Post process
    shPostProcess->LoadFile("shPostProcessVS.cso", Shader::Vertex);
    shPostProcess->LoadFile("shPostProcessPS.cso", Shader::Pixel);
    
    // GUI
    shGUI->AttachShader(shPostProcess, Shader::Vertex);
    shGUI->LoadFile("shGUIPS.cso", Shader::Pixel);

    // Text without effects
    shTextSimple->LoadFile("shTexturedQuadInvVVS.cso", Shader::Vertex);
    shTextSimple->LoadFile("shTextSimplePS.cso", Shader::Pixel);

    // Text with effects
    shTextEffects->AttachShader(shTextSimple, Shader::Vertex);
    shTextEffects->LoadFile("shTextEffectsPS.cso", Shader::Pixel);

    // SDF Text rendering w/o any effects
    shTextSimpleSDF->AttachShader(shTextSimple, Shader::Vertex);
    shTextSimpleSDF->LoadFile("shTextSimpleSDFPS.cso", Shader::Pixel);

    // SDF Text rendering w/o any effects
    shTextEffectsSDF->AttachShader(shTextSimple, Shader::Vertex);
    shTextEffectsSDF->LoadFile("shTextEffectsSDFPS.cso", Shader::Pixel);

    // 3D Particles
    shParticles3D->LoadFile("shParticles3DVS.cso", Shader::Vertex);
    shParticles3D->LoadFile("shParticles3DGS.cso", Shader::Geometry);
    shParticles3D->LoadFile("shParticles3DPS.cso", Shader::Pixel);
    shParticles3D->LoadFile("shParticles3DCS.cso", Shader::Compute);

    // Clean shaders
    shGUI->ReleaseBlobs();
    shTexturedQuad->ReleaseBlobs();
    shPostProcess->ReleaseBlobs();
    shTextSimple->ReleaseBlobs();
    shTextEffects->ReleaseBlobs();
    shTextSimpleSDF->ReleaseBlobs();
    shTextEffectsSDF->ReleaseBlobs();
    shParticles3D->ReleaseBlobs();
#pragma endregion 

#pragma region Text
    fRegular = new Font("fConsolasSDF.fnt", sPoint, true);
    fRegular->SetSpacing(.9f);
    
    gTextFactory = new TextFactory(shTextSimpleSDF);
    gTextFactory->SetFont(fRegular);
    tTest = gTextFactory->Build((std::string("FPS: ") + std::to_string(60)).c_str());
    
    gTextController = new TextController(gTextFactory, cfg.CurrentWidth, cfg.CurrentHeight2, 16.f);

    TextEffects* data1 = gTextFactory->MapTextEffects();
        data1->_Color = { .7, .9, 0.f, 1.f };
    gTextFactory->UnmapTextEffects();

    SDFSettings* data2 = gTextFactory->MapSDFSettings();
        data2->_CharWidth = .4f;
        data2->_Softening = .1f;
        data2->_BorderWidth = .5f;
        data2->_BorderSoft = .1f;
    gTextFactory->UnmapSDFSettings();
#pragma endregion

    // Particles
    std::cout << "[ParticleTest]: Generating " << _Num << " particles..." << std::endl;
    std::vector<Particle> _Particles;
    _Particles.reserve(_Num);

    for( int i = 0; i < _Num; i++ ) {
        float x = (float(rand() % 60) - 30.f) * 1.f;
        float y = (float(rand() % 60) - 30.f) * 1.f;
        float z = (float(rand() % 60) - 30.f) * 1.f;

        Particle p = {
            { x, y, z }, 
            { 0.f, 0.f, 0.f }
        };

        _Particles.push_back(std::move(p));
    }

    cbDataBuffer = new ConstantBuffer();
    cbDataBuffer->CreateDefault(sizeof(DataBuffer));

    sbParticles->CreateDefault(_Num, &_Particles[0], true);

#pragma region Load models
    // Create model
   

#pragma endregion 
}

void _DirectX::Unload() {
    // Release structured buffers
    sbParticles->Release();
    //delete[] _Particles;

    // Release states
    rsFrontCull->Release();
    pBlendState0->Release();
    pBlendStateAdditive->Release();

    // Release materials
    mDefault->Release();

    // Release textures
    tBlueNoiseRG->Release();
    tClearPixel->Release();
    //tDefault->Release(); // Material will delete it
    tGradient->Release();

    // Release shaders
    shGUI->DeleteShaders();
    shTexturedQuad->DeleteShaders();
    shPostProcess->DeleteShaders();
    shTextSimple->DeleteShaders();
    shTextEffects->DeleteShaders();
    shTextSimpleSDF->DeleteShaders();
    shTextEffectsSDF->DeleteShaders();
    shParticles3D->DeleteShaders();
    
    // Text engine
    fRegular->Release();
    tTest->Release();
}

int main() {
    // Create global engine objects
    gWindow = new Window();
    gDirectX = new _DirectX();
    gAudioDevice = new AudioDevice();
    
    // Window config
    WindowConfig winCFG;
    winCFG.Borderless  = false;
    winCFG.Windowed    = true;
    winCFG.ShowConsole = true;
    winCFG.Width       = 1024;
    winCFG.Height      = 540;
    winCFG.Title       = L"Particles Test - Luna Engine";
    winCFG.Icon        = L"Engine/Assets/Engine.ico";

    // Create window
    gWindow->Create(winCFG);
    winCFG = gWindow->GetCFG();

    // Get input devices
    gInput = gWindow->GetInputDevice();
    gKeyboard = gInput->GetKeyboard();
    gMouse = gInput->GetMouse();
#if USE_GAMEPADS
    for( int i = 0; i < NUM_GAMEPAD; i++ ) gGamepad[i] = gInput->GetGamepad(i);
#endif

    // Audio device config
    AudioDeviceConfig adCFG;
    adCFG.Flags = 0;

    // Create audio device
    gAudioDevice->Create(adCFG);

    // DirectX config
    DirectXConfig dxCFG;
    dxCFG.BufferCount = 2;
    dxCFG.Width = winCFG.CurrentWidth;
    dxCFG.Height = winCFG.CurrentHeight2;
    dxCFG.m_hwnd = gWindow->GetHWND();
    dxCFG.RefreshRate = 60;// 60;
    dxCFG.UseHDR = true;
    dxCFG.DeferredContext = false;
    dxCFG.Windowed = winCFG.Windowed;
    dxCFG.Ansel = USE_ANSEL;

    // <strike>TODO: MSAA Support</strike>
    // TODO: Remove MSAA from here
    // TODO: Add TAA support
    dxCFG.MSAA = false;
    dxCFG.MSAA_Samples = 1;
    dxCFG.MSAA_Quality = 0;

    // Create device and swap chain
    if( gDirectX->ShowError(gDirectX->Create(dxCFG)) ) { return 1; }

    //std::cout << "Ansel avaliable: " << ansel::isAnselAvailable() << std::endl;

    // Debug report if we can
#ifdef _DEBUG
    if( gDirectX->gDebug ) gDirectX->gDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
#endif

    // Include ImGUI
#if _DEBUG_BUILD
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplWin32_Init(gWindow->GetHWND());
    ImGui_ImplDX11_Init(gDirectX->gDevice, gDirectX->gContext);
    ImGui::StyleColorsDark();
#endif

    // Set frame function
    bool(_DirectX::*gFrameFunction)(void);
    gFrameFunction = &_DirectX::FrameFunction;

    gWindow->SetFrameFunction(gFrameFunction); // Ref to function
    gWindow->SetDirectX(gDirectX);             // Ref to global object
    //gWindow->SetAudioDevice(0, gAudioDevice);  // Ref to global audio device[0]

    // Set directx object
    DirectXChild::SetDirectX(gDirectX);

    // Set audio object
    AudioDeviceChild::SetAudioDevice(gAudioDevice);

    // Set physics object
    //PhysicsEngineChild::SetPhysicsEngine(gPhysicsEngine);

    // Load game data
    gDirectX->Load();

    // Start rendering loop
    gWindow->Loop();

    // Unload game
#if _DEBUG_BUILD
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
#endif

    gWindow->Destroy();
    gDirectX->Unload();
    gAudioDevice->Release();
}
