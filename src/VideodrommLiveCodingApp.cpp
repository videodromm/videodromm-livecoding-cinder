#include "VideodrommLiveCodingApp.h"

void VideodrommLiveCodingApp::prepare(Settings *settings)
{
	settings->setWindowSize(640, 480);
	//settings->setBorderless();
	settings->setWindowPos(0, 0);
}
void VideodrommLiveCodingApp::setup()
{
	// maximize fps
	disableFrameRate();
	gl::enableVerticalSync(false);
	// Settings
	mVDSettings = VDSettings::create();
	// Session
	mVDSession = VDSession::create(mVDSettings);
	// Utils
	mVDUtils = VDUtils::create(mVDSettings);
	// Animation
	mVDAnimation = VDAnimation::create(mVDSettings, mVDSession);
	// Message router
	mVDRouter = VDRouter::create(mVDSettings, mVDAnimation, mVDSession);
	// Mix
	mMixesFilepath = getAssetPath("") / mVDSettings->mAssetsPath / "mixes.xml";
	if (fs::exists(mMixesFilepath)) {
		// load textures from file if one exists
		mMixes = VDMix::readSettings(mVDSettings, mVDAnimation, mVDRouter, loadFile(mMixesFilepath));
	}
	else {
		// otherwise create a texture from scratch
		mMixes.push_back(VDMix::create(mVDSettings, mVDAnimation, mVDRouter));
	}
	mVDAnimation->tapTempo();

	mVDUtils->getWindowsResolution();
	setWindowSize(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight);

	mVDSettings->iResolution.x = mVDSettings->mRenderWidth;
	mVDSettings->iResolution.y = mVDSettings->mRenderHeight;
	// UI
	mVDUI = VDUI::create(mVDSettings, mMixes[0], mVDRouter, mVDAnimation, mVDSession);

	mouseGlobal = false;

	static float f = 0.0f;
	// mouse cursor and UI
	setUIVisibility(mVDSettings->mCursorVisible);
	// windows
	mIsShutDown = false;
	mIsResizing = true;
	mMainWindow = getWindow();
	mMainWindow->getSignalDraw().connect(std::bind(&VideodrommLiveCodingApp::drawMain, this));
	mMainWindow->getSignalResize().connect(std::bind(&VideodrommLiveCodingApp::resizeWindow, this));
	/*if (mVDSettings->mStandalone) {
		createRenderWindow();
		setWindowSize(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight);
		}
		else {

		}*/
}
void VideodrommLiveCodingApp::createRenderWindow()
{
	deleteRenderWindows();
	mVDUtils->getWindowsResolution();

	mVDSettings->iResolution.x = mVDSettings->mRenderWidth;
	mVDSettings->iResolution.y = mVDSettings->mRenderHeight;

	CI_LOG_V("createRenderWindow, resolution:" + toString(mVDSettings->iResolution.x) + "x" + toString(mVDSettings->iResolution.y));

	string windowName = "render";
	mRenderWindow = createWindow(Window::Format().size(mVDSettings->iResolution.x, mVDSettings->iResolution.y));

	// create instance of the window and store in vector
	allRenderWindows.push_back(mRenderWindow);

	mRenderWindow->setBorderless();
	mRenderWindow->getSignalDraw().connect(std::bind(&VideodrommLiveCodingApp::drawRender, this));
	mVDSettings->mRenderPosXY = ivec2(mVDSettings->mRenderX, mVDSettings->mRenderY);//20141214 was 0
	mRenderWindow->setPos(50, 50);
	mRenderWindowTimer = 0.0f;
	timeline().apply(&mRenderWindowTimer, 1.0f, 2.0f).finishFn([&]{ positionRenderWindow(); });
}
void VideodrommLiveCodingApp::positionRenderWindow()
{
	mRenderWindow->setPos(mVDSettings->mRenderX, mVDSettings->mRenderY);
}
void VideodrommLiveCodingApp::deleteRenderWindows()
{
#if defined( CINDER_MSW )
	for (auto wRef : allRenderWindows) DestroyWindow((HWND)mRenderWindow->getNative());
#endif
	allRenderWindows.clear();
}
void VideodrommLiveCodingApp::setUIVisibility(bool visible)
{
	if (visible)
	{
		showCursor();
	}
	else
	{
		hideCursor();
	}
}
void VideodrommLiveCodingApp::update()
{
	mVDSettings->iFps = getAverageFps();
	mVDSettings->sFps = toString(floor(mVDSettings->iFps));
	mMixes[0]->update();
	mVDAnimation->update();
	mVDRouter->update();

	updateWindowTitle();
}
void VideodrommLiveCodingApp::updateWindowTitle()
{
	//mMainWindow->setTitle(mVDSettings->sFps + " fps Live Coding");
}
void VideodrommLiveCodingApp::cleanup()
{
	if (!mIsShutDown)
	{
		mIsShutDown = true;
		CI_LOG_V("shutdown");
		ui::disconnectWindow(getWindow());
		ui::Shutdown();
		// save settings
		mVDSettings->save();
		mVDSession->save();
		deleteRenderWindows();
		mVDRouter->wsDisconnect();
		quit();
	}
}
void VideodrommLiveCodingApp::mouseDown(MouseEvent event)
{

}
void VideodrommLiveCodingApp::keyDown(KeyEvent event)
{

	if (!mVDAnimation->handleKeyDown(event)) {
		// Animation did not handle the key, so handle it here
		switch (event.getCode()) {
		case KeyEvent::KEY_ESCAPE:
			// quit the application
			quit();
			break;
		case KeyEvent::KEY_l:
			mVDAnimation->load();
			break;
		case KeyEvent::KEY_h:
			// mouse cursor
			mVDSettings->mCursorVisible = !mVDSettings->mCursorVisible;
			setUIVisibility(mVDSettings->mCursorVisible);
			break;
		}
	}
}

