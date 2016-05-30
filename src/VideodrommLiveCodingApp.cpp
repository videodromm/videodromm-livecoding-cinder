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
	mMixesFilepath = getAssetPath("") / "mixes.xml";
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
	mVDUI = VDUI::create(mVDSettings, mMixes[0]);


	// imgui
	margin = 3;
	inBetween = 3;
	// mPreviewFboWidth 80 mPreviewFboHeight 60 margin 10 inBetween 15 mPreviewWidth = 160;mPreviewHeight = 120;
	w = mVDSettings->mPreviewFboWidth + margin;
	h = mVDSettings->mPreviewFboHeight * 2.3;
	largeW = (mVDSettings->mPreviewFboWidth + margin) * 4;
	largeH = (mVDSettings->mPreviewFboHeight + margin) * 5;
	largePreviewW = mVDSettings->mPreviewWidth + margin;
	largePreviewH = (mVDSettings->mPreviewHeight + margin) * 2.4;
	displayHeight = mVDSettings->mMainWindowHeight - 50;
	yPosRow1 = 18;
	yPosRow2 = 500;
	yPosRow3 = 600;

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

	mIsResizing = true;
	getWindow()->getSignalResize().connect(std::bind(&VideodrommLiveCodingApp::resizeWindow, this));
}

