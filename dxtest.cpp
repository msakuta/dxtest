
#define NOMINMAX

#include "perlinNoise.h"
#include "Player.h"
#include "World.h"
#include "Game.h"
#include "Texture.h"
#include <assert.h>
#include <windows.h>
#include <D3D11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
extern "C"{
#include <clib/c.h>
#include <clib/suf/suf.h>
#include <clib/suf/sufbin.h>
#include <clib/timemeas.h>
}
#include <cpplib/vec3.h>
#include <cpplib/vec4.h>
#include <cpplib/quat.h>
#include <cpplib/dstring.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <sstream>
//#include <algorithm>
/** \file
 *  \brief The main source
 */

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "D3Dcompiler.lib")

namespace dxtest{

using namespace DirectX;

/// \brief Somewhat smart pointer that automatically Release() when the pointer is destructed.
///
/// It's not much smart, hence it's not named smart or something but just AutoRelease.
/// Although it's dumb, it helps greatly on Release() management.
/// The problems with Release()-ing manually are not only being prone to forget,
/// but also there's no intuitive way to properly Release() on error.
/// C++'s stack unwinding work best for this purpose.
template<typename T>
class AutoRelease{
protected:
	T *p;
public:
	AutoRelease(T *a = NULL) : p(a){
		if(p) p->AddRef();
	}
	~AutoRelease(){
		if(p) p->Release();
		p = NULL;
	}
	operator T*(){return p;}
	operator const T*()const{return p;}
	T *operator->(){return p;}
	AutoRelease &operator=(T *a){
		if(p) p->Release();
		p = a;
		return *this;
	}
	T **getpp(){return &p;}
};

static HWND hWndApp;
AutoRelease<ID3D11Device> pd3d;
AutoRelease<ID3D11DeviceContext> pdev;
AutoRelease<ID3D11RenderTargetView> backbuffer;
AutoRelease<ID3D11Texture2D> depthStencil;
AutoRelease<ID3D11DepthStencilView> depthStencilView;
AutoRelease<IDXGISwapChain> swapchain;
AutoRelease<ID3D11Buffer> g_pVB; // Buffer to hold vertices
AutoRelease<ID3D11Buffer> g_ground; // Ground surface vertices
std::auto_ptr<Texture> g_pTextures[6]; // Our texture
const char *textureNames[6] = {"cursor.png", "grass.jpg", "dirt.jpg", "gravel.png", "rock.jpg", "water.png"};
//ID3D11XFont g_font;
//ID3D11Sprite g_sprite;
AutoRelease<ID3D11Buffer> pVBuffer; // Vertex buffer
AutoRelease<ID3D11Buffer> pIndexBuffer; // Index buffer
AutoRelease<ID3D11VertexShader> pVS;
AutoRelease<ID3D11PixelShader> pPS;
AutoRelease<ID3D11InputLayout> pLayout;
AutoRelease<ID3D11Buffer> pConstantBuffer;
AutoRelease<ID3D11ShaderResourceView> pTextureRV;
AutoRelease<ID3D11SamplerState> pSamplerLinear;

XMMATRIX g_World1, g_World2;
XMMATRIX g_View;
XMMATRIX g_Projection;

const int windowWidth = 1024; ///< The window width for DirectX drawing. Aspect ratio is defined in conjunction with windowHeight.
const int windowHeight = 768; ///< The window height for DirectX drawing. Aspect ratio is defined in conjunction with windowWidth.

static bool mouse_tracking = false;
static bool mouse_captured = false;
int s_mousex, s_mousey;
static int s_mousedragx, s_mousedragy;
static int s_mouseoldx, s_mouseoldy;
static POINT mouse_pos = {0, 0};

const int Game::maxViewDistance = CELLSIZE * 2;

/// <summary>
/// The magic number sequence to identify this file as a binary save file.
/// </summary>
/// <remarks>Taken from Subversion repository's UUID. Temporarily this value is different from xnatest's one, but
/// I hope some day dxtest and xnatest will share save file.</remarks>
const byte Game::saveFileSignature[] = { 0x55, 0x0f, 0x0c, 0xd0, 0xa3, 0x6f, 0x72, 0x4b };//{ 0x83, 0x1f, 0x50, 0xec, 0x5b, 0xf7, 0x40, 0x3a };

/// <summary>
/// The current version of this program's save file.
/// </summary>
const int Game::saveFileVersion = 1;

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE)

struct ConstantBuffer
{
	XMMATRIX mWorld;
	XMMATRIX mView;
	XMMATRIX mProjection;
	XMFLOAT4 vLightDir[1];
	XMFLOAT4 vLightColor[1];
	XMFLOAT4 vAmbientLight;
	XMFLOAT4 vOutputColor;
};

struct CUSTOMVERTEX{
	Vec3f v;
	DWORD c;
};

struct VERTEX
{
	XMFLOAT3 Position;      // position
	XMFLOAT3 Normal;        // normal vector
	XMFLOAT2 Tex;           // texture coordinates
};

//const int D3DFVF_TEXTUREVERTEX = (D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1);

struct TextureVertex{
	Vec3f position;
	Vec3f normal;
	FLOAT tu, tv;
};



Texture *sampleTexture;

static const D3D11_BUFFER_DESC textureBufferDesc = {
	sizeof(D3D11_BUFFER_DESC),
	D3D11_USAGE_DEFAULT,
	D3D11_BIND_VERTEX_BUFFER,
	0,
	0,
	sizeof(TextureVertex)
};


static Game game;
static World world(game);
static Player player(game);





static int CC = 10560;
static suf_t *suf;

static bool InitPipeline();
static HRESULT InitGeometry();



HRESULT InitD3D(HWND hWnd)
{
	HRESULT hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE,
		NULL, 0, NULL, 0, D3D11_SDK_VERSION,
		pd3d.getpp(), NULL, pdev.getpp());
	if(!pd3d)
		return E_FAIL;

