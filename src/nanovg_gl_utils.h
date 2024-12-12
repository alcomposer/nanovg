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

static void nvgluBlurFramebuffer(NVGcontext* ctx, NVGLUframebuffer* fb, int total_width, int total_height, float blurAmount, float alpha) {
    // Vertex Shader
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

    // Fragment Shader
    const char* fragmentShaderSource = R"(
        #version 150 core

        in vec2 TexCoords;
        uniform sampler2D texture1;
        uniform float blurAmount;   // Control the blur intensity
        uniform int samples;        // Number of blur samples
        uniform int LOD;            // Level of detail (for mipmap sampling)
		uniform float alpha;

        // Gaussian function to calculate weight
        float gaussian(vec2 i, float sigma) {
            return exp(-0.5 * dot(i /= sigma, i)) / (6.28 * sigma * sigma);
        }

        // Perform Gaussian blur by sampling around the pixel
        vec4 blur(sampler2D sp, vec2 U, vec2 scale, int samples, float sigma) {
            vec4 color = vec4(0.0);
            float weightSum = 0.0;

            // Perform blur in a grid around the current pixel
            for (int x = -samples; x <= samples; x++) {
                for (int y = -samples; y <= samples; y++) {
                    vec2 offset = vec2(x, y) * scale;
                    float weight = gaussian(vec2(x, y), sigma);
                    color += weight * textureLod(sp, U + offset, float(LOD));
                    weightSum += weight;
                }
            }

            // Normalize the accumulated color
            return color / weightSum;
        }

        void main(void) {
            vec2 scale = 1.0 / textureSize(texture1, 0); // Normalized texel size
            float sigma = blurAmount * 0.8;  // Adjust blur intensity with blurAmount
            gl_FragColor = vec4(blur(texture1, TexCoords, scale, samples, sigma).rgb * alpha, alpha);
        }
    )";

    // Compile shaders
    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &vertexShaderSource, nullptr);
    glCompileShader(vert);

    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &fragmentShaderSource, nullptr);
    glCompileShader(frag);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vert);
    glAttachShader(shaderProgram, frag);
    glBindAttribLocation(shaderProgram, 0, "aPos");
    glBindAttribLocation(shaderProgram, 1, "aTexCoord");
    glLinkProgram(shaderProgram);

    glDeleteShader(vert);
    glDeleteShader(frag);

    // Prepare for rendering
    GLuint vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    float vertices[] = {
        // Positions       // Texture Coords
        -1.0f, -1.0f,      0.0f, 0.0f,
         1.0f, -1.0f,      1.0f, 0.0f,
         1.0f,  1.0f,      1.0f, 1.0f,
        -1.0f,  1.0f,      0.0f, 1.0f,
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

    // Blur passes
	GLint currentProgram;
	glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);

    glUseProgram(shaderProgram);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
    GLuint blurAmountLocation = glGetUniformLocation(shaderProgram, "blurAmount");
    GLuint samplesLocation = glGetUniformLocation(shaderProgram, "samples");
    GLuint LODLocation = glGetUniformLocation(shaderProgram, "LOD");
	GLuint alphaLocation = glGetUniformLocation(shaderProgram, "alpha");

    NVGLUframebuffer* temp_fb = nvgluCreateFramebuffer(ctx, total_width, total_height, NVG_IMAGE_NEAREST);

    // Set sample count for the blur
    int samples = 20;  // Increase this value for smoother blur

    // Set level of detail for mipmap sampling (0 for base level)
    int LOD = 0;

    for (int i = 0; i < 2; ++i) { // Horizontal pass, then vertical pass
        glUniform1f(blurAmountLocation, blurAmount);
        glUniform1i(samplesLocation, samples);
        glUniform1i(LODLocation, LOD);
    	glUniform1f(alphaLocation, alpha);

        if (i == 0) {
            nvgluBindFramebuffer(temp_fb);
            glBindTexture(GL_TEXTURE_2D, fb->texture);
        } else {
            nvgluBindFramebuffer(fb);
            glBindTexture(GL_TEXTURE_2D, temp_fb->texture);
        }

        glViewport(0, 0, total_width, total_height);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    // Cleanup
    nvgluDeleteFramebuffer(temp_fb);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteProgram(shaderProgram);

	// Reset the previous shader program
	glUseProgram(currentProgram);
}




#endif // NANOVG_GL_IMPLEMENTATION
#endif // NANOVG_GL_UTILS_H
