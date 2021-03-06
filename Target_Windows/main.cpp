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

// Texture atlas
#include "Engine/Textures/Atlas.h"

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
// Atlas
Atlas *gTextureAtlas0;

// Text
Text *tTest;
Font *fRegular;
TextFactory *gTextFactory;
TextController *gTextController;

// Cameras
Camera *cPlayer, *c2DScreen;

// Shaders
Shader *shGUI, *shTexturedQuad, *shPostProcess, *shRenderChunk, 
       *shTextSimple, *shTextEffects, *shTextSimpleSDF, *shTextEffectsSDF;

// Models


// Textures and materials
Texture *tDefault, *tBlueNoiseRG, *tClearPixel, *tOpacityDefault, *tSpecularDefault, *tTestTex;
DiffuseMap *mDefaultDiffuse;
Sampler *sPoint, *sMipLinear, *sPointClamp, *sMipLinearOpacity, *sMipLinearRougness;

Material *mDefault;

// Render Targets


// States
BlendState *pBlendState0;
RasterState *rsFrontCull;
//ID3D11RasterizerState *rsFrontCull;

// Viewports
D3D11_VIEWPORT vpMain;
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
    gContext->ClearRenderTargetView(gRTV, Clear);
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

    // Enable depth test
    //gContext->OMSetDepthStencilState(pDSS_Default, 1);

    // Set defaults
    gContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    gContext->RSSetState(gRSDefault);

    // Render debug GUI
    if( bDebugGUI ) {
        // 3 is best number here
        std::vector<ID3D11ShaderResourceView*> pDebugTextures = {

        };
        
        shGUI->Bind();
        sPoint->Bind(Shader::Pixel);

        // 
        size_t size  = pDebugTextures.size();
        float width  = (cfg.Width / static_cast<float>(size));
        float height = width * .5f;
        
        // Normalize
        width  /= cfg.Width;
        height /= cfg.Height;

        // Right to Left offset and 2 * Width
        float left = width * static_cast<float>(size) * .5f;
        float w2   = width * 2.f;
        float h    = height * .5f + height * (size - 1.f) - .1f;

        // 
        for( size_t i = 0; i < size; i++ ) {
            if( pDebugTextures[i] == nullptr ) { continue; }

            // 
            //DirectX::XMMATRIX mOffset = DirectX::XMMatrixTranslation(.67f - width * i * 2.f, .67f, 0.f);
            //DirectX::XMMATRIX mOffset = DirectX::XMMatrixTranslation(w2 * (i - w2) - left, h, 0.);
            DirectX::XMMATRIX mOffset = DirectX::XMMatrixTranslation(w2 * i - width * size * .75f, h, 0.f);
            c2DScreen->SetWorldMatrix(DirectX::XMMatrixScaling(width, height, 1.f) * mOffset);
            c2DScreen->BuildConstantBuffer();
            c2DScreen->BindBuffer(Shader::Vertex, 0);

            // Bind texture
            gContext->PSSetShaderResources(0, 1, &pDebugTextures[i]);

            // Render plane
            gContext->Draw(6, 0);
        }
    }

    // 2D Rendering
    ComposeUI();

    // TODO: MT support
    if( cfg.DeferredContext ) {
        gContext->FinishCommandList(false, &pCommandList);
        gContext->ExecuteCommandList(pCommandList, true);
    }

    // End of frame
    gSwapchain->Present(1, 0);
    return false;
}