/*	D3DPRESENT_PARAMETERS pp;
	ZeroMemory(&pp, sizeof pp);
	pp.Windowed = TRUE;
	pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	pp.BackBufferFormat = D3DFMT_UNKNOWN;
	pp.EnableAutoDepthStencil = TRUE;
	pp.AutoDepthStencilFormat = D3DFMT_D16;

	HRESULT hr = pd3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
		D3DCREATE_SOFTWARE_VERTEXPROCESSING,
		&pp, &pdev);*/

	if(FAILED(hr))
		return E_FAIL;

	// Obtain DirectX Graphics Infrastructor object interface
	AutoRelease<IDXGIDevice1> pDXGI;
	if(FAILED(pd3d->QueryInterface(__uuidof(IDXGIDevice1), (void**)pDXGI.getpp()))){
		MessageBox(hWnd, TEXT("QueryInterface"), TEXT("Err"), MB_ICONSTOP);
		return E_FAIL;
	}

	// Obtain the adapter from DXGI
	AutoRelease<IDXGIAdapter> pAdapter;
	if(FAILED(pDXGI->GetAdapter(pAdapter.getpp()))){
		MessageBox(hWnd, TEXT("GetAdapter"), TEXT("Err"), MB_ICONSTOP);
		return E_FAIL;
	}

	// Obtain factory
	AutoRelease<IDXGIFactory> pDXGIFactory = NULL;
	pAdapter->GetParent(__uuidof(IDXGIFactory), (void**)pDXGIFactory.getpp());
	if(pDXGIFactory == NULL){
		MessageBox(hWnd, TEXT("GetParent"), TEXT("Err"), MB_ICONSTOP);
		return E_FAIL;
	}

	// Allow fullscreen by ALT+Enter
	if(FAILED(pDXGIFactory->MakeWindowAssociation(hWnd, 0))){
		MessageBox(hWnd, TEXT("MakeWindowAssociation"), TEXT("Err"), MB_ICONSTOP);
		return E_FAIL;
	}

	// Creating the swap chain
	DXGI_SWAP_CHAIN_DESC hDXGISwapChainDesc;
	hDXGISwapChainDesc.BufferDesc.Width = windowWidth;
	hDXGISwapChainDesc.BufferDesc.Height = windowHeight;
	hDXGISwapChainDesc.BufferDesc.RefreshRate.Numerator  = 0;
	hDXGISwapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	hDXGISwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	hDXGISwapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	hDXGISwapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	hDXGISwapChainDesc.SampleDesc.Count = 1;
	hDXGISwapChainDesc.SampleDesc.Quality = 0;
	hDXGISwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	hDXGISwapChainDesc.BufferCount = 1;
	hDXGISwapChainDesc.OutputWindow = hWnd;
	hDXGISwapChainDesc.Windowed = TRUE;
	hDXGISwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	hDXGISwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	if(FAILED(pDXGIFactory->CreateSwapChain(pd3d, &hDXGISwapChainDesc, swapchain.getpp()))){
		MessageBox(hWnd, TEXT("CreateSwapChain"), TEXT("Err"), MB_ICONSTOP);
		return E_FAIL;
	}

	// Obtain backbuffer from swap chain
	AutoRelease<ID3D11Texture2D> pBackBuffer = NULL;
	if(FAILED(swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)pBackBuffer.getpp()))){
		MessageBox(hWnd, TEXT("SwpChain GetBuffer"), TEXT("Err"), MB_ICONSTOP);
		return E_FAIL;
	}

	// Create a render target object from the back buffer
	if(FAILED(pd3d->CreateRenderTargetView(pBackBuffer, NULL, backbuffer.getpp()))){
		MessageBox(hWnd, TEXT("CreateRenderTargetView"), TEXT("Err"), MB_ICONSTOP);
		return E_FAIL;
	}

	// Create depth stencil texture
	D3D11_TEXTURE2D_DESC descDepth;
	ZeroMemory( &descDepth, sizeof(descDepth) );
	descDepth.Width = windowWidth;
	descDepth.Height = windowHeight;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc.Count = 1;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;
	hr = pd3d->CreateTexture2D(&descDepth, nullptr, depthStencil.getpp());
	if( FAILED( hr ) )
		return hr;

	// Create the depth stencil view
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory( &descDSV, sizeof(descDSV) );
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;
	hr = pd3d->CreateDepthStencilView(depthStencil, &descDSV, depthStencilView.getpp());
	if( FAILED( hr ) )
		return hr;

	// Set the render target to current
	pdev->OMSetRenderTargets(1, backbuffer.getpp(), depthStencilView);

	// Viewport
	D3D11_VIEWPORT vp;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	vp.Width = windowWidth;
	vp.Height = windowHeight;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	pdev->RSSetViewports(1, &vp);

	if(!InitPipeline())
		return E_FAIL;


	// Initialize the world matrix
	g_World1 = XMMatrixIdentity();

	// Initialize the view matrix
	XMVECTOR Eye = XMVectorSet( 0.0f, 0.5f, -5.0f, 0.0f );
	XMVECTOR At = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
	XMVECTOR Up = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
	g_View = XMMatrixLookAtLH( Eye, At, Up );

	// Initialize the projection matrix
	g_Projection = XMMatrixPerspectiveFovLH( XM_PIDIV2, windowWidth / (FLOAT)windowHeight, 0.01f, 100.0f );

	hr = InitGeometry();
	if(FAILED(hr))
		return hr;

	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));

	// Create the constant buffer
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(ConstantBuffer);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = pd3d->CreateBuffer(&bd, nullptr, pConstantBuffer.getpp());
	if( FAILED( hr ) )
		return hr;

	// Sample PNG texture loaded from a file.
	sampleTexture = LoadTexture(pd3d, pdev, "gravel.png");
	for(int i = 0; i < numof(g_pTextures); i++){
		Texture *tex = LoadTexture(pd3d, pdev, textureNames[i]);
		if(tex == NULL)
		{
			std::stringstream ss;
			ss << "Could not find " << textureNames[i];
			MessageBoxA( NULL, ss.str().c_str(), "Texture Load Error", MB_OK );
			return E_FAIL;
		}
		g_pTextures[i] = std::auto_ptr<Texture>(tex);
	}

	// Create the sample state
	D3D11_SAMPLER_DESC sampDesc;
	ZeroMemory( &sampDesc, sizeof(sampDesc) );
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	hr = pd3d->CreateSamplerState(&sampDesc, pSamplerLinear.getpp());
	if( FAILED( hr ) )
		return hr;

#if 0
	// Turn off culling, so we see the front and back of the triangle
	pdev->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );

	// Turn off D3D lighting, since we are providing our own vertex colors
	pdev->SetRenderState( D3DRS_LIGHTING, FALSE );

    // Turn on the zbuffer
    pdev->SetRenderState( D3DRS_ZENABLE, TRUE );

	D3DXCreateFont(pdev, 20, 0, FW_BOLD, 0, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_DONTCARE, TEXT("Courier"), &g_font );

	D3DXCreateSprite(pdev, &g_sprite);
#endif

	return S_OK;
}

static HRESULT InitGeometry()
{
	D3D11_BUFFER_DESC bd;

	ZeroMemory(&bd, sizeof(bd));

	// Create vertex buffer
	static const VERTEX vertices[] =
	{
		{XMFLOAT3(0, 0, 0), XMFLOAT3(0, -1, 0), XMFLOAT2(0, 0)},
		{XMFLOAT3(0, 0, 1), XMFLOAT3(0, -1, 0), XMFLOAT2(0, 1)},
		{XMFLOAT3(1, 0, 1), XMFLOAT3(0, -1, 0), XMFLOAT2(1, 1)},
		{XMFLOAT3(1, 0, 0), XMFLOAT3(0, -1, 0), XMFLOAT2(1, 0)},

		{XMFLOAT3(0, 1, 0), XMFLOAT3(0, 1, 0), XMFLOAT2(0, 0)},
		{XMFLOAT3(1, 1, 0), XMFLOAT3(0, 1, 0), XMFLOAT2(1, 0)},
		{XMFLOAT3(1, 1, 1), XMFLOAT3(0, 1, 0), XMFLOAT2(1, 1)},
		{XMFLOAT3(0, 1, 1), XMFLOAT3(0, 1, 0), XMFLOAT2(0, 1)},

		{XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, -1), XMFLOAT2(0, 0)},
		{XMFLOAT3(1, 0, 0), XMFLOAT3(0, 0, -1), XMFLOAT2(1, 0)},
		{XMFLOAT3(1, 1, 0), XMFLOAT3(0, 0, -1), XMFLOAT2(1, 1)},
		{XMFLOAT3(0, 1, 0), XMFLOAT3(0, 0, -1), XMFLOAT2(0, 1)},

		{XMFLOAT3(0, 0, 1), XMFLOAT3(0, 0, 1), XMFLOAT2(0, 0)},
		{XMFLOAT3(0, 1, 1), XMFLOAT3(0, 0, 1), XMFLOAT2(0, 1)},
		{XMFLOAT3(1, 1, 1), XMFLOAT3(0, 0, 1), XMFLOAT2(1, 1)},
		{XMFLOAT3(1, 0, 1), XMFLOAT3(0, 0, 1), XMFLOAT2(1, 0)},

		{XMFLOAT3(0, 0, 0), XMFLOAT3(-1, 0, 0), XMFLOAT2(0, 0)},
		{XMFLOAT3(0, 1, 0), XMFLOAT3(-1, 0, 0), XMFLOAT2(1, 0)},
		{XMFLOAT3(0, 1, 1), XMFLOAT3(-1, 0, 0), XMFLOAT2(1, 1)},
		{XMFLOAT3(0, 0, 1), XMFLOAT3(-1, 0, 0), XMFLOAT2(0, 1)},

		{XMFLOAT3(1, 0, 0), XMFLOAT3(1, 0, 0), XMFLOAT2(0, 0)},
		{XMFLOAT3(1, 0, 1), XMFLOAT3(1, 0, 0), XMFLOAT2(0, 1)},
		{XMFLOAT3(1, 1, 1), XMFLOAT3(1, 0, 0), XMFLOAT2(1, 1)},
		{XMFLOAT3(1, 1, 0), XMFLOAT3(1, 0, 0), XMFLOAT2(1, 0)},
	};
	bd.Usage = D3D11_USAGE_DYNAMIC;                // write access access by CPU and GPU
	bd.ByteWidth = sizeof vertices;             // size is the VERTEX struct * 3
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;       // use as a vertex buffer
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;    // allow CPU to write in buffer

	if(FAILED(pd3d->CreateBuffer(&bd, NULL, pVBuffer.getpp())))
		return E_FAIL;       // create the buffer

	D3D11_MAPPED_SUBRESOURCE ms;
	if(FAILED(pdev->Map(pVBuffer, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms)))
		return E_FAIL;   // map the buffer
	memcpy(ms.pData, vertices, sizeof(vertices));                // copy the data
	pdev->Unmap(pVBuffer, NULL);                                 // unmap the buffer

	// Create index buffer
	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory( &InitData, sizeof(InitData) );
	static const WORD indices[] =
	{
		2,1,0,
		0,3,2,

		6,5,4,
		4,7,6,

		10,9,8,
		8,11,10,

		14,13,12,
		12,15,14,

		18,17,16,
		16,19,18,

		22,21,20,
		20,23,22
	};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof indices;        // 36 vertices needed for 12 triangles in a triangle list
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;
	InitData.pSysMem = indices;
	HRESULT hr = pd3d->CreateBuffer(&bd, &InitData, pIndexBuffer.getpp());
	if( FAILED( hr ) )
		return hr;

	// Set index buffer
	pdev->IASetIndexBuffer( pIndexBuffer, DXGI_FORMAT_R16_UINT, 0 );

	return S_OK;
}


