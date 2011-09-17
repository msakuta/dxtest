#include <assert.h>
#include <windows.h>
#include <d3dx9.h>
extern "C"{
#include <clib/c.h>
#include <clib/suf/suf.h>
#include <clib/suf/sufbin.h>
#include <clib/rseq.h>
}
#include <cpplib/vec3.h>
#include <cpplib/vec4.h>
#include <cpplib/quat.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

static HWND hWndApp;
IDirect3D9 *pd3d;
IDirect3DDevice9 *pdev;
LPDIRECT3DVERTEXBUFFER9 g_pVB = NULL; // Buffer to hold vertices
LPDIRECT3DVERTEXBUFFER9 g_ground = NULL; // Ground surface vertices
LPDIRECT3DTEXTURE9      g_pTexture = NULL; // Our texture
LPDIRECT3DTEXTURE9      g_pTexture2 = NULL;


#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE)

struct CUSTOMVERTEX{
	Vec3f v;
	DWORD c;
};


const int D3DFVF_TEXTUREVERTEX = (D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1);

struct TextureVertex{
	D3DXVECTOR3 position;
	D3DCOLOR color;
	FLOAT tu, tv;
};

class Player{
public:
	Player() : pos(0, 0, -20), rot(0,0,0,1){}
	const Vec3d &getPos()const{return pos;}
	const Quatd &getRot()const{return rot;}
	void setPos(const Vec3d &apos){pos = apos;}
	void setRot(const Quatd &arot){rot = arot;}
	Vec3d pos;
	Quatd rot;
};

static Player player;

static int CC = 10560;
static suf_t *suf;

HRESULT InitD3D(HWND hWnd)
{
	pd3d = Direct3DCreate9(D3D_SDK_VERSION);
	if(!pd3d)
		return E_FAIL;

	D3DPRESENT_PARAMETERS pp;
	ZeroMemory(&pp, sizeof pp);
	pp.Windowed = TRUE;
	pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	pp.BackBufferFormat = D3DFMT_UNKNOWN;
	pp.EnableAutoDepthStencil = TRUE;
	pp.AutoDepthStencilFormat = D3DFMT_D16;

	HRESULT hr = pd3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
		D3DCREATE_SOFTWARE_VERTEXPROCESSING,
		&pp, &pdev);

	if(FAILED(hr))
		return E_FAIL;

	// Turn off culling, so we see the front and back of the triangle
	pdev->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );

	// Turn off D3D lighting, since we are providing our own vertex colors
	pdev->SetRenderState( D3DRS_LIGHTING, FALSE );

    // Turn on the zbuffer
    pdev->SetRenderState( D3DRS_ZENABLE, TRUE );

	return S_OK;
}