void VideodrommLiveCodingApp::update()
{
	mVDSettings->iFps = getAverageFps();
	mVDSettings->sFps = toString(floor(mVDSettings->iFps));

	mVDAnimation->update();
	mVDRouter->update();

	updateWindowTitle();
}
void VideodrommLiveCodingApp::updateWindowTitle()
{
	getWindow()->setTitle(mVDSettings->sFps + " fps Live Coding");
}
void VideodrommLiveCodingApp::cleanup()
{
	CI_LOG_V("shutdown");
	// save settings
	mVDSettings->save();
	mVDSession->save();
	ui::Shutdown();
	quit();
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
	int index = (int)(event.getX() / (margin + w));// +1;

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

void VideodrommLiveCodingApp::draw()
{
	ImGuiStyle& style = ui::GetStyle();
	if (mIsResizing) {
		mIsResizing = false;
		// set ui window and io events callbacks 
		ui::connectWindow(getWindow());
		ui::initialize();

#pragma region style
		// our theme variables

		style.WindowPadding = ImVec2(3, 3);
		style.FramePadding = ImVec2(2, 2);
		style.ItemSpacing = ImVec2(3, 3);
		style.ItemInnerSpacing = ImVec2(3, 3);
		style.WindowMinSize = ImVec2(w, mVDSettings->mPreviewFboHeight);
		// new style
		style.Alpha = 1.0;
		style.WindowFillAlphaDefault = 0.83;
		style.ChildWindowRounding = 3;
		style.WindowRounding = 3;
		style.FrameRounding = 3;

		style.Colors[ImGuiCol_Text] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.00f, 0.40f, 0.41f, 1.00f);
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
		style.Colors[ImGuiCol_ChildWindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		style.Colors[ImGuiCol_Border] = ImVec4(0.00f, 1.00f, 1.00f, 0.65f);
		style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		style.Colors[ImGuiCol_FrameBg] = ImVec4(0.44f, 0.80f, 0.80f, 0.18f);
		style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.44f, 0.80f, 0.80f, 0.27f);
		style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.44f, 0.81f, 0.86f, 0.66f);
		style.Colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.18f, 0.21f, 0.73f);
		style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.54f);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.27f);
		style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.20f);
		style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.22f, 0.29f, 0.30f, 0.71f);
		style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.00f, 1.00f, 1.00f, 0.44f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.00f, 1.00f, 1.00f, 0.74f);
		style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_ComboBg] = ImVec4(0.16f, 0.24f, 0.22f, 0.60f);
		style.Colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 1.00f, 1.00f, 0.68f);
		style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 1.00f, 1.00f, 0.36f);
		style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.76f);
		style.Colors[ImGuiCol_Button] = ImVec4(0.00f, 0.65f, 0.65f, 0.46f);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.01f, 1.00f, 1.00f, 0.43f);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.62f);
		style.Colors[ImGuiCol_Header] = ImVec4(0.00f, 1.00f, 1.00f, 0.33f);
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 1.00f, 1.00f, 0.42f);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.54f);
		style.Colors[ImGuiCol_Column] = ImVec4(0.00f, 0.50f, 0.50f, 0.33f);
		style.Colors[ImGuiCol_ColumnHovered] = ImVec4(0.00f, 0.50f, 0.50f, 0.47f);
		style.Colors[ImGuiCol_ColumnActive] = ImVec4(0.00f, 0.70f, 0.70f, 1.00f);
		style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 1.00f, 1.00f, 0.54f);
		style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.00f, 1.00f, 1.00f, 0.74f);
		style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_CloseButton] = ImVec4(0.00f, 0.78f, 0.78f, 0.35f);
		style.Colors[ImGuiCol_CloseButtonHovered] = ImVec4(0.00f, 0.78f, 0.78f, 0.47f);
		style.Colors[ImGuiCol_CloseButtonActive] = ImVec4(0.00f, 0.78f, 0.78f, 1.00f);
		style.Colors[ImGuiCol_PlotLines] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
		style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.00f, 1.00f, 1.00f, 0.22f);
		style.Colors[ImGuiCol_TooltipBg] = ImVec4(0.00f, 0.13f, 0.13f, 0.90f);
		style.Colors[ImGuiCol_ModalWindowDarkening] = ImVec4(0.04f, 0.10f, 0.09f, 0.51f);
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
	aShader->uniform("iAudio0", 0);
	aShader->uniform("iFreq0", mVDAnimation->iFreqs[0]);
	aShader->uniform("iFreq1", mVDAnimation->iFreqs[1]);
	aShader->uniform("iFreq2", mVDAnimation->iFreqs[2]);
	aShader->uniform("iFreq3", mVDAnimation->iFreqs[3]);
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

	mMixes[0]->getFboTexture(2)->bind(0);
	mMixes[0]->getFboTexture(1)->bind(1);

	gl::drawSolidRect(Rectf(0, 0, mVDSettings->mFboWidth, mVDSettings->mFboHeight));
	// stop drawing into the FBO
	mFbo->unbindFramebuffer();
	mMixes[0]->getFboTexture(1)->unbind();
	mMixes[0]->getFboTexture(2)->unbind();

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
			if (ImGui::MenuItem("New")) {}
			if (ImGui::MenuItem("Open", "Ctrl+O")) {}
			if (ImGui::BeginMenu("Open Recent"))
			{
				ImGui::MenuItem("live.frag");
				ImGui::EndMenu();
			}
			if (ImGui::MenuItem("Save", "Ctrl+S")) {}
			if (ImGui::MenuItem("Save As..")) {}
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

	static int currentWindowRow1 = 0;
	static int currentWindowRow2 = 0;
	static int currentWindowRow3 = 0;
	static int selectedLeftInputTexture = 2;
	static int selectedRightInputTexture = 1;

	xPos = margin;

