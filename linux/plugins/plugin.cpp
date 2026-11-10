#include "compositor.h"
#include <gsr/plugin.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <QFileInfo>
#include <chrono>
#include <cstdio>
#include <map>

using namespace Shadowless;
namespace {
struct Texture { GLuint id = 0; qint64 key = 0; };
struct Plugin {
    CompositorWorker worker;
    QVector<RenderLayer> frames;
    std::map<QString, Texture> textures;
    GLuint program = 0, vao = 0, buffer = 0;
    GLint sampler = -1, opacity = -1;
    quint64 calls = 0, uploads = 0, renderNs = 0;
};
GLuint shader(GLenum type, const QByteArray &source) {
    const GLuint result = glCreateShader(type); const char *text = source.constData();
    glShaderSource(result, 1, &text, nullptr); glCompileShader(result);
    GLint ok = 0; glGetShaderiv(result, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048]; glGetShaderInfoLog(result, sizeof(log), nullptr, log);
        fprintf(stderr, "Shadowless overlay shader: %s\n", log); glDeleteShader(result); return 0;
    }
    return result;
}
void draw(const gsr_plugin_draw_params *params, void *userdata) {
    auto &p = *static_cast<Plugin *>(userdata); p.worker.snapshot(&p.frames);
    if (p.frames.isEmpty() && p.textures.empty()) return;
    const auto begin = std::chrono::steady_clock::now();
    GLint program, vao, buffer, active, texture, srcRgb, dstRgb, srcAlpha, dstAlpha, equationRgb, equationAlpha, unpack;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program); glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer); glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    glActiveTexture(GL_TEXTURE0); glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
    glGetIntegerv(GL_BLEND_SRC_RGB, &srcRgb); glGetIntegerv(GL_BLEND_DST_RGB, &dstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcAlpha); glGetIntegerv(GL_BLEND_DST_ALPHA, &dstAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &equationRgb); glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &equationAlpha);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpack);
    const GLboolean blend = glIsEnabled(GL_BLEND), depth = glIsEnabled(GL_DEPTH_TEST), scissor = glIsEnabled(GL_SCISSOR_TEST);
    glEnable(GL_BLEND); glDisable(GL_DEPTH_TEST); glDisable(GL_SCISSOR_TEST);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD); glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glUseProgram(p.program); glUniform1i(p.sampler, 0); glBindVertexArray(p.vao); glBindBuffer(GL_ARRAY_BUFFER, p.buffer);
    for (auto it = p.textures.begin(); it != p.textures.end();) {
        const bool exists = std::any_of(p.frames.cbegin(), p.frames.cend(), [&](const RenderLayer &f) { return f.layer.id == it->first; });
        if (!exists) { glDeleteTextures(1, &it->second.id); it = p.textures.erase(it); } else ++it;
    }
    // The first row is the topmost layer, matching the Windows editor.
    for (auto it = p.frames.crbegin(); it != p.frames.crend(); ++it) {
        const auto &frame = *it; auto &cached = p.textures[frame.layer.id];
        if (!cached.id) glGenTextures(1, &cached.id);
        glBindTexture(GL_TEXTURE_2D, cached.id);
        if (cached.key != frame.image.cacheKey()) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frame.image.width(), frame.image.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, frame.image.constBits());
            cached.key = frame.image.cacheKey(); ++p.uploads;
        }
        const auto rect = frame.layer.fitRect(QSize(static_cast<int>(params->width), static_cast<int>(params->height)), frame.image.size());
        const float left = static_cast<float>(2. * rect.left() / params->width - 1.);
        const float right = static_cast<float>(2. * rect.right() / params->width - 1.);
        // GSR's plugin framebuffer is inverted before encoding, unlike a desktop framebuffer.
        const float top = static_cast<float>(2. * rect.top() / params->height - 1.);
        const float bottom = static_cast<float>(2. * rect.bottom() / params->height - 1.);
        const GLfloat vertices[] = {left,top,0,0, left,bottom,0,1, right,top,1,0, right,bottom,1,1};
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glUniform1f(p.opacity, frame.layer.opacity / 100.f); glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    glBindVertexArray(static_cast<GLuint>(vao)); glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(buffer));
    glUseProgram(static_cast<GLuint>(program)); glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture)); glActiveTexture(static_cast<GLenum>(active));
    glPixelStorei(GL_UNPACK_ALIGNMENT, unpack); glBlendEquationSeparate(equationRgb, equationAlpha);
    glBlendFuncSeparate(srcRgb, dstRgb, srcAlpha, dstAlpha);
    if (!blend) glDisable(GL_BLEND);
    if (depth) glEnable(GL_DEPTH_TEST);
    if (scissor) glEnable(GL_SCISSOR_TEST);
    ++p.calls; p.renderNs += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - begin).count();
}
bool damaged(void *userdata) { return !static_cast<Plugin *>(userdata)->frames.isEmpty(); }
}

