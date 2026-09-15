#pragma once

/*
 * A small pixel-art bitmap font, drawn as textured quads (via
 * ColorTextureProgram) instead of the vector-line font DrawLines uses.
 *
 * The glyph atlas (assets/pixel-font.png) is *not* new art: it's decoded
 * straight from the font tiles baked into game1's own asset pipeline
 * (game1/assets/tiles.png -> assets/build_assets.py -> assets/assets.dat),
 * so it's still a self-authored asset, just reused across the two projects.
 *
 * Usage mirrors DrawLines' immediate-mode style:
 *   PixelText text(clip_from_local);
 *   text.draw("SCORE: 5", anchor, 0.08f);
 *   // destructor uploads the accumulated quads and issues one draw call
 */

#include "GL.hpp"
#include "Load.hpp"

#include <glm/glm.hpp>

#include <string>
#include <vector>

//characters present in the font atlas, left-to-right/top-to-bottom in glyph order
// (matches game1's Assets::FontChars exactly -- see game1/AssetIndices.hpp):
extern char const pixel_font_chars[];

enum : uint32_t {
	PixelFontCols = 8,   //glyphs per atlas row
	PixelFontGlyphPx = 8 //size (in texels) of one glyph cell, both directions
};

struct PixelFont {
	GLuint texture = 0;
	glm::uvec2 size = glm::uvec2(0); //atlas texture size, in texels
};

extern Load< PixelFont > pixel_font;

struct PixelText {
	//Start drawing; will remember the clip-from-local matrix used to place quads:
	PixelText(glm::mat4 const &clip_from_local);

	//draws 'text' with its top-left corner at 'anchor' (local space);
	// each glyph is a 'size' x 'size' square, advancing left-to-right, one glyph per character;
	// characters not present in the font are skipped (but still advance, like a space);
	//returns the position just past the last glyph (useful for chaining onto more text):
	glm::vec3 draw(std::string const &text, glm::vec3 const &anchor, float size, glm::u8vec4 const &color = glm::u8vec4(0xff));

	//Finish drawing (uploads + issues the draw call):
	~PixelText();

	glm::mat4 clip_from_local;

	struct Vertex {
		Vertex(glm::vec3 const &Position_, glm::u8vec4 const &Color_, glm::vec2 const &TexCoord_) : Position(Position_), Color(Color_), TexCoord(TexCoord_) { }
		glm::vec3 Position;
		glm::u8vec4 Color;
		glm::vec2 TexCoord;
	};
	std::vector< Vertex > attribs;
};
