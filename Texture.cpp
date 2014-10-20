#include "Texture.h"

#include <png.h>
#include <jpeglib.h>
#include <jerror.h>
#include <vector>
#include <stdint.h>


namespace dxtest{

struct Color
{
	uint8_t R,G,B,A;

	Color() {}

	// Constructs color by channel. Alpha is set to 0xFF (fully visible)
	// if not specified.
	Color(unsigned char r,unsigned char g,unsigned char b, unsigned char a = 0xFF)
		: R(r), G(g), B(b), A(a) { }

	// 0xAARRGGBB - Common HTML color Hex layout
	Color(unsigned c)
		: R((unsigned char)(c>>16)), G((unsigned char)(c>>8)),
		B((unsigned char)c), A((unsigned char)(c>>24)) { }

	bool operator==(const Color& b) const
	{
		return R == b.R && G == b.G && B == b.B && A == b.A;
	}

	void  GetRGBA(float *r, float *g, float *b, float* a) const
	{
		*r = R / 255.0f;
		*g = G / 255.0f;
		*b = B / 255.0f;
		*a = A / 255.0f;
	}
};



struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */

  jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

static void my_error_exit (j_common_ptr cinfo)
{
	/* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
	my_error_ptr myerr = (my_error_ptr) cinfo->err;

	/* Disable the message if the image is not a JPEG, to avoid a message
	 when PNG formatted file is about to be read. */
	/* We could postpone this until after returning, if we chose. */
	if(cinfo->err->msg_code != JERR_NO_SOI)
		(*cinfo->err->output_message) (cinfo);

	/* Return control to the setjmp point */
	longjmp(myerr->setjmp_buffer, 1);
}


static Texture *LoadJPEG(ID3D11Device *pd3d, ID3D11DeviceContext *pcon, const char *fname){
	FILE *infile = fopen(fname, "rb");
	if(!infile)
		return NULL;

	jpeg_decompress_struct cinfo;
	my_error_mgr jerr;
	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_error_exit;
	/* Establish the setjmp return context for my_error_exit to use. */
	if (setjmp(jerr.setjmp_buffer)) {
		/* If we get here, the JPEG code has signaled an error.
		 * We need to clean up the JPEG object, close the input file, and return.
		 */
		jpeg_destroy_decompress(&cinfo);
		fclose(infile);
		return NULL;
	}
	jpeg_create_decompress(&cinfo);

	/* Choose source manager */
	jpeg_stdio_src(&cinfo, infile);

	(void) jpeg_read_header(&cinfo, TRUE);
	(void) jpeg_start_decompress(&cinfo);
	JDIMENSION src_row_stride = cinfo.output_width * cinfo.output_components;
	std::vector<Color> panel;
	panel.resize(cinfo.output_width * cinfo.output_height);
	JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)
		((j_common_ptr) &cinfo, JPOOL_IMAGE, src_row_stride, 1);		/* Output row buffer */
	while (cinfo.output_scanline < cinfo.output_height) {
		(void) jpeg_read_scanlines(&cinfo, buffer, 1);

		// We cannot just memcpy it because of byte ordering.
		if(cinfo.output_components == 3) for(int j = 0; j < cinfo.output_width; j++){
			JSAMPLE *src = &buffer[0][j * cinfo.output_components];
			panel[j + cinfo.output_width * (cinfo.output_scanline - 1)] = Color(src[2], src[1], src[0], 255);
		}
		else if(cinfo.output_components == 1) for(int j = 0; j < cinfo.output_width; j++){
			JSAMPLE *src = &buffer[0][j * cinfo.output_components];
			panel[j + cinfo.output_width * (cinfo.output_scanline - 1)] = Color(src[0], src[0], src[0], 255);
		}
	}

	(void) jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	fclose(infile);

	return CreateTexture(pd3d, pcon, Texture_RGBA|Texture_GenMipmaps, cinfo.output_width, cinfo.output_height, &panel.front());
}

