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
	//disableFrameRate();
	//gl::enableVerticalSync(false);
	// Settings
	mVDSettings = VDSettings::create();
	// Session
	mVDSession = VDSession::create(mVDSettings);

	mVDSession->getWindowsResolution();
	setWindowSize(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight);

	mVDSettings->iResolution.x = mVDSettings->mRenderWidth;
	mVDSettings->iResolution.y = mVDSettings->mRenderHeight;
	// UI
	mVDUI = VDUI::create(mVDSettings, mVDSession);

	mouseGlobal = false;
	mFadeInDelay = true;
	//static float f = 0.0f;
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
	setFrameRate(mVDSession->getTargetFps());
}
void VideodrommLiveCodingApp::createRenderWindow()
{
	deleteRenderWindows();
	mVDSession->getWindowsResolution();

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
	mVDSession->setControlValue(20, getAverageFps());
	mVDSession->update();
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
		quit();
	}
}

void VideodrommLiveCodingApp::mouseMove(MouseEvent event)
{
	if (!mVDSession->handleMouseMove(event)) {
		// let your application perform its mouseMove handling here
	}
}

void VideodrommLiveCodingApp::mouseDown(MouseEvent event)
{
	if (!mVDSession->handleMouseDown(event)) {
		// let your application perform its mouseDown handling here
	}
}

void VideodrommLiveCodingApp::mouseDrag(MouseEvent event)
{
	if (!mVDSession->handleMouseDrag(event)) {
		// let your application perform its mouseDrag handling here
	}
}

void VideodrommLiveCodingApp::mouseUp(MouseEvent event)
{
	if (!mVDSession->handleMouseUp(event)) {
		// let your application perform its mouseUp handling here
	}
}
void VideodrommLiveCodingApp::keyDown(KeyEvent event)
{
	if (!mVDSession->handleKeyDown(event)) {
		switch (event.getCode()) {
		case KeyEvent::KEY_ESCAPE:
			// quit the application
			quit();
			break;
		case KeyEvent::KEY_h:
			// mouse cursor and ui visibility
			mVDSettings->mCursorVisible = !mVDSettings->mCursorVisible;
			setUIVisibility(mVDSettings->mCursorVisible);
			break;
		}
	}
}

void VideodrommLiveCodingApp::keyUp(KeyEvent event)
{
	if (!mVDSession->handleKeyUp(event)) {
	}
}
void VideodrommLiveCodingApp::resizeWindow()
{
	mVDUI->resize();
	mVDSession->resize();
}
void VideodrommLiveCodingApp::fileDrop(FileDropEvent event)
{
	mVDSession->fileDrop(event);
}

void VideodrommLiveCodingApp::drawRender()
{
	gl::clear(Color::black());
	//gl::setMatricesWindow(toPixels(getWindowSize()));
	if (mFadeInDelay) {
		mVDSettings->iAlpha = 0.0f;
		if (getElapsedFrames() > mVDSession->getFadeInDelay()) {
			mFadeInDelay = false;
			timeline().apply(&mVDSettings->iAlpha, 0.0f, 1.0f, 1.5f, EaseInCubic());
		}
	}
	gl::setMatricesWindow(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight, false);
	// live coding fbo gl::draw(mFbo->getColorTexture(), getWindowBounds());
	gl::draw(mVDSession->getMixTexture(), getWindowBounds());

}

void VideodrommLiveCodingApp::drawMain()
{
	mMainWindow->setTitle(mVDSettings->sFps + " fps Live Coding");

	gl::clear(Color::black());
	//	gl::setMatricesWindow(toPixels(getWindowSize()));
	gl::setMatricesWindow(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight, false);
	gl::draw(mVDSession->getRenderTexture(), getWindowBounds());
	//gl::draw(mVDSession->getStreamedTexture(), getWindowBounds()); 
	// imgui
	if (!mVDSettings->mCursorVisible) return;

	mVDUI->Run("UI", (int)getAverageFps());
	if (mVDUI->isReady()) {
	}

}

CINDER_APP(VideodrommLiveCodingApp, RendererGl, &VideodrommLiveCodingApp::prepare)

