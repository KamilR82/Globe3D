// compile: g++ -o output.exe main.cpp -mwindows -lopengl32 -lglu32 -Os -s

#define UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <math.h> // sin, cos, ...

#define GL_PI 3.1415926535f
#include <gl\gl.h>	//lib opengl32 - OpenGL
#include <gl\glu.h> //lib glu32 - OpenGL Utility Library

#include "ArcBall.h"
ArcBallT ArcBall(0, 0);
Point2fT MousePt;
BOOL isClicked = FALSE;

HDC hDC = NULL;	  // main window device context
HGLRC hRC = NULL; // main render device context

#define TIMER_INDEX_FRAME 1 // next frame timer
#define TIMER_TIME_FRAME 32 // 1000 / ms = fps

const wchar_t lpClassName[] = L"OpenGL_Globe3D";
const wchar_t lpWindowTitle[] = L"Globe 3D";

BOOL fullscreen = TRUE; // fullscreen or normal window

#define DISPLAY_LISTS 2
GLuint list[DISPLAY_LISTS]; // contiguous set of display lists

Matrix4fT Transform; // final matrix
Matrix3fT ThisRot;	 // actual rotation matrix
Matrix3fT LastRot;	 // last rotation matrix

float delta = 0.25f;

static void glSceneGen()
{
	// raw data
	struct Vertex
	{
		float x;
		float y;
		float z;
	} rawVertex[11][20];

	float pi10 = GL_PI / 10; // (GL_PI / 180) * 18
	for (int i = 0; i < 11; i++)
	{
		for (int j = 0; j < 20; j++)
		{
			rawVertex[i][j].x = sin((float)i * pi10) * cos((float)j * pi10);
			rawVertex[i][j].y = sin((float)i * pi10) * sin((float)j * pi10);
			rawVertex[i][j].z = cos((float)i * pi10);
		}
	}

	// generate lists
	list[0] = glGenLists(DISPLAY_LISTS); // generate a contiguous set of empty display lists

	glNewList(list[0], GL_COMPILE); // globe
	glLineWidth(2.0f);
	// axis
	glColor3f(0.0f, 0.0f, 1.0f);
	glPushAttrib(GL_ENABLE_BIT);
	glLineStipple(10, 0xAAAA);
	glEnable(GL_LINE_STIPPLE);
	glBegin(GL_LINES);
	glVertex3f(rawVertex[0][0].x, rawVertex[0][0].y, rawVertex[0][0].z + 0.2f);
	glVertex3f(rawVertex[10][0].x, rawVertex[10][0].y, rawVertex[10][0].z - 0.2f);
	glEnd();
	glPopAttrib();
	// parallels
	for (int i = 1; i < 10; i++) // skip first and last
	{
		glColor3f(0.5f, 0.5f, 1.0f);
		glBegin(GL_LINE_LOOP);
		for (int j = 0; j < 20; j++)
			glVertex3f(rawVertex[i][j].x, rawVertex[i][j].y, rawVertex[i][j].z); // parallels
		glEnd();
	}
	// meridians
	for (int j = 0; j < 20; j++)
	{
		glColor3f(0.2f, 0.2f, 1.0f);
		glBegin(GL_LINE_STRIP);
		for (int i = 0; i < 11; i++)
			glVertex3f(rawVertex[i][j].x, rawVertex[i][j].y, rawVertex[i][j].z); // meridians
		glEnd();
	}
	glEndList();

	list[1] = list[0] + 1; // corner axes
	glNewList(list[1], GL_COMPILE);
	glLineWidth(1.0f);
	glBegin(GL_LINES);
	{
		glColor3f(1.0f, 0.0f, 0.0f); // x - red
		glVertex3f(0.0f, 0.0f, 0.0f);
		glVertex3f(0.1f, 0.0f, 0.0f);
		glColor3f(0.0f, 1.0f, 0.0f); // y - green
		glVertex3f(0.0f, 0.0f, 0.0f);
		glVertex3f(0.0f, 0.1f, 0.0f);
		glColor3f(0.0f, 0.0f, 1.0f); // z - blue
		glVertex3f(0.0f, 0.0f, 0.0f);
		glVertex3f(0.0f, 0.0f, -0.1f);
	}
	glEnd();
	glEndList();
}

static void glSceneDraw()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear screen

	// globe
	glLoadIdentity();				 // resets transformation matrix back to its default state
	glTranslatef(0.0f, 0.0f, -3.0f); // to back
	glMultMatrixf(Transform.M);		 // rotation
	glCallList(list[0]);

	// corner axis
	glLoadIdentity();				  // resets transformation matrix back to its default state
	glTranslatef(-1.0f, -1.0f, 0.0f); // to corner
	glMultMatrixf(Transform.M);		  // rotation
	glCallList(list[1]);

	glFlush(); // flush render pipeline
}

