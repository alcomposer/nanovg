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
	glBlitFramebuffer(x, y, x2, y2, x, y, x2, y2, GL_COLOR_BUFFER_BIT, GL_NEAREST);

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
	// Check for multisampling support
	GLint maxSamples;
	glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
	std::cout << "Maximum MSAA samples supported: " << maxSamples << std::endl;
	int msaaSamples = 0;
	if (maxSamples > 9) {
		msaaSamples = 9;
		glEnable(GL_MULTISAMPLE);
	}

    GLint defaultFBO;
    GLint defaultRBO;
    NVGLUframebuffer* fb = NULL;

    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &defaultFBO);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &defaultRBO);

    fb = (NVGLUframebuffer*)malloc(sizeof(NVGLUframebuffer));
    if (fb == NULL) goto error;
    memset(fb, 0, sizeof(NVGLUframebuffer));

    fb->ctx = ctx;

    // Create the standard texture for resolved output
    fb->image = nvgCreateImageRGBA(ctx, w, h, imageFlags | NVG_IMAGE_FLIPY, NULL);
    fb->texture = nvglImageHandle(ctx, fb->image);

    // Create a multisampled framebuffer object
    glGenFramebuffers(1, &fb->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fb->fbo);

    // Create a multisampled renderbuffer for color
    glGenRenderbuffers(1, &fb->rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, fb->rbo);

	if (imageFlags & NVG_IMAGE_MULTISAMPLE && msaaSamples > 0) {
		glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaaSamples, GL_RGBA8, w, h);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb->texture, 0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, fb->rbo);
	} else {
		glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, w, h);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb->texture, 0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fb->rbo);
	}

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        goto error;
    }

    // Restore default framebuffer bindings
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
