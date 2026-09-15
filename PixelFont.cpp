#include "PixelFont.hpp"

#include "ColorTextureProgram.hpp"
#include "load_save_png.hpp"
#include "data_path.hpp"
#include "gl_errors.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <cstddef>

char const pixel_font_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 /-!:.";

Load< PixelFont > pixel_font(LoadTagDefault, []() -> PixelFont const * {
	PixelFont *ret = new PixelFont();

	std::vector< glm::u8vec4 > data;
	load_png(data_path("pixel-font.png"), &ret->size, &data, UpperLeftOrigin);

	glGenTextures(1, &ret->texture);
	glBindTexture(GL_TEXTURE_2D, ret->texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GLsizei(ret->size.x), GLsizei(ret->size.y), 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
	//nearest filtering keeps the pixel-art look crisp when scaled up:
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);

	GL_ERRORS();

	return ret;
});

//All PixelText instances share a vertex array object and vertex buffer, initialized at load time
// (mirrors the setup in DrawLines.cpp):
static GLuint vertex_buffer = 0;
static GLuint vertex_array = 0;

static Load< void > setup_buffers(LoadTagDefault, [](){
	glGenBuffers(1, &vertex_buffer);

	glGenVertexArrays(1, &vertex_array);
	glBindVertexArray(vertex_array);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

	glVertexAttribPointer(
		color_texture_program->Position_vec4, 3, GL_FLOAT, GL_FALSE,
		sizeof(PixelText::Vertex), (GLbyte *)0 + offsetof(PixelText::Vertex, Position)
	);
	glEnableVertexAttribArray(color_texture_program->Position_vec4);

	glVertexAttribPointer(
		color_texture_program->Color_vec4, 4, GL_UNSIGNED_BYTE, GL_TRUE,
		sizeof(PixelText::Vertex), (GLbyte *)0 + offsetof(PixelText::Vertex, Color)
	);
	glEnableVertexAttribArray(color_texture_program->Color_vec4);

	glVertexAttribPointer(
		color_texture_program->TexCoord_vec2, 2, GL_FLOAT, GL_FALSE,
		sizeof(PixelText::Vertex), (GLbyte *)0 + offsetof(PixelText::Vertex, TexCoord)
	);
	glEnableVertexAttribArray(color_texture_program->TexCoord_vec2);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	GL_ERRORS();
});

PixelText::PixelText(glm::mat4 const &clip_from_local_) : clip_from_local(clip_from_local_) {
}

glm::vec3 PixelText::draw(std::string const &text, glm::vec3 const &anchor_in, float size, glm::u8vec4 const &color) {
	glm::vec3 anchor = anchor_in;

	//each glyph's 8x8 cell has a fair bit of blank margin baked in (it's a
	// fixed-width bitmap font), so advancing by less than a full cell tightens
	// up the letter spacing without needing to touch the atlas itself:
	constexpr float Advance = 0.72f;

	float tex_w = float(pixel_font->size.x);
	float tex_h = float(pixel_font->size.y);

	for (char ch : text) {
		int index = -1;
		for (uint32_t i = 0; pixel_font_chars[i] != '\0'; ++i) {
			if (pixel_font_chars[i] == ch) {
				index = int(i);
				break;
			}
		}

		if (index >= 0) {
			uint32_t col = uint32_t(index) % PixelFontCols;
			uint32_t row = uint32_t(index) / PixelFontCols;

			float u0 = float(col * PixelFontGlyphPx) / tex_w;
			float u1 = float((col + 1) * PixelFontGlyphPx) / tex_w;
			float v0 = float(row * PixelFontGlyphPx) / tex_h;
			float v1 = float((row + 1) * PixelFontGlyphPx) / tex_h;

			glm::vec3 top_left = anchor;
			glm::vec3 top_right = anchor + glm::vec3(size, 0.0f, 0.0f);
			glm::vec3 bottom_left = anchor + glm::vec3(0.0f, -size, 0.0f);
			glm::vec3 bottom_right = anchor + glm::vec3(size, -size, 0.0f);

			attribs.emplace_back(top_left, color, glm::vec2(u0, v0));
			attribs.emplace_back(bottom_left, color, glm::vec2(u0, v1));
			attribs.emplace_back(top_right, color, glm::vec2(u1, v0));

			attribs.emplace_back(top_right, color, glm::vec2(u1, v0));
			attribs.emplace_back(bottom_left, color, glm::vec2(u0, v1));
			attribs.emplace_back(bottom_right, color, glm::vec2(u1, v1));
		}
		//(unsupported characters are skipped, but still advance, like a space)

		anchor.x += size * Advance;
	}

	return anchor;
}

PixelText::~PixelText() {
	if (attribs.empty()) return;

	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(attribs.size() * sizeof(attribs[0])), attribs.data(), GL_STREAM_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glUseProgram(color_texture_program->program);
	glUniformMatrix4fv(color_texture_program->OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(clip_from_local));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, pixel_font->texture);

	//glyph background is transparent, so blend it over whatever's already drawn:
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBindVertexArray(vertex_array);
	glDrawArrays(GL_TRIANGLES, 0, GLsizei(attribs.size()));
	glBindVertexArray(0);

	glDisable(GL_BLEND);

	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);

	GL_ERRORS();
}