#if 1
HRESULT InitGeometry()
{
    // Use D3DX to create a texture from a file based image
//    if( FAILED( D3DXCreateTextureFromFile( pdev, L"banana.bmp", &g_pTexture ) ) )
	if( FAILED( D3DXCreateTextureFromFile( pdev, L"grass.jpg", &g_pTexture ) ) )
	{
		MessageBox( NULL, L"Could not find grass.jpg", L"Textures.exe", MB_OK );
		return E_FAIL;
    }

	if( FAILED( D3DXCreateTextureFromFile( pdev, L"banana.bmp", &g_pTexture2 ) ) )
	{
		MessageBox( NULL, L"Could not find banana.bmp", L"Textures.exe", MB_OK );
		return E_FAIL;
    }


	const char *fname = "../gltestplus/models/interceptor0.bin";
	{
		suf_t *ret;
		FILE *fp;
		long size;
		fp = fopen(fname, "rb");
		if(!fp)
			return NULL;
		fseek(fp, 0, SEEK_END);
		size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		ret = (suf_t*)malloc(size);
		fread(ret, size, 1, fp);
		fclose(fp);
		suf = RelocateSUF(ret);
	}

	CUSTOMVERTEX *g_Vertices = NULL;
	CC = 0;

//	double scale = 1., offset = 200.;
	double scale = 2e-2, offset = 0.;
	for(int i = 0; i < suf->np; i++) for(int j = 2; j < suf->p[i]->p.n; j++){
		for(int k = 0; k < 3; k++){
			int kk = k == 0 ? 0 : j - 2 + k;
			g_Vertices = (CUSTOMVERTEX*)::realloc(g_Vertices, (CC + 1) * sizeof *g_Vertices);
			g_Vertices[CC].v[0] = scale * suf->v[suf->p[i]->uv.v[kk].p][0] + offset;
			g_Vertices[CC].v[1] = scale * suf->v[suf->p[i]->uv.v[kk].p][1] + offset;
			g_Vertices[CC].v[2] = scale * suf->v[suf->p[i]->uv.v[kk].p][2] + offset;
//			g_Vertices[CC].v[3] = 1.f;
			g_Vertices[CC].c = 0xff0000 + i * 255 / suf->np;
			CC++;
		}
//		g_Vertices[i].v = Vec4f(cos(float(i)) * 200 + 200, sin(float(i)) * 200 + 200, 0.5f, 1.f);
//		g_Vertices[i].c = 0xff0000 + i * 255 / CC;
	}

    if( FAILED( pdev->CreateVertexBuffer(CC * sizeof(*g_Vertices), 0, D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &g_pVB, NULL ) ) )
    {
        return E_FAIL;
    }

    VOID* pVertices;

    if( FAILED( g_pVB->Lock( 0, CC * sizeof(*g_Vertices), (void**)&pVertices, 0 ) ) )
        return E_FAIL;

    memcpy( pVertices, g_Vertices, CC * sizeof(*g_Vertices) );
    g_pVB->Unlock();

	free(g_Vertices);


	TextureVertex vertices[] = {
		{Vec4f(0, 0, 0, 1), D3DCOLOR_XRGB(255, 255, 127), 0, 0},
		{Vec4f(0, 0, 1, 1), D3DCOLOR_XRGB(255, 127, 127), 0, 1},
		{Vec4f(1, 0, 1, 1), D3DCOLOR_XRGB(127, 255, 127), 1, 1},
		{Vec4f(1, 0, 1, 1), D3DCOLOR_XRGB(127, 255, 127), 1, 1},
		{Vec4f(1, 0, 0, 1), D3DCOLOR_XRGB(127, 255, 127), 1, 1},
		{Vec4f(0, 0, 0, 1), D3DCOLOR_XRGB(255, 255, 127), 0, 0},

		{Vec4f(0, 1, 0, 1), D3DCOLOR_XRGB(255, 255, 127), 0, 0},
		{Vec4f(1, 1, 0, 1), D3DCOLOR_XRGB(255, 255, 255), 1, 0},
		{Vec4f(1, 1, 1, 1), D3DCOLOR_XRGB(127, 255, 127), 1, 1},
		{Vec4f(1, 1, 1, 1), D3DCOLOR_XRGB(127, 255, 127), 1, 1},
		{Vec4f(0, 1, 1, 1), D3DCOLOR_XRGB(255, 127, 127), 0, 1},
		{Vec4f(0, 1, 0, 1), D3DCOLOR_XRGB(255, 255, 127), 0, 0},

		{Vec4f(0, 0, 0, 1), D3DCOLOR_XRGB(255, 255, 127), 0, 0},
		{Vec4f(1, 0, 0, 1), D3DCOLOR_XRGB(255, 255, 255), 1, 0},
		{Vec4f(1, 1, 0, 1), D3DCOLOR_XRGB(127, 255, 127), 1, 1},
		{Vec4f(1, 1, 0, 1), D3DCOLOR_XRGB(127, 255, 127), 1, 1},
		{Vec4f(0, 1, 0, 1), D3DCOLOR_XRGB(255, 127, 127), 0, 1},
		{Vec4f(0, 0, 0, 1), D3DCOLOR_XRGB(255, 255, 127), 0, 0},

		{Vec4f(0, 0, 1, 1), D3DCOLOR_XRGB(255, 255, 127), 0, 0},
		{Vec4f(0, 1, 1, 1), D3DCOLOR_XRGB(255, 127, 127), 0, 1},
		{Vec4f(1, 1, 1, 1), D3DCOLOR_XRGB(127, 255, 127), 1, 1},
		{Vec4f(1, 1, 1, 1), D3DCOLOR_XRGB(127, 255, 127), 1, 1},
		{Vec4f(1, 0, 1, 1), D3DCOLOR_XRGB(255, 255, 255), 1, 0},
		{Vec4f(0, 0, 1, 1), D3DCOLOR_XRGB(255, 255, 127), 0, 0},

		{Vec4f(0, 0, 0, 1), D3DCOLOR_XRGB(255, 255, 127), 0, 0},
		{Vec4f(0, 1, 0, 1), D3DCOLOR_XRGB(255, 255, 255), 1, 0},
		{Vec4f(0, 1, 1, 1), D3DCOLOR_XRGB(127, 255, 127), 1, 1},
		{Vec4f(0, 1, 1, 1), D3DCOLOR_XRGB(127, 255, 127), 1, 1},
		{Vec4f(0, 0, 1, 1), D3DCOLOR_XRGB(255, 127, 127), 0, 1},
		{Vec4f(0, 0, 0, 1), D3DCOLOR_XRGB(255, 255, 127), 0, 0},

		{Vec4f(1, 0, 0, 1), D3DCOLOR_XRGB(255, 255, 127), 0, 0},
		{Vec4f(1, 0, 1, 1), D3DCOLOR_XRGB(255, 127, 127), 0, 1},
		{Vec4f(1, 1, 1, 1), D3DCOLOR_XRGB(127, 255, 127), 1, 1},
		{Vec4f(1, 1, 1, 1), D3DCOLOR_XRGB(127, 255, 127), 1, 1},
		{Vec4f(1, 1, 0, 1), D3DCOLOR_XRGB(255, 255, 255), 1, 0},
		{Vec4f(1, 0, 0, 1), D3DCOLOR_XRGB(255, 255, 127), 0, 0},
	};

    if( FAILED( pdev->CreateVertexBuffer(sizeof(vertices), 0, D3DFVF_TEXTUREVERTEX, D3DPOOL_DEFAULT, &g_ground, NULL ) ) )
    {
        return E_FAIL;
    }

	if( FAILED( g_ground->Lock( 0, sizeof(vertices), (void**)&pVertices, 0 ) ) )
		return E_FAIL;

    memcpy( pVertices, vertices, sizeof(vertices) );
    g_ground->Unlock();


	return S_OK;
}
#endif

