#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

#include "ps2icon.h"
#include "ps2render.h"
#include "mcio.h"


static uint32_t TIM2RGBA(const uint8_t *buf)
{
	uint8_t RGBA[4];
	uint16_t lRGB = (int16_t) (buf[1] << 8) | buf[0];

	RGBA[0] = 8 * (lRGB & 0x1F);
	RGBA[1] = 8 * ((lRGB >> 5) & 0x1F);
	RGBA[2] = 8 * (lRGB >> 10);
	RGBA[3] = 0xFF;

	return *((uint32_t *) &RGBA);
}

//Bytes still readable at 'off' in a buffer of 'len' bytes
#define ICON_AVAIL(len, off)	(((off) < (len)) ? ((len) - (off)) : 0)

//Texels the output buffer can still take
#define ICON_TEXELS		(128 * 128)

void ps2icon_free(ps2icon_t *icon)
{
	if (!icon)
		return;

	free(icon->shapes);
	free(icon->normals);
	free(icon->uvs);
	free(icon->colors);
	free(icon->texture);
	memset(icon, 0, sizeof(*icon));
}

/* 12.4 fixed point, as the file stores every coordinate. */
#define ICON_F16(v)  ((float)(int16_t)(v) / 4096.0f)

int ps2icon_parse(const uint8_t* iData, size_t len, ps2icon_t *out)
{
	uint32_t i;
	uint16_t j;
	Icon_Header header;
	Animation_Header anim_header;
	Frame_Data animation;
	uint32_t *lTexturePtr, *lRGBA;
	size_t offset = 0, vertex_size, geom = 0;
	int s_i, v_i, ok = -1;

	if (!out)
		return -1;

	memset(out, 0, sizeof(*out));

	lTexturePtr = (uint32_t *) calloc(ICON_TEXELS, sizeof(uint32_t));
	if (!lTexturePtr)
		return -1;

	out->texture = lTexturePtr;

	//read header:
	if (len < sizeof(Icon_Header))
		return ok;

	memcpy(&header, iData, sizeof(Icon_Header));
	offset += sizeof(Icon_Header);

	//n_vertices has to be divisible by three, that's for sure:
	if(header.file_id != 0x010000 || header.n_vertices % 3 != 0)
		return ok;

	//a vertex needs at least one animation shape, and the count is bounded so
	//the size calculation below cannot overflow
	if(header.animation_shapes == 0 || header.animation_shapes > 0xFFFF)
		return ok;

	//read icon data from file: https://ghulbus-inc.de/projects/ps2iconsys/
	///Vertex data
	// each vertex consists of animation_shapes tuples for vertex coordinates,
	// followed by one vertex coordinate tuple for normal coordinates
	// followed by one texture data tuple for texture coordinates and color
	vertex_size = (size_t)sizeof(Vertex_Coord) * header.animation_shapes
			+ sizeof(Vertex_Coord) + sizeof(Texture_Data);

	//divide instead of multiplying out, so the check cannot overflow
	if (header.n_vertices > ICON_AVAIL(len, offset) / vertex_size)
		return ok;

	//pull the geometry out on the way past. A renderer needs every shape
	//(they are morph targets), the normals, and the texture coordinates and
	//colour each vertex carries.
	out->shape_count = (int)header.animation_shapes;
	out->vertex_count = (int)header.n_vertices;
	out->shapes = malloc(sizeof(float) * 3 * header.animation_shapes * header.n_vertices);
	out->normals = malloc(sizeof(float) * 3 * header.n_vertices);
	out->uvs = malloc(sizeof(float) * 2 * header.n_vertices);
	out->colors = malloc(4 * (size_t)header.n_vertices);

	if (!out->shapes || !out->normals || !out->uvs || !out->colors) {
		ps2icon_free(out);
		return -1;
	}

	geom = offset;
	for (v_i = 0; v_i < out->vertex_count; v_i++) {
		for (s_i = 0; s_i < out->shape_count; s_i++) {
			const uint8_t *p = &iData[geom];
			float *dst = &out->shapes[((size_t)s_i * out->vertex_count + v_i) * 3];

			dst[0] = ICON_F16(p[0] | (p[1] << 8));
			dst[1] = ICON_F16(p[2] | (p[3] << 8));
			dst[2] = ICON_F16(p[4] | (p[5] << 8));
			geom += sizeof(Vertex_Coord);
		}

		out->normals[v_i * 3 + 0] = ICON_F16(iData[geom + 0] | (iData[geom + 1] << 8));
		out->normals[v_i * 3 + 1] = ICON_F16(iData[geom + 2] | (iData[geom + 3] << 8));
		out->normals[v_i * 3 + 2] = ICON_F16(iData[geom + 4] | (iData[geom + 5] << 8));
		geom += sizeof(Vertex_Coord);

		out->uvs[v_i * 2 + 0] = ICON_F16(iData[geom + 0] | (iData[geom + 1] << 8));
		out->uvs[v_i * 2 + 1] = ICON_F16(iData[geom + 2] | (iData[geom + 3] << 8));
		memcpy(&out->colors[v_i * 4], &iData[geom + 4], 4);
		geom += sizeof(Texture_Data);
	}

	offset += vertex_size * header.n_vertices;

	//animation data
	// preceeded by an animation header, there is a frame data/key set for every frame:
	if (ICON_AVAIL(len, offset) < sizeof(Animation_Header))
		return ok;

	memcpy(&anim_header, &iData[offset], sizeof(Animation_Header));
	offset += sizeof(Animation_Header);

	//read animation data:
	for(i=0; i<anim_header.n_frames; i++) {
		if (ICON_AVAIL(len, offset) < sizeof(Frame_Data))
			return ok;

		memcpy(&animation, &iData[offset], sizeof(Frame_Data));
		offset += sizeof(Frame_Data);

		/* The still the web thumbnailer draws is the first frame's shape, but
		 * only when there is a sequence to interpolate: with fewer than two
		 * frames it falls back to shape 0. Matching that keeps the two
		 * renderers showing the same pose. */
		if (i == 0 && anim_header.n_frames >= 2)
			out->still_shape = (int)animation.shape_id < out->shape_count
					 ? (int)animation.shape_id : out->shape_count - 1;

		if (animation.n_keys > ICON_AVAIL(len, offset) / sizeof(Frame_Key))
			return ok;

		offset += sizeof(Frame_Key) * animation.n_keys;
	}

	//everything the renderer needs has been read; the texture is a bonus
	ok = 0;

	lRGBA = lTexturePtr;

	if (header.texture_type <= 7)
	{	// Uncompressed texture
		// Some icons carry no texture at all: the file ends after the animation
		// block and the model is drawn from its vertex colours. Hand back the
		// cleared buffer rather than reading past the end of the file.
		if (ICON_AVAIL(len, offset) < (ICON_TEXELS * 2))
			return ok;

		for (i = 0; i < ICON_TEXELS; i++, offset += 2)
			*lRGBA++ = TIM2RGBA(&iData[offset]);
	}
	else
	{	//Compressed texture
		offset += 4;

		while ((lRGBA - lTexturePtr) < ICON_TEXELS)
		{
			if (ICON_AVAIL(len, offset) < 2)
				break;

			j = (int16_t) (iData[offset + 1] << 8) | iData[offset];

			if (0xFF00 == (j & 0xFF00))
			{	//a run of literal texels
				for (j = (0x0000 - j) & 0xFFFF; j > 0; j--)
				{
					offset += 2;

					if (ICON_AVAIL(len, offset) < 2 || (lRGBA - lTexturePtr) >= ICON_TEXELS)
						break;

					*lRGBA++ = TIM2RGBA(&iData[offset]);
				}
			}
			else
			{	//one texel repeated j times
				offset += 2;

				if (ICON_AVAIL(len, offset) < 2)
					break;

				for (; j > 0; j--)
				{
					if ((lRGBA - lTexturePtr) >= ICON_TEXELS)
						break;

					*lRGBA++ = TIM2RGBA(&iData[offset]);
				}
			}
			offset += 2;
		}
	}

	/* Only a full 128x128 counts. A truncated RLE stream leaves the tail of
	 * the buffer zeroed, and handing that to the renderer as a texture draws
	 * the model black; the JavaScript parser reports the same case as
	 * textureless so the model falls back to its vertex colours. */
	out->has_texture = (lRGBA - lTexturePtr) == ICON_TEXELS;

	return ok;
}

