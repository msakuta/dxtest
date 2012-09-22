#include "perlinNoise.h"
#include "Player.h"
#include "World.h"
#include "Game.h"
#include <assert.h>
#include <windows.h>
#include <d3dx9.h>
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
/** \file
 *  \brief The main source
 */


namespace dxtest{

static HWND hWndApp;
IDirect3D9 *pd3d;
IDirect3DDevice9 *pdev;
LPDIRECT3DVERTEXBUFFER9 g_pVB = NULL; // Buffer to hold vertices
LPDIRECT3DVERTEXBUFFER9 g_ground = NULL; // Ground surface vertices
LPDIRECT3DTEXTURE9      g_pTextures[4] = {NULL}; // Our texture
const char *textureNames[4] = {"cursor.png", "grass.jpg", "dirt.jpg", "gravel.png"};
LPD3DXFONT g_font;
LPD3DXSPRITE g_sprite;

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

struct CUSTOMVERTEX{
	Vec3f v;
	DWORD c;
};


const int D3DFVF_TEXTUREVERTEX = (D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1);

struct TextureVertex{
	D3DXVECTOR3 position;
	D3DXVECTOR3 normal;
	FLOAT tu, tv;
};



static Game game;
static World world(game);
static Player player(game);





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

	D3DXCreateFont(pdev, 20, 0, FW_BOLD, 0, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH | FF_DONTCARE, TEXT("Courier"), &g_font );

	D3DXCreateSprite(pdev, &g_sprite);

	return S_OK;
}