//-----------------------------------------------------------------------------
// Name: SetupMatrices()
// Desc: Sets up the world, view, and projection transform matrices.
//-----------------------------------------------------------------------------
VOID SetupMatrices()
{

    // Set up our view matrix. A view matrix can be defined given an eye point,
    // a point to lookat, and a direction for which way is up. Here, we set the
    // eye five units back along the z-axis and up three units, look at the
    // origin, and define "up" to be in the y-direction.
    D3DXVECTOR3 vEyePt( 0.0f, 1.50f, -20.0f );
    D3DXVECTOR3 vLookatPt( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vUpVec( 0.0f, 1.0f, 0.0f );
    D3DXMATRIXA16 matEye;
//    D3DXMatrixLookAtLH( &matEye, &vEyePt, &vLookatPt, &vUpVec );
	D3DXMatrixRotationQuaternion(&matEye, (D3DXQUATERNION*)(&player.rot.cast<float>()));
	D3DXMATRIXA16 matRot;
	FLOAT fYaw = clock() % 5000 * (2.f * D3DX_PI) / 5000.f;
//	D3DXMatrixRotationY(&matRot, fYaw);
	D3DXMatrixTranslation(&matRot, -player.pos[0], -player.pos[1], -player.pos[2]);
    D3DXMATRIXA16 matView;
	D3DXMatrixMultiply(&matView, &matRot, &matEye);
    pdev->SetTransform( D3DTS_VIEW, &matView );

    // For the projection matrix, we set up a perspective transform (which
    // transforms geometry from 3D view space to 2D viewport space, with
    // a perspective divide making objects smaller in the distance). To build
    // a perpsective transform, we need the field of view (1/4 pi is common),
    // the aspect ratio, and the near and far clipping planes (which define at
    // what distances geometry should be no longer be rendered).
    D3DXMATRIXA16 matProj;
    D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI / 4, 1.0f, 1.0f, 100.0f );
    pdev->SetTransform( D3DTS_PROJECTION, &matProj );

	pdev->LightEnable(0, FALSE);
}

