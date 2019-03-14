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
    displayWindow = SDL_CreateWindow("Pong by Richard Shu", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 360, SDL_WINDOW_OPENGL);
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

	// Set time to an initial value of 0
	float lastFrameTicks = 0.0f;

	// Game attributes
	bool isGameOver = false;
	bool playerWon;
	float distancePerSecond = 1.0f; // Set the movement speed of the paddles and ball

	// Set user and AI paddle attributes
	float userPaddleY = 0.0f;
	float aiPaddleY = 0.0f;
	bool aiPaddleGoingUp = true; // Default value
	const float paddleWidth = 0.1f;
	const float paddleHeight = 0.5f;
	const float paddleOffsetX = 1.5f;

	// Set ball attributes
	float ballX = 0.0f;
	float ballY = 0.0f;
	bool isBallGoingUp = true; // Default value
	bool isBallGoingRight = true; // Default value
	const float ballRadius = 0.1f;

    SDL_Event event;
    bool done = false;
	while (!done) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
		}

		// Calculate elapsed time
		float ticks = (float)SDL_GetTicks() / 1000.0f;
		float elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks; // Reset

		// Allow user to move the paddle up and down
		const Uint8 *keys = SDL_GetKeyboardState(NULL);
		if (keys[SDL_SCANCODE_UP] && (userPaddleY + (paddleHeight / 2) < 1.0f)) {
			userPaddleY += elapsed * distancePerSecond;
		}
		else if (keys[SDL_SCANCODE_DOWN] && (userPaddleY - (paddleHeight / 2) > -1.0f)) {
			userPaddleY -= elapsed * distancePerSecond;
		}

		if (isGameOver) {
			ballX = 0.0f;
			ballY = 0.0f;
			isGameOver = false;
			if (playerWon) {
				isBallGoingRight = true;
				glClearColor(0.5f, 1.0f, 0.83f, 1.0f); // Set background color to light green since player won
			}
			else {
				isBallGoingRight = false;
				glClearColor(1.0f, 0.71f, 0.88f, 0.76f); // Set background color to light red since AI won
			}
		}

		// Handle AI's paddle movement (goes up and down repeatedly)
		if (aiPaddleGoingUp) {
			if (aiPaddleY + (paddleHeight / 2) < 1.0f) {
				aiPaddleY += elapsed * distancePerSecond;
			}
			else {
				aiPaddleGoingUp = false;
			}
		}
		else {
			if (aiPaddleY - (paddleHeight / 2) > -1.0f) {
				aiPaddleY -= elapsed * distancePerSecond;
			}
			else {
				aiPaddleGoingUp = true;
			}
		}

		// Handle ball movement along the y-axis
		if (isBallGoingUp) {
			if (ballY + 0.05 < 1.0f) {
				ballY += elapsed * distancePerSecond;
			}
			else {
				isBallGoingUp = false;
			}
		}
		else {
			if (ballY - 0.05 > -1.0f) {
				ballY -= elapsed * distancePerSecond;
			}
			else {
				isBallGoingUp = true;
			}
		}

		// Handle ball movement along the x-axis
		if (isBallGoingRight) {
			ballX += elapsed * distancePerSecond;
			if (ballX - (ballRadius / 2) > 1.777f) {
				isGameOver = true;
				playerWon = false;
			}
			else {
				// Check for paddle collision
				if (ballY < userPaddleY + (paddleHeight / 2) && 
					ballY > userPaddleY - (paddleHeight / 2) && 
					ballX + (ballRadius / 2) > paddleOffsetX - paddleWidth && 
					ballX + (ballRadius / 2) < paddleOffsetX - paddleWidth + 0.01f) {	// The 0.01f is necessary to make sure the 
					isBallGoingRight = false;											// ball only rebounds if it hits the paddle.
				}
			}
		}
		else {
			if (ballX - (ballRadius / 2) > -1.777f) {
				ballX -= elapsed * distancePerSecond;

				// Check for paddle collision
				if (ballY < aiPaddleY + (paddleHeight / 2) && 
					ballY > aiPaddleY - (paddleHeight / 2) && 
					ballX - (ballRadius / 2) < paddleWidth - paddleOffsetX &&
					ballX - (ballRadius / 2) > paddleWidth - paddleOffsetX - 0.01f) {	// The 0.01f is necessary to make sure the 
					isBallGoingRight = true;											// ball only rebounds if it hits the paddle
				}
			}
			else {
				isGameOver = true;
				playerWon = true;
			}
		}

		glClear(GL_COLOR_BUFFER_BIT);

		// Use the shader program for untextured polygons
		glUseProgram(program.programID);

		// Create a paddle by combining 2 triangles together. Height = 0.5f. Width = 0.1f.
		float paddleVertices[] = { -0.05f, 0.25f, -0.05f, -0.25f, 0.05f, -0.25f, 0.05f, -0.25f, 0.05f, 0.25f, -0.05f, 0.25f };
		glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, paddleVertices);
		glEnableVertexAttribArray(program.positionAttribute);

		// Offset user paddle to the right side
		modelMatrix = glm::mat4(1.0f);
		modelMatrix = glm::translate(modelMatrix, glm::vec3(paddleOffsetX, userPaddleY, 0.0f));
		program.SetModelMatrix(modelMatrix);
		program.SetColor(0.2f, 0.8f, 0.4f, 1.0f); // Green
		glDrawArrays(GL_TRIANGLES, 0, 6); // Read in 6 pairs of vertices at a time (rather than 3) since we combined the 2 triangles into 1 object

		// Offset AI paddle to the left side
		modelMatrix = glm::mat4(1.0f);
		modelMatrix = glm::translate(modelMatrix, glm::vec3(-paddleOffsetX, aiPaddleY, 0.0f));
		program.SetModelMatrix(modelMatrix);
		program.SetColor(1.0f, 0.0f, 0.0f, 1.0f); // Red
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDisableVertexAttribArray(program.positionAttribute);

		// Create the ball
		float ballVertices[] = { -0.05f, 0.05f, -0.05f, -0.05f, 0.05f, -0.05f, 0.05f, -0.05f, 0.05f, 0.05f, -0.05f, 0.05f };
		glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, ballVertices);
		glEnableVertexAttribArray(program.positionAttribute);

		// Draw the ball
		modelMatrix = glm::mat4(1.0f);
		modelMatrix = glm::translate(modelMatrix, glm::vec3(ballX, ballY, 0.0f));
		program.SetModelMatrix(modelMatrix);
		program.SetColor(0.0f, 0.0f, 0.0f, 1.0f); // Black
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glDisableVertexAttribArray(program.positionAttribute);

		SDL_GL_SwapWindow(displayWindow);
    }
    
    SDL_Quit();
    return 0;
}
