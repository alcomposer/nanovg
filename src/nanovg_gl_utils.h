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
	GLuint texture;  // Colour attachment 0
	GLuint texture2; // Colour attachment 1
	int image;
	int image2;
	int width = 0;
	int height = 0;
};
typedef struct NVGLUframebuffer NVGLUframebuffer;

// Helper function to create GL frame buffer to render to.
void nvgluBindFramebuffer(NVGLUframebuffer* fb);
NVGLUframebuffer* nvgluCreateFramebuffer(NVGcontext* ctx, int w, int h, int imageFlags);
void nvgluGenerateMipmaps(NVGLUframebuffer* fb);
void nvgluDeleteFramebuffer(NVGLUframebuffer* fb);

#ifdef NANOVG_GL_IMPLEMENTATION

static GLint defaultFBO = -1;

// Blit a framebuffer to the currently bound framebuffer with XY offset inside source framebuffer
void nvgluBlitFramebuffer(NVGcontext* ctx, NVGLUframebuffer* fb, int srcXPos, int srcYPos, int desXPos, int desYPos, int w, int h)
{
    //glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);

	int sx = srcXPos;
	int sy = fb->height - (srcYPos + h);

	int dx = desXPos;
	int dy = h - (desYPos + h);

	//int dx = 0;
	//int dy = 0;

	// Store the currently bound draw framebuffer
	GLint currentDrawFBO;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &currentDrawFBO);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb->fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, currentDrawFBO);

    //glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	glBlitFramebuffer(
		sx, sy,                    // Source lower-left corner
		sx + w, sy + h,          // Source upper-right corner
		dx, dy,                    // Destination lower-left corner
		dx + w, dy + h,          // Destination upper-right corner
		GL_COLOR_BUFFER_BIT,       // Copy color buffer
		GL_NEAREST                 // Interpolation method
	);


    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        printf("OpenGL Error after glBlitFramebuffer: %d\n", error);
    }

    //glEnable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

NVGLUframebuffer* nvgluCreateFramebuffer(NVGcontext* ctx, int w, int h, int imageFlags)
{
	bool doubleColAtt = imageFlags & NVG_DOUBLE_COLOUR_ATTACH;

	GLint defaultFBO;
	GLint defaultRBO;
	NVGLUframebuffer* fb = NULL;

	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &defaultFBO);
	glGetIntegerv(GL_RENDERBUFFER_BINDING, &defaultRBO);

	fb = (NVGLUframebuffer*)malloc(sizeof(NVGLUframebuffer));
	if (fb == NULL) goto error;
	memset(fb, 0, sizeof(NVGLUframebuffer));

	fb->width = w;
	fb->height = h;

	fb->image = nvgCreateImageRGBA(ctx, w, h, imageFlags | NVG_IMAGE_FLIPY, NULL);
	fb->texture = nvglImageHandle(ctx, fb->image);

	if (doubleColAtt) {
		fb->image2 = nvgCreateImageRGBA(ctx, w, h, imageFlags | NVG_IMAGE_FLIPY, NULL);
		fb->texture2 = nvglImageHandle(ctx, fb->image2);
	}

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

	if (doubleColAtt)
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, fb->texture2, 0);

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
	if (fb->image2 >= 0)
		nvgDeleteImage(fb->ctx, fb->image2);
	fb->ctx = NULL;
	fb->fbo = 0;
	fb->rbo = 0;
	fb->texture = 0;
	fb->texture2 = 0;
	fb->image = -1;
	fb->image2 = -1;
	free(fb);
}

#endif

#define NANOVG_GL_IMPLEMENTATION
#ifdef NANOVG_GL_IMPLEMENTATION

