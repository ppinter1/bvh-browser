#include "view.h"
#include <SDL_opengl.h>
#include <SDL_ttf.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

View::View (int x, int y, int w, int h) : m_x(x), m_y(y), m_width(w), m_height(h), 
										  m_tx(x), m_ty(y), m_twidth(w), m_theight(h),
										  m_visible(false), m_paused(false), m_state(EMPTY),
										  m_text(0), m_bvh(0), m_name(0) {
	m_near 	= 0.1f;
	m_far 	= 1000.f;
	m_frame = 0;
	updateProjection();

	m_camera = BVH_Math::BVH_Math::vec3 (60, 60, 60);
	updateCamera();
}

View::~View() {}

void View::setBVH (BVH* bvh, const char* name) {

	if (m_bvh) {

		delete m_bvh;
		delete [] m_final;
		if (m_name) free (m_name);
		m_name = 0;
	}

	m_bvh 	= bvh;
	m_frame = 0;

	if (bvh) {

		m_name 	= strdup (name);
		m_final = new BVH_Math::Transform[m_bvh->getPartCount()];
		updateBones(0);
	}
}

TTF_Font* staticFont = 0;

void View::setFont (const char* fontName, int size) {

	if (fontName) {

		if (!TTF_WasInit()) TTF_Init();
		staticFont = TTF_OpenFont (fontName, size);
		if (!staticFont) printf ("Failed to load font %s\n", fontName);
	
	} else TTF_CloseFont (staticFont);
}

