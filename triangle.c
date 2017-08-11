/***********************************************************
 * File: triangle.c
 *
 * Description:
 *   Minimal single-file demo to render a triangle on Raspberry Pi 3 Mobel B with OpenGL ES 2.0
 *
 * Original versions of this code are at:
 *   https://github.com/peepo/openGL-RPi-tutorial/tree/master/tutorial02_red_triangle
 *   https://github.com/raspberrypi/firmware/blob/master/opt/vc/src/hello_pi/hello_triangle2/triangle2.c
 *
 ***********************************************************/

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#include "bcm_host.h"
#include "GLES2/gl2.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"

typedef struct
{
	// Screen dimensions in pixels
	uint32_t screen_width;
	uint32_t screen_height;

	// OpenGL|ES objects
	EGLDisplay display;
	EGLSurface surface;
	EGLContext context;

	// Internal resource references
	GLuint vshader; // Vertex shader
	GLuint fshader; // Fragment shader
	GLuint program; // Shader program
	GLuint attr_vertex; // List of vertices for points of the triangle
	GLuint vbo_triangle; // Vertex buffer in GPU memory

	//
	GLuint verbose;
} OPENGL_STATE_T;
static OPENGL_STATE_T _state, *state=&_state;

#define check() assert(glGetError() == 0)

static void showlog(GLint shader)
{
	// Prints the compile log for a shader
	char log[1024];
	glGetShaderInfoLog(shader,sizeof log,NULL,log);
	printf("%d:shader:\n%s\n", shader, log);
}

static void showprogramlog(GLint shader)
{
	// Prints the information log for a program object
	char log[1024];
	glGetProgramInfoLog(shader,sizeof log,NULL,log);
	printf("%d:program:\n%s\n", shader, log);
}

/***********************************************************
 * Name: init_ogl
 *
 * Arguments:
 *   OPENGL_STATE_T *state = holds OGLES model/state info
 *
 * Description:
 *   Sets the display, OpenGL|ES context and screen stuff
 *
 * Returns:
 *   void
 *
 ***********************************************************/