#if 1
static XMVECTOR frustum[6];

//-----------------------------------------------------------------------------
// Name: SetupMatrices()
// Desc: Sets up the world, view, and projection transform matrices.
//-----------------------------------------------------------------------------
VOID SetupMatrices()
{
	Quatf q = player.rot.cast<float>();
	XMVECTOR rot;
	for(int i = 0; i < 4; i++)
		rot.m128_f32[i] = q[i];
	XMMATRIX matEye = XMMatrixRotationQuaternion(rot);
	XMMATRIX matRot = XMMatrixTranslation(-player.pos[0], -player.pos[1], -player.pos[2]);
	XMMATRIX matView = XMMatrixMultiply(matRot, matEye);
	g_View = matView;

#if 0
    // For the projection matrix, we set up a perspective transform (which
    // transforms geometry from 3D view space to 2D viewport space, with
    // a perspective divide making objects smaller in the distance). To build
    // a perpsective transform, we need the field of view (1/4 pi is common),
    // the aspect ratio, and the near and far clipping planes (which define at
    // what distances geometry should be no longer be rendered).
    D3DXMATRIXA16 matProj;
    D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI / 4, (double)windowWidth / windowHeight, .30f, 100.0f );
    pdev->SetTransform( D3DTS_PROJECTION, &matProj );

	D3DXMATRIX VP = matView * matProj;

	D3DXVECTOR4 col0(VP(0, 0), VP(1, 0), VP(2, 0), VP(3, 0));
	D3DXVECTOR4 col1(VP(0, 1), VP(1, 1), VP(2, 1), VP(3, 1));
	D3DXVECTOR4 col2(VP(0, 2), VP(1, 2), VP(2, 2), VP(3, 2));
	D3DXVECTOR4 col3(VP(0, 3), VP(1, 3), VP(2, 3), VP(3, 3));

	frustum[0] = (D3DXPLANE)col2;
	frustum[1] = (D3DXPLANE)(col3 - col2);
	frustum[2] = (D3DXPLANE)(col3 + col0);
	frustum[3] = (D3DXPLANE)(col3 - col0);  // right
	frustum[4] = (D3DXPLANE)(col3 - col1);  // top
	frustum[5] = (D3DXPLANE)(col3 + col1);  // bottom

	// Normalize the frustum
	for( int i = 0; i < 6; ++i )
		D3DXPlaneNormalize( &frustum[i], &frustum[i] );


	// Set up material
	D3DMATERIAL9 mtrl;
	ZeroMemory( &mtrl, sizeof(mtrl) );
	mtrl.Diffuse.r = mtrl.Ambient.r = 1.0f;
	mtrl.Diffuse.g = mtrl.Ambient.g = 1.0f;
	mtrl.Diffuse.b = mtrl.Ambient.b = 1.0f;
	mtrl.Diffuse.a = mtrl.Ambient.a = 1.0f;
	pdev->SetMaterial( &mtrl );

	// Set up light
	D3DLIGHT9 light;
	ZeroMemory( &light, sizeof(light) );
	light.Type = D3DLIGHT_DIRECTIONAL;
	light.Diffuse.r = 1.0f;
	light.Diffuse.g = 0.9f;
	light.Diffuse.b = 0.9f;
	light.Diffuse.a = 1.0f;
	light.Ambient.r = .3f;
	light.Ambient.g = .4f;
	light.Ambient.b = .4f;
	light.Ambient.a = .0f;
	D3DXVECTOR3 vecDir = D3DXVECTOR3(-cosf(55/*timeGetTime()*//360.0f),
                     -2.0f,
                     -sinf(55/*timeGetTime()*//360.0f) );
	D3DXVec3Normalize( (D3DXVECTOR3*)&light.Direction, &vecDir );
	light.Range = 1000.0f;
	light.Phi = .5 * D3DX_PI;
	light.Theta = .8 * D3DX_PI;
	pdev->SetLight(0, &light);
	pdev->LightEnable(0, TRUE);

	pdev->SetRenderState(D3DRS_LIGHTING, TRUE);
	pdev->SetRenderState( D3DRS_AMBIENT, 0x00202020 );
#endif
}

/// <summary>Test if given bounding box intersects or included in a frustum.</summary>
/// <remarks>Test assumes frustum planes face inward.</remarks>
/// <returns>True if intersects</returns>
bool FrustumCheck(XMVECTOR min, XMVECTOR max, const XMVECTOR* frustum)
{
	XMVECTOR P;
	XMVECTOR Q;

	for(int i = 0; i < 6; ++i)
	{
		// For each coordinate axis x, y, z...
		for(int j = 0; j < 3; ++j)
		{
			// Make PQ point in the same direction as the plane normal on this axis.
			if( frustum[i].m128_f32[j] > 0.0f )
			{
				P.m128_f32[j] = min.m128_f32[j];
				Q.m128_f32[j] = max.m128_f32[j];
			}
			else 
			{
				P.m128_f32[j] = max.m128_f32[j];
				Q.m128_f32[j] = min.m128_f32[j];
			}
		}

		if(XMPlaneDotCoord(frustum[i], Q).m128_f32[0] < 0.0f  )
			return false;
	}
	return true;
} 


void RotateModel(){
    // Set up the rotation matrix to generate 1 full rotation (2*PI radians) 
    // every 1000 ms. To avoid the loss of precision inherent in very high 
    // floating point numbers, the system time is modulated by the rotation 
    // period before conversion to a radian angle.
    UINT iTime = clock() % 5000;
    FLOAT fAngle = iTime * ( 2.0f * XM_PI ) / 5000.0f;
	FLOAT fPitch = sin(clock() % 33333 * (2.f * XM_PI) / 33333.f);
	XMMATRIX matPitch = XMMatrixRotationX(fPitch);
	XMMATRIX matYaw = XMMatrixRotationY(fAngle);
    // For our world matrix, we will just rotate the object about the y-axis.
	XMMATRIX matWorld = XMMatrixMultiply(matYaw, matPitch);
	g_World1 = matWorld;
//    pdev->SetTransform( D3DTS_WORLD, &matWorld );
}
#endif


