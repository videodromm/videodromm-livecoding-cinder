#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

#include "VDConsole.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace VideoDromm;

class VideodrommLiveCodingApp : public App {

public:

	void setup() override;
	void mouseDown(MouseEvent event) override;
	void update() override;
	void draw() override;
	void cleanup() override;
private:

};