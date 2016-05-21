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
		mMixes = VDMix::readSettings(loadFile(mMixesFilepath));
	}
	else {
		// otherwise create a texture from scratch
		mMixes.push_back(VDMix::create());
	}
	mVDAnimation->tapTempo();

	mVDUtils->getWindowsResolution();

	mVDSettings->iResolution.x = mVDSettings->mRenderWidth;
	mVDSettings->iResolution.y = mVDSettings->mRenderHeight;

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
	yPosRow1 = 100 + margin;
	yPosRow2 = yPosRow1 + largePreviewH + margin;
	yPosRow3 = yPosRow2 + h*1.4 + margin;

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
	try
	{
		fs::path fragFile = getAssetPath("") / "live.frag";
		if (fs::exists(fragFile)) {
			mFboTextureFragmentShaderString = loadString(loadAsset("live.frag"));
		}
		else
		{
			mError = "live.frag does not exist, should quit";
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
	/*mVDSettings->iChannelTime[0] = getElapsedSeconds();
	mVDSettings->iChannelTime[1] = getElapsedSeconds() - 1;
	mVDSettings->iChannelTime[2] = getElapsedSeconds() - 2;
	mVDSettings->iChannelTime[3] = getElapsedSeconds() - 3;
	//
	mVDSettings->iGlobalTime = getElapsedSeconds();*/

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
	//ui::Shutdown();
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
	aShader->uniform("iFreq0", mVDSettings->iFreqs[0]);
	aShader->uniform("iFreq1", mVDSettings->iFreqs[1]);
	aShader->uniform("iFreq2", mVDSettings->iFreqs[2]);
	aShader->uniform("iFreq3", mVDSettings->iFreqs[3]);
	aShader->uniform("iChannelTime", mVDSettings->iChannelTime, 4);
	aShader->uniform("iColor", vec3(mVDSettings->controlValues[1], mVDSettings->controlValues[2], mVDSettings->controlValues[3]));// mVDSettings->iColor);
	aShader->uniform("iBackgroundColor", vec3(mVDSettings->controlValues[5], mVDSettings->controlValues[6], mVDSettings->controlValues[7]));// mVDSettings->iBackgroundColor);
	aShader->uniform("iSteps", (int)mVDSettings->controlValues[20]);
	aShader->uniform("iRatio", mVDSettings->controlValues[11]);//check if needed: +1;//mVDSettings->iRatio); 
	aShader->uniform("width", 1);
	aShader->uniform("height", 1);
	aShader->uniform("iRenderXY", mVDSettings->mRenderXY);
	aShader->uniform("iZoom", mVDSettings->controlValues[22]);
	aShader->uniform("iAlpha", mVDSettings->controlValues[4] * mVDSettings->iAlpha);
	aShader->uniform("iBlendmode", mVDSettings->iBlendMode);
	aShader->uniform("iChromatic", mVDSettings->controlValues[10]);
	aShader->uniform("iRotationSpeed", mVDSettings->controlValues[19]);
	aShader->uniform("iCrossfade", mVDSettings->controlValues[18]);
	aShader->uniform("iPixelate", mVDSettings->controlValues[15]);
	aShader->uniform("iExposure", mVDSettings->controlValues[14]);
	aShader->uniform("iDeltaTime", 1.0f);// mVDAnimation->iDeltaTime);
	aShader->uniform("iFade", (int)mVDSettings->iFade);
	aShader->uniform("iToggle", (int)mVDSettings->controlValues[46]);
	aShader->uniform("iLight", (int)mVDSettings->iLight);
	aShader->uniform("iLightAuto", (int)mVDSettings->iLightAuto);
	aShader->uniform("iGreyScale", (int)mVDSettings->iGreyScale);
	aShader->uniform("iTransition", mVDSettings->iTransition);
	aShader->uniform("iAnim", mVDSettings->iAnim.value());
	aShader->uniform("iRepeat", (int)mVDSettings->iRepeat);
	aShader->uniform("iVignette", (int)mVDSettings->controlValues[47]);
	aShader->uniform("iInvert", (int)mVDSettings->controlValues[48]);
	aShader->uniform("iDebug", (int)mVDSettings->iDebug);
	aShader->uniform("iShowFps", (int)mVDSettings->iShowFps);
	aShader->uniform("iFps", mVDSettings->iFps);
	aShader->uniform("iTempoTime", 1.0f);// mVDAnimation->iTempoTime);
	aShader->uniform("iGlitch", (int)mVDSettings->controlValues[45]);
	aShader->uniform("iTrixels", mVDSettings->controlValues[16]);
	aShader->uniform("iGridSize", mVDSettings->controlValues[17]);
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

	mMixes[0]->getFboTexture(0)->bind(0);
	mMixes[0]->getFboTexture(1)->bind(1);

	gl::drawSolidRect(Rectf(0, 0, mVDSettings->mFboWidth, mVDSettings->mFboHeight));
	// stop drawing into the FBO
	mFbo->unbindFramebuffer();
	mMixes[0]->getFboTexture(1)->unbind();
	mMixes[0]->getFboTexture(0)->unbind();

	gl::clear(Color::black());
	gl::setMatricesWindow(toPixels(getWindowSize()));
	gl::draw(mFbo->getColorTexture());


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

#pragma region Info

	ui::SetNextWindowSize(ImVec2(1000, 100), ImGuiSetCond_Once);
	ui::SetNextWindowPos(ImVec2(xPos, margin), ImGuiSetCond_Once);
	sprintf(buf, "Videodromm Fps %c %d###fps", "|/-\\"[(int)(ImGui::GetTime() / 0.25f) & 3], (int)mVDSettings->iFps);
	ui::Begin(buf);
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
			timeValues[timeValues_offset] = mVDSettings->maxVolume;
			timeValues_offset = (timeValues_offset + 1) % timeValues.size();
		}

		ui::SliderFloat("mult x", &mVDSettings->controlValues[13], 0.01f, 10.0f);
		ui::SameLine();
		//ImGui::PlotHistogram("Histogram", mMixes[0]->getSmallSpectrum(), 7, 0, NULL, 0.0f, 255.0f, ImVec2(0, 30));
		//ui::SameLine();

		if (mVDSettings->maxVolume > 240.0) ui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
		ui::PlotLines("Volume", &timeValues.front(), (int)timeValues.size(), timeValues_offset, toString(mVDUtils->formatFloat(mVDSettings->maxVolume)).c_str(), 0.0f, 255.0f, ImVec2(0, 30));
		if (mVDSettings->maxVolume > 240.0) ui::PopStyleColor();
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
	switch (currentWindowRow2) {
	case 0:
		// textures
#pragma region textures
		for (int i = 0; i < mMixes[0]->getInputTexturesCount(0); i++) {
			ui::SetNextWindowSize(ImVec2(w, h*1.4));
			ui::SetNextWindowPos(ImVec2((i * (w + inBetween)) + margin, yPosRow2));
			ui::Begin(mMixes[0]->getInputTextureName(0, i).c_str(), NULL, ImVec2(0, 0), ui::GetStyle().Alpha, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
			{
				//BEGIN
				/*sprintf_s(buf, "WS##s%d", i);
				if (ui::Button(buf))
				{
				sprintf_s(buf, "IMG=%d.jpg", i);
				//mBatchass->wsWrite(buf);
				}
				if (ui::IsItemHovered()) ui::SetTooltip("Send texture file name via WebSockets");
				ui::SameLine();
				sprintf(buf, "FV##s%d", i);
				if (ui::Button(buf))
				{
				mVDTextures->flipTexture(i);
				}*/
				ui::PushID(i);
				ui::Image((void*)mMixes[0]->getFboInputTexture(0, i)->getId(), ivec2(mVDSettings->mPreviewFboWidth, mVDSettings->mPreviewFboHeight));
				ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(i / 7.0f, 0.6f, 0.6f));
				ui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(i / 7.0f, 0.7f, 0.7f));
				ui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(i / 7.0f, 0.8f, 0.8f));

				//if (ui::Button("Stop Load")) mVDImageSequences[0]->stopLoading();
				//ui::SameLine();

				/*if (mVDTextures->inputTextureIsSequence(i)) {
				if (!(mVDTextures->inputTextureIsLoadingFromDisk(i))) {
				ui::SameLine();
				sprintf_s(buf, "l##s%d", i);
				if (ui::Button(buf))
				{
				mVDTextures->inputTextureToggleLoadingFromDisk(i);
				}
				if (ui::IsItemHovered()) ui::SetTooltip("Pause loading from disk");
				}
				ui::SameLine();
				sprintf_s(buf, "p##s%d", i);
				if (ui::Button(buf))
				{
				mVDTextures->inputTexturePlayPauseSequence(i);
				}
				if (ui::IsItemHovered()) ui::SetTooltip("Play/Pause");
				ui::SameLine();
				sprintf_s(buf, "b##s%d", i);
				if (ui::Button(buf))
				{
				mVDTextures->inputTextureSyncToBeatSequence(i);
				}
				if (ui::IsItemHovered()) ui::SetTooltip("Sync to beat");
				ui::SameLine();
				sprintf_s(buf, "r##s%d", i);
				if (ui::Button(buf))
				{
				mVDTextures->inputTextureReverseSequence(i);
				}
				if (ui::IsItemHovered()) ui::SetTooltip("Reverse");
				playheadPositions[i] = mVDTextures->inputTextureGetPlayheadPosition(i);
				sprintf_s(buf, "p%d##s%d", playheadPositions[i], i);
				if (ui::Button(buf))
				{
				mVDTextures->inputTextureSetPlayheadPosition(i, playheadPositions[i]);
				}

				if (ui::SliderInt("scrub", &playheadPositions[i], 0, mVDTextures->inputTextureGetMaxFrame(i)))
				{
				mVDTextures->inputTextureSetPlayheadPosition(i, playheadPositions[i]);
				}
				speeds[i] = mVDTextures->inputTextureGetSpeed(i);
				if (ui::SliderInt("speed", &speeds[i], 0.0f, 6.0f))
				{
				mVDTextures->inputTextureSetSpeed(i, speeds[i]);
				}

				}*/

				//END
				ui::PopStyleColor(3);
				ui::PopID();
			}
			ui::End();
		}