static const char TexVertexShaderSrc[] =
	"Texture2D txDiffuse : register( t0 );\n"
	"SamplerState samLinear : register( s0 );\n"
	"cbuffer ConstantBuffer : register( b0 )\n"
	"{\n"
	"	matrix World;\n"
	"	matrix View;\n"
	"	matrix Projection;\n"
	"	float4 vLightDir[1];\n"
	"	float4 vLightColor[1];\n"
	"	float4 vAmbientLight;\n"
	"	float4 vOutputColor;\n"
	"}\n"
	"struct VS_INPUT\n"
	"{\n"
	"	float4 Position : POSITION;\n"
	"	float3 Norm : NORMAL;\n"
	"	float2 Tex : TEXCOORD0;\n"
	"};\n"
	"struct Varyings\n"
	"{\n"
	"	float4 Position : SV_Position;\n"
	"	float3 Norm     : NORMAL;\n"
	"	float2 Tex      : TEXCOORD0;\n"
	"};\n"
	"void main(in VS_INPUT input,\n"
	"          out Varyings ov)\n"
	"{\n"
	"	float4 worldPosition = mul(input.Position, World);\n"
	"	float4 viewPosition = mul(worldPosition, View);\n"
	"	ov.Position = mul(viewPosition, Projection);\n"
	"	ov.Norm = mul(float4(input.Norm, 0), World).xyz;\n"
	"	ov.Tex = input.Tex;\n"
	"}\n";

static const char TexPixelShaderSrc[] =
	"Texture2D txDiffuse : register( t0 );\n"
	"SamplerState samLinear : register( s0 );\n"
	"cbuffer ConstantBuffer : register( b0 )\n"
	"{\n"
	"	matrix World;\n"
	"	matrix View;\n"
	"	matrix Projection;\n"
	"	float4 vLightDir[1];\n"
	"	float4 vLightColor[1];\n"
	"	float4 vAmbientLight;\n"
	"	float4 vOutputColor;\n"
	"}\n"
	"float4 Color;\n"
	"struct Varyings\n"
	"{\n"
	"	float4 Position : SV_Position;\n"
	"	float3 Norm     : NORMAL;\n"
	"	float2 Tex      : TEXCOORD0;\n"
	"};\n"
	"float4 main(in Varyings ov) : SV_Target\n"
	"{\n"
	"	float4 finalColor = 0;\n"
	"	for(int i=0; i<1; i++)\n"
	"	{\n"
	"		finalColor += saturate(dot((float3)vLightDir[i], ov.Norm) * vLightColor[i]);\n"
	"	}\n"
	"	finalColor = saturate(finalColor + vAmbientLight);\n"
	"	finalColor.xyz *= txDiffuse.Sample( samLinear, ov.Tex );\n"
	"	finalColor.a = 1;\n"
	"	return finalColor;\n"
	"}\n";


static bool InitPipeline()
{
	// load and compile the two shaders
	AutoRelease<ID3D10Blob> VS, PS;
	if(FAILED(D3DCompile(TexVertexShaderSrc, sizeof TexVertexShaderSrc, "VShader", 0, 0, "main", "vs_4_0", 0, 0, VS.getpp(), 0)))
		return false;
	if(FAILED(D3DCompile(TexPixelShaderSrc, sizeof TexPixelShaderSrc, "PShader", 0, 0, "main", "ps_4_0", 0, 0, PS.getpp(), 0)))
		return false;

	void *vsrbuf = VS->GetBufferPointer();
	size_t vsrsize = VS->GetBufferSize();
	void *psrbuf = PS->GetBufferPointer();
	size_t psrsize = PS->GetBufferSize();

	// encapsulate both shaders into shader objects
	if(FAILED(pd3d->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), NULL, pVS.getpp())))
		return false;
	if(FAILED(pd3d->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), NULL, pPS.getpp())))
		return false;

	// set the shader objects
	pdev->VSSetShader(pVS, 0, 0);
	pdev->PSSetShader(pPS, 0, 0);

	// create the input layout object
	D3D11_INPUT_ELEMENT_DESC ied[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(VERTEX, Tex), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	int nied = numof(ied);
	if(FAILED(pd3d->CreateInputLayout(ied, nied, VS->GetBufferPointer(), VS->GetBufferSize(), pLayout.getpp())))
		return false;
	pdev->IASetInputLayout(pLayout);
	return true;
}

void capture_mouse(){
	int c;
	extern HWND hWndApp;
	HWND hwd = hWndApp;
	RECT r;
	s_mouseoldx = s_mousex;
	s_mouseoldy = s_mousey;
	GetClientRect(hwd, &r);
	mouse_pos.x = (r.left + r.right) / 2;
	mouse_pos.y = (r.top + r.bottom) / 2;
	ClientToScreen(hwd, &mouse_pos);
	SetCursorPos(mouse_pos.x, mouse_pos.y);
	c = ShowCursor(FALSE);
	while(0 <= c)
		c = ShowCursor(FALSE);
	mouse_captured = true;
}

static void uncapture_mouse(){
	extern HWND hWndApp;
	HWND hwd = hWndApp;
	mouse_captured = 0;
	s_mousedragx = s_mouseoldx;
	s_mousedragy = s_mouseoldy;
	mouse_pos.x = s_mouseoldx;
	mouse_pos.y = s_mouseoldy;
	ClientToScreen(hwd, &mouse_pos);
	SetCursorPos(mouse_pos.x, mouse_pos.y);
	while(ShowCursor(TRUE) <= 0);
/*	while(ShowCursor(TRUE) < 0);*/
}

void mouse_func(int button, int x, int y){

	s_mousex = x;
	s_mousey = y;

	if(button == WM_RBUTTONUP){
		mouse_captured = !mouse_captured;
		if(mouse_captured){
			capture_mouse();
		}
		else{
			uncapture_mouse();
		}
	}
}

static void initializeVolume(){
	world.initialize();
}

static double gtime = 0.;

static void display_func(){
	static int frame = 0;
	static timemeas_t tm;
	double dt = 0.;

	std::ofstream logwriter = std::ofstream("dxtest.log", std::ofstream::app);
	game.logwriter = &logwriter;
	CellVolume::cellInvokes = 0;
	CellVolume::cellForeignInvokes = 0;
	CellVolume::cellForeignExists = 0;

	if(frame == 0){
		TimeMeasStart(&tm);
	}
	else{
		dt = TimeMeasLap(&tm);
		TimeMeasStart(&tm);
		if(.1 < dt)
			dt = .1;
		gtime += dt;
	}

	logwriter << "Frame " << frame << ", dt = " << dt << std::endl;

	// GetKeyState() doesn't care which window is active, so we must manually check it.
	if(GetActiveWindow() == hWndApp){
		player.keyinput(dt);
	}


	player.think(dt);
	world.think(dt);

	frame++;

	int mousedelta[2] = {0, 0};
	if(mouse_captured){
		POINT p;
		if(GetActiveWindow() != hWndApp){
			mouse_captured = 0;
			while(ShowCursor(TRUE) <= 0);
		}
		if(GetCursorPos(&p) && (p.x != mouse_pos.x || p.y != mouse_pos.y)){
			mousedelta[0] = p.x - mouse_pos.x;
			mousedelta[1] = p.y - mouse_pos.y;
			player.rotateLook(p.x - mouse_pos.x, p.y - mouse_pos.y);
			SetCursorPos(mouse_pos.x, mouse_pos.y);
		}
	}

	game.draw(dt);

	logwriter << "cellInvokes = " << CellVolume::cellInvokes << ", cellForeignInvokes = " << CellVolume::cellForeignInvokes << ", cellForeignExists = " << CellVolume::cellForeignExists << std::endl;
	game.logwriter = NULL;
}