static void glSceneCalc()
{
	Quat4fT ThisQuat;
	ArcBall.drag(&MousePt, &ThisQuat);					   // mouse points to quat
	Matrix3fSetRotationFromQuat4f(&ThisRot, &ThisQuat);	   // quat to matrix
	Matrix3fMulMatrix3f(&ThisRot, &LastRot);			   // add last rotation
	Matrix4fSetRotationFromMatrix3f(&Transform, &ThisRot); // set end rotation
}

static void glSceneUpdate()
{
	glSceneCalc();
	glSceneDraw();
	SwapBuffers(hDC);
}

static void glResize(int width, int height)
{
	if (height == 0)
		height = 1; // Prevent A Divide By Zero

	glViewport(0, 0, width, height); // Set Viewport to window dimensions

	GLfloat aspectRatio = (GLfloat)width / (GLfloat)height;

	// GL_PROJECTION - Perspective Projection  Render object affected by the distance, a car is seen smaller when it move away.
	glMatrixMode(GL_PROJECTION);					// Select The Projection Matrix
	glLoadIdentity();								// Reset The Projection Matrix
	gluPerspective(45.0f, aspectRatio, 0.0f, 5.0f); // Aspect Ratio (zNear & zFar are always positive !!!)

	// GL_MODELVIEW - Orthographic Projection Render object NOT affected by the distance like a menu, a text on the screen, 2D objects ...
	// glOrtho(0, width, 0, height, 0, 1);//Establish clipping volume (left, right, bottom, top, near, far)
	/*
	if (width <= height)
		glOrtho(-viewVolume, viewVolume, -viewVolume / aspectRatio, viewVolume / aspectRatio, 0.0, 5.0);
	else
		glOrtho(-viewVolume * aspectRatio, viewVolume * aspectRatio, -viewVolume, viewVolume, 0.0, 5.0);
		*/
	glMatrixMode(GL_MODELVIEW); // Select The Modelview Matrix
	glLoadIdentity();			// Reset The Modelview Matrix

	ArcBall.setBounds((GLfloat)width, (GLfloat)height); // update mouse bounds for arcball
}

static void glResize(HWND hWnd, int width = 0, int height = 0)
{
	if (width == 0 || height == 0)
	{
		RECT rc;
		GetClientRect(hWnd, &rc);
		width = rc.right - rc.left;
		height = rc.bottom - rc.top;
	}
	glResize(width, height);
}

static void SetFullscreen(HWND hWnd, BOOL bFullscreen)
{
	if (bFullscreen)
	{
		LONG_PTR style = (LONG_PTR)GetClassLongPtr(hWnd, GCL_STYLE);
		SetClassLongPtr(hWnd, GCL_STYLE, style | CS_NOCLOSE); // disable Alt+F4

		ShowWindow(hWnd, SW_MAXIMIZE); // maximize window
		SetWindowLongPtr(hWnd, GWL_STYLE, WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_POPUP | WS_MAXIMIZE);
		SetWindowLongPtr(hWnd, GWL_EXSTYLE, WS_EX_APPWINDOW | WS_EX_TOPMOST);
		// SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
		ShowWindow(hWnd, SW_MAXIMIZE); // send WM_SIZE
	}
	else
	{
		LONG_PTR style = (LONG_PTR)GetClassLongPtr(hWnd, GCL_STYLE);
		SetClassLongPtr(hWnd, GCL_STYLE, style & ~CS_NOCLOSE); // enable Alt+F4

		ShowWindow(hWnd, SW_RESTORE); // restore window
		SetWindowLongPtr(hWnd, GWL_STYLE, WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW);
		SetWindowLongPtr(hWnd, GWL_EXSTYLE, WS_EX_APPWINDOW);
		// SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
		ShowWindow(hWnd, SW_RESTORE); // send WM_SIZE
	}
}