static void init_ogl(OPENGL_STATE_T *state)
{
	bcm_host_init();
	int32_t success = 0;
	EGLBoolean result;
	EGLint num_config;

	static EGL_DISPMANX_WINDOW_T nativewindow;

	DISPMANX_ELEMENT_HANDLE_T dispman_element;
	DISPMANX_DISPLAY_HANDLE_T dispman_display;
	DISPMANX_UPDATE_HANDLE_T dispman_update;
	VC_RECT_T dst_rect;
	VC_RECT_T src_rect;

	static const EGLint attribute_list[] =
	{
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_NONE
	};

	static const EGLint context_attributes[] =
	{
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
	EGLConfig config;

	// Get an EGL display connection
	state->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	assert(state->display!=EGL_NO_DISPLAY);
	check();

	// Initialize the EGL display connection
	result = eglInitialize(state->display, NULL, NULL);
	assert(EGL_FALSE != result);
	check();

	// Get an appropriate EGL frame buffer configuration
	result = eglChooseConfig(state->display, attribute_list, &config, 1, &num_config);
	assert(EGL_FALSE != result);
	check();

	// Get an appropriate EGL frame buffer configuration
	result = eglBindAPI(EGL_OPENGL_ES_API);
	assert(EGL_FALSE != result);
	check();

	// Create an EGL rendering context
	state->context = eglCreateContext(state->display, config, EGL_NO_CONTEXT, context_attributes);
	assert(state->context!=EGL_NO_CONTEXT);
	check();

	// Create an EGL window surface
	success = graphics_get_display_size(0 /* LCD */, &state->screen_width, &state->screen_height);
	assert( success >= 0 );

	dst_rect.x = 0;
	dst_rect.y = 0;
	dst_rect.width = state->screen_width;
	dst_rect.height = state->screen_height;

	src_rect.x = 0;
	src_rect.y = 0;
	src_rect.width = state->screen_width << 16;
	src_rect.height = state->screen_height << 16;

	dispman_display = vc_dispmanx_display_open( 0 /* LCD */);
	dispman_update = vc_dispmanx_update_start( 0 );

	dispman_element = vc_dispmanx_element_add ( dispman_update, dispman_display,
		0/*layer*/, &dst_rect, 0/*src*/,
		&src_rect, DISPMANX_PROTECTION_NONE, 0 /*alpha*/, 0/*clamp*/, 0/*transform*/);

	nativewindow.element = dispman_element;
	nativewindow.width = state->screen_width;
	nativewindow.height = state->screen_height;
	vc_dispmanx_update_submit_sync( dispman_update );
	check();

	state->surface = eglCreateWindowSurface( state->display, config, &nativewindow, NULL );
	assert(state->surface != EGL_NO_SURFACE);
	check();

	// Connect the context to the surface
	result = eglMakeCurrent(state->display, state->surface, state->surface, state->context);
	assert(EGL_FALSE != result);
	check();

	// Set background color and clear buffers
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	check();
}

/***********************************************************
 * Name: init_scene
 *
 * Arguments:
 *   void
 *
 * Description:
 *   Creates simple shaders and loads the triangle vertex array into GPU memory.
 *   These are one-time setup operations for the entire scene.
 *
 * Returns:
 *   void
 *
 ***********************************************************/
void init_scene()
{
   const GLchar *vShaderSource =
      "attribute vec4 vertex;                     \n"
      "void main()                                \n"
      "{                                          \n"
      "    gl_Position = vertex;                  \n"
      "}                                          \n";

   const GLchar *fShaderSource =
      "void main()                                         \n"
      "{                                                   \n"
      "    gl_FragColor = vec4(0.0, 0.0, 1.0, 0.5);        \n"
      "}                                                   \n";

	// Vertex shader
	state->vshader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(state->vshader, 1, &vShaderSource, 0);
	glCompileShader(state->vshader);
	check();
	if (state->verbose) showlog(state->vshader);

	// Fragment shader
	state->fshader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(state->fshader, 1, &fShaderSource, 0);
	glCompileShader(state->fshader);
	check();
	if (state->verbose) showlog(state->fshader);

	// Linked shader program
	state->program = glCreateProgram();
	glAttachShader(state->program, state->vshader);
	glAttachShader(state->program, state->fshader);
	glLinkProgram(state->program);
	check();
	if (state->verbose) showprogramlog(state->program);

	// Shader resources are no longer needed after compiling and linking
	glDeleteShader(state->vshader);
	glDeleteShader(state->fshader);

	// Get the "vertex" attribute location
	state->attr_vertex = glGetAttribLocation(state->program, "vertex");

	// A counter-clockwise triangle
	GLfloat triangle_vertex_data[] = {
		-1.0f, -1.0f, 0.0f, // Lower left
		 1.0f, -1.0f, 0.0f, // Lower right
		 0.0f,  1.0f, 0.0f  // Top center
	};

	// Upload triangle vertex data to a buffer
	glGenBuffers(1, &state->vbo_triangle);
	check();
	glBindBuffer(GL_ARRAY_BUFFER, state->vbo_triangle);
	glBufferData(GL_ARRAY_BUFFER, sizeof(triangle_vertex_data), triangle_vertex_data, GL_STATIC_DRAW);
}

/***********************************************************
 * Name: render
 *
 * Arguments:
 *   long delta = microseconds since last call to render(delta)
 *
 * Description:
 *   Standard OpenGL rendering function with time variance for smooth animations (not used here).
 *   This would typically be called repeatedly inside a continuous rendering loop.
 *
 * Returns:
 *   void
 *
 ***********************************************************/
void render(long delta)
{
	printf("%ld microseconds\n", delta);

	glUseProgram(state->program);
	glVertexAttribPointer(state->attr_vertex, 3, GL_FLOAT, 0, 3 * sizeof(GLfloat), 0);
	glEnableVertexAttribArray(state->attr_vertex);
	glDrawArrays(GL_TRIANGLES, 0, 3);
}

/***********************************************************
 * Name: main
 *
 * Arguments:
 *   void
 *
 * Description:
 *   Program entry point: setup the OpenGL screen, the scene, and then run the rendering loop
 *
 * Returns:
 *   int
 *
 ***********************************************************/
int main ()
{
	// Timings for smooth render() animation, if needed
	struct timeval tv, tv_last;

	// Clear application state
	memset( state, 0, sizeof( *state ) );

	// Start OGLES
	init_ogl(state);

	// Create simple fragment and vertex shaders, and load geometry buffers
	init_scene();

	// Set the viewport to fill the screen
	glViewport(0, 0, state->screen_width, state->screen_height);

	// Render loop
	while(1)
	{
		// Clear
		glClear(GL_COLOR_BUFFER_BIT);

		// Draw
		gettimeofday(&tv, NULL);
		render((tv.tv_sec - tv_last.tv_sec) * 1e6 + tv.tv_usec - tv_last.tv_usec); // Elapsed microseconds
		tv_last = tv;

		// Update the display by swapping front/back buffers
		eglSwapBuffers(state->display, state->surface);
		check();
	}

	// Cleanup
	glDeleteProgram(state->program);
	glDeleteBuffers(1, &state->vbo_triangle);

	// Exit
	return 0;
}
