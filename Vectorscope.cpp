#include "Vectorscope.h"
using namespace ffglex;

static CFFGLPluginInfo PluginInfo(
    PluginFactory< Vectorscope >,// Create method
    "VCTR",                      // Plugin unique ID of maximum length 4.
    "Vectorscope",               // Plugin name
    2,                           // API major version number
    1,                           // API minor version number
    1,                           // Plugin major version number
    0,                           // Plugin minor version number
    FF_EFFECT,                   // Plugin type
    "Displays a vectorscope",    // Plugin description
    "Vectorscope FFGL Plugin"    // About
);

static const char _vertexShaderCode[] = R"(#version 410 core
uniform vec2 MaxUV;
layout( location = 0 ) in vec4 vPosition;
layout( location = 1 ) in vec2 vUV;
out vec2 uv;
void main()
{
    gl_Position = vPosition;
    uv = vUV * MaxUV;
})";

static const char _fragmentShaderCode[] = R"(#version 410 core
uniform sampler2D InputTexture;
uniform vec2 Resolution;
in vec2 uv;
out vec4 fragColor;

const float PI = 3.14159265359;

vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

void main()
{
    vec2 center = vec2(0.5, 0.5);
    float radius = 0.5;
    vec2 pos = uv - center;
    
    // Check if we're inside the circular scope area
    if (length(pos) > radius) {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    
    // Initialize color accumulator and sample count
    vec3 acc = vec3(0.0);
    float count = 0.0;
    
    // Sample multiple points from the input texture
    for (int y = 0; y < int(Resolution.y); y += 4) {
        for (int x = 0; x < int(Resolution.x); x += 4) {
            vec2 sampleUV = vec2(float(x) / Resolution.x, float(y) / Resolution.y);
            vec3 rgb = texture(InputTexture, sampleUV).rgb;
            vec3 hsv = rgb2hsv(rgb);
            
            // Convert HSV to polar coordinates
            float angle = hsv.x * 2.0 * PI;
            float dist = hsv.y * radius;
            
            vec2 huePos = center + vec2(cos(angle), sin(angle)) * dist;
            
            // Accumulate color if this hue position matches our current fragment
            if (distance(huePos, uv) < 0.005) {
                acc += rgb * hsv.z; // Weighted by value (brightness)
                count += hsv.z;
            }
        }
    }
    
    // Average the accumulated color
    vec3 color = count > 0.0 ? acc / count : vec3(0.0);
    
    // Apply some glow effect
    color = 1.0 - exp(-color * 3.0);
    
    fragColor = vec4(color, 1.0);
}
)";

Vectorscope::Vectorscope() :
    CFFGLPlugin()
{
    SetMinInputs(1);
    SetMaxInputs(1);
    FFGLLog::LogToHost("Created Vectorscope effect");
}

Vectorscope::~Vectorscope()
{
}

FFResult Vectorscope::InitGL(const FFGLViewportStruct* vp)
{
    if (!shader.Compile(_vertexShaderCode, _fragmentShaderCode))
    {
        DeInitGL();
        return FF_FAIL;
    }
    if (!quad.Initialise())
    {
        DeInitGL();
        return FF_FAIL;
    }

    return CFFGLPlugin::InitGL(vp);
}

FFResult Vectorscope::ProcessOpenGL(ProcessOpenGLStruct* pGL)
{
    if (pGL->numInputTextures < 1 || pGL->inputTextures[0] == NULL)
        return FF_FAIL;

    ScopedShaderBinding shaderBinding(shader.GetGLID());
    ScopedSamplerActivation activateSampler(0);
    Scoped2DTextureBinding textureBinding(pGL->inputTextures[0]->Handle);

    shader.Set("InputTexture", 0);
    
    FFGLTexCoords maxCoords = GetMaxGLTexCoords(*pGL->inputTextures[0]);
    shader.Set("MaxUV", maxCoords.s, maxCoords.t);
    
    // Set the resolution of the input texture
    shader.Set("Resolution", float(pGL->inputTextures[0]->Width), float(pGL->inputTextures[0]->Height));

    quad.Draw();

    return FF_SUCCESS;
}

FFResult Vectorscope::DeInitGL()
{
    shader.FreeGLResources();
    quad.Release();
    return FF_SUCCESS;
}