/* The parser and ps2render both deal in R,G,B,A bytes. The rest of Apollo PS4
 * (LoadVmcTexture's SDL masks, svpng) takes each pixel as a native 0xRRGGBBAA
 * word, so the bytes are reordered on the way out. */
static uint8_t* rgbaToNative(uint8_t *img, int pixels)
{
	uint32_t *px = (uint32_t*) img;

	for (int i = 0; px && i < pixels; i++)
		px[i] = __builtin_bswap32(px[i]);

	return img;
}

/* The save's icon.sys, for the lights the renderer shades the model with.
 * Returns 0, or negative when there is none worth using. */
static int readIconSys(const char* folder, ps2_IconSys_t *sys)
{
	int fd, r;
	char filePath[256];

	snprintf(filePath, sizeof(filePath), "%s/icon.sys", folder);

	fd = mcio_mcOpen(filePath, sceMcFileAttrReadable | sceMcFileAttrFile);
	if (fd < 0)
		return fd;

	r = mcio_mcRead(fd, sys, sizeof(ps2_IconSys_t));
	mcio_mcClose(fd);

	return (r == sizeof(ps2_IconSys_t) && memcmp(sys->magic, "PS2D", 4) == 0) ? 0 : -1;
}

/* Read an icon off the mounted card and parse it. Returns 0 or negative. */
int ps2icon_load(const char* folder, const char* iconfile, ps2icon_t *out)
{
	int fd, r;
	uint8_t *buf;
	char filePath[256];
	struct io_dirent st;

	if (!out)
		return -1;

	memset(out, 0, sizeof(*out));
	snprintf(filePath, sizeof(filePath), "%s/%s", folder, iconfile);

	if (mcio_mcStat(filePath, &st) < 0)
		return -1;

	fd = mcio_mcOpen(filePath, sceMcFileAttrReadable | sceMcFileAttrFile);
	if (fd < 0)
		return fd;

	buf = malloc(st.stat.size ? st.stat.size : 1);
	if (!buf) {
		mcio_mcClose(fd);
		return -1;
	}

	r = mcio_mcRead(fd, buf, st.stat.size);
	mcio_mcClose(fd);

	//only the bytes actually read are valid input for the parser
	r = ps2icon_parse(buf, (r > 0) ? (size_t)r : 0, out);
	free(buf);

	if (r < 0)
		ps2icon_free(out);

	return r;
}

