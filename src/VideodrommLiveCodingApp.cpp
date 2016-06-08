#include "VideodrommLiveCodingApp.h"

void VideodrommLiveCodingApp::prepare(Settings *settings)
{
	settings->setWindowSize(1024, 768);
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
		mMixes = VDMix::readSettings(mVDSettings, mVDAnimation, loadFile(mMixesFilepath));
	}
	else {
		// otherwise create a texture from scratch
		mMixes.push_back(VDMix::create(mVDSettings, mVDAnimation));
	}
	mVDAnimation->tapTempo();

	mVDUtils->getWindowsResolution();

	mVDSettings->iResolution.x = mVDSettings->mRenderWidth;
	mVDSettings->iResolution.y = mVDSettings->mRenderHeight;
	// UI
	mVDUI = VDUI::create(mVDSettings, mMixes[0], mVDRouter, mVDAnimation, mVDSession);

	mouseGlobal = false;
	removeUI = false;
	static float f = 0.0f;
	// mouse cursor
	if (mVDSettings->mCursorVisible) {
		showCursor();
	}
	else {
		hideCursor();
	}
	// load vertex shader 
	try
	{
		fs::path vertexFile = getAssetPath("") / "passthru.vert";
		if (fs::exists(vertexFile)) {
			mPassthruVextexShaderString = loadString(loadAsset("passthru.vert"));
			CI_LOG_V("passthru.vert loaded");
		}
		else
		{
			CI_LOG_V("passthru.vert does not exist, should quit");
		}
	}
	catch (gl::GlslProgCompileExc &exc)
	{
		mError = string(exc.what());
		CI_LOG_V("unable to load/compile passthru vertex shader:" + string(exc.what()));
	}
	catch (const std::exception &e)
	{
		mError = string(e.what());
		CI_LOG_V("unable to load passthru vertex shader:" + string(e.what()));
	}
	// load passthru fragment shader
	mShaderTextToLoad = false;
	try
	{
		fs::path fragFile = getAssetPath("") / "live.frag";
		if (fs::exists(fragFile)) {
			mFboTextureFragmentShaderString = loadString(loadAsset("live.frag"));
			mShaderTextToLoad = true;
		}
		else
		{
			mError = "live.frag does not exist";
			CI_LOG_V(mError);
		}
		aShader = gl::GlslProg::create(mPassthruVextexShaderString, mFboTextureFragmentShaderString);
		aShader->setLabel("live");
		CI_LOG_V("live.frag loaded and compiled");
	}
	catch (gl::GlslProgCompileExc &exc)
	{
		mError = string(exc.what());
		CI_LOG_V("unable to load/compile live fragment shader:" + string(exc.what()));
	}
	catch (const std::exception &e)
	{
		mError = string(e.what());
		CI_LOG_V("unable to load live fragment shader:" + string(e.what()));
	}
	// render fbo
	gl::Fbo::Format fboFormat;
	mFbo = gl::Fbo::create(mVDSettings->mFboWidth, mVDSettings->mFboHeight, fboFormat.colorTexture());
	// windows
	mIsShutDown = false;
	mIsResizing = true;
	mMainWindow = getWindow();
	mMainWindow->getSignalDraw().connect(std::bind(&VideodrommLiveCodingApp::drawMain, this));
	mMainWindow->getSignalResize().connect(std::bind(&VideodrommLiveCodingApp::resizeWindow, this));
	createRenderWindow();
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
void VideodrommLiveCodingApp::update()
{
	mVDSettings->iFps = getAverageFps();
	mVDSettings->sFps = toString(floor(mVDSettings->iFps));

	mVDAnimation->update();
	mVDRouter->update();

	if (mVDSettings->shaderEditIndex != 0) {
		mFboTextureFragmentShaderString = mMixes[0]->getFboFragmentShaderText(mVDSettings->shaderEditIndex);
		mShaderTextToLoad = true;
		mVDSettings->shaderEditIndex = 0;
	}
	updateWindowTitle();
}
void VideodrommLiveCodingApp::updateWindowTitle()
{
	if (!mIsShutDown && mMainWindow) mMainWindow->setTitle(mVDSettings->sFps + " fps Live Coding");
}
void VideodrommLiveCodingApp::cleanup()
{
	if (!mIsShutDown)
	{
		mIsShutDown = true;
		CI_LOG_V("shutdown");
		removeUI = true;
		ui::disconnectWindow(getWindow());
		ui::Shutdown();
		// save settings
		mVDSettings->save();
		mVDSession->save();
		deleteRenderWindows();
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
		case KeyEvent::KEY_c:
			// mouse cursor
			mVDSettings->mCursorVisible = !mVDSettings->mCursorVisible;
			if (mVDSettings->mCursorVisible) {
				hideCursor();
			}
			else {
				showCursor();
			}
			break;
		case KeyEvent::KEY_h:
			removeUI = !removeUI;
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
	mIsResizing = true;
	// disconnect ui window and io events callbacks
	ui::disconnectWindow(getWindow());
}
void VideodrommLiveCodingApp::fileDrop(FileDropEvent event)
{
	int index = (int)(event.getX() / (mVDSettings->uiElementWidth + mVDSettings->uiMargin));// +1;

	string ext = "";
	// use the last of the dropped files
	boost::filesystem::path mPath = event.getFile(event.getNumFiles() - 1);
	string mFile = mPath.string();
	int dotIndex = mFile.find_last_of(".");
	int slashIndex = mFile.find_last_of("\\");

	if (dotIndex != std::string::npos && dotIndex > slashIndex) ext = mFile.substr(mFile.find_last_of(".") + 1);

	if (ext == "wav" || ext == "mp3") {
		mMixes[0]->loadAudioFile(mFile);
	}
	else if (ext == "png" || ext == "jpg") {
		if (index < 1) index = 1;
		if (index > 3) index = 3;
		mMixes[0]->loadImageFile(mFile, index, true);
	}
	else if (ext == "glsl") {
		if (index > mMixes[0]->getFboCount() - 1) index = mMixes[0]->getFboCount() - 1;
		int rtn = mMixes[0]->loadFboFragmentShader(mFile, index);
		if (rtn > -1) {
			// load success
			// reset zoom
			mVDAnimation->controlValues[22] = 1.0f;
			// update text in editor
			mFboTextureFragmentShaderString = mMixes[0]->getFboFragmentShaderText(index);
			mShaderTextToLoad = true;
		}
	}
	else if (ext == "xml") {
	}
	else if (ext == "mov") {
		//loadMovie(index, mFile);
	}
	else if (ext == "txt") {
	}
	else if (ext == "") {
		// try loading image sequence from dir
		//loadImageSequence(index, mFile);
	}

}

void VideodrommLiveCodingApp::drawRender()
{
	gl::clear(Color::black());
	gl::setMatricesWindow(toPixels(getWindowSize()));
	gl::draw(mFbo->getColorTexture(), getWindowBounds());
}

void VideodrommLiveCodingApp::drawMain()
{
	ImGuiStyle& style = ui::GetStyle();
	if (mIsResizing) {
		mIsResizing = false;
		// set ui window and io events callbacks 
		ui::connectWindow(getWindow());
		ui::initialize();

#pragma region style
		// our theme variables
		ImGuiStyle& style = ui::GetStyle();
		style.WindowRounding = 4;
		style.WindowPadding = ImVec2(3, 3);
		style.FramePadding = ImVec2(2, 2);
		style.ItemSpacing = ImVec2(3, 3);
		style.ItemInnerSpacing = ImVec2(3, 3);
		style.WindowMinSize = ImVec2(mVDSettings->mPreviewFboWidth, mVDSettings->mPreviewFboHeight);
		style.Alpha = 0.6f;
		style.Colors[ImGuiCol_Text] = ImVec4(0.89f, 0.92f, 0.94f, 1.00f);
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
		style.Colors[ImGuiCol_Border] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
		style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.38f);
		style.Colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
		style.Colors[ImGuiCol_TitleBg] = ImVec4(0.4f, 0.0f, 0.21f, 1.00f);
		style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.17f, 0.17f, 0.17f, 1.00f);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.97f, 0.0f, 0.17f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
		style.Colors[ImGuiCol_ComboBg] = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
		style.Colors[ImGuiCol_CheckMark] = ImVec4(0.99f, 0.22f, 0.22f, 0.50f);
		style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.65f, 0.25f, 0.25f, 1.00f);
		style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.8f, 0.35f, 0.35f, 1.00f);
		style.Colors[ImGuiCol_Button] = ImVec4(0.17f, 0.17f, 0.17f, 1.00f);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.38f, 0.38f, 0.38f, 1.00f);
		style.Colors[ImGuiCol_Header] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
		style.Colors[ImGuiCol_Column] = ImVec4(0.04f, 0.04f, 0.04f, 0.22f);
		style.Colors[ImGuiCol_ColumnHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
		style.Colors[ImGuiCol_ColumnActive] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
		style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.65f, 0.25f, 0.25f, 1.00f);
		style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.8f, 0.35f, 0.35f, 1.00f);
		style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.9f, 0.45f, 0.45f, 1.00f);
		style.Colors[ImGuiCol_CloseButton] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
		style.Colors[ImGuiCol_CloseButtonHovered] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
		style.Colors[ImGuiCol_CloseButtonActive] = ImVec4(0.49f, 0.49f, 0.49f, 1.00f);
		style.Colors[ImGuiCol_PlotLines] = ImVec4(0.65f, 0.25f, 0.25f, 1.00f);
		style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.8f, 0.35f, 0.35f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.65f, 0.25f, 0.25f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.8f, 0.35f, 0.35f, 1.00f);
		style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
		style.Colors[ImGuiCol_TooltipBg] = ImVec4(0.65f, 0.25f, 0.25f, 1.00f);
