#include "rendering.hpp"
#include "util.hpp"
#include "graph.hpp"
#include "camera.hpp"
#include <GLFW/glfw3.h>
#include <FTGL/ftgl.h>

namespace
{
	void glNormal(V3 const & v) { ::glNormal3d(v.x, v.y, v.z); }
	void glVertex(V3 const & v) { ::glVertex3d(v.x, v.y, v.z); }
	void glTranslate(V3 const & v) { ::glTranslated(v.x, v.y, v.z); }
	void glColor(V3 v) { ::glColor3d(v.x, v.y, v.z); }

	void pillar(V3 from, V3 to, double from_radius, double to_radius, unsigned faces)
	{
		V3 a = normalize(cross(to - from, V3{1,1,1} - from));
		V3 b = normalize(cross(to - from, a));

		double s = 2 * M_PI / faces;

		glBegin(GL_TRIANGLES);

		for (unsigned i = 0; i != faces; ++i)
		{
			glNormal(a * sin(i * s) + b * cos(i * s));
			glVertex(from + a * from_radius * std::sin( i    * s) + b * from_radius * std::cos( i    * s));
			glVertex(to   + a *   to_radius * std::sin( i    * s) + b *   to_radius * std::cos( i    * s));

			glNormal(a * sin((i + 1) * s) + b * cos((i + 1) * s));
			glVertex(from + a * from_radius * std::sin((i+1) * s) + b * from_radius * std::cos((i+1) * s));

			glVertex(from + a * from_radius * std::sin((i+1) * s) + b * from_radius * std::cos((i+1) * s));
			glVertex(to   + a *   to_radius * std::sin((i+1) * s) + b *   to_radius * std::cos((i+1) * s));

			glNormal(a * sin(i * s) + b * cos(i * s));
			glVertex(to   + a *   to_radius * std::sin( i    * s) + b *   to_radius * std::cos( i    * s));
		}

		glEnd();
	}

	void gluLookAt(V3 eye, V3 center, V3 up)
	{
		::gluLookAt(
			eye.x, eye.y, eye.z,
			center.x, center.y, center.z,
			up.x, up.y, up.z);
	}

	void grid(V3 const color)
	{
		glNormal3d(0, 1, 0);
		glColor3f(color.x, color.y, color.z);
		glLineWidth(2);

		glBegin(GL_LINES);
			for (double i = -4; i <= 4; ++i)
			{
				glVertex(V3{i/2, 0,  -2});
				glVertex(V3{i/2, 0,   2});
				glVertex(V3{ -2, 0, i/2});
				glVertex(V3{  2, 0, i/2});
			}
		glEnd();
	}

	void render(Viables const * const viables, Position const & pos,
		optional<PlayerJoint> const highlight_joint,
		optional<PlayerNum> const first_person_player, bool const edit_mode,
		Camera const & camera, Style const & style)
	{
		// draw limbs:

		for (PlayerNum p = 0; p != 2; ++p)
		{
			glColor(playerDefs[p].color);
			Player const & player = pos[p];

			foreach (s : segments())
				if (s.visible)
				{
					auto const a = s.ends[0], b = s.ends[1];

					if (b == Head && p == first_person_player) continue;

					auto mid = (player[a] + player[b]) / 2;
					pillar(player[a], mid, jointDefs[a].radius, s.midpointRadius, 30);
					pillar(mid, player[b], s.midpointRadius, jointDefs[b].radius, 30);
				}
		}

		// draw joints:

		foreach (pj : playerJoints)
		{
			if (pj.player == first_person_player && pj.joint == Head) continue;

			auto color = playerDefs[pj.player].color;
			bool highlight = pj == highlight_joint;
			double extraBig = highlight ? 0.01 : 0.005;

			if (edit_mode)
				color = highlight ? yellow : white;
			else if (!viables || (*viables)[pj].total_dist == 0)
				extraBig = 0;
			else
				color = highlight
					? white * 0.7 + color * 0.3
					: white * 0.4 + color * 0.6;

			glColor(color);

			static GLUquadricObj * Sphere = gluNewQuadric(); // todo: evil
			glPushMatrix();
				glTranslate(pos[pj]);
				gluSphere(Sphere, jointDefs[pj.joint].radius + extraBig, 20, 20);
			glPopMatrix();
		}
	}

