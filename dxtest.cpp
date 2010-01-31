#include <windows.h>
#include <d3d9.h>

static HWND hWndApp;
IDirect3D9 *pd3d;
IDirect3DDevice9 *pdev;

static void display_func(){
	pdev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0,0,255), 1.0f, 0);

	if(SUCCEEDED(pdev->BeginScene())){
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
