#include "videodrommLiveCodingApp.h"
 
void videodrommLiveCodingApp::prepare(Settings *settings)
{
	settings->setWindowSize(1280, 720);
	//settings->setBorderless();
	settings->setWindowPos(0, 0);
	
#ifdef _DEBUG
	settings->setConsoleWindowEnabled();
#else
#endif  // _DEBUG
}

videodrommLiveCodingApp::videodrommLiveCodingApp(): mSpoutOut("VDLiveCoding", app::getWindowSize())
{
	// Settings
	mVDSettings = VDSettings::create("LiveCoding");
	// Session
	mVDSession = VDSession::create(mVDSettings);

	mVDSession->getWindowsResolution();
	// TODO render window size must be < main window size or else it overlaps
	setWindowSize(mVDSettings->mMainWindowWidth - 300, mVDSettings->mMainWindowHeight - 100);

	// UI
	mVDUI = VDUI::create(mVDSettings, mVDSession);

	mouseGlobal = false;
	mFadeInDelay = true;
	// mouse cursor and UI
	toggleCursorVisibility(mVDSettings->mCursorVisible);
	mVDSession->toggleUI();
	// windows
	mIsShutDown = false;
	isWindowReady = false;
	mMainWindow = getWindow();
	mMainWindow->getSignalDraw().connect(std::bind(&videodrommLiveCodingApp::drawMain, this));
	mMainWindow->getSignalResize().connect(std::bind(&videodrommLiveCodingApp::resizeWindow, this));
	if (mVDSettings->mStandalone) {
		createRenderWindow();
		setWindowSize(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight);
	}
	else {

	}
	setFrameRate(mVDSession->getTargetFps());
	//CI_LOG_V("setup");
	
}
void videodrommLiveCodingApp::createRenderWindow()
{
	isWindowReady = false;
	mVDUI->resize();

	deleteRenderWindows();
	mVDSession->getWindowsResolution();

	//CI_LOG_V("createRenderWindow, resolution:" + toString(mVDSettings->mRenderWidth) + "x" + toString(mVDSettings->mRenderHeight));

	string windowName = "render";
	mRenderWindow = createWindow(Window::Format().size(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight));

	// create instance of the window and store in vector
	allRenderWindows.push_back(mRenderWindow);

	mRenderWindow->setBorderless();
	mRenderWindow->getSignalDraw().connect(std::bind(&videodrommLiveCodingApp::drawRender, this));
	mVDSettings->mRenderPosXY = ivec2(mVDSettings->mRenderX, mVDSettings->mRenderY);
	mRenderWindow->setPos(50, 50);
	mRenderWindowTimer = 0.0f;
	timeline().apply(&mRenderWindowTimer, 1.0f, 2.0f).finishFn([&] { positionRenderWindow(); });
}
void videodrommLiveCodingApp::positionRenderWindow()
{
	mRenderWindow->setPos(mVDSettings->mRenderX, mVDSettings->mRenderY);
	isWindowReady = true;
}
void videodrommLiveCodingApp::deleteRenderWindows()
{
#if defined( CINDER_MSW )
	for (auto wRef : allRenderWindows) DestroyWindow((HWND)mRenderWindow->getNative());
#endif
	allRenderWindows.clear();
}
void videodrommLiveCodingApp::toggleCursorVisibility(bool visible)
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
void videodrommLiveCodingApp::update()
{
	switch (mVDSession->getCmd()) {
	case 0:
		createRenderWindow();
		break;
	case 1:
		deleteRenderWindows();
		break;
	}
	mVDSession->setFloatUniformValueByIndex(mVDSettings->IFPS, getAverageFps());
	mVDSession->update();
}
void videodrommLiveCodingApp::cleanup()
{
	if (!mIsShutDown)
	{
		mIsShutDown = true;
		//CI_LOG_V("shutdown");
		ui::disconnectWindow(getWindow());
		ui::Shutdown();
		// save settings
		mVDSettings->save();
		mVDSession->save();
		deleteRenderWindows();
		quit();
	}
}