struct TextureData{
	float scale;
	int size;
	int index;
};
struct MaterialData{
	const TextureData &tex;
	Cell::Type type;
};
static const TextureData textureData[] = {
	{1, 64, 0},
	{0.125f, 512, 1},
	{0.5f, 128, 2},
	{0.5f, 128, 3},
	{1.0f, 64, 4},
	{1.0f, 64, 5}
};
static const MaterialData types[] = {
	{textureData[1], Cell::Grass}, {textureData[2], Cell::Dirt}, {textureData[3], Cell::Gravel}, {textureData[4], Cell::Rock},
	{textureData[1], Cell::HalfGrass}, {textureData[2], Cell::HalfDirt}, {textureData[3], Cell::HalfGravel}, {textureData[4], Cell::HalfRock}
};

class xmvc{
	XMVECTOR v;
public:
	operator XMVECTOR&(){return v;}
	operator Vec3f(){return Vec3f(v.m128_f32[0], v.m128_f32[1], v.m128_f32[2]);}
	xmvc(const Vec3f &a) : v(XMLoadFloat4(&XMFLOAT4(a[0], a[1], a[2], 0))){}
	xmvc(const XMVECTOR &a) : v(a){}
};

void dxtest::Game::draw(double dt)const{
	// clear the back buffer to a deep blue
	pdev->ClearRenderTargetView(backbuffer, Vec4f(0.0f, 0.2f, 0.4f, 1.0f));

	// Clear the depth buffer to 1.0 (max depth)
	pdev->ClearDepthStencilView( depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0 );

	// Rotation for the first cube
	g_World1 = XMMatrixRotationY( gtime );

	// Orbiting second cube
	XMMATRIX mSpin = XMMatrixRotationZ( -gtime );
	XMMATRIX mOrbit = XMMatrixRotationY( -gtime * 2.0f );
	XMMATRIX mTranslate = XMMatrixTranslation( -4.0f, 0.0f, 0.0f );
	XMMATRIX mScale = XMMatrixScaling( 0.3f, 0.3f, 0.3f );
	g_World2 = mScale * mSpin * mTranslate * mOrbit;

	// Setup our lighting parameters
	XMFLOAT4 vLightDirs[2] =
	{
		XMFLOAT4( -0.577f, 0.577f, -0.577f, 1.0f ),
//		XMFLOAT4( 0.0f, -1.0f, -0.0f, 1.0f ),
	};
	XMFLOAT4 vLightColors[2] =
	{
		XMFLOAT4( 0.5f, 0.5f, 0.5f, 1.0f ),
//		XMFLOAT4( 0.5f, 0.3f, 0.0f, 1.0f )
	};

	SetupMatrices();

	//
	// Update variables
	//
	ConstantBuffer cb;
	cb.mWorld = XMMatrixTranspose( g_World1 );
	cb.mView = XMMatrixTranspose( g_View );
	cb.mProjection = XMMatrixTranspose( g_Projection );
	cb.vLightDir[0] = vLightDirs[0];
//	cb.vLightDir[1] = vLightDirs[1];
	cb.vLightColor[0] = vLightColors[0];
//	cb.vLightColor[1] = vLightColors[1];
	cb.vAmbientLight = XMFLOAT4(0.1, 0.15, 0.2, 1.0);
	cb.vOutputColor = XMFLOAT4(1, 0, 0, 0);
	pdev->UpdateSubresource(pConstantBuffer, 0, nullptr, &cb, 0, 0);

	// select which vertex buffer to display
	UINT stride = sizeof(VERTEX);
	UINT offset = 0;
	pdev->IASetVertexBuffers(0, 1, pVBuffer.getpp(), &stride, &offset);

	pdev->VSSetShader( pVS, nullptr, 0 );
	pdev->VSSetConstantBuffers(0, 1, pConstantBuffer.getpp());
	pdev->PSSetShader( pPS, nullptr, 0 );
	pdev->PSSetConstantBuffers(0, 1, pConstantBuffer.getpp());
	pdev->PSSetShaderResources( 0, 1, pTextureRV.getpp() );
//	pdev->PSSetShaderResources( 0, 1, pTextureRV.getpp() );
	pdev->PSSetShaderResources( 0, 1, &sampleTexture->TexSv );
	pdev->PSSetSamplers( 0, 1, pSamplerLinear.getpp() );

	// select which primtive type we are using
	pdev->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


#if 1
/*	if(SUCCEEDED(pdev->BeginScene()))*/{

		XMMATRIX matWorld = XMMatrixIdentity();
		g_World1 = matWorld;

/*		pdev->SetTexture( 0, g_pTexture);
		pdev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		pdev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        pdev->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
        pdev->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );*/

//		pdev->SetStreamSource( 0, g_ground, 0, sizeof( TextureVertex ) );
//        pdev->SetFVF( D3DFVF_TEXTUREVERTEX );

		const Vec3i inf = World::real2ind(player->getPos());

		// The first pass only draws solid cells.
//		pdev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		for(World::VolumeMap::iterator it = world->volume.begin(); it != world->volume.end(); it++){
			const Vec3i &key = it->first;
			CellVolume &cv = it->second;

			// If all content is air, skip drawing
			if(cv.getSolidCount() == 0)
				continue;

			// Examine if intersects or included in viewing frustum
			if(!FrustumCheck(xmvc(World::ind2real(key * CELLSIZE).cast<float>()), xmvc(World::ind2real((key + Vec3i(1,1,1)) * CELLSIZE).cast<float>()), frustum))
				continue;

			// Cull too far CellVolumes
			if ((key[0] + 1) * CELLSIZE + maxViewDistance < inf[0])
				continue;
			if (inf[0] < key[0] * CELLSIZE - maxViewDistance)
				continue;
			if ((key[1] + 1) * CELLSIZE + maxViewDistance < inf[1])
				continue;
			if (inf[1] < key[1] * CELLSIZE - maxViewDistance)
				continue;
			if ((key[2] + 1) * CELLSIZE + maxViewDistance < inf[2])
				continue;
			if (inf[2] < key[2] * CELLSIZE - maxViewDistance)
				continue;

			for(int ix = 0; ix < CELLSIZE; ix++){
				for(int iz = 0; iz < CELLSIZE; iz++){
					// This detail culling is not much effective.
					//if (bf.Contains(new BoundingBox(ind2real(keyindex + new Vec3i(ix, kv.Value.scanLines[ix, iz, 0], iz)), ind2real(keyindex + new Vec3i(ix + 1, kv.Value.scanLines[ix, iz, 1] + 1, iz + 1)))) != ContainmentType.Disjoint)
					const int (&scanLines)[CELLSIZE][CELLSIZE][2] = cv.getScanLines();
					for (int iy = scanLines[ix][iz][0]; iy < scanLines[ix][iz][1]; iy++){

						// Cull too far Cells
						if (cv(ix, iy, iz).getType() == Cell::Air)
							continue;
						if (maxViewDistance < abs(ix + it->first[0] * CELLSIZE - inf[0]))
							continue;
						if (maxViewDistance < abs(iy + it->first[1] * CELLSIZE - inf[1]))
							continue;
						if (maxViewDistance < abs(iz + it->first[2] * CELLSIZE - inf[2]))
							continue;

						// If the Cell is buried under ground, it's no use examining each face of the Cell.
						if(6 <= cv(ix, iy, iz).getAdjacents())
							continue;

						bool x0 = !cv(ix - 1, iy, iz).isTranslucent();
						bool x1 = !cv(ix + 1, iy, iz).isTranslucent();
						bool y0 = !cv(ix, iy - 1, iz).isTranslucent();
						bool y1 = !cv(ix, iy + 1, iz).isTranslucent();
						bool z0 = !cv(ix, iy, iz - 1).isTranslucent();
						bool z1 = !cv(ix, iy, iz + 1).isTranslucent();
						const Cell &cell = cv(ix, iy, iz);
//						pdev->SetTexture(0, g_pTextures[cell.getType() & ~Cell::HalfBit]);
						pdev->PSSetShaderResources( 0, 1, &g_pTextures[cell.getType() & ~Cell::HalfBit]->TexSv );
						matWorld = XMMatrixTranslation(
							it->first[0] * CELLSIZE + (ix - CELLSIZE / 2),
							it->first[1] * CELLSIZE + (iy - CELLSIZE / 2),
							it->first[2] * CELLSIZE + (iz - CELLSIZE / 2));

						if(cell.getType() == Cell::Water)
							continue;

						if(cell.getType() & Cell::HalfBit){
							XMMATRIX matscale = XMMatrixScaling(1, 0.5, 1.);
							XMMATRIX matresult = XMMatrixMultiply(matscale, matWorld);

							cb.mWorld = XMMatrixTranspose(matresult);
							pdev->UpdateSubresource(pConstantBuffer, 0, nullptr, &cb, 0, 0);
						}
						else{
							g_World1 = matWorld;
							cb.mWorld = XMMatrixTranspose(g_World1);
							pdev->UpdateSubresource(pConstantBuffer, 0, nullptr, &cb, 0, 0);
						}
						if(!x0 && !x1 && !y0 && !y1)
							pdev->DrawIndexed(36, 0, 0);
						else{
							if(!x0)
								pdev->DrawIndexed(2 * 3, 8 * 3, 0);
							if(!x1)
								pdev->DrawIndexed(2 * 3, 10 * 3, 0);
							if(!y0)
								pdev->DrawIndexed(2 * 3, 0, 0);
							if(!y1)
								pdev->DrawIndexed(2 * 3, 2 * 3, 0);
							if(!z0)
								pdev->DrawIndexed(2 * 3, 4 * 3, 0);
							if(!z1)
								pdev->DrawIndexed(2 * 3, 6 * 3, 0);
						}
					}
				}
			}
		}

		int pass = 0;
		int all = 0;

		// The second pass draw transparent objects
		pdev->PSSetShaderResources(0, 1, &g_pTextures[Cell::Water]->TexSv);
//		pdev->SetTexture(0, g_pTextures[Cell::Water]);
//		pdev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
//		pdev->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
//		pdev->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
//		pdev->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);
		for(World::VolumeMap::iterator it = world->volume.begin(); it != world->volume.end(); it++){
			const Vec3i &key = it->first;
			CellVolume &cv = it->second;

			// If all content is air, skip drawing
			if(cv.getSolidCount() == 0)
				continue;

			// Examine if intersects or included in viewing frustum
			if(!FrustumCheck(xmvc(World::ind2real(key * CELLSIZE).cast<float>()), xmvc(World::ind2real((key + Vec3i(1,1,1)) * CELLSIZE).cast<float>()), frustum))
				continue;

			// Cull too far CellVolumes
			if ((key[0] + 1) * CELLSIZE + maxViewDistance < inf[0])
				continue;
			if (inf[0] < key[0] * CELLSIZE - maxViewDistance)
				continue;
			if ((key[1] + 1) * CELLSIZE + maxViewDistance < inf[1])
				continue;
			if (inf[1] < key[1] * CELLSIZE - maxViewDistance)
				continue;
			if ((key[2] + 1) * CELLSIZE + maxViewDistance < inf[2])
				continue;
			if (inf[2] < key[2] * CELLSIZE - maxViewDistance)
				continue;

			for(int ix = 0; ix < CELLSIZE; ix++){
				for(int iz = 0; iz < CELLSIZE; iz++){
					// This detail culling is not much effective.
					//if (bf.Contains(new BoundingBox(ind2real(keyindex + new Vec3i(ix, kv.Value.scanLines[ix, iz, 0], iz)), ind2real(keyindex + new Vec3i(ix + 1, kv.Value.scanLines[ix, iz, 1] + 1, iz + 1)))) != ContainmentType.Disjoint)
					const int (&scanLines)[CELLSIZE][CELLSIZE][2] = cv.getTranScanLines();
					for (int iy = scanLines[ix][iz][0]; iy < scanLines[ix][iz][1]; iy++){
						const Cell &cell = cv(ix, iy, iz);
						all++;

						// Cull too far Cells
						if (cell.getType() != Cell::Water)
							continue;
						if (maxViewDistance < abs(ix + it->first[0] * CELLSIZE - inf[0]))
							continue;
						if (maxViewDistance < abs(iy + it->first[1] * CELLSIZE - inf[1]))
							continue;
						if (maxViewDistance < abs(iz + it->first[2] * CELLSIZE - inf[2]))
							continue;

						// If the Cell is buried under ground, it's no use examining each face of the Cell.
						if(6 <= cell.getAdjacents())
							continue;

						bool x0 = !cv(ix - 1, iy, iz).isTranslucent();
						bool x1 = !cv(ix + 1, iy, iz).isTranslucent();
						bool y0 = !cv(ix, iy - 1, iz).isTranslucent();
						bool y1 = !cv(ix, iy + 1, iz).isTranslucent();
						bool z0 = !cv(ix, iy, iz - 1).isTranslucent();
						bool z1 = !cv(ix, iy, iz + 1).isTranslucent();
						matWorld = XMMatrixTranslation(
							it->first[0] * CELLSIZE + (ix - CELLSIZE / 2),
							it->first[1] * CELLSIZE + (iy - CELLSIZE / 2),
							it->first[2] * CELLSIZE + (iz - CELLSIZE / 2));

						x0 |= cv(ix - 1, iy, iz).getType() != cell.Air;
						x1 |= cv(ix + 1, iy, iz).getType() != cell.Air;
						y0 |= cv(ix, iy - 1, iz).getType() != cell.Air;
						y1 |= cv(ix, iy + 1, iz).getType() != cell.Air;
						z0 |= cv(ix, iy, iz - 1).getType() != cell.Air;
						z1 |= cv(ix, iy, iz + 1).getType() != cell.Air;

						g_World1 = matWorld;
//						pdev->SetTransform(D3DTS_WORLD, &matWorld);

						if(!x0 && !x1 && !y0 && !y1)
							pdev->DrawIndexed(36, 0, 0);
						else{
							if(!x0)
								pdev->DrawIndexed(2 * 3, 8 * 3, 0);
							if(!x1)
								pdev->DrawIndexed(2 * 3, 10 * 3, 0);
							if(!y0)
								pdev->DrawIndexed(2 * 3, 0, 0);
							if(!y1)
								pdev->DrawIndexed(2 * 3, 2 * 3, 0);
							if(!z0)
								pdev->DrawIndexed(2 * 3, 4 * 3, 0);
							if(!z1)
								pdev->DrawIndexed(2 * 3, 6 * 3, 0);
						}
						pass += !x0 || !x1 || !y0 || !y1 || !z0 || !z1;
					}
				}
			}
		}

#if 0
		{
			RECT r = {-numof(g_pTextures) / 2 * 64 + windowWidth / 2, windowHeight - 64, numof(g_pTextures) / 2 * 64 + windowWidth / 2, windowHeight};
			XMMATRIX mat, matscale, mattrans;
			XMRECT dr = {-numof(types) / 2 * 64 + windowWidth / 2 - 8, windowHeight - 64 - 8, numof(types) / 2 * 64 + windowWidth / 2 + 8, windowHeight};
			
			pdev->Clear(1, &dr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 63), 0.0f, 0);
			for(int i = 0; i < numof(types); i++){
				const MaterialData &type = types[i];
				Cell::Type t = type.type;
				bool half = t & Cell::HalfBit;

				D3DXMatrixScaling( &matscale, type.tex.scale, type.tex.scale, 1. );
				D3DXMatrixTranslation(&mattrans, (i - numof(types) / 2) * 64 + windowWidth / 2, windowHeight - 64 + (half ? 32 : 0), 0);
				D3DXMatrixMultiply(&mat, &matscale, &mattrans);
				g_sprite->SetTransform(&mat);
				g_sprite->Begin(D3DXSPRITE_ALPHABLEND);
				RECT srcrect = {0, (half ? type.tex.size / 2 : 0), type.tex.size, type.tex.size};
				g_sprite->Draw(g_pTextures[type.tex.index], &srcrect, NULL, &D3DXVECTOR3(0, 0, 0),
					D3DCOLOR_ARGB(player->curtype == t ? 255 : 127,255,255,255));
				g_sprite->End();

				// Show cursor
				if(player->curtype == t){
					D3DXMatrixTranslation(&mattrans, (i - numof(types) / 2) * 64 + windowWidth / 2, windowHeight - 64, 0);
					g_sprite->SetTransform(&mattrans);
					g_sprite->Begin(D3DXSPRITE_ALPHABLEND);
					g_sprite->Draw(g_pTextures[0], NULL, NULL, &D3DXVECTOR3(0, 0, 0),
						D3DCOLOR_ARGB(255,255,255,255));
					g_sprite->End();
				}
				r.left = (i - numof(types) / 2) * 64 + windowWidth / 2;
				r.right = r.left + 64;
				g_font->DrawTextA(NULL, dstring() << player->getBricks(t), -1, &r, 0, D3DCOLOR_ARGB(255, 255, 25, 25));
			}

		}
		if(player->showMiniMap)
			drawMiniMap(dt);

		RECT rct = {0, 0, 500, 20};
		g_font->DrawTextA(NULL, dstring() << "Frametime: " << dt, -1, &rct, 0, D3DCOLOR_ARGB(255, 255, 25, 25));
		rct.top += 20, rct.bottom += 20;
		g_font->DrawTextA(NULL, dstring() << "pos: " << player->pos[0] << ", " << player->pos[1] << ", " << player->pos[2], -1, &rct, 0, D3DCOLOR_ARGB(255, 255, 25, 25));
		rct.top += 20, rct.bottom += 20;
		g_font->DrawTextA(NULL, dstring() << "velo: " << player->velo[0] << ", " << player->velo[1] << ", " << player->velo[2], -1, &rct, 0, D3DCOLOR_ARGB(255, 255, 25, 25));
		rct.top += 20, rct.bottom += 20;
		g_font->DrawTextA(NULL, dstring() << "bricks: " << player->bricks[1] << ", " << player->bricks[2] << ", " << player->bricks[3] << ", " << player->bricks[4], -1, &rct, 0, D3DCOLOR_ARGB(255, 255, 25, 25));
		rct.top += 20, rct.bottom += 20;
		g_font->DrawTextA(NULL, dstring() << "abund: " << world->getBricks(1) << ", " << world->getBricks(2) << ", " << world->getBricks(3) << ", " << world->getBricks(4) << ", " << world->getBricks(5), -1, &rct, 0, D3DCOLOR_ARGB(255, 255, 25, 25));
