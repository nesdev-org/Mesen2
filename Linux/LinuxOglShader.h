#pragma once

#include "Linux/include/glad/egl.h"
#include "Linux/include/glad/gl.h"

class LinuxOglShader
{
private:
	GLuint _programID = 0;

public:
	~LinuxOglShader()
	{
		Cleanup();
	}

	bool Compile(const char* vertexSource, const char* fragmentSource)
	{
		GLuint vert = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vert, 1, &vertexSource, nullptr);
		glCompileShader(vert);

		GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(frag, 1, &fragmentSource, nullptr);
		glCompileShader(frag);

		_programID = glCreateProgram();
		glAttachShader(_programID, vert);
		glAttachShader(_programID, frag);
		glLinkProgram(_programID);

		glDeleteShader(vert);
		glDeleteShader(frag);

		return _programID != 0;
	}

	void Bind()
	{
		if(_programID) {
			glUseProgram(_programID);
		}
	}

	void Cleanup()
	{
		if(_programID) {
			glDeleteProgram(_programID);
			_programID = 0;
		}
	}
};