#include <SDL/SDL.h>
#include <GL/gl.h>
#include <cstdio>
#include <vector>

#include "view.h"

struct App {
	View* mainView;
	View* activeView;
	std::vector<View*> views;
} app;



void mainLoop();

int main(int argc, char* argv[]) {
	printf(
		"bvh-browser (c) Sam Gynn\n"
		"http://sam.draknek.org/projects/bvh-browser\n"
		"Distributed under GPL\n");
	
	
	// Parse arguments
	for(int i=1; i<argc; ++i) {
		// valid: bvh, zip, directory

	}


	// setup SDL window
	int width = 1280;
	int height = 1024;

	int r = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
	if(r<0) {
		fprintf(stderr, "Unable to initialise SDL: %s\n", SDL_GetError());
		return 1;
	}

	atexit(SDL_Quit);
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1);
	if(!SDL_SetVideoMode( width, height, 32, SDL_OPENGL)) {
		fprintf(stderr, "Unable to create window: %s\n", SDL_GetError());
		SDL_Quit();
		return 2;
	}

	SDL_WM_SetCaption("bvh-viewer", 0);

	glEnable(GL_DEPTH_TEST);

	// Set up views
	app.mainView = new View(0,0,width,height);
	app.activeView = app.mainView;
	app.views.push_back(app.mainView);

	app.mainView->loadFile(argc<2? "test1.bvh": argv[1]);

	mainLoop();

	return 0;

}


void mainLoop() {
	bool running = true;
	SDL_Event event;
	uint ticks, lticks;
	ticks = lticks = SDL_GetTicks();
	bool rotate = false;

	while(running) {
		if(SDL_PollEvent(&event)) {
			switch(event.type) {
			case SDL_QUIT:
				running = false;
				break;
			case SDL_VIDEORESIZE:
				break;

			case SDL_ACTIVEEVENT:
				break;

			case SDL_MOUSEBUTTONDOWN:
				if(event.button.button == SDL_BUTTON_WHEELUP) app.activeView->zoomView(0.9);
				if(event.button.button == SDL_BUTTON_WHEELDOWN) app.activeView->zoomView(1.1);
				rotate = true;
				break;

			case SDL_MOUSEBUTTONUP:
				rotate = false;
				break;

			case SDL_KEYDOWN:
				if(event.key.keysym.sym == SDLK_z) app.activeView->autoZoom();
				if(event.key.keysym.sym == SDLK_SPACE) app.activeView->togglePause();

			case SDL_KEYUP:
				if(event.key.keysym.sym == SDLK_ESCAPE) running = false;
				break;

			default:
				break;
			}
		}

		else {
			// Rotation
			int mx, my;
			SDL_GetRelativeMouseState(&mx, &my);
			if(rotate) {
				app.activeView->rotateView(-mx*0.01, my*0.01);
			}
			

			
			// Update all views
			lticks = ticks;
			ticks = SDL_GetTicks();
			float time = (ticks - lticks) * 0.001; // ticks in miliseconds

			for(size_t i=0; i<app.views.size(); ++i) {
				app.views[i]->update(time);
			}

			// Render everything
			for(size_t i=0; i<app.views.size(); ++i) {
				app.views[i]->render();
			}

			// Limit to 60fps?
			uint t = ticks - lticks;
			if(t < 10) SDL_Delay(10 - t);

			SDL_GL_SwapBuffers();
		}
	}
}