static LRESULT CALLBACK WindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
		hDC = GetDC(hWnd);
		PIXELFORMATDESCRIPTOR pfd;
		ZeroMemory(&pfd, sizeof(pfd));
		pfd.nSize = sizeof(pfd);
		pfd.nVersion = 1;
		pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
		pfd.iPixelType = PFD_TYPE_RGBA;	 // request an RGBA format
		pfd.cColorBits = 24;			 // number of bits per pixel excluding alpha
		pfd.cDepthBits = 16;			 // 16Bit Z-Buffer (Depth Buffer)
		pfd.iLayerType = PFD_MAIN_PLANE; // main drawing layer
		SetPixelFormat(hDC, ChoosePixelFormat(hDC, &pfd), &pfd);

		hRC = wglCreateContext(hDC); // create render context
		wglMakeCurrent(hDC, hRC);	 // enable render context

		glDisable(GL_DEPTH_TEST);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);				   // implement transparency
		glEnable(GL_BLEND);								   // enable transparency
		glShadeModel(GL_SMOOTH);						   // smooth shading
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST); // quality of color and texture coordinate interpolation
		glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);		   // sampling quality of antialiased points
		glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);			   // sampling quality of antialiased lines
		glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);		   // sampling quality of antialiased polygons
		glEnable(GL_POINT_SMOOTH);						   // enable points antialiasing
		glEnable(GL_LINE_SMOOTH);						   // enable lines antialiasing
		glEnable(GL_POLYGON_SMOOTH);					   // enable polygons antialiasing

		glFogi(GL_FOG_MODE, GL_LINEAR); // linear fog
		glFogf(GL_FOG_START, 1.0f);		// fog begin
		glFogf(GL_FOG_END, 4.0f);		// fog end
		glHint(GL_FOG_HINT, GL_NICEST); // fog quality
		glEnable(GL_FOG);				// enable fog

		glClearColor(.1f, .1f, .1f, .0f); // background color

		glSceneGen();

		// matrix reset
		Matrix3fSetIdentity(&LastRot);
		Matrix3fSetIdentity(&ThisRot);
		Matrix4fSetIdentity(&Transform);

		SetTimer(hWnd, TIMER_INDEX_FRAME, TIMER_TIME_FRAME, NULL);
		break;
	case WM_ERASEBKGND: // window background must be erased (for example, when a window is resized)
		return 0;		// disable background erase (remove flickering)
	case WM_LBUTTONDOWN:
		isClicked = TRUE;
		LastRot = ThisRot;
		MousePt.s.X = (GLfloat)LOWORD(lParam);
		MousePt.s.Y = (GLfloat)HIWORD(lParam);
		ArcBall.click(&MousePt); // update start vector
		break;
	case WM_MOUSEMOVE:
		if (isClicked)
		{
			MousePt.s.X = (GLfloat)LOWORD(lParam);
			MousePt.s.Y = (GLfloat)HIWORD(lParam);
		}
		break;
	case WM_LBUTTONUP:
		isClicked = FALSE;
		delta *= -1; // reverse move
		break;
	case WM_KEYUP:
		if (wParam == VK_ESCAPE) // exit app
			DestroyWindow(hWnd);
		else if (wParam == VK_F11) // toogle fullscreen
			SetFullscreen(hWnd, fullscreen = !fullscreen);
		break;
	case WM_TIMER:
		if (wParam == TIMER_INDEX_FRAME)
		{
			if (!isClicked) // auto move
			{
				MousePt.s.X += delta;
				MousePt.s.Y += delta;
			}
			glSceneUpdate();
		}
		break;
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED) // SIZE_RESTORED or SIZE_MAXIMIZED
			glResize(hWnd, LOWORD(lParam), HIWORD(lParam));
		break;
	case WM_SYSCOMMAND:
		if (wParam == SC_SCREENSAVE && fullscreen) // executes the screen saver application
			return 0;							   // disable
		break;
	case WM_DESTROY:
		KillTimer(hWnd, TIMER_INDEX_FRAME);
		glDeleteLists(list[0], DISPLAY_LISTS); // delete lists
		// clean up and exit
		if (hRC)
		{
			wglMakeCurrent(NULL, NULL); // release rendering context
			wglDeleteContext(hRC);		// delete rendering context
		}
		ReleaseDC(hWnd, hDC);
		PostQuitMessage(0);
		break;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

	WNDCLASSEX wcex = {};
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC; // CS_NOCLOSE = disable Alt+F4
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); // (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpfnWndProc = (WNDPROC)WindowProcedure;
	wcex.lpszClassName = lpClassName;
	if (!RegisterClassEx(&wcex))
		return -1;

	HWND hWnd = CreateWindowEx(WS_EX_APPWINDOW, lpClassName, lpWindowTitle, WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, HWND_DESKTOP, NULL, hInstance, NULL);
	if (!hWnd)
	{
		UnregisterClass(lpClassName, hInstance);
		return -2;
	}
	SetFullscreen(hWnd, fullscreen);
	ShowWindow(hWnd, SW_SHOW); // show window
	SetForegroundWindow(hWnd); // slightly higher priority
	SetFocus(hWnd);			   // sets keyboard focus to the window

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	UnregisterClass(lpClassName, hInstance);
	return 0;
}