#pragma endregion style
	}
	gl::clear(Color::black());
	gl::color(Color::white());
	gl::setMatricesWindow(mVDSettings->mMainWindowWidth, mVDSettings->mMainWindowHeight, false);

#pragma region draw
	// draw using the mix shader
	mFbo->bindFramebuffer();
	//gl::setViewport(mVDFbos[mVDSettings->mMixFboIndex].fbo.getBounds());

	// clear the FBO
	gl::clear();
	gl::setMatricesWindow(mVDSettings->mFboWidth, mVDSettings->mFboHeight);

	aShader->bind();
	aShader->uniform("iGlobalTime", mVDSettings->iGlobalTime);
	//20140703 aShader->uniform("iResolution", vec3(mVDSettings->mRenderResoXY.x, mVDSettings->mRenderResoXY.y, 1.0));
	aShader->uniform("iResolution", vec3(mVDSettings->mFboWidth, mVDSettings->mFboHeight, 1.0));
	aShader->uniform("iChannelResolution", mVDSettings->iChannelResolution, 4);
	aShader->uniform("iMouse", vec4(mVDSettings->mRenderPosXY.x, mVDSettings->mRenderPosXY.y, mVDSettings->iMouse.z, mVDSettings->iMouse.z));//iMouse =  Vec3i( event.getX(), mRenderHeight - event.getY(), 1 );
	aShader->uniform("iChannel0", 0);
	aShader->uniform("iChannel1", 1);
	aShader->uniform("iChannel2", 2);
	aShader->uniform("iAudio0", 0);
	aShader->uniform("iFreq0", mVDAnimation->iFreqs[0]);
	aShader->uniform("iFreq1", mVDAnimation->iFreqs[1]);
	aShader->uniform("iFreq2", mVDAnimation->iFreqs[2]);
	aShader->uniform("iFreq3", mVDAnimation->iFreqs[3]);
	aShader->uniform("spectrum", vec3(mVDAnimation->iFreqs[0], mVDAnimation->iFreqs[1], mVDAnimation->iFreqs[2]));
	aShader->uniform("iChannelTime", mVDSettings->iChannelTime, 4);
	aShader->uniform("iColor", vec3(mVDAnimation->controlValues[1], mVDAnimation->controlValues[2], mVDAnimation->controlValues[3]));// mVDSettings->iColor);
	aShader->uniform("iBackgroundColor", vec3(mVDAnimation->controlValues[5], mVDAnimation->controlValues[6], mVDAnimation->controlValues[7]));// mVDSettings->iBackgroundColor);
	aShader->uniform("iSteps", (int)mVDAnimation->controlValues[20]);
	aShader->uniform("iRatio", mVDAnimation->controlValues[11]);//check if needed: +1;//mVDSettings->iRatio); 
	aShader->uniform("width", 1);
	aShader->uniform("height", 1);
	aShader->uniform("iRenderXY", mVDSettings->mRenderXY);
	aShader->uniform("iZoom", mVDAnimation->controlValues[22]);
	aShader->uniform("iAlpha", mVDAnimation->controlValues[4] * mVDSettings->iAlpha);
	aShader->uniform("iBlendmode", mVDSettings->iBlendMode);
	aShader->uniform("iChromatic", mVDAnimation->controlValues[10]);
	aShader->uniform("iRotationSpeed", mVDAnimation->controlValues[19]);
	aShader->uniform("iCrossfade", mVDAnimation->controlValues[18]);
	aShader->uniform("iPixelate", mVDAnimation->controlValues[15]);
	aShader->uniform("iExposure", mVDAnimation->controlValues[14]);
	aShader->uniform("iDeltaTime", 1.0f);// mVDAnimation->iDeltaTime);
	aShader->uniform("iFade", (int)mVDSettings->iFade);
	aShader->uniform("iToggle", (int)mVDAnimation->controlValues[46]);
	aShader->uniform("iLight", (int)mVDSettings->iLight);
	aShader->uniform("iLightAuto", (int)mVDSettings->iLightAuto);
	aShader->uniform("iGreyScale", (int)mVDSettings->iGreyScale);
	aShader->uniform("iTransition", mVDSettings->iTransition);
	aShader->uniform("iAnim", mVDSettings->iAnim.value());
	aShader->uniform("iRepeat", (int)mVDSettings->iRepeat);
	aShader->uniform("iVignette", (int)mVDAnimation->controlValues[47]);
	aShader->uniform("iInvert", (int)mVDAnimation->controlValues[48]);
	aShader->uniform("iDebug", (int)mVDSettings->iDebug);
	aShader->uniform("iShowFps", (int)mVDSettings->iShowFps);
	aShader->uniform("iFps", mVDSettings->iFps);
	aShader->uniform("iTempoTime", 1.0f);// mVDAnimation->iTempoTime);
	aShader->uniform("iGlitch", (int)mVDAnimation->controlValues[45]);
	aShader->uniform("iTrixels", mVDAnimation->controlValues[16]);
	aShader->uniform("iGridSize", mVDAnimation->controlValues[17]);
	aShader->uniform("iBeat", mVDSettings->iBeat);
	aShader->uniform("iSeed", mVDSettings->iSeed);
	aShader->uniform("iRedMultiplier", mVDSettings->iRedMultiplier);
	aShader->uniform("iGreenMultiplier", mVDSettings->iGreenMultiplier);
	aShader->uniform("iBlueMultiplier", mVDSettings->iBlueMultiplier);
	aShader->uniform("iFlipH", 0);
	aShader->uniform("iFlipV", 0);
	aShader->uniform("iParam1", mVDSettings->iParam1);
	aShader->uniform("iParam2", mVDSettings->iParam2);
	aShader->uniform("iXorY", mVDSettings->iXorY);
	aShader->uniform("iBadTv", mVDSettings->iBadTv);

	if (mMixes[0]->isFboUsed()) {
		mMixes[0]->getFboTexture(2)->bind(0);
		mMixes[0]->getFboTexture(1)->bind(1);
	}
	else {
		for (unsigned int t = 0; t < mMixes[0]->getInputTexturesCount(); t++) {
			mMixes[0]->getInputTexture(t)->bind(t);
		}
		//mMixes[0]->getInputTexture(1)->bind(0);
		//mMixes[0]->getInputTexture(2)->bind(1);
	}

	gl::drawSolidRect(Rectf(0, 0, mVDSettings->mFboWidth, mVDSettings->mFboHeight));
	// stop drawing into the FBO
	mFbo->unbindFramebuffer();
	if (mMixes[0]->isFboUsed()) {
		mMixes[0]->getFboTexture(1)->unbind();
		mMixes[0]->getFboTexture(2)->unbind();
	}
	else {
		for (unsigned int t = 0; t < mMixes[0]->getInputTexturesCount(); t++) {
			mMixes[0]->getInputTexture(t)->unbind();
		}
		//mMixes[0]->getInputTexture(2)->unbind();
		//mMixes[0]->getInputTexture(1)->unbind();
	}

	gl::clear(Color::black());
	gl::setMatricesWindow(toPixels(getWindowSize()));
	gl::draw(mFbo->getColorTexture(), getWindowBounds());

