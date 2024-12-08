//
// Copyright (c) 2009-2013 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
#ifndef NANOVG_GL_UTILS_H
#define NANOVG_GL_UTILS_H

struct NVGLUframebuffer {
	NVGcontext* ctx;
	GLuint fbo;
	GLuint rbo;
	GLuint texture;
	int image;
};
typedef struct NVGLUframebuffer NVGLUframebuffer;

// Helper function to create GL frame buffer to render to.
void nvgluBindFramebuffer(NVGLUframebuffer* fb);
NVGLUframebuffer* nvgluCreateFramebuffer(NVGcontext* ctx, int w, int h, int imageFlags);
void nvgluGenerateMipmaps(NVGLUframebuffer* fb);
void nvgluDeleteFramebuffer(NVGLUframebuffer* fb);

#ifdef NANOVG_GL_IMPLEMENTATION

static GLint defaultFBO = -1;

void nvgluBlitFramebuffer(NVGcontext* ctx, NVGLUframebuffer* fb, int x, int y, int w, int h)
{
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);

    int x2 = x + w;
    int y2 = y + h;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb->fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, defaultFBO);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glBlitFramebuffer(x, y, x2, y2, x, y, x2, y2, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glFinish();

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        printf("OpenGL Error after glBlitFramebuffer: %d\n", error);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, defaultFBO);

    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

NVGLUframebuffer* nvgluCreateFramebuffer(NVGcontext* ctx, int w, int h, int imageFlags)
{
	GLint defaultFBO;
	GLint defaultRBO;
	NVGLUframebuffer* fb = NULL;

	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &defaultFBO);
	glGetIntegerv(GL_RENDERBUFFER_BINDING, &defaultRBO);

	fb = (NVGLUframebuffer*)malloc(sizeof(NVGLUframebuffer));
	if (fb == NULL) goto error;
	memset(fb, 0, sizeof(NVGLUframebuffer));

	fb->image = nvgCreateImageRGBA(ctx, w, h, imageFlags | NVG_IMAGE_FLIPY, NULL);
	fb->texture = nvglImageHandle(ctx, fb->image);


	fb->ctx = ctx;

	// frame buffer object
	glGenFramebuffers(1, &fb->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fb->fbo);

	// render buffer object
	glGenRenderbuffers(1, &fb->rbo);
	glBindRenderbuffer(GL_RENDERBUFFER, fb->rbo);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, w, h);

	// combine all
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb->texture, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fb->rbo);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
#ifdef GL_DEPTH24_STENCIL8
		// If GL_STENCIL_INDEX8 is not supported, try GL_DEPTH24_STENCIL8 as a fallback.
		// Some graphics cards require a depth buffer along with a stencil.
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb->texture, 0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fb->rbo);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
#endif // GL_DEPTH24_STENCIL8
			goto error;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, defaultFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, defaultRBO);
	return fb;
error:
	glBindFramebuffer(GL_FRAMEBUFFER, defaultFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, defaultRBO);
	nvgluDeleteFramebuffer(fb);
	return NULL;
}

void nvgluBindFramebuffer(NVGLUframebuffer* fb)
{
	if (defaultFBO == -1) glGetIntegerv(GL_FRAMEBUFFER_BINDING, &defaultFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, fb != NULL ? fb->fbo : defaultFBO);
}

static void nvgluReadPixels(NVGcontext* ctx, NVGLUframebuffer* fb, int x, int y, int width, int height, int total_height, void* data) {
    // Bind the framebuffer associated with the NVGLUframebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, fb->fbo);

    // Set the pixel storage alignment (important for correct data reads)
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    glReadPixels(x, total_height - y - height, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);

    // Unbind the framebuffer to restore the default state
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void nvgluAccumulateARGBNoFlip(NVGcontext* ctx, NVGLUframebuffer* fb, NVGLUframebuffer* full_fb, int x, int y, int w, int h, int total_width, int total_height, void* data) {
	const char* vertexShaderSource = R"(
		#version 150 core

		in vec3 aPos;
		in vec2 aTexCoord;

		out vec2 TexCoords;

		void main(void) {
		    gl_Position = vec4(aPos, 1.0);
		    TexCoords = aTexCoord;
		}
	)";
	const char* fragmentShaderSource = R"(
		#version 150 core

		in vec2 TexCoords;

		uniform sampler2D texture1;

		void main(void) {
			vec2 flippedTexCoords = vec2(TexCoords.x, 1.0 - TexCoords.y);
			gl_FragColor = texture(texture1, flippedTexCoords).bgra;

			//gl_FragColor = vec4(TexCoords.x, TexCoords.y, 1.0, 1.0); // test pattern
		}
	)";
	GLuint vert = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vert, 1, &vertexShaderSource, 0);
	glCompileShader(vert);

	// Check for compile errors
	GLint success;
	glGetShaderiv(vert, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLint logLength;
		glGetShaderiv(vert, GL_INFO_LOG_LENGTH, &logLength);
		char* log = new char[logLength];
		glGetShaderInfoLog(vert, logLength, &logLength, log);
		std::cerr << "Vertex compilation failed:\n" << log << std::endl;
		delete[] log;
	}

	GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(frag, 1, &fragmentShaderSource, 0);
	glCompileShader(frag);

	glGetShaderiv(frag, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLint logLength;
		glGetShaderiv(frag, GL_INFO_LOG_LENGTH, &logLength);
		char* log = new char[logLength];
		glGetShaderInfoLog(frag, logLength, &logLength, log);
		std::cerr << "Fragment compilation failed:\n" << log << std::endl;
		delete[] log;
	}

	GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vert);
	glAttachShader(shaderProgram, frag);

	glBindAttribLocation(shaderProgram, 0, "aPos");
	glBindAttribLocation(shaderProgram, 1, "aTexCoord");

	glLinkProgram(shaderProgram);

	glDeleteShader(vert);
	glDeleteShader(frag);

	// Finish Shader setup

    // Render setup
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity(); // Reset ModelView matrix

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity(); // Reset Projection matrix

	nvgluBindFramebuffer(full_fb);
	glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, total_width, total_height);

    glUseProgram(shaderProgram);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fb->texture);
    glUniform1i(glGetUniformLocation(shaderProgram, "uTexture"), 0);

    // Render quad
    float vertices[] = {
        // Positions   // Texture Coords
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
    };
    GLuint indices[] = { 0, 1, 2, 0, 2, 3 };
    GLuint vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // Read pixels from the ARGB framebuffer
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data);

    // Cleanup
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
	glDeleteProgram(shaderProgram);
}



void nvgluGenerateMipmaps(NVGLUframebuffer* fb)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fb->texture);
    glGenerateMipmap(GL_TEXTURE_2D);
}

void nvgluDeleteFramebuffer(NVGLUframebuffer* fb)
{
	if (fb == NULL) return;
	if (fb->fbo != 0)
		glDeleteFramebuffers(1, &fb->fbo);
	if (fb->rbo != 0)
		glDeleteRenderbuffers(1, &fb->rbo);
	if (fb->image >= 0)
		nvgDeleteImage(fb->ctx, fb->image);
	fb->ctx = NULL;
	fb->fbo = 0;
	fb->rbo = 0;
	fb->texture = 0;
	fb->image = -1;
	free(fb);
}

#endif // NANOVG_GL_IMPLEMENTATION
#endif // NANOVG_GL_UTILS_H