void RotateModel(){
    // For our world matrix, we will just rotate the object about the y-axis.
    D3DXMATRIXA16 matWorld;

    // Set up the rotation matrix to generate 1 full rotation (2*PI radians) 
    // every 1000 ms. To avoid the loss of precision inherent in very high 
    // floating point numbers, the system time is modulated by the rotation 
    // period before conversion to a radian angle.
    UINT iTime = clock() % 5000;
    FLOAT fAngle = iTime * ( 2.0f * D3DX_PI ) / 5000.0f;
	FLOAT fPitch = sin(clock() % 33333 * (2.f * D3DX_PI) / 33333.f);
	D3DXMATRIXA16 matPitch;
	D3DXMatrixRotationX(&matPitch, fPitch);
	D3DXMATRIXA16 matYaw;
    D3DXMatrixRotationY(&matYaw, fAngle);
	D3DXMatrixMultiply(&matWorld, &matYaw, &matPitch);
    pdev->SetTransform( D3DTS_WORLD, &matWorld );
}

const int CELLSIZE = 16;

class Cell{
public:
	enum Type{Air, Grass, Banana};

	Cell(Type t = Air) : type(t){}
	Type getType()const{return type;}
protected:
	enum Type type;
};

static Cell massvolume[CELLSIZE][CELLSIZE][CELLSIZE];


class PerlinNoiseCallback{
public:
	virtual void operator()(float, int ix, int iy) = 0;
};

class FieldAssign : public PerlinNoiseCallback{
	typedef float (&fieldType)[CELLSIZE][CELLSIZE];
	fieldType field;
public:
	FieldAssign(fieldType field) : field(field){}
	void operator()(float f, int ix, int iy){
		field[ix][iy] = f;
	}
};

static void perlin_noise(long seed, PerlinNoiseCallback &callback){
	byte work[CELLSIZE][CELLSIZE];
	float work2[CELLSIZE][CELLSIZE] = {0};
	float maxwork2 = 0;
	int octave;
	struct random_sequence rs;
	init_rseq(&rs, seed);
	for(octave = 0; (1 << octave) < CELLSIZE; octave += 1){
		int cell = 1 << octave;
		int xi, yi, zi;
		int k;
		for(xi = 0; xi < CELLSIZE / cell; xi++) for(yi = 0; yi < CELLSIZE / cell; yi++){
			int base;
			base = rseq(&rs) % 256;
			if(octave == 0)
				callback(base, xi, yi);
			else
				work[xi][yi] = /*rseq(&rs) % 32 +*/ base;
		}
		if(octave == 0){
			for(xi = 0; xi < CELLSIZE; xi++) for(yi = 0; yi < CELLSIZE; yi++)
				work2[xi][yi] = work[xi][yi];
		}
		else for(xi = 0; xi < CELLSIZE; xi++) for(yi = 0; yi < CELLSIZE; yi++){
			int xj, yj, zj;
			int sum[4] = {0};
			for(k = 0; k < 1; k++){
				for(xj = 0; xj <= 1; xj++) for(yj = 0; yj <= 1; yj++){
					sum[k] += (double)work[xi / cell + xj][yi / cell + yj]
					* (xj ? xi % cell : (cell - xi % cell - 1)) / (double)cell
					* (yj ? yi % cell : (cell - yi % cell - 1)) / (double)cell;
				}
				work2[xi][yi] = work2[xi][yi] + sum[k];
				if(maxwork2 < work2[xi][yi])
					maxwork2 = work2[xi][yi];
			}
		}
	}
	for(int xi = 0; xi < CELLSIZE; xi++) for(int yi = 0; yi < CELLSIZE; yi++){
		callback(work2[xi][yi] / maxwork2, xi, yi);
	}
}

static void initializeVolume(){
	float field[CELLSIZE][CELLSIZE];
	perlin_noise(12321, FieldAssign(field));
	for(int ix = 0; ix < CELLSIZE; ix++) for(int iy = 0; iy < CELLSIZE; iy++) for(int iz = 0; iz < CELLSIZE; iz++){
		massvolume[ix][iy][iz] = Cell(field[ix][iz] * 8 < iy ? Cell::Air : Cell::Grass);
	}
}

static void display_func(){
	static int frame = 0;
	pdev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(frame / 16, frame / 3, frame), 1.0f, 0);
	frame++;

