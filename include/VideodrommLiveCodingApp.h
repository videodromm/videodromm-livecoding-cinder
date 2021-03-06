#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

// UserInterface
#include "CinderImGui.h"
// Settings
#include "VDSettings.h"
// Session
#include "VDSession.h"
// UI
#include "VDUI.h"
// log
//#include "VDLog.h"
// Spout
#include "CiSpoutOut.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace videodromm;

#define IM_ARRAYSIZE(_ARR)			((int)(sizeof(_ARR)/sizeof(*_ARR)))

class videodrommLiveCodingApp : public App {

public:
	static void prepare(Settings *settings);

	videodrommLiveCodingApp();
	void mouseMove(MouseEvent event) override;
	void mouseDown(MouseEvent event) override;
	void mouseDrag(MouseEvent event) override;
	void mouseUp(MouseEvent event) override;
	void keyDown(KeyEvent event) override;
	void keyUp(KeyEvent event) override;
	void fileDrop(FileDropEvent event) override;
	void update() override;
	void drawMain();
	void drawRender();
	void cleanup() override;
	void toggleCursorVisibility(bool visible);
private:
	// Settings
	VDSettingsRef				mVDSettings;
	// Session
	VDSessionRef				mVDSession;
	// UI
	VDUIRef						mVDUI;
	
	// handle resizing for imgui
	void						resizeWindow();
	// imgui
	float						color[4];
	float						backcolor[4];
	int							playheadPositions[12];
	int							speeds[12];

	float						f = 0.0f;
	char						buf[64];
	unsigned int				i, j;

	bool						mouseGlobal;

	string						mError;

	// windows
	WindowRef					mMainWindow;
	bool						mIsShutDown;
	// render
	void						createRenderWindow();
	void						deleteRenderWindows();
	vector<WindowRef>			allRenderWindows;
	void						positionRenderWindow();
	WindowRef					mRenderWindow;
	bool						isWindowReady;
	// timeline
	Anim<float>					mRenderWindowTimer;
	bool						mFadeInDelay;
	// spout
	SpoutOut					mSpoutOut;
};