#pragma region Editor
	ui::SetNextWindowPos(ImVec2(xPos, yPosRow1), ImGuiSetCond_Once);
	ui::SetNextWindowSize(ImVec2(620, yPosRow2 - 40), ImGuiSetCond_FirstUseEver);
	sprintf(buf, "Videodromm Fps %c %d###fps", "|/-\\"[(int)(ui::GetTime() / 0.25f) & 3], (int)getAverageFps());
	ui::Begin(buf);
	{
		static bool read_only = false;

		size_t const MAX = 16384; // maximum number of chars
		static char mShaderText[MAX] =
			"uniform vec3 iResolution;\n"
			"uniform vec3 iColor;\n"
			"uniform float iGlobalTime;\n"
			"uniform sampler2D iChannel0;\n"
			"uniform sampler2D iChannel1;\n"
			"\n"
			"out vec4 oColor;\n"
			"void main(void) {\n"
			"\tvec2 uv = gl_FragCoord.xy / iResolution.xy;\n"
			"\tvec4 t0 = texture2D(iChannel0, uv);\n"
			"\tvec4 t1 = texture2D(iChannel1, uv);\n"
			"\toColor = vec4(t0.x, t1.y, cos(iGlobalTime), 1.0);\n"
			"}\n";
		if (mShaderTextToLoad) {
			mShaderTextToLoad = false;
			std::copy(mFboTextureFragmentShaderString.begin(), (mFboTextureFragmentShaderString.size() >= MAX ? mFboTextureFragmentShaderString.begin() + MAX : mFboTextureFragmentShaderString.end()), mShaderText);
		}
		ui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ui::Checkbox("Read-only", &read_only);
		ui::PopStyleVar();
		//if (ui::InputTextMultiline("##source", mShaderText, IM_ARRAYSIZE(mShaderText), ImVec2(-1.0f, ui::GetTextLineHeight() * 16), ImGuiInputTextFlags_AllowTabInput | (read_only ? ImGuiInputTextFlags_ReadOnly : 0))) {
		if (ui::InputTextMultiline("##source", mShaderText, IM_ARRAYSIZE(mShaderText), ImVec2(-1.0f, yPosRow2 - 140.0f), ImGuiInputTextFlags_AllowTabInput | (read_only ? ImGuiInputTextFlags_ReadOnly : 0))) {
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
#pragma region Info

	xPos = margin;
	ui::SetNextWindowSize(ImVec2(1000, 200), ImGuiSetCond_Once);
	ui::SetNextWindowPos(ImVec2(xPos, yPosRow3), ImGuiSetCond_Once);
	ui::Begin("Info");
	{
		ImGui::PushItemWidth(mVDSettings->mPreviewFboWidth);

		ImGui::RadioButton("Textures", &currentWindowRow2, 0); ImGui::SameLine();
		ImGui::RadioButton("Fbos", &currentWindowRow2, 1); ImGui::SameLine();
		ImGui::RadioButton("Shaders", &currentWindowRow2, 2); ImGui::SameLine();

		ImGui::RadioButton("Osc", &currentWindowRow3, 0); ImGui::SameLine();
		ImGui::RadioButton("Midi", &currentWindowRow3, 1); ImGui::SameLine();
		ImGui::RadioButton("Chn", &currentWindowRow3, 2); ui::SameLine();
		ImGui::RadioButton("Blend", &currentWindowRow3, 3); ui::SameLine();

		if (ui::Button("Save Params"))
		{
			// save params
			mVDSettings->save();
		}
		ui::SameLine();

		ui::Text("Msg: %s", mVDSettings->mMsg.c_str());
#pragma region Audio

		if (ui::Button("x##spdx")) { mVDSettings->iSpeedMultiplier = 1.0; }
		ui::SameLine();
		ui::SliderFloat("speed x", &mVDSettings->iSpeedMultiplier, 0.01f, 5.0f, "%.1f");
		ui::SameLine();
		ui::Text("Beat %d ", mVDSettings->iBeat);
		ui::SameLine();
		ui::Text("Beat Idx %d ", mVDAnimation->iBeatIndex);
		//ui::SameLine();
		//ui::Text("Bar %d ", mVDAnimation->iBar);
		ui::SameLine();

		if (ui::Button("x##bpbx")) { mVDSession->iBeatsPerBar = 1; }
		ui::SameLine();
		ui::SliderInt("beats per bar", &mVDSession->iBeatsPerBar, 1, 8);

		ui::SameLine();
		ui::Text("Time %.2f", mVDSettings->iGlobalTime);
		ui::SameLine();
		ui::Text("Trk %s %.2f", mVDSettings->mTrackName.c_str(), mVDSettings->liveMeter);
		//			ui::Checkbox("Playing", &mVDSettings->mIsPlaying);
		ui::SameLine();
		mVDSettings->iDebug ^= ui::Button("Debug");
		ui::SameLine();

		ui::Text("Tempo %.2f ", mVDSession->getBpm());
		ui::Text("Target FPS %.2f ", mVDSession->getTargetFps());
		ui::SameLine();
		if (ui::Button("Tap tempo")) { mVDAnimation->tapTempo(); }
		ui::SameLine();
		if (ui::Button("Time tempo")) { mVDAnimation->mUseTimeWithTempo = !mVDAnimation->mUseTimeWithTempo; }
		ui::SameLine();

		//void Batchass::setTimeFactor(const int &aTimeFactor)
		ui::SliderFloat("time x", &mVDAnimation->iTimeFactor, 0.0001f, 1.0f, "%.01f");
		ui::SameLine();

		static ImVector<float> timeValues; if (timeValues.empty()) { timeValues.resize(40); memset(&timeValues.front(), 0, timeValues.size()*sizeof(float)); }
		static int timeValues_offset = 0;
		// audio maxVolume
		static float tRefresh_time = -1.0f;
		if (ui::GetTime() > tRefresh_time + 1.0f / 20.0f)
		{
			tRefresh_time = ui::GetTime();
			timeValues[timeValues_offset] = mVDAnimation->maxVolume;
			timeValues_offset = (timeValues_offset + 1) % timeValues.size();
		}

		ui::SliderFloat("mult x", &mVDAnimation->controlValues[13], 0.01f, 40.0f);
		ui::SameLine();
		ui::PlotHistogram("Histogram", mVDAnimation->iFreqs, 7, 0, NULL, 0.0f, 255.0f, ImVec2(0, 30));// mMixes[0]->getSmallSpectrum()
		ui::SameLine();
		/*if (mVDSettings->iDebug) {
			CI_LOG_V("maxvol:" + toString(mVDUtils->formatFloat(mVDAnimation->maxVolume)) + " " + toString(mVDAnimation->maxVolume));
			}*/
		if (mVDAnimation->maxVolume > 240.0) ui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
		ui::PlotLines("Volume", &timeValues.front(), (int)timeValues.size(), timeValues_offset, toString(mVDUtils->formatFloat(mVDAnimation->maxVolume)).c_str(), 0.0f, 255.0f, ImVec2(0, 30));
		if (mVDAnimation->maxVolume > 240.0) ui::PopStyleColor();
		ui::SameLine();
		// fps
		static ImVector<float> values; if (values.empty()) { values.resize(100); memset(&values.front(), 0, values.size()*sizeof(float)); }
		static int values_offset = 0;
		static float refresh_time = -1.0f;
		if (ui::GetTime() > refresh_time + 1.0f / 6.0f)
		{
			refresh_time = ui::GetTime();
			values[values_offset] = mVDSettings->iFps;
			values_offset = (values_offset + 1) % values.size();
		}
		if (mVDSettings->iFps < 12.0) ui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
		ui::PlotLines("FPS", &values.front(), (int)values.size(), values_offset, mVDSettings->sFps.c_str(), 0.0f, mVDSession->getTargetFps(), ImVec2(0, 30));
		if (mVDSettings->iFps < 12.0) ui::PopStyleColor();


#pragma endregion Audio

		ui::PopItemWidth();
	}
	ui::End();
	xPos = margin + 1000;

#pragma endregion Info

	xPos = margin;
	showVDUI(currentWindowRow2);
}
// UI
void VideodrommLiveCodingApp::showVDUI(unsigned int window) {
	mVDUI->Run("UI", window);
}

CINDER_APP(VideodrommLiveCodingApp, RendererGl, &VideodrommLiveCodingApp::prepare)