//		rct.top += 20, rct.bottom += 20;
//		g_font->DrawTextA(NULL, dstring() << "pass/all: " << pass << "/" << all, -1, &rct, 0, D3DCOLOR_ARGB(255, 255, 25, 25));

		pdev->EndScene();
#endif
	}

#endif
	// switch the back buffer and the front buffer
	swapchain->Present(0, 0);
}

inline Vec3i VecSignModulo(const Vec3i &v, int divisor){
	return Vec3i(SignModulo(v[0], divisor), SignModulo(v[1], divisor), SignModulo(v[2], divisor));
}

inline Vec3i VecSignDiv(const Vec3i &v, int divisor){
	return Vec3i(SignDiv(v[0], divisor), SignDiv(v[1], divisor), SignDiv(v[2], divisor));
}


/// \brief The internal function that draws the mini map.
///
/// draw() got long, so I wanted to subdivide it into subroutines.
/// The mini map is fairly independent and has some code size that deserves subroutinize.
void Game::drawMiniMap(double dt)const{
#if 0
	D3DXMATRIX mat, matscale, mattrans;
	static const int mapCellSize = 8;
	static const int mapCells = 128 / mapCellSize;
	static const int mapCellVolumes = (mapCells + CELLSIZE - 1) / CELLSIZE;
	static const int mapHeightRange = 16;
	static const int mapHeightCellVolumes = (mapHeightRange + CELLSIZE - 1) / CELLSIZE;
	const int cx = windowWidth - 128;
	static const int cy = 128;
	D3DRECT drMap = {cx - 128, cy - 128, cx + 128, cy + 128};

	// Minimize CellVolume search by accumulating height and cell reference to this buffer.
	// TODO: Dynamic scaling
	int heights[mapCells * 2][mapCells * 2] = {0};
	const Cell *cells[mapCells * 2][mapCells * 2] = {NULL};
	
	// Fill the background with void color.
	pdev->Clear(1, &drMap, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 63), 0.0f, 0);

	const Vec3i pos = world->real2ind(player->pos);
	const Vec3i cvpos = VecSignDiv(pos, CELLSIZE);
	for(int cvx = cvpos[0] - mapCellVolumes; cvx <= cvpos[0] + mapCellVolumes; cvx++) for(int cvz = cvpos[2] - mapCellVolumes; cvz <= cvpos[2] + mapCellVolumes; cvz++){
		for(int cvy = cvpos[1] + mapHeightCellVolumes; cvpos[1] - mapHeightCellVolumes <= cvy; cvy--){

			// Find CellVolume of interest.
			Vec3i dpos = Vec3i(cvx, cvy, cvz);
			World::VolumeMap::iterator it = world->volume.find(dpos);
			if(it == world->volume.end())
				continue;
			CellVolume &cv = it->second;

			// Find up CellVolume prior to process to prevent lookup per every scanline.
			World::VolumeMap::iterator upit = world->volume.find(Vec3i(cvx, cvy + 1, cvz));

			// Obtain local position
			const Vec3i lpos = pos - dpos * CELLSIZE;

			const int maxiy = std::min(lpos[1] + mapHeightRange, CELLSIZE - 1);

			for(int ix = std::max(0, lpos[0] - mapCells); ix < std::min(CELLSIZE, lpos[0] + mapCells); ix++){
				for(int iz = std::max(0, lpos[2] - mapCells); iz < std::min(CELLSIZE, lpos[2] + mapCells); iz++){
					// Utilizing scanlines for finding cell height for mini map will greatly reduce lookup time.
					const int (&scanLine)[2] = cv.getScanLines()[ix][iz];
					const int (&tranScanLine)[2] = cv.getTranScanLines()[ix][iz];

					// Start from one cell up of the scanline's top, Y axis descending order.
					// Scan line's end position is exclusive, so NOT subtracting 1 from scanLine[1] makes it
					// begin from excess cell.
					int iy = std::min(lpos[1] + mapHeightRange, std::max(scanLine[1], tranScanLine[1]));
					bool space = false;

					// If the excess cell exceeds border of this CellVolume, query the up CellVolume whether it's
					// a air cell.
					if(iy == CELLSIZE){
						if(upit == world->volume.end() || upit->second(ix, 0, iz).getType() == Cell::Air)
							space = true;
						iy--;
					}

					// Descending scan to find the first boundary of air cell and other types of cells.
					for(; std::max(lpos[1] - mapHeightRange, std::min(scanLine[0], tranScanLine[0])) <= iy; iy--){
						if(cv(ix, iy, iz).getType() != Cell::Air){
							if(space){
								int rx = ix + cvx * CELLSIZE - pos[0] + mapCells;
								int ry = iy + cvy * CELLSIZE - pos[1] + mapHeightRange;
								int rz = iz + cvz * CELLSIZE - pos[2] + mapCells;
								if(heights[rx][rz] < ry){
									heights[rx][rz] = ry;
									cells[rx][rz] = &cv(ix, iy, iz);
								}
								break;
							}
						}
						else
							space = true;
					}
				}
			}
		}
	}
