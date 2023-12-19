#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>
#include <vector>

enum process {DEMODULATE, BRDF_DEMODULATE};

struct PicData {
    float* color;
    int height;
    int width;
};

class Processor
{
private:
    std::string demodulateShaderCode, brdfDemodulateShaderCode;
    std::vector<PicData> picDatas;
    int height;
    int width;
    GLuint mProgram;
    GLuint* mTextures;
    float* result;

    void createTextures(int texNum);
    void bindTexture(const GLchar* varName, int layer);

public:
    int prepareData(std::vector<const char*> picPath);    // 读取需要的图像数据，参数为图像所在的路径
    int configEnv();    // 创建OpenGL运行环境
    int configOpenGL(process processType);  // 配置OpenGL
    int execute(process processType);  // 执行OpenGL
    int saveRes(const char* savePath);
    void clear();

    Processor() {
        demodulateShaderCode =
            "#version 450\n"
            "layout(local_size_x=8, local_size_y=8) in;\n"
            "layout(binding=0) uniform sampler2D frame;\n"
            "layout(binding=1) uniform sampler2D albedo;\n"
            "layout(rgba32f, binding=0) writeonly uniform image2D res;\n"
            "void main() {\n"
            "    ivec2 pixCoord = ivec2(gl_GlobalInvocationID.xy);\n"
            "    vec3 color = texelFetch(frame, pixCoord, 0).xyz;\n"
            "    vec3 albedo = texelFetch(albedo, pixCoord, 0).xyz;\n"
            "    vec3 resColor = color;\n"  
            "    if (albedo.x != 0.0)\n"
            "        resColor.x = color.x / albedo.x;\n"
            "    if (albedo.y != 0.0)\n"
            "        resColor.y = color.y / albedo.y;\n"
            "    if (albedo.z != 0.0)\n"
            "        resColor.z = color.z / albedo.z;\n"
            "    imageStore(res, pixCoord, vec4(resColor, 1.0));\n"
            "}\n";

        brdfDemodulateShaderCode =
            "#version 450\n"
            "layout(local_size_x=8, local_size_y=8) in;\n"
            "layout(binding=0) uniform sampler2D albedo;\n"
            "layout(binding=1) uniform sampler2D MMR;\n"    // motion vector and metallic and roughness
            "layout(binding=2) uniform sampler2D NoV;\n"
            "layout(binding=3) uniform sampler2D precomputed;\n"
            "layout(binding=4) uniform sampler2D specular;\n"
            "layout(rgba32f, binding=0) writeonly uniform image2D res;\n"
            "void main() {\n"
            "    ivec2 pixCoord = ivec2(gl_GlobalInvocationID.xy);\n"
            "    vec2 uv = vec2(texelFetch(NoV, pixCoord, 0).x, texelFetch(MMR, pixCoord, 0).a);\n"
            "    vec2 computed = texture(precomputed, uv).xy;\n"
            "    vec3 albe = texelFetch(albedo, pixCoord, 0).xyz;\n"
            "    vec3 spec = texelFetch(specular, pixCoord, 0).xxx;\n"
            "    vec3 meta = texelFetch(MMR, pixCoord, 0).zzz;\n"
            "    vec3 specularColor = 0.08 * spec + (albe - 0.08 * spec) * meta;\n"
            "    vec3 result = albe * (vec3(1.0) - meta) + vec3(computed.x) * specularColor + computed.yyy;\n"
            "    imageStore(res, pixCoord, vec4(result, 1.0));\n"
            "}\n";
    }
};