// Blurs a framebuffer with 2 colour attachments using a multi-pass blur in linear colour
static void nvgluBlurFramebuffer(NVGcontext* ctx, NVGLUframebuffer* fb, int x, int y, int w, int h, int total_width, int total_height, float blurStrength) {
    // Shader sources
    const char* vertexShaderSource = R"(
        #version 150 core

        in vec4 position;
        in vec2 inputTextureCoordinate;

        uniform float texelWidthOffset;
        uniform float texelHeightOffset;

        out vec2 blurCoordinates[5];
		out vec2 TexCoord;

        void main() {
            gl_Position = position;

			TexCoord = inputTextureCoordinate;

            vec2 singleStepOffset = vec2(texelWidthOffset, texelHeightOffset);
            blurCoordinates[0] = inputTextureCoordinate.xy;
            blurCoordinates[1] = inputTextureCoordinate.xy + singleStepOffset * 1.407333f;
            blurCoordinates[2] = inputTextureCoordinate.xy - singleStepOffset * 1.407333f;
            blurCoordinates[3] = inputTextureCoordinate.xy + singleStepOffset * 3.294215f;
            blurCoordinates[4] = inputTextureCoordinate.xy - singleStepOffset * 3.294215f;
        }
    )";

    const char* sRGBToLinearFragmentShader = R"(
        #version 150 core

		in vec2 TexCoord;

        out vec4 FragColor;

        uniform sampler2D inputImageTexture;

        vec3 sRGBToLinear(vec3 color) {
            return pow(color, vec3(2.2f));
        }

        vec3 sRGBToLinear_Approx(vec3 color) {
            return color * color;
        }

        void main() {
            vec3 color = texture(inputImageTexture, TexCoord).rgb;
            FragColor = vec4(sRGBToLinear_Approx(color), 1.0f);
        }
    )";

    const char* blurFragmentShader = R"(
        #version 150 core

        in vec2 blurCoordinates[5];
        out vec4 FragColor;

        uniform sampler2D inputImageTexture;

        void main() {
            vec4 sum = vec4(0.0f);
            sum += texture(inputImageTexture, blurCoordinates[0]) * 0.204164f;
            sum += texture(inputImageTexture, blurCoordinates[1]) * 0.304005f;
            sum += texture(inputImageTexture, blurCoordinates[2]) * 0.304005f;
            sum += texture(inputImageTexture, blurCoordinates[3]) * 0.093913f;
            sum += texture(inputImageTexture, blurCoordinates[4]) * 0.093913f;
            FragColor = sum;
        }
    )";

    const char* linearToSRGBFragmentShader = R"(
        #version 150 core

        in vec2 TexCoord;
        out vec4 FragColor;

        uniform sampler2D inputImageTexture;

        vec3 linearToSRGB(vec3 color) {
            return pow(color, vec3(1.0f / 2.2f));
        }

        vec3 linearToSRGB_Approx(vec3 color) {
            return sqrt(color);
        }

        void main() {
            vec3 color = texture(inputImageTexture, TexCoord).rgb;
            FragColor = vec4(linearToSRGB_Approx(color), 1.0f);
        }
    )";

    // Compile and link shaders
    auto compileShader = [](const char *vertSource, const char *fragSource) {
	    // Create and compile vertex shader
	    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	    glShaderSource(vertexShader, 1, &vertSource, nullptr);
	    glCompileShader(vertexShader);

	    // Check for vertex shader compilation errors
	    GLint vertexCompiled;
	    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &vertexCompiled);
	    if (!vertexCompiled) {
		    GLint logLength;
		    glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &logLength);
		    std::vector<GLchar> log(logLength);
		    glGetShaderInfoLog(vertexShader, logLength, nullptr, log.data());
		    std::cerr << "Vertex Shader Compilation Error:\n" << log.data() << std::endl;
	    }

	    // Create and compile fragment shader
	    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	    glShaderSource(fragmentShader, 1, &fragSource, nullptr);
	    glCompileShader(fragmentShader);

	    // Check for fragment shader compilation errors
	    GLint fragmentCompiled;
	    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &fragmentCompiled);
	    if (!fragmentCompiled) {
		    GLint logLength;
		    glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &logLength);
		    std::vector<GLchar> log(logLength);
		    glGetShaderInfoLog(fragmentShader, logLength, nullptr, log.data());
		    std::cerr << "Fragment Shader Compilation Error:\n" << log.data() << std::endl;
	    }

	    // Create and link shader program
	    GLuint program = glCreateProgram();
	    glAttachShader(program, vertexShader);
	    glAttachShader(program, fragmentShader);
	    glLinkProgram(program);

	    // Check for linking errors
	    GLint programLinked;
	    glGetProgramiv(program, GL_LINK_STATUS, &programLinked);
	    if (!programLinked) {
		    GLint logLength;
		    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
		    std::vector<GLchar> log(logLength);
		    glGetProgramInfoLog(program, logLength, nullptr, log.data());
		    std::cerr << "Shader Program Linking Error:\n" << log.data() << std::endl;
	    }

	    // Clean up shaders
	    glDeleteShader(vertexShader);
	    glDeleteShader(fragmentShader);

	    return program;
    };

    GLuint sRGBToLinearShader = compileShader(vertexShaderSource, sRGBToLinearFragmentShader);
    GLuint blurShader = compileShader(vertexShaderSource, blurFragmentShader);
    GLuint linearToSRGBShader = compileShader(vertexShaderSource, linearToSRGBFragmentShader);

    // Setup quad
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

	// Bind the vertex attributes to the vertex shader - TODO: Use getLocation so it's clearer
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

	GLint currentProgram;
	glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);

	//glDisable(GL_SCISSOR_TEST);

    // Step 1: Convert sRGB to Linear Space
	glUseProgram(sRGBToLinearShader);

    nvgluBindFramebuffer(fb);

    glBindTexture(GL_TEXTURE_2D, fb->texture);

	GLenum drawBuffers[1];
	drawBuffers[0] = GL_COLOR_ATTACHMENT1;
	glDrawBuffers(1, drawBuffers);

    glViewport(0, 0, w, h);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // Step 2: Perform Blur in Linear Space (Blur can only be applied correctly in linear space)
    glUseProgram(blurShader);
    GLuint texelWidthOffsetLoc = glGetUniformLocation(blurShader, "texelWidthOffset");
    GLuint texelHeightOffsetLoc = glGetUniformLocation(blurShader, "texelHeightOffset");