//		if(iy == pos[1] + mapHeightRange/*scanLine[1]*/)
//			continue;

	for(int ix = 0; ix < mapCells * 2; ix++) for(int iz = 0; iz < mapCells * 2; iz++){
		if(!cells[ix][iz])
			continue;
		const Cell &cell = *cells[ix][iz]/*world->cell(ix, iy, iz)*//*cv(ipos[0], iy, ipos[2])*/;
		const TextureData &tex = textureData[cell.getType() & ~Cell::HalfBit];

		D3DXMatrixScaling( &matscale, tex.scale * mapCellSize / 64, tex.scale * mapCellSize / 64, 1. );
		D3DXMatrixTranslation(&mattrans, cx + (ix - mapCells) * mapCellSize, cy + (iz - mapCells) * mapCellSize, 0);
		D3DXMatrixMultiply(&mat, &matscale, &mattrans);
		g_sprite->SetTransform(&mat);
		g_sprite->Begin(D3DXSPRITE_ALPHABLEND);
		RECT srcrect = {0, 0, tex.size, tex.size};
		int col = (heights[ix][iz]) * 128 / (2 * mapHeightRange) + 127;
		g_sprite->Draw(g_pTextures[tex.index], &srcrect, NULL, &D3DXVECTOR3(0, 0, 0),
			D3DCOLOR_ARGB(255,col,col,col));
		g_sprite->End();

	}