#if 1
HRESULT InitGeometry()
{
    // Use D3DX to create a texture from a file based image
//    if( FAILED( D3DXCreateTextureFromFile( pdev, L"banana.bmp", &g_pTexture ) ) )
	for(int i = 0; i < numof(g_pTextures); i++){
		if( FAILED( D3DXCreateTextureFromFileA( pdev, textureNames[i], &g_pTextures[i] ) ) )
		{
			std::stringstream ss;
			ss << "Could not find " << textureNames[i];
			MessageBoxA( NULL, ss.str().c_str(), "Texture Load Error", MB_OK );
			return E_FAIL;
		}
	}

/*	if( FAILED( D3DXCreateTextureFromFile( pdev, L"banana.bmp", &g_pTexture2 ) ) )
	{
		MessageBox( NULL, L"Could not find banana.bmp", L"Textures.exe", MB_OK );
		return E_FAIL;
    }*/

#if 0
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
#endif

	CUSTOMVERTEX *g_Vertices = NULL;
	CC = 0;

//	double scale = 1., offset = 200.;
#if 0
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
#else
    VOID* pVertices;
#endif

	TextureVertex vertices[] = {
		{Vec4f(0, 0, 0, 1), Vec4f(0, -1, 0, 0), 0, 0},
		{Vec4f(0, 0, 1, 1), Vec4f(0, -1, 0, 0), 0, 1},
		{Vec4f(1, 0, 1, 1), Vec4f(0, -1, 0, 0), 1, 1},
		{Vec4f(1, 0, 1, 1), Vec4f(0, -1, 0, 0), 1, 1},
		{Vec4f(1, 0, 0, 1), Vec4f(0, -1, 0, 0), 1, 0},
		{Vec4f(0, 0, 0, 1), Vec4f(0, -1, 0, 0), 0, 0},

		{Vec4f(0, 1, 0, 1), Vec4f(0, 1, 0, 0), 0, 0},
		{Vec4f(1, 1, 0, 1), Vec4f(0, 1, 0, 0), 1, 0},
		{Vec4f(1, 1, 1, 1), Vec4f(0, 1, 0, 0), 1, 1},
		{Vec4f(1, 1, 1, 1), Vec4f(0, 1, 0, 0), 1, 1},
		{Vec4f(0, 1, 1, 1), Vec4f(0, 1, 0, 0), 0, 1},
		{Vec4f(0, 1, 0, 1), Vec4f(0, 1, 0, 0), 0, 0},

		{Vec4f(0, 0, 0, 1), Vec4f(0, 0, -1, 0), 0, 0},
		{Vec4f(1, 0, 0, 1), Vec4f(0, 0, -1, 0), 1, 0},
		{Vec4f(1, 1, 0, 1), Vec4f(0, 0, -1, 0), 1, 1},
		{Vec4f(1, 1, 0, 1), Vec4f(0, 0, -1, 0), 1, 1},
		{Vec4f(0, 1, 0, 1), Vec4f(0, 0, -1, 0), 0, 1},
		{Vec4f(0, 0, 0, 1), Vec4f(0, 0, -1, 0), 0, 0},

		{Vec4f(0, 0, 1, 1), Vec4f(0, 0, 1, 0), 0, 0},
		{Vec4f(0, 1, 1, 1), Vec4f(0, 0, 1, 0), 0, 1},
		{Vec4f(1, 1, 1, 1), Vec4f(0, 0, 1, 0), 1, 1},
		{Vec4f(1, 1, 1, 1), Vec4f(0, 0, 1, 0), 1, 1},
		{Vec4f(1, 0, 1, 1), Vec4f(0, 0, 1, 0), 1, 0},
		{Vec4f(0, 0, 1, 1), Vec4f(0, 0, 1, 0), 0, 0},

		{Vec4f(0, 0, 0, 1), Vec4f(-1, 0, 0, 0), 0, 0},
		{Vec4f(0, 1, 0, 1), Vec4f(-1, 0, 0, 0), 1, 0},
		{Vec4f(0, 1, 1, 1), Vec4f(-1, 0, 0, 0), 1, 1},
		{Vec4f(0, 1, 1, 1), Vec4f(-1, 0, 0, 0), 1, 1},
		{Vec4f(0, 0, 1, 1), Vec4f(-1, 0, 0, 0), 0, 1},
		{Vec4f(0, 0, 0, 1), Vec4f(-1, 0, 0, 0), 0, 0},

		{Vec4f(1, 0, 0, 1), Vec4f(1, 0, 0, 0), 0, 0},
		{Vec4f(1, 0, 1, 1), Vec4f(1, 0, 0, 0), 0, 1},
		{Vec4f(1, 1, 1, 1), Vec4f(1, 0, 0, 0), 1, 1},
		{Vec4f(1, 1, 1, 1), Vec4f(1, 0, 0, 0), 1, 1},
		{Vec4f(1, 1, 0, 1), Vec4f(1, 0, 0, 0), 1, 0},
		{Vec4f(1, 0, 0, 1), Vec4f(1, 0, 0, 0), 0, 0},
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

static D3DXPLANE frustum[6];

//-----------------------------------------------------------------------------
// Name: SetupMatrices()
// Desc: Sets up the world, view, and projection transform matrices.
//-----------------------------------------------------------------------------
VOID SetupMatrices()
{

	D3DXMATRIXA16 matEye;
	D3DXMatrixRotationQuaternion(&matEye, (D3DXQUATERNION*)(&player.rot.cast<float>()));
	D3DXMATRIXA16 matRot;
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

}

/// <summary>Test if given bounding box intersects or included in a frustum.</summary>
/// <remarks>Test assumes frustum planes face inward.</remarks>
/// <returns>True if intersects</returns>
bool FrustumCheck(D3DXVECTOR3 min, D3DXVECTOR3 max, const D3DXPLANE* frustum)
{
	D3DXVECTOR3 P;
	D3DXVECTOR3 Q;

	for(int i = 0; i < 6; ++i)
	{
		// For each coordinate axis x, y, z...
		for(int j = 0; j < 3; ++j)
		{
			// Make PQ point in the same direction as the plane normal on this axis.
			if( frustum[i][j] > 0.0f )
			{
				P[j] = min[j];
				Q[j] = max[j];
			}
			else 
			{
				P[j] = max[j];
				Q[j] = min[j];
			}
		}

		if(D3DXPlaneDotCoord(&frustum[i], &Q) < 0.0f  )
			return false;
	}
	return true;
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

void dxtest::Game::draw(double dt)const{

	pdev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(127, 191, 255), 1.0f, 0);


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

/*		pdev->SetTexture( 0, g_pTexture);
		pdev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		pdev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        pdev->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
        pdev->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );*/

		pdev->SetStreamSource( 0, g_ground, 0, sizeof( TextureVertex ) );
        pdev->SetFVF( D3DFVF_TEXTUREVERTEX );

		const Vec3i inf = World::real2ind(player->getPos());

		for(World::VolumeMap::iterator it = world->volume.begin(); it != world->volume.end(); it++){
			const Vec3i &key = it->first;
			CellVolume &cv = it->second;

			// If all content is air, skip drawing
			if(cv.getSolidCount() == 0)
				continue;

			// Examine if intersects or included in viewing frustum
			if(!FrustumCheck((D3DXVECTOR3)World::ind2real(key * CELLSIZE).cast<float>(), (D3DXVECTOR3)World::ind2real((key + Vec3i(1,1,1)) * CELLSIZE).cast<float>(), frustum))
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
						pdev->SetTexture(0, g_pTextures[cell.getType() & ~Cell::HalfBit]);
						D3DXMatrixTranslation(&matWorld,
							it->first[0] * CELLSIZE + (ix - CELLSIZE / 2),
							it->first[1] * CELLSIZE + (iy - CELLSIZE / 2),
							it->first[2] * CELLSIZE + (iz - CELLSIZE / 2));
						if(cell.getType() & Cell::HalfBit){
							D3DXMATRIXA16 matscale, matresult;
							D3DXMatrixScaling(&matscale, 1, 0.5, 1.);
							D3DXMatrixMultiply(&matresult, &matscale, &matWorld);
							pdev->SetTransform(D3DTS_WORLD, &matresult);
						}
						else
							pdev->SetTransform(D3DTS_WORLD, &matWorld);
						if(!x0 && !x1 && !y0 && !y1)
							pdev->DrawPrimitive( D3DPT_TRIANGLELIST, 0, 12 );
						else{
							if(!x0)
								pdev->DrawPrimitive( D3DPT_TRIANGLELIST, 8 * 3, 2 );
							if(!x1)
								pdev->DrawPrimitive( D3DPT_TRIANGLELIST, 10 * 3, 2 );
							if(!y0)
								pdev->DrawPrimitive( D3DPT_TRIANGLELIST, 0, 2 );
							if(!y1)
								pdev->DrawPrimitive( D3DPT_TRIANGLELIST, 2 * 3, 2 );
							if(!z0)
								pdev->DrawPrimitive( D3DPT_TRIANGLELIST, 4 * 3, 2 );
							if(!z1)
								pdev->DrawPrimitive( D3DPT_TRIANGLELIST, 6 * 3, 2 );
						}
					}
				}
			}
		}

		{
			RECT r = {-numof(g_pTextures) / 2 * 64 + windowWidth / 2, windowHeight - 64, numof(g_pTextures) / 2 * 64 + windowWidth / 2, windowHeight};
			D3DXMATRIX mat, matscale, mattrans;
			static const Cell::Type types[] = {Cell::Grass, Cell::Dirt, Cell::Gravel, Cell::HalfGrass, Cell::HalfDirt, Cell::HalfGravel};
			static const float scales[] = {1, 0.125f, 0.5f, 0.5f};
			static const int sizes[] = {64, 512, 128, 128};
			D3DRECT dr = {-numof(types) / 2 * 64 + windowWidth / 2 - 8, windowHeight - 64 - 8, numof(types) / 2 * 64 + windowWidth / 2 + 8, windowHeight};
			
			pdev->Clear(1, &dr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 63), 0.0f, 0);
			for(int h = 0; h < numof(types); h++){
				int i = types[h] % numof(g_pTextures);
				Cell::Type t = types[h];
				bool half = t & Cell::HalfBit;

				D3DXMatrixScaling( &matscale, scales[i], scales[i], 1. );
				D3DXMatrixTranslation(&mattrans, (h - numof(types) / 2) * 64 + windowWidth / 2, windowHeight - 64 + (half ? 32 : 0), 0);
				D3DXMatrixMultiply(&mat, &matscale, &mattrans);
				g_sprite->SetTransform(&mat);
				g_sprite->Begin(D3DXSPRITE_ALPHABLEND);
				RECT srcrect = {0, (half ? sizes[i] / 2 : 0), sizes[i], sizes[i]};
				g_sprite->Draw(g_pTextures[i], &srcrect, NULL, &D3DXVECTOR3(0, 0, 0),
					D3DCOLOR_ARGB(player->curtype == t ? 255 : 127,255,255,255));
				g_sprite->End();

				// Show cursor
				if(player->curtype == t){
					g_sprite->SetTransform(&mattrans);
					g_sprite->Begin(D3DXSPRITE_ALPHABLEND);
					g_sprite->Draw(g_pTextures[0], NULL, NULL, &D3DXVECTOR3(0, 0, 0),
						D3DCOLOR_ARGB(255,255,255,255));
					g_sprite->End();
				}
				r.left = (h - numof(types) / 2) * 64 + windowWidth / 2;
				r.right = r.left + 64;
				g_font->DrawTextA(NULL, dstring() << player->getBricks(t), -1, &r, 0, D3DCOLOR_ARGB(255, 255, 25, 25));
			}
		}

		RECT rct = {0, 0, 500, 20};
		g_font->DrawTextA(NULL, dstring() << "Frametime: " << dt, -1, &rct, 0, D3DCOLOR_ARGB(255, 255, 25, 25));
		rct.top += 20, rct.bottom += 20;
		g_font->DrawTextA(NULL, dstring() << "pos: " << player->pos[0] << ", " << player->pos[1] << ", " << player->pos[2], -1, &rct, 0, D3DCOLOR_ARGB(255, 255, 25, 25));
		rct.top += 20, rct.bottom += 20;
		g_font->DrawTextA(NULL, dstring() << "bricks: " << player->bricks[1] << ", " << player->bricks[2] << ", " << player->bricks[3], -1, &rct, 0, D3DCOLOR_ARGB(255, 255, 25, 25));
		rct.top += 20, rct.bottom += 20;
		g_font->DrawTextA(NULL, dstring() << "abund: " << world->getBricks(1) << ", " << world->getBricks(2) << ", " << world->getBricks(3), -1, &rct, 0, D3DCOLOR_ARGB(255, 255, 25, 25));

		pdev->EndScene();
	}

	HRESULT hr = pdev->Present(NULL, NULL, NULL, NULL);
//	if(hr == D3DERR_DEVICELOST)
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