#ifdef FIB_PASSES
	// Fibonacci blur pass offsets
	const int passes = 6;
	const int blurPassOffset[passes] = { 1, 1, 2, 3, 5, 8 };
#else
	// Power of 2 blur pass offsets
	const int passes = 4;
	const int blurPassOffset[passes] = { 1, 2, 4, 8 };
#endif

    for (int i = 0; i < passes; ++i) {
        float index = blurPassOffset[i] * std::min(blurStrength, 1.0f);

        // Horizontal pass
        glBindTexture(GL_TEXTURE_2D, fb->texture2);

    	drawBuffers[0] = GL_COLOR_ATTACHMENT0;
    	glDrawBuffers(1, drawBuffers);

        glUniform1f(texelWidthOffsetLoc, index / w);
        glUniform1f(texelHeightOffsetLoc, 0.0f);
        //glViewport(0, 0, w, h);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // Vertical pass
        glBindTexture(GL_TEXTURE_2D, fb->texture);

    	drawBuffers[0] = GL_COLOR_ATTACHMENT1;
    	glDrawBuffers(1, drawBuffers);

        glUniform1f(texelWidthOffsetLoc, 0.0f);
        glUniform1f(texelHeightOffsetLoc, index / h);
        //glViewport(0, 0, w, h);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    }

    // Step 3: Convert Linear Space Back to sRGB
	glUseProgram(linearToSRGBShader);
    glBindTexture(GL_TEXTURE_2D, fb->texture2);

	drawBuffers[0] = GL_COLOR_ATTACHMENT0;
	glDrawBuffers(1, drawBuffers);

    //glViewport(0, 0, w, h);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // Cleanup
    glDeleteProgram(sRGBToLinearShader);
    glDeleteProgram(blurShader);
    glDeleteProgram(linearToSRGBShader);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);

	// Restore the previous program
	glUseProgram(currentProgram);
	//glEnable(GL_SCISSOR_TEST);
}

#endif // NANOVG_GL_IMPLEMENTATION
#endif // NANOVG_GL_UTILS_H
