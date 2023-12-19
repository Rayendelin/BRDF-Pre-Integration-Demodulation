#include "processor.h"
#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"


// 获取函数报错并输出错误信息
bool checkGlError(const char* funcName) {
    GLint err = glGetError();
    if (err != GL_NO_ERROR) {
        printf("GL error after %s(): 0x%08x\n", funcName, err);
        return true;
    }
    return false;
}

// 创建并编译着色器
GLuint createShader(GLenum shaderType, std::string shaderCode) {
    GLuint shader = glCreateShader(shaderType);
    if (!shader) {
        checkGlError("glCreateShader");
        return 0;
    }
    const char* src = shaderCode.c_str();
    glShaderSource(shader, 1, &src, nullptr);

    GLint compiled = GL_FALSE;
    glCompileShader(shader);
    checkGlError("glCompileShader");
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint infoLogLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLen);
        if (infoLogLen > 0) {
            GLchar* infoLog = (GLchar*)malloc(infoLogLen);
            if (infoLog) {
                glGetShaderInfoLog(shader, infoLogLen, nullptr, infoLog);
                printf("Could not compile compute shader: %s", infoLog);
                free(infoLog);
            }
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

// 创建program，编译和链接着色器
GLuint createProgram(std::string computeShaderCode) {
    GLuint computeShader = 0;
    GLuint program = 0;
    GLint linked = GL_FALSE;

    computeShader = createShader(GL_COMPUTE_SHADER, computeShaderCode);
    if (!computeShader)
        goto exit;

    program = glCreateProgram();
    if (!program) {
        checkGlError("glCreateProgram");
        goto exit;
    }
    glAttachShader(program, computeShader);
    glLinkProgram(program);
    checkGlError("glLinkProgram");
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint infoLogLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLen);
        if (infoLogLen) {
            GLchar* infoLog = (GLchar*)malloc(infoLogLen);
            if (infoLog) {
                glGetProgramInfoLog(program, infoLogLen, nullptr, infoLog);
                printf("Could not link program: %s", infoLog);
                free(infoLog);
            }
        }
        glDeleteProgram(program);
        program = 0;
    }

exit:
    glDeleteShader(computeShader);
    return program;
}

void Processor::createTextures(int texNum)
{
    mTextures = new GLuint[texNum];
    glGenTextures(texNum, mTextures);
    for (int i = 0; i < texNum; i++) {
        glBindTexture(GL_TEXTURE_2D, mTextures[i]);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);    // x坐标超出范围采样纹理边缘
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);    // y坐标超出范围采样纹理边缘
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        if (i == texNum - 1) {     // 最后一张纹理是用来存放结果的，只需要开辟空间
            glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, width, height);
            continue;
        }
        else {
            glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, picDatas[i].width, picDatas[i].height);
            checkGlError("glTexStorage2D");
        }
        
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, picDatas[i].width, picDatas[i].height, GL_RGBA, GL_FLOAT, picDatas[i].color);
        checkGlError("glTexSubImage2D");
    }
}

int Processor::prepareData(std::vector<const char*> picPaths)
{
    for (auto picPath : picPaths) {
        PicData picData;
        int width;
        int height;
        const char* err;
        int ret = LoadEXR(&(picData.color), &(picData.width), &(picData.height), picPath, &err);
        if (ret != TINYEXR_SUCCESS) {
            printf("Read %s Error: %s", picPath, err);
            FreeEXRErrorMessage(err);
            return -1;
        }
        picDatas.push_back(picData);
        this->height = picData.height;
        this->width = picData.width;
    }
    return picDatas.size();
}

int Processor::configEnv()
{
    // 初始化GLFW
    glfwInit();
    // 配置GLFW参数
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);  // 主版本号
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);  // 次版本号
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 核心模式
    glfwWindowHint(GLFW_VISIBLE, 0);    // 禁止窗口显示

    // 创建窗口对象，参数：宽、高、名称
    GLFWwindow* window = glfwCreateWindow(800, 600, "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
        printf("Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }
    // 设置为当前线程的主上下文
    glfwMakeContextCurrent(window);

    // 初始化GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        printf("Failed to initialize GLAD\n");
        return -1;
    }

    glViewport(0, 0, 800, 600);
	
    return 0;
}

void Processor::bindTexture(const GLchar* varName, int layer) {
    glActiveTexture(GL_TEXTURE0 + layer);
    glBindTexture(GL_TEXTURE_2D, mTextures[layer]);
    glUniform1i(glGetUniformLocation(mProgram, varName), layer);
}

int Processor::configOpenGL(process processType)
{
    if (processType == DEMODULATE) {
        mProgram = createProgram(demodulateShaderCode);
        if (!mProgram)
            return -1;
        glUseProgram(mProgram);
        checkGlError("glUseProgram");

        createTextures(3);

        glActiveTexture(GL_TEXTURE2);
        glBindImageTexture(0, mTextures[2], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        checkGlError("glBindImageTexture");
        bindTexture("frame", 0);
        bindTexture("albedo", 1);
    }
    else if (processType == BRDF_DEMODULATE) {
        mProgram = createProgram(brdfDemodulateShaderCode);
        if (!mProgram)
            return -1;
        glUseProgram(mProgram);
        checkGlError("glUseProgram");

        createTextures(6);

        glActiveTexture(GL_TEXTURE5);
        glBindImageTexture(0, mTextures[5], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        checkGlError("glBindImageTexture");
        bindTexture("albedo", 0);
        bindTexture("MMR", 1);
        bindTexture("NoV", 2);
        bindTexture("precomputed", 3);
        bindTexture("specular", 4);
    }

	return 0;
}

int Processor::execute(process processType)
{
    int resIndex = 0;

    switch (processType) {
    case DEMODULATE:
        resIndex = 2;
        break;
    case BRDF_DEMODULATE:
        resIndex = 5;
        break;
    }

    int numX = floor(width / 8);
    int numY = floor(height / 8);
    glDispatchCompute(numX, numY, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    result = new float[4 * width * height];
    GLuint frameBuffer;
    glGenFramebuffers(1, &frameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[resIndex], 0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, result);
    checkGlError("glReadPixels");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return 0;
}

int Processor::saveRes(const char* savePath)
{
    if (result == nullptr) {
        printf("There is no data to save\n");
        return -1;
    }
    const char* err;
    int ret = SaveEXR(result, width, height, 4, 0, savePath, &err);
    if (ret != TINYEXR_SUCCESS) {
        printf("Save result to %s Error: %s", savePath, err);
        FreeEXRErrorMessage(err);
        return -1;
    }
    return 0;
}

void Processor::clear()
{
    picDatas.clear();
    if (result != nullptr) {
        free(result);
        result = nullptr;
    }
    width = 0;
    height = 0;
}