/*	for(int i = 0; i < 10; i++){
		int ix, iy, iz;
		ix = rand() % CELLSIZE;
		iy = rand() % CELLSIZE;
		iz = rand() % CELLSIZE;
		massvolume[ix][iy][iz] = Cell(Cell::Type(rand() % 3));
	}*/

	if(SUCCEEDED(pdev->BeginScene())){
		SetupMatrices();

		D3DXMATRIXA16 matWorld;
		D3DXMatrixIdentity(&matWorld);
	    pdev->SetTransform( D3DTS_WORLD, &matWorld );

		pdev->SetTexture( 0, g_pTexture);
		pdev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		pdev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        pdev->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
        pdev->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

		pdev->SetStreamSource( 0, g_ground, 0, sizeof( TextureVertex ) );
        pdev->SetFVF( D3DFVF_TEXTUREVERTEX );

		for(int ix = 0; ix < CELLSIZE; ix++){
			for(int iy = 0; iy < CELLSIZE; iy++){
				for(int iz = 0; iz < CELLSIZE; iz++){
					if(massvolume[ix][iy][iz].getType() != Cell::Air){
						pdev->SetTexture( 0, massvolume[ix][iy][iz].getType() == Cell::Grass ? g_pTexture : g_pTexture2);
						D3DXMatrixTranslation(&matWorld, (ix - CELLSIZE / 2) * 1, (iy - CELLSIZE / 2) * 1, (iz - CELLSIZE / 2) * 1);
						pdev->SetTransform(D3DTS_WORLD, &matWorld);
						pdev->DrawPrimitive( D3DPT_TRIANGLELIST, 0, 12 );
					}
				}
			}
		}

		RotateModel();

		pdev->SetStreamSource( 0, g_pVB, 0, sizeof( CUSTOMVERTEX ) );
        pdev->SetFVF( D3DFVF_CUSTOMVERTEX );

		for(int n = 0; n < 100; n++)
	        pdev->DrawPrimitive( D3DPT_TRIANGLELIST, 0, CC/3 );
/*		for(int n = 0; n < 20; n++){
			int c = 0;
			for(int i = 0; i < suf->np; i++){
				pdev->DrawPrimitive(D3DPT_TRIANGLEFAN, c, suf->p[i]->uv.n);
				c += suf->p[i]->uv.n;
			}
		}*/

		pdev->EndScene();
	}

	HRESULT hr = pdev->Present(NULL, NULL, NULL, NULL);
//	if(hr == D3DERR_DEVICELOST)
}

const double movespeed = .1;
const double rotatespeed = acos(0.) / 10.;

static void key_func(unsigned char key, int x, int y){
	switch(key){
		case 'w': player.pos += movespeed * Vec3d(0,0,1); break;
		case 's': player.pos += movespeed * Vec3d(0,0,-1); break;
		case 'a': player.pos += movespeed * Vec3d(-1,0,0); break;
		case 'd': player.pos += movespeed * Vec3d(1,0,0); break;
		case 'q': player.pos += movespeed * Vec3d(0,1,0); break;
		case 'z': player.pos += movespeed * Vec3d(0,-1,0); break;
		case '4': player.rot = player.rot.rotate(rotatespeed, player.rot.trans(Vec3d(0, 1, 0))); break;
		case '6': player.rot = player.rot.rotate(rotatespeed, player.rot.trans(Vec3d(0, -1, 0))); break;
		case '8': player.rot = player.rot.rotate(rotatespeed, player.rot.trans(Vec3d(1, 0, 0))); break;
		case '2': player.rot = player.rot.rotate(rotatespeed, player.rot.trans(Vec3d(-1, 0, 0))); break;
	}
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

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int nShow){
	ATOM atom;
	HWND hWnd;
	{
		HINSTANCE hInst;
		RECT rc = {100,100,100+1024,100+768};
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
			wcex.lpszClassName	= (L"dxtest");
			wcex.hIconSm		= LoadIcon(hInst, IDI_APPLICATION);

			atom = RegisterClassEx(&wcex);
		}


		AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW | WS_SYSMENU | WS_CAPTION, FALSE);

		hWndApp = hWnd = CreateWindow(LPCTSTR(atom), L"dxtest", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInst, NULL);

		if( !SUCCEEDED( InitD3D(hWnd) ) )
			return 0;
        
		if( !SUCCEEDED( InitGeometry() ) )
			return 0;

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
	pd3d->Release();
	return 0;
}