#endif
}

bool Game::save(){
	try{
		static char customfilter[64] = "Save File\0*.sav\0";
		static char file[MAX_PATH] = "save.sav";
		OPENFILENAMEA ofn;
		ofn.lStructSize = sizeof ofn;
		ofn.hwndOwner = hWndApp;
		ofn.hInstance = NULL;
		ofn.lpstrFilter = "Save File\0*.sav\0";
		ofn.lpstrCustomFilter = customfilter;
		ofn.nMaxCustFilter = sizeof customfilter;
		ofn.nFilterIndex = 1;
		ofn.lpstrFile = file;
		ofn.nMaxFile = sizeof file;
		ofn.lpstrFileTitle = NULL;
		ofn.nMaxFileTitle = 0;
		ofn.lpstrInitialDir = ".";
		ofn.lpstrTitle = NULL;
		ofn.lpstrDefExt = NULL;
		ofn.Flags = OFN_OVERWRITEPROMPT;
		if(GetSaveFileNameA(&ofn)){
			std::ofstream fs(file, std::ios_base::trunc | std::ios_base::binary);
			game.serialize(fs);
			fs.close();
		}
	}
	catch (std::exception &e){
		*game.logwriter << e.what() << std::endl;
		return false;
	}
	return true;
}

bool Game::load(){
	try{
		static char customfilter[64] = "Save File\0*.sav\0";
		static char file[MAX_PATH] = "save.sav";
		OPENFILENAMEA ofn;
		ofn.lStructSize = sizeof ofn;
		ofn.hwndOwner = hWndApp;
		ofn.hInstance = NULL;
		ofn.lpstrFilter = "Save File\0*.sav\0";
		ofn.lpstrCustomFilter = customfilter;
		ofn.nMaxCustFilter = sizeof customfilter;
		ofn.nFilterIndex = 1;
		ofn.lpstrFile = file;
		ofn.nMaxFile = sizeof file;
		ofn.lpstrFileTitle = NULL;
		ofn.nMaxFileTitle = OFN_FILEMUSTEXIST;
		ofn.lpstrInitialDir = ".";
		ofn.lpstrTitle = NULL;
		ofn.lpstrDefExt = NULL;
		ofn.Flags = 0;
		if(GetOpenFileNameA(&ofn)){
			std::ifstream fs(file, std::ios_base::binary);
			game.unserialize(fs);
			fs.close();
		}
	}
	catch (std::exception &e){
		*game.logwriter << e.what() << std::endl;
		return false;
	}
	return true;
}

/// Obsolete, since we catch the key strokes via GetKeyState.
/// But it would be better to catch the Windows Messages if the frame rate is really low.
static void key_func(unsigned char key, int x, int y){
	key; x; y;
}


static LRESULT WINAPI CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam){
	static HDC hdc;
	static HGLRC hgl = NULL;
	static int moc = 0;
	switch(message){
		case WM_CREATE:
			initializeVolume();
			break;

		case WM_TIMER:
			display_func();
			break;

		case WM_CHAR:
			key_func(wParam, 0, 0);
			break;

		case WM_MOUSELEAVE:
			while(ShowCursor(TRUE) <= 0);
			mouse_tracking = 0;
			break;

		case WM_MOUSEMOVE:
			if(!mouse_tracking){
				TRACKMOUSEEVENT evt;
				evt.cbSize = sizeof evt;
				evt.dwFlags = TME_LEAVE;
				evt.hwndTrack = hWnd;
				TrackMouseEvent(&evt);
				while(0 <= ShowCursor(FALSE));
				mouse_tracking = true;
			}
			if(!mouse_captured){
				s_mousex = LOWORD(lParam);
				s_mousey = HIWORD(lParam);
//				pl.mousemove(hWnd, s_mousex - s_mousedragx, s_mousey - s_mousedragy, wParam, lParam);

/*				if((wParam & MK_RBUTTON) && !mouse_captured){
					capture_mouse();
				}*/
			}
			break;

		case WM_RBUTTONDOWN:
		case WM_LBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_LBUTTONUP:
			mouse_func(message, LOWORD(lParam), HIWORD(lParam));
			return 0;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

}

using namespace dxtest;

int WINAPI ::WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int nShow){
	ATOM atom;
	HWND hWnd;
	{
		HINSTANCE hInst;
		RECT rc = {100,100,100+windowWidth,100+windowHeight};
/*		RECT rc = {0,0,0+512,0+384};*/
		hInst = GetModuleHandle(NULL);

		{
			WNDCLASSEX wcex;

			wcex.cbSize = sizeof(WNDCLASSEX); 

			wcex.style			= CS_HREDRAW | CS_VREDRAW;
			wcex.lpfnWndProc	= (WNDPROC)WndProc;
			wcex.cbClsExtra		= 0;
			wcex.cbWndExtra		= 0;
			wcex.hInstance		= hInst;
			wcex.hIcon			= LoadIcon(hInst, IDI_APPLICATION);
			wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
			wcex.hbrBackground	= NULL;
			wcex.lpszMenuName	= NULL;
			wcex.lpszClassName	= TEXT("dxtest");
			wcex.hIconSm		= LoadIcon(hInst, IDI_APPLICATION);

			atom = RegisterClassEx(&wcex);
		}


		AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW | WS_SYSMENU | WS_CAPTION, FALSE);

		hWndApp = hWnd = CreateWindow(LPCTSTR(atom), TEXT("dxtest"), WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInst, NULL);

		if( !SUCCEEDED( InitD3D(hWnd) ) )
			return 0;

//		if( !SUCCEEDED( InitGeometry() ) )
//			return 0;

		do{
			MSG msg;
			if(PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)){
				if(GetMessage(&msg, NULL, 0, 0) <= 0)
					break;
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
				SendMessage(hWnd, WM_TIMER, 0, 0);
		}while (true);
	}
	swapchain->SetFullscreenState(FALSE, NULL);    // switch to windowed mode
	return 0;
}

void dxtest::Game::serialize(std::ostream &o){
	o.write((char*)saveFileSignature, sizeof saveFileSignature);
	o.write((char*)&saveFileVersion, sizeof saveFileVersion);
	player->serialize(o);
	world->serialize(o);
}

void dxtest::Game::unserialize(std::istream &is){
    byte signature[sizeof saveFileSignature];
	is.read((char*)signature, sizeof signature);
    if (memcmp(signature, saveFileSignature, sizeof signature))
		throw std::exception("File signature mismatch");

    int version;
	is.read((char*)&version, sizeof version);
	if(version != saveFileVersion){
		std::stringstream ss;
		ss << "File version mismatch, file = " << version << ", program = " << saveFileVersion;
		throw std::exception(ss.str().c_str());
	}

	player->unserialize(is);
	world->unserialize(is);
}

