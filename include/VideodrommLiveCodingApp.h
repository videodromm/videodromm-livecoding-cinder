#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

// UserInterface
#include "CinderImGui.h"
// Console
#include "VDConsole.h"
// Settings
#include "VDSettings.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace VideoDromm;

#define IM_ARRAYSIZE(_ARR)			((int)(sizeof(_ARR)/sizeof(*_ARR)))

class VideodrommLiveCodingApp : public App {

public:

	void setup() override;
	void mouseDown(MouseEvent event) override;
	void update() override;
	void draw() override;
	void cleanup() override;
private:
	// Settings
	VDSettingsRef				mVDSettings;
	// handle resizing for imgui
	void						resizeWindow();
	bool						mIsResizing;
	void						updateWindowTitle();
	// imgui
	float						color[4];
	float						backcolor[4];
	int							playheadPositions[12];
	int							speeds[12];
	// mPreviewFboWidth 80 mPreviewFboHeight 60 margin 10 inBetween 15
	int							w;
	int							h;
	int							displayHeight;
	int							xPos;
	int							xPosCol1;
	int							xPosCol2;
	int							xPosCol3;
	int							yPos;
	int							yPosRow1;
	int							yPosRow2;
	int							yPosRow3;
	int							largeW;
	int							largeH;
	int							largePreviewW;
	int							largePreviewH;
	int							margin;
	int							inBetween;

	float						f = 0.0f;
	char						buf[64];

	bool						mouseGlobal;
	bool						removeUI;
	// shader
	gl::GlslProgRef				aShader;
	//! default vertex shader
	std::string					mPassthruVextexShaderString;
	//! default fragment shader
	std::string					mFboTextureFragmentShaderString;
	string						mError;
	// fbo
	gl::FboRef					mFbo;
};