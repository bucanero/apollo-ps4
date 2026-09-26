/*
 * ps2render.c - software renderer for PS2 save icons
 *
 * See include/ps2render.h. Every matrix, the camera distance, the light model
 * and the 0x80-centred vertex colours are taken from web-ps2/icon3d.js so the
 * CLI and the web page draw the same picture.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ps2render.h"

#define TEX_SIZE   128
#define MAX_SIZE   4096      /* a rendered icon larger than this is a mistake */

/* The camera icon3d.js uses: the model is scaled into a unit sphere, so the
 * distance and field of view are fixed. */
#define CAM_Z      (-3.2f)
#define CAM_FOV    0.7f
#define CAM_NEAR   0.1f
#define CAM_FAR    100.0f

/* ------------------------------------------------------------------ */
/* mat4, column-major, matching the JS helpers exactly                 */
/* ------------------------------------------------------------------ */

static void m_identity(float *m)
{
	memset(m, 0, sizeof(float) * 16);
	m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void m_multiply(const float *a, const float *b, float *o)
{
	int c, r;

	for (c = 0; c < 4; c++)
		for (r = 0; r < 4; r++)
			o[c * 4 + r] = a[r] * b[c * 4] + a[4 + r] * b[c * 4 + 1] +
				       a[8 + r] * b[c * 4 + 2] + a[12 + r] * b[c * 4 + 3];
}

static void m_perspective(float fovy, float aspect, float near, float far, float *m)
{
	float f = 1.0f / tanf(fovy / 2.0f), nf = 1.0f / (near - far);

	memset(m, 0, sizeof(float) * 16);
	m[0] = f / aspect;
	m[5] = f;
	m[10] = (far + near) * nf;
	m[11] = -1.0f;
	m[14] = 2.0f * far * near * nf;
}

/* ------------------------------------------------------------------ */
/* icon.sys lighting                                                   */
/* ------------------------------------------------------------------ */

/*
 * The light and colour fields are declared as byte arrays in ps2icon.h but
 * hold four little-endian floats each.
 */
static void read_vec4(const uint8_t *src, float *dst)
{
	memcpy(dst, src, sizeof(float) * 4);
}

typedef struct {
	float dir[3][3];
	float col[3][3];
	float ambient[3];
} lighting_t;

static void load_lighting(const ps2_IconSys_t *sys, lighting_t *l)
{
	/* The fallback the web renderer uses when a save has no usable icon.sys. */
	static const float def_dir[3][3] = {{0.5f,0.5f,0.5f},{-0.5f,0.5f,0.5f},{0.0f,-0.5f,0.5f}};
	static const float def_col[3][3] = {{1,1,1},{0.6f,0.6f,0.6f},{0.4f,0.4f,0.4f}};
	float v[4];
	int i, k;

	if (!sys) {
		memcpy(l->dir, def_dir, sizeof(def_dir));
		memcpy(l->col, def_col, sizeof(def_col));
		l->ambient[0] = l->ambient[1] = l->ambient[2] = 0.4f;
		return;
	}

	read_vec4(sys->light1Direction, v); for (k = 0; k < 3; k++) l->dir[0][k] = v[k];
	read_vec4(sys->light2Direction, v); for (k = 0; k < 3; k++) l->dir[1][k] = v[k];
	read_vec4(sys->light3Direction, v); for (k = 0; k < 3; k++) l->dir[2][k] = v[k];
	read_vec4(sys->light1RGB, v);       for (k = 0; k < 3; k++) l->col[0][k] = v[k];
	read_vec4(sys->light2RGB, v);       for (k = 0; k < 3; k++) l->col[1][k] = v[k];
	read_vec4(sys->light3RGB, v);       for (k = 0; k < 3; k++) l->col[2][k] = v[k];
	read_vec4(sys->ambientLightRGB, v); for (k = 0; k < 3; k++) l->ambient[k] = v[k];

	/* A direction of exactly zero cannot be normalised; the shader's
	 * normalize() would produce NaN and the vertex would go black. */
	for (i = 0; i < 3; i++) {
		float n = l->dir[i][0] * l->dir[i][0] + l->dir[i][1] * l->dir[i][1] +
			  l->dir[i][2] * l->dir[i][2];
		if (!(n > 1e-12f))
			memcpy(l->dir[i], def_dir[i], sizeof(def_dir[i]));
	}
}

/* ------------------------------------------------------------------ */
/* rasteriser                                                          */
/* ------------------------------------------------------------------ */

typedef struct {
	float x, y, z, w;      /* window space, w kept for perspective correction */
	float u, v;
	float r, g, b;
} vert_t;

static float clampf(float v, float lo, float hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

/* Bilinear sample with clamp-to-edge, matching gl.LINEAR + CLAMP_TO_EDGE. */
static void sample_tex(const uint32_t *tex, int have, float u, float v, float *out)
{
	float x, y, fx, fy;
	int x0, y0, x1, y1, k;
	const uint8_t *p;
	float c[4][3];

	if (!have) {
		out[0] = out[1] = out[2] = 1.0f;
		return;
	}

	x = clampf(u, 0.0f, 1.0f) * TEX_SIZE - 0.5f;
	y = clampf(v, 0.0f, 1.0f) * TEX_SIZE - 0.5f;

	x0 = (int)floorf(x); y0 = (int)floorf(y);
	fx = x - x0;         fy = y - y0;
	x1 = x0 + 1;         y1 = y0 + 1;

	x0 = x0 < 0 ? 0 : (x0 > TEX_SIZE - 1 ? TEX_SIZE - 1 : x0);
	x1 = x1 < 0 ? 0 : (x1 > TEX_SIZE - 1 ? TEX_SIZE - 1 : x1);
	y0 = y0 < 0 ? 0 : (y0 > TEX_SIZE - 1 ? TEX_SIZE - 1 : y0);
	y1 = y1 < 0 ? 0 : (y1 > TEX_SIZE - 1 ? TEX_SIZE - 1 : y1);

	p = (const uint8_t *)&tex[y0 * TEX_SIZE + x0]; for (k = 0; k < 3; k++) c[0][k] = p[k] / 255.0f;
	p = (const uint8_t *)&tex[y0 * TEX_SIZE + x1]; for (k = 0; k < 3; k++) c[1][k] = p[k] / 255.0f;
	p = (const uint8_t *)&tex[y1 * TEX_SIZE + x0]; for (k = 0; k < 3; k++) c[2][k] = p[k] / 255.0f;
	p = (const uint8_t *)&tex[y1 * TEX_SIZE + x1]; for (k = 0; k < 3; k++) c[3][k] = p[k] / 255.0f;

	for (k = 0; k < 3; k++)
		out[k] = (c[0][k] * (1 - fx) + c[1][k] * fx) * (1 - fy) +
			 (c[2][k] * (1 - fx) + c[3][k] * fx) * fy;
}

static float edge(const vert_t *a, const vert_t *b, float px, float py)
{
	return (px - a->x) * (b->y - a->y) - (py - a->y) * (b->x - a->x);
}

static void draw_tri(const vert_t *v0, const vert_t *v1, const vert_t *v2,
		     const uint32_t *tex, int have_tex,
		     float *depth, uint8_t *rgba, int w, int h)
{
	float area, minxf, maxxf, minyf, maxyf;
	int minx, maxx, miny, maxy, x, y;

	area = edge(v0, v1, v2->x, v2->y);
	if (fabsf(area) < 1e-9f)
		return;

	minxf = fminf(v0->x, fminf(v1->x, v2->x));
	maxxf = fmaxf(v0->x, fmaxf(v1->x, v2->x));
	minyf = fminf(v0->y, fminf(v1->y, v2->y));
	maxyf = fmaxf(v0->y, fmaxf(v1->y, v2->y));

	minx = (int)floorf(minxf); maxx = (int)ceilf(maxxf);
	miny = (int)floorf(minyf); maxy = (int)ceilf(maxyf);

	if (minx < 0) minx = 0;
	if (miny < 0) miny = 0;
	if (maxx > w - 1) maxx = w - 1;
	if (maxy > h - 1) maxy = h - 1;

	for (y = miny; y <= maxy; y++) {
		for (x = minx; x <= maxx; x++) {
			float px = x + 0.5f, py = y + 0.5f;
			float w0, w1, w2, iw, z, u, v, cr, cg, cb, t[3];
			int idx;

			w0 = edge(v1, v2, px, py) / area;
			w1 = edge(v2, v0, px, py) / area;
			w2 = edge(v0, v1, px, py) / area;

			/* No back-face culling: icon3d.js does not enable it either,
			 * and a few icons rely on seeing both sides. */
			if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f)
				continue;

			z = w0 * v0->z + w1 * v1->z + w2 * v2->z;
			idx = y * w + x;
			if (z >= depth[idx])
				continue;

			/* Attributes are interpolated over 1/w, then divided back. */
			iw = w0 / v0->w + w1 / v1->w + w2 / v2->w;
			if (!(iw > 0.0f))
				continue;

			u  = (w0 * v0->u / v0->w + w1 * v1->u / v1->w + w2 * v2->u / v2->w) / iw;
			v  = (w0 * v0->v / v0->w + w1 * v1->v / v1->w + w2 * v2->v / v2->w) / iw;
			cr = (w0 * v0->r / v0->w + w1 * v1->r / v1->w + w2 * v2->r / v2->w) / iw;
			cg = (w0 * v0->g / v0->w + w1 * v1->g / v1->w + w2 * v2->g / v2->w) / iw;
			cb = (w0 * v0->b / v0->w + w1 * v1->b / v1->w + w2 * v2->b / v2->w) / iw;

			sample_tex(tex, have_tex, u, v, t);

			depth[idx] = z;
			rgba[idx * 4 + 0] = (uint8_t)(clampf(t[0] * cr, 0.0f, 1.0f) * 255.0f + 0.5f);
			rgba[idx * 4 + 1] = (uint8_t)(clampf(t[1] * cg, 0.0f, 1.0f) * 255.0f + 0.5f);
			rgba[idx * 4 + 2] = (uint8_t)(clampf(t[2] * cb, 0.0f, 1.0f) * 255.0f + 0.5f);
			rgba[idx * 4 + 3] = 255;
		}
	}
}

/* The icon.sys gradient: four corner colours, bilinear across the image. */
static void fill_gradient(const ps2_IconSys_t *sys, uint8_t *rgba, int w, int h)
{
	const uint8_t *corner[4] = { sys->bgColourUpperLeft, sys->bgColourUpperRight,
				     sys->bgColourLowerLeft, sys->bgColourLowerRight };
	float c[4][3];
	int i, k, x, y;

	for (i = 0; i < 4; i++) {
		uint32_t v[4];
		memcpy(v, corner[i], sizeof(v));
		for (k = 0; k < 3; k++)
			c[i][k] = clampf(v[k] / 255.0f, 0.0f, 1.0f);
	}

	for (y = 0; y < h; y++) {
		float fy = h > 1 ? (float)y / (h - 1) : 0.0f;
		for (x = 0; x < w; x++) {
			float fx = w > 1 ? (float)x / (w - 1) : 0.0f;
			int idx = (y * w + x) * 4;
			for (k = 0; k < 3; k++) {
				float top = c[0][k] * (1 - fx) + c[1][k] * fx;
				float bot = c[2][k] * (1 - fx) + c[3][k] * fx;
				rgba[idx + k] = (uint8_t)(clampf(top * (1 - fy) + bot * fy,
								 0.0f, 1.0f) * 255.0f + 0.5f);
			}
			rgba[idx + 3] = 255;
		}
	}
}

int ps2icon_render(const ps2icon_t *icon, const ps2_IconSys_t *sys,
		   int size, int supersample, enum ps2render_bg bg,
		   uint8_t **out)
{
	float model[16], view[16], proj[16], flip[16], sc[16], tr[16], tmp[16], mvp[16];
	float mn[3], mx[3], center[3], radius = 1.0f;
	lighting_t light;
	const float *shape;
	vert_t *verts = NULL;
	float *depth = NULL;
	uint8_t *big = NULL, *dst = NULL;
	int w, i, k, ss, rc = -1;

	if (!icon || !out || size <= 0 || size > MAX_SIZE ||
	    icon->vertex_count < 3 || !icon->shapes)
		return -1;

	*out = NULL;
	ss = supersample < 1 ? 1 : supersample;
	if (size * ss > MAX_SIZE)
		ss = MAX_SIZE / size;
	w = size * ss;

	shape = &icon->shapes[(size_t)icon->still_shape * icon->vertex_count * 3];

	/* Bounds over every shape, not just the one drawn: the web renderer sizes
	 * the model once so an animation cannot swim in and out of frame. */
	for (k = 0; k < 3; k++) { mn[k] = 1e9f; mx[k] = -1e9f; }
	for (i = 0; i < icon->shape_count * icon->vertex_count; i++)
		for (k = 0; k < 3; k++) {
			float v = icon->shapes[(size_t)i * 3 + k];
			if (v < mn[k]) mn[k] = v;
			if (v > mx[k]) mx[k] = v;
		}
	for (k = 0; k < 3; k++)
		center[k] = (mn[k] + mx[k]) / 2.0f;
	radius = sqrtf((mx[0] - center[0]) * (mx[0] - center[0]) +
		       (mx[1] - center[1]) * (mx[1] - center[1]) +
		       (mx[2] - center[2]) * (mx[2] - center[2]));
	if (!(radius > 0.001f))
		radius = 0.001f;

	/* model = scale(1/radius) * flip * translate(-center); the still is drawn
	 * with no yaw or pitch, so the rotations icon3d.js applies are identity. */
	m_identity(flip);
	flip[5] = -1.0f;                 /* PS2 model space is Y-down */
	flip[10] = -1.0f;
	m_identity(tr);
	tr[12] = -center[0]; tr[13] = -center[1]; tr[14] = -center[2];
	m_identity(sc);
	sc[0] = sc[5] = sc[10] = 1.0f / radius;

	m_multiply(flip, tr, tmp);
	m_multiply(sc, tmp, model);

	m_identity(view);
	view[14] = CAM_Z;
	m_perspective(CAM_FOV, 1.0f, CAM_NEAR, CAM_FAR, proj);
	m_multiply(view, model, tmp);
	m_multiply(proj, tmp, mvp);

	load_lighting(sys, &light);

	verts = malloc(sizeof(vert_t) * icon->vertex_count);
	depth = malloc(sizeof(float) * (size_t)w * w);
	big = calloc((size_t)w * w, 4);
	if (!verts || !depth || !big)
		goto done;

	for (i = 0; i < w * w; i++)
		depth[i] = 1e30f;

	if (bg == PS2RENDER_BG_GRADIENT && sys)
		fill_gradient(sys, big, w, w);

	/* ---- vertex stage ---- */
	for (i = 0; i < icon->vertex_count; i++) {
		const float *p = &shape[(size_t)i * 3];
		const float *n = &icon->normals[(size_t)i * 3];
		const uint8_t *c = &icon->colors[(size_t)i * 4];
		float clip[4], nx, ny, nz, len, lr, lg, lb;
		int j;

		for (k = 0; k < 4; k++)
			clip[k] = mvp[k] * p[0] + mvp[4 + k] * p[1] +
				  mvp[8 + k] * p[2] + mvp[12 + k];

		/* mat3(model) * normal, then normalise, as the vertex shader does. */
		nx = model[0] * n[0] + model[4] * n[1] + model[8] * n[2];
		ny = model[1] * n[0] + model[5] * n[1] + model[9] * n[2];
		nz = model[2] * n[0] + model[6] * n[1] + model[10] * n[2];
		len = sqrtf(nx * nx + ny * ny + nz * nz);
		if (len > 1e-12f) { nx /= len; ny /= len; nz /= len; }

		lr = light.ambient[0]; lg = light.ambient[1]; lb = light.ambient[2];
		for (j = 0; j < 3; j++) {
			float dx = light.dir[j][0], dy = light.dir[j][1], dz = light.dir[j][2];
			float dl = sqrtf(dx * dx + dy * dy + dz * dz), d;

			if (!(dl > 1e-12f))
				continue;
			d = (nx * dx + ny * dy + nz * dz) / dl;
			if (d < 0.0f)
				d = 0.0f;
			lr += light.col[j][0] * d;
			lg += light.col[j][1] * d;
			lb += light.col[j][2] * d;
		}

		/* PS2 vertex colours are 0x80-centred; alpha is not transparency and
		 * is ignored, exactly as in the shader. */
		verts[i].r = clampf(lr, 0.0f, 2.0f) * (c[0] / 128.0f);
		verts[i].g = clampf(lg, 0.0f, 2.0f) * (c[1] / 128.0f);
		verts[i].b = clampf(lb, 0.0f, 2.0f) * (c[2] / 128.0f);

		verts[i].w = clip[3];
		if (fabsf(clip[3]) < 1e-9f) {
			verts[i].x = verts[i].y = verts[i].z = 0.0f;
		} else {
			verts[i].x = (clip[0] / clip[3] * 0.5f + 0.5f) * w;
			verts[i].y = (1.0f - (clip[1] / clip[3] * 0.5f + 0.5f)) * w;
			verts[i].z = clip[2] / clip[3];
		}
		verts[i].u = icon->uvs[(size_t)i * 2];
		verts[i].v = icon->uvs[(size_t)i * 2 + 1];
	}

	/* ---- triangles ---- */
	for (i = 0; i + 2 < icon->vertex_count; i += 3) {
		vert_t *a = &verts[i], *b = &verts[i + 1], *c = &verts[i + 2];

		/* Anything crossing the near plane is dropped rather than clipped.
		 * The model is scaled into a unit sphere 3.2 away from a near plane
		 * at 0.1, so this cannot happen for a well-formed icon. */
		if (a->w <= CAM_NEAR || b->w <= CAM_NEAR || c->w <= CAM_NEAR)
			continue;

		/* One call covers both windings: the barycentrics are divided by the
		 * signed area, so a covered pixel comes out positive either way. */
		draw_tri(a, b, c, icon->texture, icon->has_texture, depth, big, w, w);
	}

	/* ---- resolve ---- */
	if (ss == 1) {
		dst = big;
		big = NULL;
	} else {
		int ox, oy;

		dst = malloc((size_t)size * size * 4);
		if (!dst)
			goto done;

		for (oy = 0; oy < size; oy++) {
			for (ox = 0; ox < size; ox++) {
				unsigned int acc[3] = { 0, 0, 0 }, a = 0;
				int sx, sy;

				/* Weight colour by coverage: averaging the RGB of
				 * uncovered pixels would draw a dark halo around the
				 * model on a transparent background. */
				for (sy = 0; sy < ss; sy++)
					for (sx = 0; sx < ss; sx++) {
						const uint8_t *p =
							&big[(((size_t)oy * ss + sy) * w +
							      (size_t)ox * ss + sx) * 4];
						for (k = 0; k < 3; k++)
							acc[k] += (unsigned int)p[k] * p[3];
						a += p[3];
					}

				for (k = 0; k < 3; k++)
					dst[((size_t)oy * size + ox) * 4 + k] =
						a ? (uint8_t)((acc[k] + a / 2) / a) : 0;
				dst[((size_t)oy * size + ox) * 4 + 3] =
					(uint8_t)((a + (unsigned int)(ss * ss) / 2) / (ss * ss));
			}
		}
	}

	*out = dst;
	dst = NULL;
	rc = 0;

done:
	free(verts);
	free(depth);
	free(big);
	free(dst);
	return rc;
}
