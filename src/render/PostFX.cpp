#include "render/PostFX.hpp"

#include "render/Shader.hpp"

#include <glm/gtc/matrix_inverse.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    std::string readShaderFile(const std::string &filePath)
    {
        namespace fs = std::filesystem;
        std::vector<fs::path> candidates;
        candidates.emplace_back(filePath);
#ifdef ASSET_ROOT
        candidates.emplace_back(fs::path(ASSET_ROOT) / filePath);
        candidates.emplace_back(fs::path(ASSET_ROOT) / "src" / filePath);
#endif
        candidates.emplace_back(fs::path("./") / filePath);
        candidates.emplace_back(fs::path("shaders/") / fs::path(filePath).filename());
        candidates.emplace_back(fs::path("./shaders/") / fs::path(filePath).filename());
        candidates.emplace_back(fs::path("src/shaders/") / fs::path(filePath).filename());

        for (auto &c : candidates)
        {
            std::ifstream f(c);
            if (f)
            {
                std::stringstream buf;
                buf << f.rdbuf();
                return buf.str();
            }
        }
        std::cerr << "PostFX: failed to load shader: " << filePath << std::endl;
        return std::string();
    }

    std::string adjustForGLES(std::string src, bool isFragment)
    {
#if defined(__EMSCRIPTEN__)
        const std::string v330 = "#version 330 core";
        const std::string v300 = "#version 300 es";
        size_t pos = src.find(v330);
        if (pos != std::string::npos)
            src.replace(pos, v330.size(), v300);
        if (isFragment && src.find("precision") == std::string::npos)
        {
            // highp avoids precision artifacts in the sky shader's matrix
            // reconstruction and in distance-based fog at large world scales.
            size_t p = src.find(v300);
            std::string precision = "\nprecision highp float;\n";
            if (p != std::string::npos)
                src.insert(p + v300.size(), precision);
            else
                src = precision + src;
        }
#else
        (void)isFragment;
#endif
        return src;
    }

    unsigned int compileShader(unsigned int type, const char *src)
    {
        unsigned int s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        int ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            char log[1024];
            glGetShaderInfoLog(s, sizeof(log), nullptr, log);
            std::cerr << "PostFX shader compile error: " << log << std::endl;
        }
        return s;
    }
}

PostFX::PostFX() = default;

PostFX::~PostFX()
{
    destroyResources();
    if (mSkyProgram)
        glDeleteProgram(mSkyProgram);
    if (mExtractProgram)
        glDeleteProgram(mExtractProgram);
    if (mBlurProgram)
        glDeleteProgram(mBlurProgram);
    if (mCompositeProgram)
        glDeleteProgram(mCompositeProgram);
    if (mQuadVao)
        glDeleteVertexArrays(1, &mQuadVao);
}

void PostFX::destroyResources()
{
    if (mSceneDepth)
        glDeleteRenderbuffers(1, &mSceneDepth);
    if (mSceneColor)
        glDeleteTextures(1, &mSceneColor);
    if (mSceneFbo)
        glDeleteFramebuffers(1, &mSceneFbo);
    if (mBloomColorA)
        glDeleteTextures(1, &mBloomColorA);
    if (mBloomFboA)
        glDeleteFramebuffers(1, &mBloomFboA);
    if (mBloomColorB)
        glDeleteTextures(1, &mBloomColorB);
    if (mBloomFboB)
        glDeleteFramebuffers(1, &mBloomFboB);
    mSceneDepth = mSceneColor = mSceneFbo = 0;
    mBloomColorA = mBloomFboA = 0;
    mBloomColorB = mBloomFboB = 0;
}

unsigned int PostFX::compileProgram(const char *vertSrc, const char *fragSrc)
{
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertSrc);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    int ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::cerr << "PostFX program link error: " << log << std::endl;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

