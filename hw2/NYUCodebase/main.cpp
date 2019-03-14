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

	// Initialize matrices
	glm::mat4 projectionMatrix = glm::mat4(1.0f);
	glm::mat4 modelMatrix = glm::mat4(1.0f);
	glm::mat4 viewMatrix = glm::mat4(1.0f);
	projectionMatrix = glm::ortho(-1.777f, 1.777f, -1.0f, 1.0f, -1.0f, 1.0f);

	program.SetModelMatrix(modelMatrix);
	program.SetProjectionMatrix(projectionMatrix);
	program.SetViewMatrix(viewMatrix);
	
	glClearColor(1.0f, 1.0f, 0.88f, 1.0f); // Set background color to light yellow

	// "Blend" textures so their background doesn't show
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Time
	float lastFrameTicks = 0.0f; // Initial value is 0

	// User paddle y-coordinate
	float userPaddleY = 0.0f;

    SDL_Event event;
    bool done = false;
    while (!done) {
		while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
                done = true;
            }
		}

		// Handle time elapsed
		float ticks = (float)SDL_GetTicks() / 1000.0f;
		float elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks; // Reset

		// Handle user's paddle movement
		const Uint8 *keys = SDL_GetKeyboardState(NULL);
		if (keys[SDL_SCANCODE_UP] && (userPaddleY + 0.25 < 1.0)) {
			userPaddleY += elapsed * 0.5f;
		}
		else if (keys[SDL_SCANCODE_DOWN] && (userPaddleY - 0.25 > -1.0)) {
			userPaddleY -= elapsed * 0.5f;
		}

		glClear(GL_COLOR_BUFFER_BIT);

		// Untextured polygons
		glUseProgram(program.programID); // Use the shader program for untextured polygons

		// Create a paddle by combining 2 triangles together. Height = 0.5f. Width = 0.1f.
		float paddleVertices[] = { -0.05f, 0.25f, -0.05f, -0.25f, 0.05f, -0.25f, 0.05f, -0.25f, 0.05f, 0.25f, -0.05f, 0.25f };
		glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, paddleVertices);
		glEnableVertexAttribArray(program.positionAttribute);

		// Paddle color
		program.SetColor(0.2f, 0.8f, 0.4f, 1.0f); // Green

		// Offset user paddle to the right side
		modelMatrix = glm::mat4(1.0f);
		modelMatrix = glm::translate(modelMatrix, glm::vec3(1.5f, userPaddleY, 0.0f));
		program.SetModelMatrix(modelMatrix);
		glDrawArrays(GL_TRIANGLES, 0, 6); // Read in 6 pairs of vertices at a time since we combined the 2 triangles into 1 object
		
		glDisableVertexAttribArray(program.positionAttribute);

		SDL_GL_SwapWindow(displayWindow);
    }
    
    SDL_Quit();
    return 0;
}
