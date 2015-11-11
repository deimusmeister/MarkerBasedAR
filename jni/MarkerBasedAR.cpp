#include <jni.h>

#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/native_activity.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <GLES/gl.h>
#include <GLES/glext.h>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/opencv.hpp>

#include "MarkerDetector.hpp"
#include "CameraCalibration.hpp"

#define LOG_TAG "com.levon.markerbasedar"
#define LOGI(...) __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__)

// android window, supported by NDK r5 and newer
ANativeWindow* _window;

EGLDisplay _display;
EGLSurface _surface;
EGLContext _context;
GLfloat _angle;

CameraCalibration	camCalibration(6.24860291e+02 * (1440./352.), 6.24860291e+02 * (1080./288.), 1440 * 0.5f, 1080 * 0.5f);
MarkerDetector*		markerDetector = new MarkerDetector(camCalibration);

GLuint 						m_backgroundTextureId;
std::vector<Transformation> m_transformations;
int 						m_height;
int							m_width;
GLuint						viewFrameBuffer;
GLuint						viewRenderBuffer;

void destroy() {
    LOGI("Destroying context");

    eglMakeCurrent(_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(_display, _context);
    eglDestroySurface(_display, _surface);
    eglTerminate(_display);

    _display = EGL_NO_DISPLAY;
    _surface = EGL_NO_SURFACE;
    _context = EGL_NO_CONTEXT;
}

void updateBackground(cv::Mat frame)
{
    //[m_glview setFramebuffer];
	cv::Size s = frame.size();

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, m_backgroundTextureId);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, s.width, s.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, frame.data);

    int glErCode = glGetError();
    if (glErCode != GL_NO_ERROR)
    {
        std::cout << glErCode << std::endl;
    }
}

void buildProjectionMatrix(Matrix33 cameraMatrix, int screen_width, int screen_height, Matrix44& projectionMatrix)
{
    float near = 0.01;  // Near clipping distance
    float far  = 100;  // Far clipping distance

    // Camera parameters
    float f_x = cameraMatrix.data[0]; // Focal length in x axis
    float f_y = cameraMatrix.data[4]; // Focal length in y axis (usually the same?)
    float c_x = cameraMatrix.data[2]; // Camera primary point x
    float c_y = cameraMatrix.data[5]; // Camera primary point y

    projectionMatrix.data[0] = - 2.0 * f_x / screen_width;
    projectionMatrix.data[1] = 0.0;
    projectionMatrix.data[2] = 0.0;
    projectionMatrix.data[3] = 0.0;

    projectionMatrix.data[4] = 0.0;
    projectionMatrix.data[5] = 2.0 * f_y / screen_height;
    projectionMatrix.data[6] = 0.0;
    projectionMatrix.data[7] = 0.0;

    projectionMatrix.data[8] = 2.0 * c_x / screen_width - 1.0;
    projectionMatrix.data[9] = 2.0 * c_y / screen_height - 1.0;
    projectionMatrix.data[10] = -( far+near ) / ( far - near );
    projectionMatrix.data[11] = -1.0;

    projectionMatrix.data[12] = 0.0;
    projectionMatrix.data[13] = 0.0;
    projectionMatrix.data[14] = -2.0 * far * near / ( far - near );
    projectionMatrix.data[15] = 0.0;
}

void drawAR()
{
    Matrix44 projectionMatrix;
    buildProjectionMatrix(camCalibration.getIntrinsic(), m_width, m_height, projectionMatrix);

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(projectionMatrix.data);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    //glDepthFunc(GL_LESS);
    //glDepthFunc(GL_GREATER);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_NORMAL_ARRAY);

    glPushMatrix();
    glLineWidth(3.0f);

    float lineX[] = {0,0,0,1,0,0};
    float lineY[] = {0,0,0,0,1,0};
    float lineZ[] = {0,0,0,0,0,1};

    const GLfloat squareVertices[] = {
        -0.5f, -0.5f,
        0.5f,  -0.5f,
        -0.5f,  0.5f,
        0.5f,   0.5f,
    };

    const GLubyte squareColors[] = {
        255, 255,   0, 255,
        0,   255, 255, 255,
        0,     0,   0,   0,
        255,   0, 255, 255,
    };

    for (size_t transformationIndex=0; transformationIndex<m_transformations.size(); transformationIndex++)
    {
        const Transformation& transformation = m_transformations[transformationIndex];

        Matrix44 glMatrix = transformation.getMat44();
        glLoadMatrixf(reinterpret_cast<const GLfloat*>(&glMatrix.data[0]));

        // draw data
        glVertexPointer(2, GL_FLOAT, 0, squareVertices);
        glEnableClientState(GL_VERTEX_ARRAY);
        glColorPointer(4, GL_UNSIGNED_BYTE, 0, squareColors);
        glEnableClientState(GL_COLOR_ARRAY);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisableClientState(GL_COLOR_ARRAY);

        float scale = 0.5;
        glScalef(scale, scale, scale);

        glTranslatef(0, 0, 0.1f);

        glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
        glVertexPointer(3, GL_FLOAT, 0, lineX);
        glDrawArrays(GL_LINES, 0, 2);

        glColor4f(0.0f, 1.0f, 0.0f, 1.0f);
        glVertexPointer(3, GL_FLOAT, 0, lineY);
        glDrawArrays(GL_LINES, 0, 2);

        glColor4f(0.0f, 0.0f, 1.0f, 1.0f);
        glVertexPointer(3, GL_FLOAT, 0, lineZ);
        glDrawArrays(GL_LINES, 0, 2);
    }

    glPopMatrix();
    glDisableClientState(GL_VERTEX_ARRAY);
}