bool PostFX::createShaders()
{
    if (mShadersReady)
        return true;

    std::string vsrc = adjustForGLES(readShaderFile("shaders/fullscreen_vert.glsl"), false);
    std::string skySrc = adjustForGLES(readShaderFile("shaders/sky_vaporwave_frag.glsl"), true);
    std::string extractSrc = adjustForGLES(readShaderFile("shaders/bloom_extract_frag.glsl"), true);
    std::string blurSrc = adjustForGLES(readShaderFile("shaders/bloom_blur_frag.glsl"), true);
    std::string compositeSrc = adjustForGLES(readShaderFile("shaders/bloom_composite_frag.glsl"), true);

    if (vsrc.empty() || skySrc.empty() || extractSrc.empty() || blurSrc.empty() || compositeSrc.empty())
        return false;

    mSkyProgram = compileProgram(vsrc.c_str(), skySrc.c_str());
    mExtractProgram = compileProgram(vsrc.c_str(), extractSrc.c_str());
    mBlurProgram = compileProgram(vsrc.c_str(), blurSrc.c_str());
    mCompositeProgram = compileProgram(vsrc.c_str(), compositeSrc.c_str());

    if (mQuadVao == 0)
        glGenVertexArrays(1, &mQuadVao);

    mShadersReady = true;
    return true;
}