static Texture *LoadPNG(ID3D11Device *pd3d, ID3D11DeviceContext *pcon, const char* fname){
	FILE *fp = fopen(fname, "rb");
	if(fp) try{
		unsigned char header[8];
		fread(header, 1, sizeof header, fp);
		bool is_png = !png_sig_cmp(header, 0, sizeof header);
		if(is_png){
			png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, (png_voidp)nullptr, 
				[](png_structp, png_const_charp){}, [](png_structp, png_const_charp){});
			png_infop info_ptr = png_create_info_struct(png_ptr);
			if (!info_ptr)
			{
				png_destroy_read_struct(&png_ptr,
				(png_infopp)NULL, (png_infopp)NULL);
				throw 1;
			}
			png_infop end_info = png_create_info_struct(png_ptr);
			if (!end_info)
			{
				png_destroy_read_struct(&png_ptr, &info_ptr,
				(png_infopp)NULL);
				throw 2;
			}
			if (setjmp(png_jmpbuf(png_ptr))){
				png_destroy_read_struct(&png_ptr, &info_ptr,
				&end_info);
				fclose(fp);
				throw 3;
			}
			png_init_io(png_ptr, fp);
			png_set_sig_bytes(png_ptr, sizeof header);

			png_bytepp ret;

			{
				BITMAPINFO *bmi;
				png_uint_32 width, height;
				int bit_depth, color_type, interlace_type;
				int i;
				/* The native order of RGB components differs in order against Windows bitmaps,
				 * so we must instruct libpng to convert it. */
				png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_BGR, NULL);

				png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
					&interlace_type, NULL, NULL);

				/* Grayscale images are not supported.
				 * TODO: alpha channel? */
				if(bit_depth != 8 || color_type != PNG_COLOR_TYPE_RGB && color_type != PNG_COLOR_TYPE_RGBA && color_type != PNG_COLOR_TYPE_PALETTE)
					throw 12;

				/* Calculate number of components. */
				int comps = (color_type == PNG_COLOR_TYPE_PALETTE ? 1 : color_type == PNG_COLOR_TYPE_RGB ? 3 : 4);

				// Supports paletted images
				png_colorp pal;
				int npal = 0;
				if(color_type == PNG_COLOR_TYPE_PALETTE){
					png_get_PLTE(png_ptr, info_ptr, &pal, &npal);
				}

				/* png_get_rows returns array of pointers to rows allocated by the library,
				 * which must be copied to single bitmap buffer. */
				ret = png_get_rows(png_ptr, info_ptr);


				if(color_type == PNG_COLOR_TYPE_PALETTE){
					std::vector<Color> panel;
					panel.resize(width * height);
					for (int j = 0; j < height; j++){
						for (int i = 0; i < width; i++){
							int idx = ret[j][i * comps];
							panel[j * width + i] = Color(pal[idx].red,pal[idx].green,pal[idx].blue,255);
						}
					}
					Texture *retp = CreateTexture(pd3d, pcon, Texture_RGBA|Texture_GenMipmaps, width, height, &panel.front());
//					ret->SetSampleMode(Sample_Anisotropic|Sample_Repeat);
					return retp;
				}
				else if(color_type == PNG_COLOR_TYPE_RGB){
					std::vector<Color> panel;
					panel.resize(width * height);
					for (int j = 0; j < height; j++){
						for (int i = 0; i < width; i++){
							png_colorp pixel = (png_colorp)&ret[j][i * comps];
							panel[j * width + i] = Color(pixel->red, pixel->green, pixel->blue, 255);
						}
					}
					Texture *retp = CreateTexture(pd3d, pcon, Texture_RGBA|Texture_GenMipmaps, width, height, &panel.front());
					return retp;
				}
				else
					throw 13;
			}
		}
		fclose(fp);
	}
	catch(int e){
		fclose(fp);
	}
	return NULL;
}

Texture *LoadTexture(ID3D11Device *pd3d, ID3D11DeviceContext *pcon, const char* fname){
	const char *ext = strrchr(fname, '.');
	if(ext && !strcmp(ext, ".jpg"))
		return LoadJPEG(pd3d, pcon, fname);
	else
		return LoadPNG(pd3d, pcon, fname);
}

