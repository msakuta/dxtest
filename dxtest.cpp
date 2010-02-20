#include <assert.h>
#include <windows.h>
#include <d3dx9.h>
extern "C"{
#include <clib/c.h>
#include <clib/suf/suf.h>
#include <clib/suf/sufbin.h>
}
#include <cpplib/vec3.h>
#include <cpplib/vec4.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

static HWND hWndApp;
IDirect3D9 *pd3d;
IDirect3DDevice9 *pdev;
LPDIRECT3DVERTEXBUFFER9 g_pVB = NULL; // Buffer to hold vertices

typedef Vec4<float> Vec4f;
typedef Vec3<float> Vec3f;

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE)

struct CUSTOMVERTEX{
	Vec3f v;
	DWORD c;
};

static int CC = 10560;
static suf_t *suf;

#if 1
HRESULT InitVB()
{
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

    return S_OK;
}
#endif

//-----------------------------------------------------------------------------
// Name: SetupMatrices()
// Desc: Sets up the world, view, and projection transform matrices.
//-----------------------------------------------------------------------------
VOID SetupMatrices()
{
    // For our world matrix, we will just rotate the object about the y-axis.
    D3DXMATRIXA16 matWorld;

    // Set up the rotation matrix to generate 1 full rotation (2*PI radians) 
    // every 1000 ms. To avoid the loss of precision inherent in very high 
    // floating point numbers, the system time is modulated by the rotation 
    // period before conversion to a radian angle.
    UINT iTime = clock() % 1000;
    FLOAT fAngle = iTime * ( 2.0f * D3DX_PI ) / 1000.0f;
    D3DXMatrixRotationY( &matWorld, fAngle );
    pdev->SetTransform( D3DTS_WORLD, &matWorld );

    // Set up our view matrix. A view matrix can be defined given an eye point,
    // a point to lookat, and a direction for which way is up. Here, we set the
    // eye five units back along the z-axis and up three units, look at the
    // origin, and define "up" to be in the y-direction.
    D3DXVECTOR3 vEyePt( 0.0f, 3.0f,-5.0f );
    D3DXVECTOR3 vLookatPt( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vUpVec( 0.0f, 1.0f, 0.0f );
    D3DXMATRIXA16 matView;
    D3DXMatrixLookAtLH( &matView, &vEyePt, &vLookatPt, &vUpVec );
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
}



static void display_func(){
	static int frame = 0;
	pdev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(frame / 16, frame / 3, frame), 1.0f, 0);
	frame++;

	if(SUCCEEDED(pdev->BeginScene())){
		SetupMatrices();
        // Draw the triangles in the vertex buffer. This is broken into a few
        // steps. We are passing the vertices down a "stream", so first we need
        // to specify the source of that stream, which is our vertex buffer. Then
        // we need to let D3D know what vertex shader to use. Full, custom vertex
        // shaders are an advanced topic, but in most cases the vertex shader is
        // just the FVF, so that D3D knows what type of vertices we are dealing
        // with. Finally, we call DrawPrimitive() which does the actual rendering
        // of our geometry (in this case, just one triangle).
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

static LRESULT WINAPI CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam){
	static HDC hdc;
	static HGLRC hgl = NULL;
	static int moc = 0;
	switch(message){
		case WM_CREATE:
			break;

		case WM_TIMER:
			display_func();
			break;

		case WM_CHAR:
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
	pd3d = Direct3DCreate9(D3D_SDK_VERSION);
	if(!pd3d)
		return -1;
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

		D3DPRESENT_PARAMETERS pp;
		ZeroMemory(&pp, sizeof pp);
		pp.Windowed = TRUE;
		pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		pp.BackBufferFormat = D3DFMT_UNKNOWN;

		HRESULT hr = pd3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &pdev);

		if(FAILED(hr))
			return 0;
        
		if( !SUCCEEDED( InitVB() ) )
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