static void allocColorTex(unsigned int &tex, int w, int h)
{
    if (tex == 0)
        glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

bool PostFX::ensure(int width, int height)
{
    if (width < 2)
        width = 2;
    if (height < 2)
        height = 2;

    if (!createShaders())
        return false;

    if (width == mWidth && height == mHeight && mSceneFbo)
        return true;

    destroyResources();
    mWidth = width;
    mHeight = height;
    mBloomW = std::max(2, width / 2);
    mBloomH = std::max(2, height / 2);

    // Scene FBO
    glGenFramebuffers(1, &mSceneFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, mSceneFbo);
    allocColorTex(mSceneColor, mWidth, mHeight);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mSceneColor, 0);
    glGenRenderbuffers(1, &mSceneDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, mSceneDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, mWidth, mHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, mSceneDepth);

    // Bloom ping-pong FBOs
    glGenFramebuffers(1, &mBloomFboA);
    glBindFramebuffer(GL_FRAMEBUFFER, mBloomFboA);
    allocColorTex(mBloomColorA, mBloomW, mBloomH);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mBloomColorA, 0);

    glGenFramebuffers(1, &mBloomFboB);
    glBindFramebuffer(GL_FRAMEBUFFER, mBloomFboB);
    allocColorTex(mBloomColorB, mBloomW, mBloomH);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mBloomColorB, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "PostFX: bloom FBO incomplete: 0x" << std::hex << status << std::dec << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, mSceneFbo);
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "PostFX: scene FBO incomplete: 0x" << std::hex << status << std::dec << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void PostFX::beginSceneCapture()
{
    glBindFramebuffer(GL_FRAMEBUFFER, mSceneFbo);
    glViewport(0, 0, mWidth, mHeight);
    // Match the engine's classic background color so when every VFX feature
    // is disabled the view looks identical to the pre-VFX renderer.
    glClearColor(0.06f, 0.06f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void PostFX::drawFullscreenTriangle()
{
    glBindVertexArray(mQuadVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void PostFX::drawBackground(const glm::mat4 &view, const glm::mat4 &projection,
                            const glm::vec3 &camPos,
                            const editor::Context::VFX &vfx)
{
    if (!mShadersReady)
        return;

    // Nothing to draw — the scene FBO clear color is already the background.
    if (!vfx.skyEnabled && !vfx.sunEnabled && !vfx.gridEnabled)
        return;

    glm::mat4 invVP = glm::inverse(projection * view);

    // Sun screen position. In world-anchored mode we project a unit direction
    // (azimuth + elevation) onto NDC and convert to the shader's sunPos
    // convention: x in [-1, 1], y = height above horizon (where horizon = 0).
    // sunPos.y in the shader is rescaled by *0.5 then offset by +0.5, so to
    // place the sun at NDC y_ndc, we pass y = (y_ndc - 0) * 1.0 ... let's just
    // pass screen-space (sx, sy) directly and rework the shader's center calc.
    glm::vec2 sunScreen = vfx.sunPos;
    int sunVisible = vfx.sunEnabled ? 1 : 0;
    if (vfx.sunEnabled && vfx.sunWorldAnchored)
    {
        float az = glm::radians(vfx.sunAzimuth);
        float el = glm::radians(vfx.sunElevation);
        glm::vec3 sunDir(std::sin(az) * std::cos(el),
                         std::sin(el),
                         -std::cos(az) * std::cos(el));
        // Project a point at infinity (camPos + sunDir * far) into NDC.
        glm::vec4 clip = projection * view * glm::vec4(camPos + sunDir * 1000.0f, 1.0f);
        if (clip.w <= 0.0f)
        {
            sunVisible = 0;
        }
        else
        {
            glm::vec3 ndc = glm::vec3(clip) / clip.w;
            // Pass raw NDC to the shader as (ndcX, ndcY). The shader builds
            // its sunCenter as (ndcX*0.5+0.5, ndcY*0.5+0.5).
            sunScreen = glm::vec2(ndc.x, ndc.y);
        }
    }

    glUseProgram(mSkyProgram);
    glUniformMatrix4fv(glGetUniformLocation(mSkyProgram, "uInvViewProj"), 1, GL_FALSE, &invVP[0][0]);
    glUniform3f(glGetUniformLocation(mSkyProgram, "uCamPos"), camPos.x, camPos.y, camPos.z);
    glUniform3f(glGetUniformLocation(mSkyProgram, "uSkyTop"), vfx.skyTop.x, vfx.skyTop.y, vfx.skyTop.z);
    glUniform3f(glGetUniformLocation(mSkyProgram, "uSkyMid"), vfx.skyMid.x, vfx.skyMid.y, vfx.skyMid.z);
    glUniform3f(glGetUniformLocation(mSkyProgram, "uSkyBottom"), vfx.skyBottom.x, vfx.skyBottom.y, vfx.skyBottom.z);
    glUniform2f(glGetUniformLocation(mSkyProgram, "uSunPos"), sunScreen.x, sunScreen.y);
    glUniform1i(glGetUniformLocation(mSkyProgram, "uSunWorldAnchored"), vfx.sunWorldAnchored ? 1 : 0);
    glUniform1i(glGetUniformLocation(mSkyProgram, "uSunVisible"), sunVisible);
    glUniform1f(glGetUniformLocation(mSkyProgram, "uSunRadius"), vfx.sunRadius);
    glUniform3f(glGetUniformLocation(mSkyProgram, "uSunColor"), vfx.sunColor.x, vfx.sunColor.y, vfx.sunColor.z);
    glUniform1i(glGetUniformLocation(mSkyProgram, "uSunStripes"), vfx.sunStripes);
    glUniform1i(glGetUniformLocation(mSkyProgram, "uSkyEnabled"), vfx.skyEnabled ? 1 : 0);
    glUniform1i(glGetUniformLocation(mSkyProgram, "uGridEnabled"), vfx.gridEnabled ? 1 : 0);
    glUniform3f(glGetUniformLocation(mSkyProgram, "uGridColor"), vfx.gridColor.x, vfx.gridColor.y, vfx.gridColor.z);
    glUniform1f(glGetUniformLocation(mSkyProgram, "uGridScale"), vfx.gridScale);
    glUniform1f(glGetUniformLocation(mSkyProgram, "uGridFade"), vfx.gridFade);
    glUniform1f(glGetUniformLocation(mSkyProgram, "uGridLineWidth"), vfx.gridLineWidth);
    glUniform1f(glGetUniformLocation(mSkyProgram, "uGridPlaneY"), vfx.horizonY);
    float aspect = static_cast<float>(mWidth) / static_cast<float>(std::max(1, mHeight));
    glUniform1f(glGetUniformLocation(mSkyProgram, "uAspect"), aspect);

    // Draw behind scene geometry: don't write depth, don't test depth.
    GLboolean depthWasOn = glIsEnabled(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    drawFullscreenTriangle();
    glDepthMask(GL_TRUE);
    if (depthWasOn)
        glEnable(GL_DEPTH_TEST);
}

void PostFX::compositeTo(unsigned int targetFbo, int targetX, int targetY,
                         int targetW, int targetH,
                         const editor::Context::VFX &vfx,
                         float timeSeconds)
{
    if (!mShadersReady)
        return;

    GLboolean depthWasOn = glIsEnabled(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    unsigned int bloomSrc = mSceneColor;

    if (vfx.bloomEnabled)
    {
        // 1. Extract bright pixels from scene into bloomA
        glBindFramebuffer(GL_FRAMEBUFFER, mBloomFboA);
        glViewport(0, 0, mBloomW, mBloomH);
        glUseProgram(mExtractProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mSceneColor);
        glUniform1i(glGetUniformLocation(mExtractProgram, "uScene"), 0);
        glUniform1f(glGetUniformLocation(mExtractProgram, "uThreshold"), vfx.bloomThreshold);
        glUniform1f(glGetUniformLocation(mExtractProgram, "uSoftKnee"), 0.5f);
        drawFullscreenTriangle();

        // 2. Ping-pong gaussian blur. Each iteration is one H + one V pass.
        glUseProgram(mBlurProgram);
        float texelX = vfx.bloomRadius / static_cast<float>(mBloomW);
        float texelY = vfx.bloomRadius / static_cast<float>(mBloomH);
        unsigned int readTex = mBloomColorA;
        unsigned int writeFbo = mBloomFboB;
        unsigned int writeTex = mBloomColorB;
        for (int i = 0; i < vfx.bloomIterations; ++i)
        {
            // Horizontal
            glBindFramebuffer(GL_FRAMEBUFFER, writeFbo);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, readTex);
            glUniform1i(glGetUniformLocation(mBlurProgram, "uTex"), 0);
            glUniform2f(glGetUniformLocation(mBlurProgram, "uTexelStep"), texelX, 0.0f);
            drawFullscreenTriangle();
            std::swap(readTex, writeTex);
            writeFbo = (writeFbo == mBloomFboB) ? mBloomFboA : mBloomFboB;

            // Vertical
            glBindFramebuffer(GL_FRAMEBUFFER, writeFbo);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, readTex);
            glUniform1i(glGetUniformLocation(mBlurProgram, "uTex"), 0);
            glUniform2f(glGetUniformLocation(mBlurProgram, "uTexelStep"), 0.0f, texelY);
            drawFullscreenTriangle();
            std::swap(readTex, writeTex);
            writeFbo = (writeFbo == mBloomFboB) ? mBloomFboA : mBloomFboB;
        }
        bloomSrc = readTex;
    }

    // 3. Composite into the target framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, targetFbo);
    glViewport(targetX, targetY, targetW, targetH);
    glUseProgram(mCompositeProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mSceneColor);
    glUniform1i(glGetUniformLocation(mCompositeProgram, "uScene"), 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, vfx.bloomEnabled ? bloomSrc : mSceneColor);
    glUniform1i(glGetUniformLocation(mCompositeProgram, "uBloom"), 1);
    glUniform1f(glGetUniformLocation(mCompositeProgram, "uBloomIntensity"),
                vfx.bloomEnabled ? vfx.bloomIntensity : 0.0f);
    // Apply Reinhard tonemap + gamma only when bloom is on. With bloom off
    // the scene FBO already holds final, in-range colors and tonemapping
    // would darken them noticeably (Reinhard maps 1.0 → 0.5).
    glUniform1i(glGetUniformLocation(mCompositeProgram, "uTonemap"), vfx.bloomEnabled ? 1 : 0);
    glUniform1i(glGetUniformLocation(mCompositeProgram, "uScanlines"), vfx.scanlinesEnabled ? 1 : 0);
    glUniform1f(glGetUniformLocation(mCompositeProgram, "uScanlineStrength"), vfx.scanlineStrength);
    glUniform1f(glGetUniformLocation(mCompositeProgram, "uTime"), timeSeconds);
    glUniform2f(glGetUniformLocation(mCompositeProgram, "uOutputSize"),
                static_cast<float>(targetW), static_cast<float>(targetH));
    drawFullscreenTriangle();

    glActiveTexture(GL_TEXTURE0);
    glDepthMask(GL_TRUE);
    if (depthWasOn)
        glEnable(GL_DEPTH_TEST);
}