void drawBackground()
{
	int w = ANativeWindow_getWidth(_window);
	int h = ANativeWindow_getHeight(_window);

//    GLfloat w = m_glview.bounds.size.width;
//    GLfloat h = m_glview.bounds.size.height;

    const GLfloat squareVertices[] =
    {
        0, 0,
        w, 0,
        0, h,
        w, h
    };

     static const GLfloat textureVertices[] =
     {
     1, 0,
     1, 1,
     0, 0,
     0, 1
     };

    static const GLfloat proj[] =
    {
        0, -2.f/w, 0, 0,
        -2.f/h, 0, 0, 0,
        0, 0, 1, 0,
        1, 1, 0, 1
    };

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(proj);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDepthMask(GL_FALSE);
    glDisable(GL_COLOR_MATERIAL);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_backgroundTextureId);

    // Update attribute values.
    glVertexPointer(2, GL_FLOAT, 0, squareVertices);
    glEnableClientState(GL_VERTEX_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, 0, textureVertices);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glColor4f(1,1,1,1);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisable(GL_TEXTURE_2D);
}

void drawDummy()
{
	//	glClearColor(1, 0, 0, 1);
	//	glClear(GL_COLOR_BUFFER_BIT);
	//	glFlush();
}

extern "C" {

JNIEXPORT void JNICALL Java_com_levon_markerbasedar_MainActivity_Initialize(JNIEnv* env, jobject thiz, jobject surface1)
{
	if (surface1 != 0) {
		_window = ANativeWindow_fromSurface(env, surface1);
		LOGI("Got window %p", _window);
	} else {
		LOGI("Releasing window");
		ANativeWindow_release(_window);
	}

	const EGLint attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_BLUE_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_RED_SIZE, 8,
		EGL_NONE
	};
	EGLDisplay display;
	EGLConfig config;
	EGLint numConfigs;
	EGLint format;
	EGLSurface surface;
	EGLContext context;
	EGLint width;
	EGLint height;
	GLfloat ratio;

	LOGI("Initializing context");

	if ((display = eglGetDisplay(EGL_DEFAULT_DISPLAY)) == EGL_NO_DISPLAY) {
		LOGI("eglGetDisplay() returned error %d", eglGetError());
		return;
	}
	if (!eglInitialize(display, 0, 0)) {
		LOGI("eglInitialize() returned error %d", eglGetError());
		return;
	}

	if (!eglChooseConfig(display, attribs, &config, 1, &numConfigs)) {
		LOGI("eglChooseConfig() returned error %d", eglGetError());
		destroy();
		return;
	}

	if (!eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format)) {
		LOGI("eglGetConfigAttrib() returned error %d", eglGetError());
		destroy();
		return;
	}

	ANativeWindow_setBuffersGeometry(_window, 0, 0, format);

	if (!(surface = eglCreateWindowSurface(display, config, _window, 0))) {
		LOGI("eglCreateWindowSurface() returned error %d", eglGetError());
		destroy();
		return;
	}

	if (!(context = eglCreateContext(display, config, 0, 0))) {
		LOGI("eglCreateContext() returned error %d", eglGetError());
		destroy();
		return;
	}

	if (!eglMakeCurrent(display, surface, surface, context)) {
		LOGI("eglMakeCurrent() returned error %d", eglGetError());
		destroy();
		return;
	}

	if (!eglQuerySurface(display, surface, EGL_WIDTH, &width) ||
		!eglQuerySurface(display, surface, EGL_HEIGHT, &height)) {
		LOGI("eglQuerySurface() returned error %d", eglGetError());
		destroy();
		return;
	}

	_display = display;
	_surface = surface;
	_context = context;
    glBindFramebuffer(GL_FRAMEBUFFER_OES, viewFrameBuffer);
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    //
    // custom drawing will be done right here ...
    //
    glBindRenderbuffer(GL_RENDERBUFFER_OES, viewRenderBuffer);

	glGenTextures(1, &m_backgroundTextureId);
	glBindTexture(GL_TEXTURE_2D, m_backgroundTextureId);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// This is necessary for non-power-of-two textures
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glEnable(GL_DEPTH_TEST);
}

JNIEXPORT void JNICALL Java_com_levon_markerbasedar_MainActivity_ProcessFrame(JNIEnv* env, jobject thiz, jint width, jint height, jbyteArray yuv, jlong transMat)
{
    jbyte* _yuv  = env->GetByteArrayElements(yuv, 0);

    cv::Mat myuv(height + height/2, width, CV_8UC1, (unsigned char *)_yuv);
    cv::Mat mbgra(height, width, CV_8UC4);

    m_height = height;
    m_width  = width;

    cvtColor(myuv, mbgra, CV_YUV420sp2RGB, 4);

    updateBackground(mbgra);

    markerDetector->processFrame(mbgra);
	m_transformations = markerDetector->getTransformations();

//	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
//	glClear(GL_COLOR_BUFFER_BIT);

//    drawDummy();
	drawBackground();
	drawAR();

    eglSwapBuffers(_display, _surface);

    env->ReleaseByteArrayElements(yuv, _yuv, 0);
}

}