Texture::Texture(int fmt, int w, int h)
    : Tex(NULL), TexSv(NULL), TexRtv(NULL), TexDsv(NULL), Width(w), Height(h)
{
//    OVR_UNUSED(fmt);
//    Sampler = Ren->GetSamplerState(0);
}

Texture::~Texture()
{
}

/*void Texture::Set(int slot, ShaderStage stage) const
{
    Ren->SetTexture(stage, slot, this);
}*/

void Texture::SetSampleMode(int sm)
{
//    Sampler = Ren->GetSamplerState(sm);
}



Texture* CreateTexture(ID3D11Device *pd3d, ID3D11DeviceContext *pcon, int format, int width, int height, const void* data, int mipcount)
{
	DXGI_FORMAT d3dformat;
	int         bpp;
	switch(format & Texture_TypeMask)
	{
	case Texture_RGBA:
		bpp = 4;
		d3dformat = DXGI_FORMAT_R8G8B8A8_UNORM;
		break;
	case Texture_Depth:
		bpp = 0;
		d3dformat = DXGI_FORMAT_D32_FLOAT;
		break;
	default:
		return NULL;
	}

	int samples = (format & Texture_SamplesMask);
	if (samples < 1)
	{
		samples = 1;
	}

	Texture* NewTex = new Texture(format, width, height);
	NewTex->Samples = samples;

	D3D11_TEXTURE2D_DESC dsDesc;
	dsDesc.Width     = width;
	dsDesc.Height    = height;
	dsDesc.MipLevels = 1;
	dsDesc.ArraySize = 1;
	dsDesc.Format    = d3dformat;
	dsDesc.SampleDesc.Count = samples;
	dsDesc.SampleDesc.Quality = 0;
	dsDesc.Usage     = D3D11_USAGE_DEFAULT;
	dsDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	dsDesc.CPUAccessFlags = 0;
	dsDesc.MiscFlags      = 0;

	if (format & Texture_RenderTarget)
	{
		if ((format & Texture_TypeMask) == Texture_Depth)            
		{ // We don't use depth textures, and creating them in d3d10 requires different options.
			dsDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		}
		else
		{
			dsDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
		}
	}

	HRESULT hr = pd3d->CreateTexture2D(&dsDesc, NULL, &NewTex->Tex);
	if (FAILED(hr))
	{
	//        OVR_DEBUG_LOG_TEXT(("Failed to create 2D D3D texture."));
		delete NewTex;
		return NULL;
	}
	if (dsDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
	{
		pd3d->CreateShaderResourceView(NewTex->Tex, NULL, &NewTex->TexSv);
	}

	if (data)
	{
		pcon->UpdateSubresource(NewTex->Tex, 0, NULL, data, width * bpp, width * height * bpp);
	/*        if (format == (Texture_RGBA | Texture_GenMipmaps))
		{
			int srcw = width, srch = height;
			int level = 0;
			uint8_t* mipmaps = NULL;
			do
			{
				level++;
				int mipw = srcw >> 1;
				if (mipw < 1)
				{
					mipw = 1;
				}
				int miph = srch >> 1;
				if (miph < 1)
				{
					miph = 1;
				}
				if (mipmaps == NULL)
				{
					mipmaps = (uint8_t*)malloc(mipw * miph * 4);
				}
				FilterRgba2x2(level == 1 ? (const uint8_t*)data : mipmaps, srcw, srch, mipmaps);
				Context->UpdateSubresource(NewTex->Tex, level, NULL, mipmaps, mipw * bpp, miph * bpp);
				srcw = mipw;
				srch = miph;
			}
			while(srcw > 1 || srch > 1);

			if (mipmaps != NULL)
			{
				free(mipmaps);
			}
		}*/
	}

	if (format & Texture_RenderTarget)
	{
		if ((format & Texture_TypeMask) == Texture_Depth)
		{
			pd3d->CreateDepthStencilView(NewTex->Tex, NULL, &NewTex->TexDsv);
		}
		else
		{
			pd3d->CreateRenderTargetView(NewTex->Tex, NULL, &NewTex->TexRtv);
		}
	}

	return NewTex;
}


}