void View::setText (const char* text) {

	if (!staticFont) text = 0;
	if (text == 0 && m_text == 0) glDeleteTextures(1, &m_text);
	else if (text) {

		if (!m_text) glGenTextures (1, &m_text);
		glBindTexture (GL_TEXTURE_2D, m_text);

		SDL_Colour colour;							// Create text image
		colour.r = colour.g = colour.b = 255;
		SDL_Surface* s = TTF_RenderText_Blended (staticFont, text, colour);

		m_textWidth  = s->pitch / 4;
		m_textHeight = s->h;

		glTexImage2D	(GL_TEXTURE_2D, 0, GL_RGBA, m_textWidth, s->h, 0, GL_BGRA, GL_UNSIGNED_BYTE, s->pixels);
		glTexParameteri	(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri	(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		SDL_FreeSurface	(s);
	}
}

void View::setVisible (bool v) {	m_visible = v; }

bool View::isVisible() const { return m_visible; }

void View::resize (int x, int y, int w, int h, bool smooth) {

	if (!smooth) {

		m_x = x;
		m_y = y;
		m_width = w;
		m_height = h;
	}

	m_tx = x;
	m_ty = y;
	m_twidth = w;
	m_theight = h;

	updateProjection();
}

void View::move (int x, int y) {

	m_x += x;
	m_y += y;
	m_tx += x;
	m_ty += y;
}

bool View::contains (int x, int y) { return m_visible && x>=m_x && y>=m_y && x <= m_x+m_width && y <= m_y+m_height; }

void View::setCamera (float yaw, float pitch, float zoom) {

	BVH_Math::vec3 d;
	float cp = cos (pitch);
	d.x = sin (yaw) * cp;
	d.y = sin (pitch);
	d.z = cos (yaw) * cp;
	m_camera = m_target + d * zoom;
	updateCamera();
}

void View::rotateView (float yaw, float pitch) {

	BVH_Math::vec3 d = m_camera - m_target;
	float zoom = d.length();
	float oldYaw = atan2 (d.x, d.z);
	float oldPitch = atan2 (d.y, sqrt(d.x*d.x+d.z*d.z));
	setCamera (oldYaw+yaw, oldPitch+pitch, zoom);
}

void View::zoomView (float mult) {

	BVH_Math::vec3 d = m_camera - m_target;
	m_camera = m_target + d * mult;
	updateCamera();
}

inline float View::zoomToFit (const BVH_Math::vec3& point, const BVH_Math::vec3& dir, const BVH_Math::vec3* n, float* d) {

	float shift = 0;

	for (int i=0; i<4; ++i) {

		float denom = n[i].dot(dir);				// Distance to plane in dir
		float t = d[i] - n[i].dot (point) / denom;

		if (t>shift) shift = t;
	}

	if (shift > 0) {								// update frustum

		m_camera = m_camera - dir * shift;

		for (int i=0; i<4; ++i) d[i] = n[i].dot (m_camera);
	}

	return shift;
}

void View::autoZoom() {

	if (!m_bvh) return;

	BVH_Math::vec3 dir = m_target - m_camera;
	dir.normalise();
	m_camera = m_target;

	BVH_Math::vec3 n[4];
	float d[4];
	float m[16];

	BVH_Math::multMatrix (m_projectionMatrix, m_viewMatrix, m);		// Get frustum planes

	n[0] = BVH_Math::vec3 (m[3]+m[0], m[7]+m[4], m[11]+m[8]);
	n[1] = BVH_Math::vec3 (m[3]-m[0], m[7]-m[4], m[11]-m[8]);
	n[2] = BVH_Math::vec3 (m[3]+m[1], m[7]+m[5], m[11]+m[9]);
	n[3] = BVH_Math::vec3 (m[3]-m[1], m[7]-m[5], m[11]-m[9]);

	for (int i=0; i<4; ++i) d[i] = n[i].dot (m_camera);

	float shift = 0;												// Zoom out to fit points

	for (int i=0; i<m_bvh->getPartCount(); ++i) {

		shift += zoomToFit (m_final[i].offset, dir, n, d);
	}

	for (int i=0; i<m_bvh->getFrames(); ++i) {

		shift += zoomToFit (m_bvh->getPart(0)->motion[i].offset, dir, n, d);
	}

	if (shift == 0) m_camera = m_target - dir;

	updateCamera();
}

void View::togglePause() { m_paused = !m_paused; }

void View::setState (State s) { m_state = s; }

View::State View::getState() const { return m_state; }

void View::update (float time) {

	if (m_tx != m_x || m_twidth != m_width) {

		const float speed = 8000 * time;
		int dx = m_tx - m_x;
		int dy = m_ty - m_y;
		int dw = m_twidth - m_width;
		int dh = m_theight - m_height;
		int max = 0;

		if (abs (dx) > max) max = abs (dx);
		if (abs (dy) > max) max = abs (dy);
		if (abs (dw) > max) max = abs (dw);
		if (abs (dh) > max) max = abs (dh);

		#define lerp(a,b,t) (a + (b-a)*t)

		float t = max < speed? 1.0: speed / max;

		m_x = lerp (m_x, m_tx, t);
		m_y = lerp (m_y, m_ty, t);
		m_width  = lerp (m_width, m_twidth,   t);
		m_height = lerp (m_height, m_theight, t);

		updateProjection();
	}
	
	if (m_bvh && !m_paused && m_visible) {

		m_frame += time / m_bvh->getFrameTime();

		if (m_frame > m_bvh->getFrames()) m_frame = 0;

		updateBones(m_frame);
	}
}

void View::render() const {

	if (!m_visible) return;

	glViewport		(m_x, m_y, m_width, m_height);
	glMatrixMode	(GL_PROJECTION);
	glLoadMatrixf	(m_projectionMatrix);
	glMatrixMode	(GL_MODELVIEW);
	glLoadMatrixf	(m_viewMatrix);
	glTranslatef	(-m_camera.x, -m_camera.y, -m_camera.z);


	glEnableClientState	(GL_VERTEX_ARRAY);
	glPushMatrix		();
	glRotatef			(90, 1,0,0);
	glScalef			(10,10,10);
	drawGrid			();
	glPopMatrix			();

	if (m_bvh) {											// Draw skeleton

		glEnable		(GL_POLYGON_OFFSET_LINE);
		glPolygonOffset	(-1,-1);

		float matrix[16];

		for (int i=0; i<m_bvh->getPartCount(); ++i) {

			m_final[i].toMatrix (matrix);

			glPushMatrix();
			glMultMatrixf(matrix);

			const BVH_Math::vec3 zAxis (0,0,1);				// Rotate to use z axis mesh
			BVH_Math::vec3 dir = m_bvh->getPart(i)->end;
			float length = dir.length();
			dir *= 1.0 / length;

			if (dir.z < 0.999) {

				BVH_Math::vec3 n = dir.cross(zAxis);
				float d = dir.dot(zAxis);
				glRotatef( -acos(d) * 180/3.141592653592, n.x, n.y, n.z );
			}

			glScalef		(length, length, length);

			glPolygonMode	(GL_FRONT, GL_LINE);			// Draw bone mesh
			glColor4f		(0.2, 0, 0.5, 1);
			drawBone		();
			glPolygonMode	(GL_FRONT, GL_FILL);
			glColor4f		(0.5, 0, 1, 1);
			drawBone		();
			glPopMatrix		();
		}
	}

	glLoadIdentity	();										// Border?
	glMatrixMode	(GL_PROJECTION);
	glLoadIdentity	();
	glColor4f		(0.3, 0.3, 0.3, 1);

	static const float border[] = { -1,-1, 1,-1, 1,1, -1,1, -1,-1 };

	glVertexPointer	(2, GL_FLOAT, 0, border);
	glDrawArrays	(GL_LINE_STRIP, 0, 5);

	if (m_text) {											// Text

		glEnable			(GL_TEXTURE_2D);
		glEnable			(GL_BLEND);
		glBlendFunc			(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glBindTexture		(GL_TEXTURE_2D, m_text);
		glEnableClientState	(GL_TEXTURE_COORD_ARRAY);
		glColor4f			(1,1,1,1);

		float w = m_textWidth * 2.0 / m_width - 1;
		float h = m_textHeight * 2.0 / m_height - 1;
		float box[] = { -1, -1, -1, h, w, -1,w, h };
		float tex[] = { 0,1, 0,0, 1,1, 1,0 };

		glVertexPointer		(2, GL_FLOAT, 0, box);
		glTexCoordPointer	(2, GL_FLOAT, 0, tex);
		glDrawArrays		(GL_TRIANGLE_STRIP, 0, 4);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisable			(GL_TEXTURE_2D);
	}

	glDisableClientState	(GL_VERTEX_ARRAY);
}

// ------------------------------------------------- //

void View::updateBones (float frame) {

	int f 	= floor (frame);
	float t = frame - f;

	if (f >= m_bvh->getFrames()-1) {

		f = m_bvh->getFrames()-1;
		t = 0.f;
	}

	BVH_Math::Transform local;

	for (int i=0; i<m_bvh->getPartCount(); ++i) {

		const BVH::Part* part = m_bvh->getPart(i);

		if (t > 0) {

			local.offset = lerp (part->motion[f].offset, part->motion[f+1].offset, t);
			local.rotation = BVH_Math::slerp (part->motion[f].rotation, part->motion[f+1].rotation, t);

		} else {
			local = part->motion[f];
		}

		if (part->parent>=0) {

			local.offset = part->offset; 			// ?

			const BVH_Math::Transform &parent = m_final[part->parent];

			m_final[i].offset   = parent.offset + parent.rotation * local.offset;
			m_final[i].rotation = parent.rotation * local.rotation;

		} else {
			m_final[i] = local;
		}
	}
}

// ----------------------------------------------- //

void View::updateCamera() {

	const BVH_Math::vec3 up (0,1,0);

	BVH_Math::vec3 z = m_camera - m_target;
	z.normalise();

	BVH_Math::vec3 x = up.cross (z);
	x.normalise();

	BVH_Math::vec3 y = z.cross (x);
	y.normalise();
	
	float* m = m_viewMatrix;

	m[3]  = m[7] = m[11] = 0;
	m[0]  = x.x; m[4] = x.y; m[8]  = x.z;
	m[1]  = y.x; m[5] = y.y; m[9]  = y.z;
	m[2]  = z.x; m[6] = z.y; m[10] = z.z;
	m[12] = 0; m[13] = 0; m[14] = 0; m[15] = 1;
}

void View::updateProjection (float fov) {

	fov *= 3.141592653592f / 180.f;

	float aspect = (float) m_width / m_height;
	float f = 1.0f / tan(fov / 2.f);
	float* m = m_projectionMatrix;

	m[1]=m[2]=m[3]=m[4]=m[6]=m[7]=m[8]=m[9]=m[12]=m[13]=m[15] = 0.f;
	m[0] = f / aspect;
	m[5] = f;
	m[10] = (m_far + m_near) / (m_near - m_far);
	m[11] = -1.f;
	m[14] = (2.f * m_far * m_near) / (m_near - m_far);
}

void View::drawGrid() {

	struct GridVertex { float x, y; int c; };
	static GridVertex* data = 0;

	int lines 	= 15;
	int colour 	= 0x202020;
	int xaxis 	= 0x005000;
	int yaxis 	= 0x000050;

	if (!data) {

		data = new GridVertex[lines * 4];

		int k 	= 0;
		float s = lines / 2;
		float t = -s;

		for (int i=0; i<lines; ++i) {

			int c = i==lines/2? xaxis: colour;

			data[k].x = s;
			data[k].y = t;
			data[k].c = c;

			data[k+1].x = -s;
			data[k+1].y = t;
			data[k+1].c = c;

			k += 2;
			t += 1;
		}

		t = -s;
		
		for (int i=0; i<lines; ++i) {

			int c = i==lines/2? yaxis: colour;

			data[k].x = t;
			data[k].y = s;
			data[k].c = c;

			data[k+1].x = t;
			data[k+1].y = -s;
			data[k+1].c = c;

			k += 2;
			t += 1;
		}
	}

	glEnableClientState	(GL_COLOR_ARRAY);							// Draw it
	glVertexPointer		(2, GL_FLOAT, sizeof(GridVertex), data);
	glColorPointer		(3, GL_UNSIGNED_BYTE, sizeof(GridVertex), &data[0].c);
	glDrawArrays		(GL_LINES, 0, lines*4);
	glDisableClientState(GL_COLOR_ARRAY);
}

void View::drawBone() {

	static float 		 vx[18] = { 0,0,0,  .06,.06,.1,  .06,-.06,0.1,  -.06,-.06,.1, -.06,.06,.1,  0,0,1 };
	static unsigned char ix[24] = { 0,1,2, 0,2,3, 0,3,4, 0,4,1,  1,5,2, 2,5,3, 3,5,4, 4,5,1 };

	glVertexPointer		(3, GL_FLOAT, 0, vx);
	glDrawElements		(GL_TRIANGLES, 24, GL_UNSIGNED_BYTE, ix);
}
