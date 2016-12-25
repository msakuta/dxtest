#ifndef TEXTURE_H
#define TEXTURE_H

#include <D3D11.h>

namespace dxtest{

enum TextureFormat
{
	Texture_RGBA            = 0x0100,
	Texture_Depth           = 0x8000,
	Texture_TypeMask        = 0xff00,
	Texture_SamplesMask     = 0x00ff,
	Texture_RenderTarget    = 0x10000,
	Texture_GenMipmaps      = 0x20000,
};

class Texture{
public:
	ID3D11Texture2D*            Tex;
	ID3D11ShaderResourceView*   TexSv;
	ID3D11RenderTargetView*     TexRtv;
	ID3D11DepthStencilView*     TexDsv;
	mutable ID3D11SamplerState* Sampler;
	int                         Width, Height;
	int                         Samples;

	Texture(int fmt, int w, int h);
	virtual ~Texture();

	virtual int GetWidth() const    { return Width; }
	virtual int GetHeight() const   { return Height; }
	virtual int GetSamples() const  { return Samples; }

	virtual void SetSampleMode(int sm);

	// Updates texture to point to specified resources
	//  - used for slave rendering.
	void UpdatePlaceholderTexture(ID3D11Texture2D* texture, ID3D11ShaderResourceView* psrv)
	{
		Tex     = texture;
		TexSv   = psrv;
		TexRtv  = NULL;
		TexDsv  = NULL;

		D3D11_TEXTURE2D_DESC desc;
		texture->GetDesc(&desc);
		Width = desc.Width;
		Height= desc.Height;
	}


//	virtual void Set(int slot, ShaderStage stage = Shader_Fragment) const;
};

Texture *CreateTexture(ID3D11Device *pd3d, ID3D11DeviceContext *pcon, int format, int width, int height, const void* data, int mipcount = 0);
Texture *LoadTexture(ID3D11Device *pd3d, ID3D11DeviceContext *pcon, const char* fname);

}

#endif
