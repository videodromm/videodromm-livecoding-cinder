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
	//mVDUtils = VDUtils::create(mVDSettings);
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

	mVDSession->getWindowsResolution();
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
		mMixes[0]->save();
		mVDSettings->save();
		mVDSession->save();
		deleteRenderWindows();
		mVDRouter->wsDisconnect();
		quit();
	}
}

void VideodrommLiveCodingApp::mouseMove(MouseEvent event)
{
	if (!mMixes[0]->handleMouseMove(event)) {
		// let your application perform its mouseMove handling here
	}
}

void VideodrommLiveCodingApp::mouseDown(MouseEvent event)
{
	if (!mMixes[0]->handleMouseDown(event)) {
		// let your application perform its mouseDown handling here
	}
}

void VideodrommLiveCodingApp::mouseDrag(MouseEvent event)
{
	if (!mMixes[0]->handleMouseDrag(event)) {
		// let your application perform its mouseDrag handling here
	}
}

void VideodrommLiveCodingApp::mouseUp(MouseEvent event)
{
	if (!mMixes[0]->handleMouseUp(event)) {
		// let your application perform its mouseUp handling here
	}
}
void VideodrommLiveCodingApp::keyDown(KeyEvent event)
{
	if (!mMixes[0]->handleKeyDown(event)) {
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
	if (!mMixes[0]->handleKeyUp(event)) {
	}
}
void VideodrommLiveCodingApp::resizeWindow()
{
	mVDUI->resize();
	mMixes[0]->resize();
}
void VideodrommLiveCodingApp::fileDrop(FileDropEvent event)
{
	int index = (int)(event.getX() / (mVDSettings->uiLargePreviewW + mVDSettings->uiMargin));
	int y = (int)(event.getY());
	if (index < 2 || y < mVDSettings->uiYPosRow3 || y > mVDSettings->uiYPosRow3 + mVDSettings->uiPreviewH) index = 0;
	ci::fs::path mPath = event.getFile(event.getNumFiles() - 1);
	string mFile = mPath.string();
	if (mMixes[0]->loadFileFromAbsolutePath(mFile, index) > -1) {
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
	gl::draw(mMixes[0]->getRenderTexture(), getWindowBounds());

	// imgui
	if (!mVDSettings->mCursorVisible) return;

	mVDUI->Run("UI", (int)getAverageFps());
	if (mVDUI->isReady()) {
	}

}

CINDER_APP(VideodrommLiveCodingApp, RendererGl, &VideodrommLiveCodingApp::prepare)

