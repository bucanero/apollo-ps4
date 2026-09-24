/*
*
* Copyright (c) 2008 Andreas Weis (http://www.ghulbus-inc.de/)
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy  of  this   software  and  associated   documentation  files  (the
* "Software"),  to deal  in the Software  without  restriction, including
* without  limitation  the rights to  use, copy,  modify, merge, publish,
* distribute,  sublicense, and/or  sell  copies of the  Software, and  to
* permit persons to  whom the Software is furnished  to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR   IMPLIED,  INCLUDING   BUT  NOT  LIMITED  TO   THE  WARRANTIES   OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR  PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE  AUTHORS OR COPYRIGHT HOLDERS  BE LIABLE FOR  ANY
* CLAIM,  DAMAGES  OR OTHER  LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT  OR OTHERWISE,  ARISING FROM,  OUT OF  OR IN  CONNECTION  WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef PS2ICON_H
#define PS2ICON_H

#include <stdint.h>
#include <stddef.h>

#include "ps2mc.h"

//================================================================================================
//   Typedefs and Defines
//================================================================================================


/** File header
 */
typedef struct Icon_Header_t {
	unsigned int file_id;						///< reserved; should be: 0x010000 (but does not have to ;) )
	unsigned int animation_shapes;				///< number of animation shapes per vertex
	unsigned int texture_type;					///< texture type - 0x07: uncompressed, 0x06: uncompresses, 0x0f: RLE compression
	unsigned int reserved;						///< reserved; should be: 0x3F800000 (but does not have to ;) )
	unsigned int n_vertices;					///< number of vertices; must be a multiple of 3
} Icon_Header;
/** Set of vertex coordinates
 * @note The f16_* fields indicate float16 data; divide by 4096.0f to convert to float32;
 */
typedef struct Vertex_Coord_t {
	short f16_x;								///< vertex x coordinate in float16
	short f16_y;								///< vertex y coordinate in float16
	short f16_z;								///< vertex z coordinate in float16
	short f16_unknown;							///< unknown; seems to influence lightning?
} Vertex_Coord;
/** Set of texture coordinates
 * @note The f16_* fields indicate float16 data; divide by 4096.0f to convert to float32;
 */
typedef struct Texture_Data_t {
	short        f16_u;							///< vertex u texture coordinate in float16
	short        f16_v;							///< vertex v texture coordinate in float16
	unsigned int color;							///< vertex color (32 bit RGBA)
} Texture_Data;
/** Animation header
 */
typedef struct Animation_Header_t {
	unsigned int id_tag;						///< ???
	unsigned int frame_length;					///< ???
	float        anim_speed;					///< ???
	unsigned int play_offset;					///< ???
	unsigned int n_frames;						///< number of frames in the animation
} Animation_Header;
/** Per-frame animation data
 */
typedef struct Frame_Data_t {
	unsigned int shape_id;						///< shape used for this frame
	unsigned int n_keys;						///< number of keys corresponding to this frame
} Frame_Data;
/** Per-key animation data
 */
typedef struct Frame_Key_t {
	float time;									///< ???
	float value;								///< ???
} Frame_Key;

/** A parsed icon: the morph targets, the per-vertex attributes they share,
 *  and the 128x128 texture.
 *
 *  `texture` is always allocated, even for an icon that carries none - those
 *  are drawn from their vertex colours alone, and a cleared buffer is what the
 *  existing texture export has always handed back. `has_texture` says which it
 *  is, so a renderer can substitute white instead of black.
 */
typedef struct {
	int       shape_count;      ///< morph targets, at least 1
	int       vertex_count;     ///< a multiple of 3
	float    *shapes;           ///< shape_count * vertex_count * 3
	float    *normals;          ///< vertex_count * 3
	float    *uvs;              ///< vertex_count * 2
	uint8_t  *colors;           ///< vertex_count * 4, RGBA, 0x80-centred
	uint32_t *texture;          ///< 128 * 128 RGBA, never NULL after a parse
	int       has_texture;
	int       still_shape;      ///< the shape a still frame should use
} ps2icon_t;

/** Parse an .ico. Returns 0 when geometry was read, negative when the file is
 *  not a usable icon - `texture` is allocated either way, so a caller that
 *  only wants the texture can ignore the return value. Free with
 *  ps2icon_free(). */
int ps2icon_parse(const uint8_t *data, size_t len, ps2icon_t *out);

void ps2icon_free(ps2icon_t *icon);

/** Read an icon off the mounted card and parse it. Returns 0 or negative. */
int ps2icon_load(const char* folder, const char* iconfile, ps2icon_t *out);

//Get icon data as bytes
uint8_t* getIconPS2(const char* folder, const char* iconfile);

#endif