void VideodrommLiveCodingApp::keyUp(KeyEvent event)
{
	if (!mVDAnimation->handleKeyUp(event)) {
		// Animation did not handle the key, so handle it here
	}
}
void VideodrommLiveCodingApp::resizeWindow()
{
	mVDUI->resize();
}
void VideodrommLiveCodingApp::fileDrop(FileDropEvent event)
{
	int index = (int)(event.getX() / (mVDSettings->uiLargePreviewW + mVDSettings->uiMargin));
	ci::fs::path mPath = event.getFile(event.getNumFiles() - 1);
	string mFile = mPath.string();
	if (mMixes[0]->loadFileFromAbsolutePath(mFile, index) > -1) {
		// load success
		// reset zoom
		mVDAnimation->controlValues[22] = 1.0f;
	}
}

void VideodrommLiveCodingApp::drawRender()
{
	gl::clear(Color::black());
	//gl::setMatricesWindow(toPixels(getWindowSize()));
	gl::setMatricesWindow(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight, false);
	// live coding fbo gl::draw(mFbo->getColorTexture(), getWindowBounds());
	gl::draw(mMixes[0]->getMixTexture(), getWindowBounds());
}

void VideodrommLiveCodingApp::drawMain()
{
	mMainWindow->setTitle(mVDSettings->sFps + " fps Live Coding");

	gl::clear(Color::black());
	//	gl::setMatricesWindow(toPixels(getWindowSize()));
	gl::setMatricesWindow(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight, false);
	gl::draw(mMixes[0]->getMixTexture(), getWindowBounds());

	// imgui
	if (!mVDSettings->mCursorVisible) return;

	mVDUI->Run("UI", (int)getAverageFps());
	if (mVDUI->isReady()) {
	}

}

CINDER_APP(VideodrommLiveCodingApp, RendererGl, &VideodrommLiveCodingApp::prepare)