//Get the icon as a 128x128 image: the 3D model rendered as the PS2 browser
//would show it, or the flat texture when there is no model to render
uint8_t* getIconPS2(const char* folder, const char* iconfile)
{
	int fd, r;
	uint8_t *buf, *out = NULL;
	char filePath[256];
	struct io_dirent st;
	ps2icon_t icon;
	ps2_IconSys_t sys;

	snprintf(filePath, sizeof(filePath), "%s/%s", folder, iconfile);

	if (mcio_mcStat(filePath, &st) < 0)
		return calloc(ICON_TEXELS, sizeof(uint32_t));

	fd = mcio_mcOpen(filePath, sceMcFileAttrReadable | sceMcFileAttrFile);
	if (fd < 0)
		return calloc(ICON_TEXELS, sizeof(uint32_t));

	buf = malloc(st.stat.size ? st.stat.size : 1);
	if (!buf) {
		mcio_mcClose(fd);
		return calloc(ICON_TEXELS, sizeof(uint32_t));
	}

	r = mcio_mcRead(fd, buf, st.stat.size);
	mcio_mcClose(fd);

	//only the bytes actually read are valid input for the parser
	r = ps2icon_parse(buf, (r > 0) ? (size_t)r : 0, &icon);
	free(buf);

	//same size as the texture, so callers take either one; with no icon.sys
	//the renderer falls back to the default lighting
	if (r == 0 && ps2icon_render(&icon, (readIconSys(folder, &sys) == 0) ? &sys : NULL,
			128, 4, PS2RENDER_BG_TRANSPARENT, &out) < 0)
		out = NULL;

	//no usable geometry, or the render failed: the flat texture it always was
	if (!out) {
		out = (uint8_t*) icon.texture;
		icon.texture = NULL;
	}
	ps2icon_free(&icon);

	if (!out)
		return calloc(ICON_TEXELS, sizeof(uint32_t));

	return rgbaToNative(out, ICON_TEXELS);
}
