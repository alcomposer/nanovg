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

	// Store the currently bound draw framebuffer
	GLint currentDrawFBO;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &currentDrawFBO);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb->fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, currentDrawFBO);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glBlitFramebuffer(x, y, x2, y2, x, y, x2, y2, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glFinish();

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        printf("OpenGL Error after glBlitFramebuffer: %d\n", error);
    }

    //glBindFramebuffer(GL_FRAMEBUFFER, defaultFBO);

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
	if (defaultFBO == -1)
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &defaultFBO);

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

static void nvgluBlurFramebuffer(NVGcontext* ctx, NVGLUframebuffer* fb, NVGLUframebuffer* temp_fb, int total_width, int total_height, float blurStrength) {

	// Multi-pass blur vert & frag implemented from: http://www.sunsetlakesoftware.com/2013/10/21/optimizing-gaussian-blurs-mobile-gpu/index.html

    // Vertex Shader
    const char* vertexShaderSource = R"(
        #version 150 core

        in vec4 position;
        in vec2 inputTextureCoordinate;

        uniform float texelWidthOffset;
        uniform float texelHeightOffset;

        out vec2 blurCoordinates[5];

        void main() {
            gl_Position = position;

            vec2 singleStepOffset = vec2(texelWidthOffset, texelHeightOffset);
            blurCoordinates[0] = inputTextureCoordinate.xy;
            blurCoordinates[1] = inputTextureCoordinate.xy + singleStepOffset * 1.407333;
            blurCoordinates[2] = inputTextureCoordinate.xy - singleStepOffset * 1.407333;
            blurCoordinates[3] = inputTextureCoordinate.xy + singleStepOffset * 3.294215;
            blurCoordinates[4] = inputTextureCoordinate.xy - singleStepOffset * 3.294215;
        }
    )";

    // Fragment Shader
    const char* fragmentShaderSource = R"(
        #version 150 core

        in vec2 blurCoordinates[5];
        out vec4 FragColor;

        uniform sampler2D inputImageTexture;

        void main() {
            vec4 sum = vec4(0.0);
            sum += texture(inputImageTexture, blurCoordinates[0]) * 0.204164;
            sum += texture(inputImageTexture, blurCoordinates[1]) * 0.304005;
            sum += texture(inputImageTexture, blurCoordinates[2]) * 0.304005;
            sum += texture(inputImageTexture, blurCoordinates[3]) * 0.093913;
            sum += texture(inputImageTexture, blurCoordinates[4]) * 0.093913;
            FragColor = sum;
        }
    )";

    // Compile shaders
    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &vertexShaderSource, nullptr);
    glCompileShader(vert);

    GLint vertSuccess;
    glGetShaderiv(vert, GL_COMPILE_STATUS, &vertSuccess);
    if (!vertSuccess) {
        char infoLog[512];
        glGetShaderInfoLog(vert, 512, nullptr, infoLog);
        fprintf(stderr, "Vertex Shader Compilation Failed:\n%s\n", infoLog);
    }

    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &fragmentShaderSource, nullptr);
    glCompileShader(frag);

    GLint fragSuccess;
    glGetShaderiv(frag, GL_COMPILE_STATUS, &fragSuccess);
    if (!fragSuccess) {
        char infoLog[512];
        glGetShaderInfoLog(frag, 512, nullptr, infoLog);
        fprintf(stderr, "Fragment Shader Compilation Failed:\n%s\n", infoLog);
    }

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vert);
    glAttachShader(shaderProgram, frag);
    glBindAttribLocation(shaderProgram, 0, "position");
    glBindAttribLocation(shaderProgram, 1, "inputTextureCoordinate");
    glLinkProgram(shaderProgram);

    GLint programSuccess;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &programSuccess);
    if (!programSuccess) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        fprintf(stderr, "Shader Program Linking Failed:\n%s\n", infoLog);
    }

    glDeleteShader(vert);
    glDeleteShader(frag);

    // Quad setup
    GLuint vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    float vertices[] = {
        // Positions      // Texture Coords
        -1.0f, -1.0f,     0.0f, 0.0f,
         1.0f, -1.0f,     1.0f, 0.0f,
         1.0f,  1.0f,     1.0f, 1.0f,
        -1.0f,  1.0f,     0.0f, 1.0f,
    };
    GLuint indices[] = { 0, 1, 2, 0, 2, 3 };

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Setup for rendering
    GLint currentProgram;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    glUseProgram(shaderProgram);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(shaderProgram, "inputImageTexture"), 0);

    GLuint texelWidthOffsetLoc = glGetUniformLocation(shaderProgram, "texelWidthOffset");
    GLuint texelHeightOffsetLoc = glGetUniformLocation(shaderProgram, "texelHeightOffset");

    // Perform 8 passes (4 horizontal, 4 vertical)
    for (int i = 0; i < 4; ++i) {
		float index = static_cast<float>(pow(2, i)) * std::min(blurStrength, 1.0f);

	    // Horizontal pass
    	nvgluBindFramebuffer(temp_fb);
    	glBindTexture(GL_TEXTURE_2D, fb->texture);

    	glUniform1f(texelWidthOffsetLoc, index / total_width);
    	glUniform1f(texelHeightOffsetLoc, 0.0f);

    	glViewport(0, 0, total_width, total_height);
    	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    	// Vertical pass
    	nvgluBindFramebuffer(fb);
    	glBindTexture(GL_TEXTURE_2D, temp_fb->texture);

    	glUniform1f(texelWidthOffsetLoc, 0.0f);
    	glUniform1f(texelHeightOffsetLoc, index / total_height);

    	glViewport(0, 0, total_width, total_height);
    	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    // Cleanup
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteProgram(shaderProgram);

    // Restore the previous program
    glUseProgram(currentProgram);
}


#endif // NANOVG_GL_IMPLEMENTATION
#endif // NANOVG_GL_UTILS_H
