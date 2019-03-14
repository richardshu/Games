#ifdef _WINDOWS
#include <GL/glew.h>
#endif
#include <SDL.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL_opengl.h>
#include <SDL_image.h>

#include "ShaderProgram.h"
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"

// Load images in any format with STB_image
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifdef _WINDOWS
#define RESOURCE_FOLDER ""
#else
#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

SDL_Window* displayWindow;

GLuint LoadTexture(const char *filePath) {
	int w, h, comp;
	unsigned char* image = stbi_load(filePath, &w, &h, &comp, STBI_rgb_alpha);
	if (image == NULL) {
		std::cout << "Unable to load image. Make sure the path is correct\n";
		assert(false);
	}
	GLuint retTexture;
	glGenTextures(1, &retTexture);
	glBindTexture(GL_TEXTURE_2D, retTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	stbi_image_free(image);
	return retTexture;
}

int main(int argc, char *argv[])
{
    SDL_Init(SDL_INIT_VIDEO);
    displayWindow = SDL_CreateWindow("My Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 360, SDL_WINDOW_OPENGL);
    SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
    SDL_GL_MakeCurrent(displayWindow, context);

#ifdef _WINDOWS
    glewInit();
#endif

	glViewport(0, 0, 640, 360);

	// For untextured polygons
	ShaderProgram program;
	program.Load(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");

	// For textured polygons
	ShaderProgram texturedProgram;
	texturedProgram.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");

	// Load textures!
	GLuint cherryTexture = LoadTexture(RESOURCE_FOLDER"cherry.png");
	GLuint mouseTexture = LoadTexture(RESOURCE_FOLDER"mouse.png");
	GLuint mushroomTexture = LoadTexture(RESOURCE_FOLDER"mushroom.png");

	// Initialize matrices
	glm::mat4 projectionMatrix = glm::mat4(1.0f);
	glm::mat4 modelMatrix = glm::mat4(1.0f);
	glm::mat4 viewMatrix = glm::mat4(1.0f);
	projectionMatrix = glm::ortho(-1.777f, 1.777f, -1.0f, 1.0f, -1.0f, 1.0f);
	
	glClearColor(1.0f, 1.0f, 0.88f, 1.0f); // Set background color to light yellow

	// "Blend" textures so their background doesn't show
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    SDL_Event event;
    bool done = false;
    while (!done) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
                done = true;
            }
        }
		glClear(GL_COLOR_BUFFER_BIT);

		// Untextured polygons
		glUseProgram(program.programID); // Use the shader program for untextured polygons

		program.SetModelMatrix(modelMatrix);
		program.SetProjectionMatrix(projectionMatrix);
		program.SetViewMatrix(viewMatrix);

		// Create a triangle polygon
		float triangleVertices[] = { 0.5f, -0.5f, 0.0f, 0.5f, -0.5f, -0.5f };
		glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, triangleVertices);
		glEnableVertexAttribArray(program.positionAttribute);

		// First triangle
		program.SetColor(0.2f, 0.8f, 0.4f, 1.0f); // Green

		modelMatrix = glm::mat4(1.0f);
		modelMatrix = glm::translate(modelMatrix, glm::vec3(-1.277f, -0.5f, 0.0f));
		program.SetModelMatrix(modelMatrix);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		
		// Second triangle
		program.SetColor(1.0f, 0.0f, 0.0f, 1.0f); // Red

		modelMatrix = glm::mat4(1.0f);
		modelMatrix = glm::translate(modelMatrix, glm::vec3(1.277f, -0.5f, 0.0f));
		program.SetModelMatrix(modelMatrix);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		glDisableVertexAttribArray(program.positionAttribute);

		// Textured polygons
		glUseProgram(texturedProgram.programID); // Use the shader program for textured polygons

		texturedProgram.SetModelMatrix(modelMatrix);
		texturedProgram.SetProjectionMatrix(projectionMatrix);
		texturedProgram.SetViewMatrix(viewMatrix);

		// Create a square polygon (made up of 2 triangles)
		float vertices[] = { -0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5, 0.5, -0.5, 0.5 };
		glVertexAttribPointer(texturedProgram.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
		glEnableVertexAttribArray(texturedProgram.positionAttribute);

		// Create a square texture to map onto the square polygon (also made up of 2 triangles)
		float texCoords[] = { 0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0 };
		glVertexAttribPointer(texturedProgram.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
		glEnableVertexAttribArray(texturedProgram.texCoordAttribute);

		// Render cherry texture
		glBindTexture(GL_TEXTURE_2D, cherryTexture);

		modelMatrix = glm::mat4(1.0f);
		modelMatrix = glm::translate(modelMatrix, glm::vec3(0.0f, -0.5f, 0.0f));
		texturedProgram.SetModelMatrix(modelMatrix);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		// Render mouse texture
		glBindTexture(GL_TEXTURE_2D, mouseTexture);

		modelMatrix = glm::mat4(1.0f);
		modelMatrix = glm::translate(modelMatrix, glm::vec3(-0.75f, 0.5f, 0.0f));
		texturedProgram.SetModelMatrix(modelMatrix);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		// Render mushroom texture
		glBindTexture(GL_TEXTURE_2D, mushroomTexture);

		modelMatrix = glm::mat4(1.0f);
		modelMatrix = glm::translate(modelMatrix, glm::vec3(0.75f, 0.5f, 0.0f));
		texturedProgram.SetModelMatrix(modelMatrix);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		// Disable vertex atributes
		glDisableVertexAttribArray(texturedProgram.positionAttribute);
		glDisableVertexAttribArray(texturedProgram.texCoordAttribute);

		SDL_GL_SwapWindow(displayWindow);
    }
    
    SDL_Quit();
    return 0;
}