#pragma endregion textures
		break;
	case 1:
		// Fbos
#pragma region fbos
		for (int i = 0; i < mMixes[0]->getFboCount(); i++)
		{
			ui::SetNextWindowSize(ImVec2(w, h));
			ui::SetNextWindowPos(ImVec2((i * (w + inBetween)) + margin, yPosRow2));
			ui::Begin(mMixes[0]->getFboLabel(i).c_str(), NULL, ImVec2(0, 0), ui::GetStyle().Alpha, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
			{

				//ui::PushID(i);
				ui::Image((void*)mMixes[0]->getFboTexture(i)->getId(), ivec2(mVDSettings->mPreviewFboWidth, mVDSettings->mPreviewFboHeight));
				/*ui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(i / 7.0f, 0.6f, 0.6f));
				ui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(i / 7.0f, 0.7f, 0.7f));
				ui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(i / 7.0f, 0.8f, 0.8f));

				sprintf_s(buf, "FV##fbo%d", i);
				if (ui::Button(buf)) mVDTextures->flipFboV(i);
				if (ui::IsItemHovered()) ui::SetTooltip("Flip vertically");

				ui::PopStyleColor(3);
				ui::PopID();*/
			}
			ui::End();
		}
#pragma endregion fbos
		break;
	}
	xPos = margin;

	ui::SetNextWindowPos(ImVec2(xPos, 500), ImGuiSetCond_Once);

	ui::SetNextWindowSize(ImVec2(620, 800), ImGuiSetCond_FirstUseEver);
	sprintf(buf, "Videodromm Fps %c %d###fps", "|/-\\"[(int)(ui::GetTime() / 0.25f) & 3], (int)getAverageFps());
	if (!ui::Begin(buf))
	{
		ui::End();
		return;
	}
	{
		static bool read_only = false;
		static char text[1024 * 16] =
			"/*\n"
			" Fragment shader.\n"
			"*/\n\n"
			"void main(void) {\n"
			//"\tvec2 uv = v_texcoord;\n"
			"\tgl_FragColor = vec4(1.0,0.0,0.0,1.0);\n"
			"}\n";

		ui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ui::Checkbox("Read-only", &read_only);
		ui::PopStyleVar();
		if (ui::InputTextMultiline("##source", text, IM_ARRAYSIZE(text), ImVec2(-1.0f, ui::GetTextLineHeight() * 16), ImGuiInputTextFlags_AllowTabInput | (read_only ? ImGuiInputTextFlags_ReadOnly : 0))) {
			// text changed
			CI_LOG_V("text changed");
			try
			{
				aShader = gl::GlslProg::create(mPassthruVextexShaderString, text);
				aShader->setLabel("live");
				CI_LOG_V("live.frag loaded and compiled");
				mFboTextureFragmentShaderString = text;
				stringstream sParams;
				sParams << "/*{ \"title\" : \"live\" }*/ " << mFboTextureFragmentShaderString;
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
	if (ui::Begin("ImGui Metrics"))
	{
		ui::Text("ImGui %s", ui::GetVersion());
		ui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ui::GetIO().Framerate, ui::GetIO().Framerate);
		ui::Text("%d vertices, %d indices (%d triangles)", ui::GetIO().MetricsRenderVertices, ui::GetIO().MetricsRenderIndices, ui::GetIO().MetricsRenderIndices / 3);
		ui::Text("%d allocations", ui::GetIO().MetricsAllocs);
	}
	ui::End();
}

CINDER_APP(VideodrommLiveCodingApp, RendererGl, &VideodrommLiveCodingApp::prepare)