	void drawViables(Graph const & graph, Viables const & viable, PlayerJoint const j, SeqNum const current_sequence,
		Camera const & camera, Style const & style, bool const edit_mode)
	{
		foreach (v : viable[j].viables)
		{

			if (v.second.end - v.second.begin < 1) continue;

			auto const r = v.second.reorientation;
			auto & seq = graph[v.first].positions;

			if (v.second.seqNum == current_sequence)
				glColor4f(1, 1, 1, 0.6);
			else
				glColor4f(1, 1, 0, 0.3);

			glDisable(GL_DEPTH_TEST);

			glBegin(GL_LINE_STRIP);
			for (PosNum i = v.second.begin; i != v.second.end; ++i) glVertex(apply(r, seq[i], j));
			glEnd();

			glPointSize(25);
			glBegin(GL_POINTS);
			for (PosNum i = v.second.begin; i != v.second.end; ++i)
				if (i == 0 || i == seq.size() - 1)
					glVertex(apply(r, seq[i], j));
			glEnd();
			glEnable(GL_DEPTH_TEST);

			if (edit_mode && v.second.seqNum == current_sequence)
				for (PosNum i = v.second.begin + 1; i != v.second.end; ++i)
					renderText(style.frameFont, world2screen(camera, apply(r, seq[i], j)), to_string(i));
		}
	}
}

Style::Style()
{
	if (frameFont.Error()) throw runtime_error("could not load font");
	if (sequenceFont.Error()) throw runtime_error("could not load font");

	frameFont.FaceSize(16);
	sequenceFont.FaceSize(64);
}

void renderText(FTGLBitmapFont const & font, V2 where, string const & text)
{
	const_cast<FTGLBitmapFont &>(font)
		.Render(text.c_str(), -1, FTPoint(where.x, where.y, 0));
}

void renderWindow(
	std::vector<View> const & views,
	Viables const * const viables,
	Graph const & graph,
	GLFWwindow * const window,
	Position const & position,
	Camera camera,
	optional<PlayerJoint> highlight_joint,
	bool const edit_mode,
	int const left, int const bottom,
	int const width, int const height,
	SeqNum const current_sequence,
	Style const & style)
{
	glEnable(GL_SCISSOR_TEST);

	foreach (v : views)
	{
		int
			x = left + v.x * width,
			y = bottom + v.y * height,
			w = v.w * width,
			h = v.h * height;

		glViewport(x, y, w, h);
		glScissor(x, y, w, h);

		camera.setViewportSize(v.fov, w, h);

		glClearColor(style.background_color.x, style.background_color.y, style.background_color.z, 0);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glEnable(GL_LIGHT0);
		glEnable(GL_LIGHTING);
		glEnable(GL_COLOR_MATERIAL);

		glMatrixMode(GL_PROJECTION);
		glLoadMatrixd(camera.projection().data());

		glMatrixMode(GL_MODELVIEW);

		if (v.first_person)
		{
			auto const & p = position[*v.first_person];

			glLoadIdentity();
			gluLookAt(p[Head], (p[LeftHand] + p[RightHand]) / 2., p[Head] - p[Neck]);
		}
		else
			glLoadMatrixd(camera.model_view().data());

		GLfloat light_diffuse[] = {0.5, 0.5, 0.5, 1.0};
		GLfloat light_ambient[] = {0.3, 0.3, 0.3, 0.0};
		GLfloat light_position[] = {1.0, 2.0, 1.0, 0.0};

		glLightfv(GL_LIGHT0, GL_AMBIENT, light_ambient);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
		glLightfv(GL_LIGHT0, GL_POSITION, light_position);

		glEnable(GL_DEPTH);
		glEnable(GL_DEPTH_TEST);

		grid(style.grid_color);
		render(viables, position, highlight_joint, v.first_person, edit_mode, camera, style);

		if (viables && highlight_joint)
		{
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glLineWidth(4);
			glNormal3d(0, 1, 0);
			drawViables(graph, *viables, *highlight_joint, current_sequence, camera, style, edit_mode);

			glDisable(GL_DEPTH_TEST);
			glPointSize(20);
			glColor(white);

			glBegin(GL_POINTS);
			glVertex(position[*highlight_joint]);
			glEnd();
		}
	}

/*
	string desc = graph[current_sequence].description.front();

	if (desc == "...") desc = "to " + graph[graph.to(current_sequence).node].description.front();
	desc = replace_all(desc, "\\n", " ");

	renderText(style.sequenceFont, V2{10,20}, desc);
*/
}