extern "C" bool gsr_plugin_init(const gsr_plugin_init_params *params, gsr_plugin_init_return *ret) {
    const QString socket = qEnvironmentVariable("SHADOWLESS_OVERLAY_SOCKET");
    if (socket.isEmpty() || params->color_depth != GSR_PLUGIN_COLOR_DEPTH_8_BITS) {
        fprintf(stderr, "Shadowless overlay requires a private socket and SDR capture.\n"); return false;
    }
    auto p = std::make_unique<Plugin>(); QString error;
    if (!p->worker.start(socket, QSize(static_cast<int>(params->width), static_cast<int>(params->height)), &error)) {
        fprintf(stderr, "Shadowless overlay: %s\n", qPrintable(error)); return false;
    }
    const QByteArray version = params->graphics_api == GSR_PLUGIN_GRAPHICS_API_EGL_ES ? "#version 100\nprecision mediump float;\n" : "#version 130\n";
    const GLuint vertex = shader(GL_VERTEX_SHADER, version + "attribute vec2 pos; attribute vec2 uv; varying vec2 texcoord; void main(){texcoord=uv;gl_Position=vec4(pos,0.,1.);}");
    const GLuint fragment = shader(GL_FRAGMENT_SHADER, version + "varying vec2 texcoord; uniform sampler2D image; uniform float opacity; void main(){vec4 c=texture2D(image,texcoord);gl_FragColor=vec4(c.rgb,c.a*opacity);}");
    if (!vertex || !fragment) { if (vertex) glDeleteShader(vertex); if (fragment) glDeleteShader(fragment); return false; }
    p->program = glCreateProgram(); glAttachShader(p->program, vertex); glAttachShader(p->program, fragment);
    glBindAttribLocation(p->program, 0, "pos"); glBindAttribLocation(p->program, 1, "uv"); glLinkProgram(p->program);
    glDeleteShader(vertex); glDeleteShader(fragment);
    GLint ok; glGetProgramiv(p->program, GL_LINK_STATUS, &ok);
    if (!ok) { glDeleteProgram(p->program); return false; }
    GLint oldVao, oldBuffer; glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVao); glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &oldBuffer);
    glGenVertexArrays(1, &p->vao); glBindVertexArray(p->vao); glGenBuffers(1, &p->buffer); glBindBuffer(GL_ARRAY_BUFFER, p->buffer);
    glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(GLfloat), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), reinterpret_cast<void *>(2 * sizeof(GLfloat)));
    glBindVertexArray(oldVao); glBindBuffer(GL_ARRAY_BUFFER, oldBuffer);
    p->sampler = glGetUniformLocation(p->program, "image"); p->opacity = glGetUniformLocation(p->program, "opacity");
    ret->name = "shadowless-overlay"; ret->version = 1; ret->draw = draw; ret->is_damaged = damaged; ret->userdata = p.release();
    return true;
}
extern "C" void gsr_plugin_deinit(void *userdata) {
    std::unique_ptr<Plugin> p(static_cast<Plugin *>(userdata));
    for (auto &entry : p->textures) glDeleteTextures(1, &entry.second.id);
    glDeleteBuffers(1, &p->buffer); glDeleteVertexArrays(1, &p->vao); glDeleteProgram(p->program);
    fprintf(stderr, "Shadowless overlay metrics: draws=%llu uploads=%llu mean_cpu_us=%.2f\n",
        static_cast<unsigned long long>(p->calls), static_cast<unsigned long long>(p->uploads), p->calls ? p->renderNs / (1000. * p->calls) : 0.);
}