#pragma endregion draw

	// imgui
	if (removeUI) return;
#pragma region menu
	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save", "Ctrl+S")) {
				// save warp settings
				//Warp::writeSettings(mWarps, writeFile("warps1.xml"));
				// save params
				mVDSettings->save();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Window"))
		{
			if (ImGui::MenuItem("Create", "Ctrl+W")) { createRenderWindow(); }
			if (ImGui::MenuItem("Delete", "Ctrl+D")) { deleteRenderWindows(); }
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Options"))
		{
			ImGui::DragFloat("Global Alpha", &style.Alpha, 0.005f, 0.20f, 1.0f, "%.2f");
			ImGui::EndMenu();
		}
		if (ImGui::MenuItem("Quit", "Alt+F4")) { cleanup(); }
		ImGui::EndMainMenuBar();
	}
#pragma endregion menu

	showVDUI((int)getAverageFps());
	
#pragma region Editor
	ui::SetNextWindowPos(ImVec2(mVDSettings->uiXPosCol1, mVDSettings->uiYPosRow2), ImGuiSetCond_Once);
	ui::SetNextWindowSize(ImVec2(mVDSettings->uiLargeW*2, mVDSettings->uiLargeH*2), ImGuiSetCond_FirstUseEver);
	ui::Begin("Editor");
	{
		static bool read_only = false;

		size_t const MAX = 16384; // maximum number of chars
		static char mShaderText[MAX] =
			"uniform vec3 iResolution;\n"
			"uniform vec3 iColor;\n"
			"uniform float iGlobalTime;\n"
			"uniform sampler2D iChannel0;\n"
			"uniform sampler2D iChannel1;\n"
			"uniform sampler2D iChannel2;\n"
			"uniform vec3 spectrum;\n"
			"\n"
			"out vec4 oColor;\n"
			"void main(void) {\n"
			"\tvec2 uv = gl_FragCoord.xy / iResolution.xy;\n"
			"\tvec4 t0 = texture(iChannel0, uv);\n"
			"\tvec4 t1 = texture(iChannel1, uv);\n"
			"\tvec4 t2 = texture(iChannel2, uv);\n"
			"\toColor = vec4(t0.x, t1.y, cos(iGlobalTime), 1.0);\n"
			"}\n";
		if (mShaderTextToLoad) {
			mShaderTextToLoad = false;
			std::copy(mFboTextureFragmentShaderString.begin(), (mFboTextureFragmentShaderString.size() >= MAX ? mFboTextureFragmentShaderString.begin() + MAX : mFboTextureFragmentShaderString.end()), mShaderText);
		}
		ui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ui::Checkbox("Read-only", &read_only);
		ui::PopStyleVar();
		if (ui::InputTextMultiline("##source", mShaderText, IM_ARRAYSIZE(mShaderText), ImVec2(-1.0f, mVDSettings->uiYPosRow2 - 200.0f), ImGuiInputTextFlags_AllowTabInput | (read_only ? ImGuiInputTextFlags_ReadOnly : 0))) {
			// text changed
			CI_LOG_V("text changed");
			try
			{
				aShader = gl::GlslProg::create(mPassthruVextexShaderString, mShaderText);
				aShader->setLabel("live");
				CI_LOG_V("live.frag loaded and compiled");
				mFboTextureFragmentShaderString = mShaderText;
				stringstream sParams;
				sParams << "/*{ \"title\" : \"" << getElapsedSeconds() << "\" }*/ " << mFboTextureFragmentShaderString;
				mVDRouter->wsWrite(sParams.str());
				//OK mVDRouter->wsWrite("/*{ \"title\" : \"live\" }*/ " + mFboTextureFragmentShaderString);

				mError = "";
			}
			catch (gl::GlslProgCompileExc &exc)
			{
				mError = string(exc.what());
				CI_LOG_V("unable to load/compile live fragment shader:" + string(exc.what()));
			}
			catch (const std::exception &e)
			{
				mError = string(e.what());
				CI_LOG_V("unable to load live fragment shader:" + string(e.what()));
			}
		}
		else {
			// nothing changed
		}
		ui::TextColored(ImColor(255, 0, 0), mError.c_str());
	}
	ui::End();
#pragma endregion Editor

}
// UI
void VideodrommLiveCodingApp::showVDUI(unsigned int fps) {
	mVDUI->Run("UI", fps);
}

CINDER_APP(VideodrommLiveCodingApp, RendererGl, &VideodrommLiveCodingApp::prepare)