void videodrommLiveCodingApp::mouseMove(MouseEvent event)
{
	if (!mVDSession->handleMouseMove(event)) {
		// let your application perform its mouseMove handling here
	}
}

void videodrommLiveCodingApp::mouseDown(MouseEvent event)
{
	if (!mVDSession->handleMouseDown(event)) {
		// let your application perform its mouseDown handling here
	}
}

void videodrommLiveCodingApp::mouseDrag(MouseEvent event)
{
	if (!mVDSession->handleMouseDrag(event)) {
		// let your application perform its mouseDrag handling here
	}
}

void videodrommLiveCodingApp::mouseUp(MouseEvent event)
{
	if (!mVDSession->handleMouseUp(event)) {
		// let your application perform its mouseUp handling here
	}
}
void videodrommLiveCodingApp::keyDown(KeyEvent event)
{
	if (!mVDSession->handleKeyDown(event)) {
		switch (event.getCode()) {
		case KeyEvent::KEY_KP_PLUS:
		//case KeyEvent::KEY_TAB:
		case KeyEvent::KEY_F11:
			createRenderWindow();
			break;
		case KeyEvent::KEY_KP_MINUS:
		case KeyEvent::KEY_BACKSPACE:
			deleteRenderWindows();
			break;
		case KeyEvent::KEY_ESCAPE:
			// quit the application
			quit();
			break;
		case KeyEvent::KEY_c:
			// mouse cursor and ui visibility
			toggleCursorVisibility(mVDSettings->mCursorVisible);
			break;
		}
	}
}
void videodrommLiveCodingApp::keyUp(KeyEvent event)
{
	if (!mVDSession->handleKeyUp(event)) {
	}
}
void videodrommLiveCodingApp::resizeWindow()
{
	mVDUI->resize();
	//mVDSession->resize();
}
void videodrommLiveCodingApp::fileDrop(FileDropEvent event)
{
	mVDSession->fileDrop(event);
}

void videodrommLiveCodingApp::drawRender()
{
	gl::clear(Color::black());
	if (mFadeInDelay) {
		mVDSettings->iAlpha = 0.0f;
		if (getElapsedFrames() > mVDSession->getFadeInDelay()) {
			mFadeInDelay = false;
			// warps resize at the end
			mVDSession->resize();
			timeline().apply(&mVDSettings->iAlpha, 0.0f, 1.0f, 1.5f, EaseInCubic());
		}
	}
	gl::setMatricesWindow(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight);// , false);
	if (isWindowReady) {
		//gl::draw(mVDSession->getRenderTexture(), getWindowBounds());
		gl::draw(mVDSession->getMixTexture(mVDSession->getCurrentEditIndex()), Area(0, 0, mVDSettings->mRenderWidth, mVDSettings->mRenderHeight));//getWindowBounds()	
	}
}

void videodrommLiveCodingApp::drawMain()
{
	mMainWindow->setTitle(mVDSettings->sFps + " fps videodromm");

	gl::clear(Color::black());
	gl::enableAlphaBlending(mVDSession->isEnabledAlphaBlending());
	// 20181024 gl::setMatricesWindow(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight, false);
	// 20181024 gl::draw(mVDSession->getMixTexture(mVDSession->getCurrentEditIndex()), Area(0, 0, mVDSettings->mRenderWidth, mVDSettings->mRenderHeight));//getWindowBounds()
	gl::setMatricesWindow(mVDSettings->mFboWidth, mVDSettings->mFboHeight, false);
	gl::draw(mVDSession->getMixTexture(mVDSession->getCurrentEditIndex()), Area(0, 0, mVDSettings->mFboWidth, mVDSettings->mFboHeight));//getWindowBounds()
	// spout sender
	if (mVDSettings->mSpoutSender) 
		//mSpoutOut.sendViewport();
		mSpoutOut.sendTexture(mVDSession->getMixTexture(mVDSession->getCurrentEditIndex()));
	// imgui
	if (mVDSession->showUI()) {
		mVDUI->Run("UI", (int)getAverageFps());
		if (mVDUI->isReady()) {
		}
	}
}

CINDER_APP(videodrommLiveCodingApp, RendererGl, &videodrommLiveCodingApp::prepare)

