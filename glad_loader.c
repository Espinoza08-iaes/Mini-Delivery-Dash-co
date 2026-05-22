#define GLAPI
#include <glad/glad.h>

static int load_function(GLADloadproc load, void **target, const char *name)
{
    *target = load(name);
    return *target != NULL;
}

int gladLoadGLLoader(GLADloadproc load)
{
    int ok = 1;

    if (load == NULL)
    {
        return 0;
    }

    ok &= load_function(load, (void **)&glad_glPixelStorei, "glPixelStorei");
    ok &= load_function(load, (void **)&glad_glCreateShader, "glCreateShader");
    ok &= load_function(load, (void **)&glad_glShaderSource, "glShaderSource");
    ok &= load_function(load, (void **)&glad_glCompileShader, "glCompileShader");
    ok &= load_function(load, (void **)&glad_glGetShaderiv, "glGetShaderiv");
    ok &= load_function(load, (void **)&glad_glGetShaderInfoLog, "glGetShaderInfoLog");
    ok &= load_function(load, (void **)&glad_glCreateProgram, "glCreateProgram");
    ok &= load_function(load, (void **)&glad_glAttachShader, "glAttachShader");
    ok &= load_function(load, (void **)&glad_glLinkProgram, "glLinkProgram");
    ok &= load_function(load, (void **)&glad_glGetProgramiv, "glGetProgramiv");
    ok &= load_function(load, (void **)&glad_glGetProgramInfoLog, "glGetProgramInfoLog");
    ok &= load_function(load, (void **)&glad_glDeleteShader, "glDeleteShader");
    ok &= load_function(load, (void **)&glad_glDeleteProgram, "glDeleteProgram");
    ok &= load_function(load, (void **)&glad_glUseProgram, "glUseProgram");
    ok &= load_function(load, (void **)&glad_glGenVertexArrays, "glGenVertexArrays");
    ok &= load_function(load, (void **)&glad_glGenBuffers, "glGenBuffers");
    ok &= load_function(load, (void **)&glad_glBindBuffer, "glBindBuffer");
    ok &= load_function(load, (void **)&glad_glBufferData, "glBufferData");
    ok &= load_function(load, (void **)&glad_glBindVertexArray, "glBindVertexArray");
    ok &= load_function(load, (void **)&glad_glVertexAttribPointer, "glVertexAttribPointer");
    ok &= load_function(load, (void **)&glad_glEnableVertexAttribArray, "glEnableVertexAttribArray");
    ok &= load_function(load, (void **)&glad_glViewport, "glViewport");
    ok &= load_function(load, (void **)&glad_glEnable, "glEnable");
    ok &= load_function(load, (void **)&glad_glClearColor, "glClearColor");
    ok &= load_function(load, (void **)&glad_glClear, "glClear");
    ok &= load_function(load, (void **)&glad_glGetUniformLocation, "glGetUniformLocation");
    ok &= load_function(load, (void **)&glad_glUniform3f, "glUniform3f");
    ok &= load_function(load, (void **)&glad_glUniform4f, "glUniform4f");
    ok &= load_function(load, (void **)&glad_glUniformMatrix4fv, "glUniformMatrix4fv");
    ok &= load_function(load, (void **)&glad_glDrawArrays, "glDrawArrays");
    ok &= load_function(load, (void **)&glad_glDrawElements, "glDrawElements");
    ok &= load_function(load, (void **)&glad_glDeleteVertexArrays, "glDeleteVertexArrays");
    ok &= load_function(load, (void **)&glad_glDeleteBuffers, "glDeleteBuffers");
    ok &= load_function(load, (void **)&glad_glGetString, "glGetString");

    ok &= load_function(load, (void **)&glad_glGenTextures, "glGenTextures");
    ok &= load_function(load, (void **)&glad_glDeleteTextures, "glDeleteTextures");
    ok &= load_function(load, (void **)&glad_glBindTexture, "glBindTexture");
    ok &= load_function(load, (void **)&glad_glTexImage2D, "glTexImage2D");
    ok &= load_function(load, (void **)&glad_glGenerateMipmap, "glGenerateMipmap");
    ok &= load_function(load, (void **)&glad_glTexParameteri, "glTexParameteri");
    ok &= load_function(load, (void **)&glad_glActiveTexture, "glActiveTexture");
    ok &= load_function(load, (void **)&glad_glUniform1i, "glUniform1i");
    ok &= load_function(load, (void **)&glad_glUniform1f, "glUniform1f");

    GLVersion.major = 3;
    GLVersion.minor = 3;
    GLAD_GL_VERSION_3_3 = 1;

    return ok;
}