float fSpeed = 20.f, fRotSpeed = 100.f, fSensetivityX = 2.f, fSensetivityY = 3.f;
float fDir = 0.f, fPitch = 0.f;
void _DirectX::Tick(float fDeltaTime) {
    const WindowConfig& winCFG = gWindow->GetCFG();

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

    // Look around
    cPlayer->TranslateLookAt(f3Move);
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
    gTextController->SetSize((float)cfg.CurrentWidth, (float)cfg.CurrentHeight);

    // Resize swapchain
    scd.BufferDesc.Width  = (UINT)cfg.CurrentWidth;
    scd.BufferDesc.Height = (UINT)cfg.CurrentHeight;

    gSwapchain->ResizeTarget(&scd.BufferDesc);

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
    shRenderChunk            = new Shader();

    cPlayer   = new Camera();
    c2DScreen = new Camera();

    tDefault         = new Texture();
    tBlueNoiseRG     = new Texture();
    tClearPixel      = new Texture();
    tOpacityDefault  = new Texture();
    tSpecularDefault = new Texture();
    tTestTex         = new Texture();

    sPoint             = new Sampler();
    sMipLinear         = new Sampler();
    sPointClamp        = new Sampler();
    sMipLinearOpacity  = new Sampler();
    sMipLinearRougness = new Sampler();

    mDefaultDiffuse = new DiffuseMap();

    mDefault = new Material();

    gTextureAtlas0 = new Atlas(1000, 501);
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

    pBlendState0->Create(pDesc_, {1.f, 1.f, 1.f, 1.f});
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
    tSpecularDefault->Load("../Textures/DarkPixel.png", DXGI_FORMAT_B8G8R8A8_UNORM);

    tTestTex->Load("../Textures/Tex_Beer_ScaleDown.PNG", DXGI_FORMAT_B8G8R8A8_UNORM);
#pragma endregion 

    // Test atlas generator
    std::cout << "[Atlas]: " << gTextureAtlas0->Add(tTestTex->GetWidth(), tTestTex->GetHeight()) << std::endl;
    //std::cout << "[Atlas]: " << gTextureAtlas0->Add(1000 - tTestTex->GetWidth(), 501) << std::endl;

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
    vpMain.Width    = (float)cfg.CurrentWidth;
    vpMain.Height   = (float)cfg.CurrentHeight2;

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
    // Render chunk shader
    shRenderChunk->LoadFile("shRenderChunkVS.cso", Shader::Vertex);
    shRenderChunk->LoadFile("shRenderChunkPS.cso", Shader::Pixel);

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

    // Clean shaders
    shGUI->ReleaseBlobs();
    shTexturedQuad->ReleaseBlobs();
    shPostProcess->ReleaseBlobs();
    shTextSimple->ReleaseBlobs();
    shTextEffects->ReleaseBlobs();
    shTextSimpleSDF->ReleaseBlobs();
    shTextEffectsSDF->ReleaseBlobs();
#pragma endregion 

#pragma region Text
    fRegular = new Font("fConsolasSDF.fnt", sPoint, true);
    fRegular->SetSpacing(.9f);
    
    gTextFactory = new TextFactory(shTextSimpleSDF);
    gTextFactory->SetFont(fRegular);
    tTest = gTextFactory->Build((std::string("FPS: ") + std::to_string(60)).c_str());
    
    gTextController = new TextController(gTextFactory, (float)cfg.CurrentWidth, (float)cfg.CurrentHeight2, 16.f);
#pragma endregion

#pragma region Load models
    // Create model
   

#pragma endregion 
}

void _DirectX::Unload() {
    // Release states
    rsFrontCull->Release();
    pBlendState0->Release();

    // Release materials
    mDefault->Release();

    // Release textures
    tBlueNoiseRG->Release();
    tClearPixel->Release();
    //tDefault->Release(); // Material will delete it
    tTestTex->Release();

    // Release shaders
    shGUI->DeleteShaders();
    shTexturedQuad->DeleteShaders();
    shPostProcess->DeleteShaders();
    shTextSimple->DeleteShaders();
    shTextEffects->DeleteShaders();
    shTextSimpleSDF->DeleteShaders();
    shTextEffectsSDF->DeleteShaders();
    
    // Text engine
    fRegular->Release();
    tTest->Release();

    // Atlas
    gTextureAtlas0->Release();
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
    winCFG.Title       = L"2DL - Luna Engine";
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
