#include "windows.h"
#include "glad/glad.h"
#include "glfw/include/GLFW/glfw3.h"
#include "linmath.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <fstream>
#include <algorithm>

#pragma comment(lib, "opengl32")
#pragma comment(lib, "glfw/lib-vc2019/glfw3dll.lib")

using namespace std;

GLFWwindow * g_window;

struct Entry {
    unsigned long long proc;
    unsigned long long thread;
    unsigned long long start;
    unsigned long long length;
    string name;
    Entry() {
      proc = thread = start = length = 0;
    }
};

struct MyColor {
    float r, g, b;
    MyColor() {}
    MyColor(GLubyte r_, GLubyte g_, GLubyte b_)
     : r(r_ / 255.0f), g(g_ / 255.0f), b(b_ / 255.0f) {}
};

std::vector<float> vertices;
std::vector<GLuint> triIndexes;
std::vector<GLuint> lineIndexes;
std::vector<MyColor> colA;
vector< float > rowpos;
vector< pair< unsigned long long, unsigned long long > > rowdata;
vector< vector< float > > procthr2coord;

std::vector<Entry> alltasks;
vector< vector< vector< Entry * > > > tasksperproc;

GLfloat m_pos[3];
int window_width = 0;
int window_height = 0;
int m_firstx = 0;
int m_firsty = 0;
int m_oldx = 0;
int m_oldy = 0;
bool drag = false;

float viewx0 = 0.0, viewx1 = 1.0;
float viewy0 = 0.0, viewy1 = 1.0;
float scalex = 0.9, scaley = 0.9;

bool selection = false;
float selectionx0, selectionx1, selectiony;
unsigned long long g_selrowidx = 0;
unsigned long long g_seltask = 0;
float g_seltime;
std::unordered_map<std::string, size_t> g_tasknames;

void getCoords(int x, int y, float &xx, float &yy) {
    xx = x/(float) window_width;
    yy = (window_height - y)/(float) window_height;

    xx *= (viewx1-viewx0)/scalex;
    yy *= (viewy1-viewy0)/scaley;
    xx += viewx0 - m_pos[0];
    yy += viewy0 - m_pos[1];
}

bool startsWith(const std::string &value1, const std::string &value2) {
    return value1.find(value2) == 0;
}

void getColor(Entry &e, MyColor &c) {

    MyColor cols[] = { 
        MyColor(128,  0,  0), MyColor(  0,128,  0), MyColor(  0,  0,128), 
        MyColor(128,128,  0), MyColor(128,  0,128), MyColor(  0,128,128),
        MyColor(128,128,128), MyColor(  0,  0,  0),
        MyColor(128, 80,  0), MyColor( 80,128,  0), MyColor(  0, 80,128), 
        MyColor(128,  0, 80), MyColor(  0,128, 80), MyColor( 80,  0,128), 
        MyColor(128,128, 80), MyColor(128, 80,128), MyColor( 80,128,128)
        
    };
    std::string s = e.name;
    size_t spos = s.find(' ');
    if (spos != std::string::npos)
        s = s.substr(0, spos);

    size_t idx = g_tasknames[s] % (sizeof(cols)/sizeof(MyColor));
    c = cols[idx];
}

void setup() {

    m_pos[0] = 0.0f;
    m_pos[1] = 0.0f;
    m_pos[2] = -1.0f;

    viewx0 = 0;
    viewy0 = 0;
    viewx1 = 0.0;
    viewy1 = 0.0;

    size_t row = 0;
	float rowheight = 1.0;
	float barheight = 0.8;
    const float proc_distance = 2.5;
	float extra_height = 0.0;

	int vertIndex = 0;

    procthr2coord.resize(tasksperproc.size());
    for (size_t proc = 0; proc < tasksperproc.size(); ++proc) {
        procthr2coord[proc].resize(tasksperproc[proc].size());
        for (size_t thread = 0; thread < tasksperproc[proc].size(); ++thread, ++row) {

			float y = extra_height + row * rowheight + 0.5;
			float y0 = y - barheight/2.0;
			float y1 = y + barheight/2.0;
            rowpos.push_back(y);
            rowdata.push_back(make_pair(proc, thread));
            procthr2coord[proc][thread] = y;

            for (size_t i = 0; i < tasksperproc[proc][thread].size(); ++i) {
				float start = ( tasksperproc[proc][thread][i]->start );
				float end = ( tasksperproc[proc][thread][i]->start + tasksperproc[proc][thread][i]->length );

                if (end > viewx1) viewx1 = end;
                if (y1 > viewy1)  viewy1 = y1;

                MyColor col;
                getColor(*tasksperproc[proc][thread][i],col);

				triIndexes.push_back(vertIndex + 0);
				triIndexes.push_back(vertIndex + 1);
				triIndexes.push_back(vertIndex + 2);

				lineIndexes.push_back(vertIndex + 0);
				lineIndexes.push_back(vertIndex + 1);
				lineIndexes.push_back(vertIndex + 1);
				lineIndexes.push_back(vertIndex + 2);
				lineIndexes.push_back(vertIndex + 2);
				lineIndexes.push_back(vertIndex + 0);

				vertices.push_back(start);
				vertices.push_back(y0);

				vertices.push_back(end);
				vertices.push_back((y0 + y1) / 2.0);

				vertices.push_back(start);
				vertices.push_back(y1);

				vertIndex += 3;

				colA.push_back(col);
                colA.push_back(col);
                colA.push_back(col);

            }
        }
        extra_height += proc_distance;
    }

    m_pos[0] = 0.05f * viewx1;
    m_pos[1] = 0.05f * viewy1;
}

void doPaint() {
    glClearColor(1,1,1,0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    glScaled(scalex, scaley, 0.0);
    glTranslated(m_pos[0], m_pos[1], m_pos[2]);

    if (selection) {
        glColor3ub(128, 255, 80);
        glRectd(selectionx0, selectiony-0.50, selectionx1, selectiony+0.50);
        glColor3ub(200, 200, 200);

		float xx, yy, yy2;
        getCoords(0, 0, xx, yy);
        getCoords(0, window_height, xx, yy2);

        glBegin(GL_LINES);
        glVertex3d(selectionx0, yy, 0.0);
        glVertex3d(selectionx0, yy2, 0.0);
        glVertex3d(selectionx1, yy, 0.0);
        glVertex3d(selectionx1, yy2, 0.0);
        glEnd();
    }

    //glBegin(GL_TRIANGLES);
    //for (size_t i = 0; i < vertices.size(); i += 3) {
    //    glColor3ub(colA[i].r, colA[i].g, colA[i].b);
    //    glVertex3d(vertices[i+0], verticesA[i+0].y, 0.0);
    //    glVertex3d(vertices[i+1], verticesA[i+1].y, 0.0);
    //    glVertex3d(vertices[i+2], verticesA[i+2].y, 0.0);
    //}
    //glEnd();

    //glBegin(GL_LINES);
    //for (size_t i = 0; i < verticesA.size(); i += 3) {
    //    glColor3ub(colA[i].r, colA[i].g, colA[i].b);
    //    glVertex3d(verticesA[i+0].x, verticesA[i+0].y, 0.0);
    //    glVertex3d(verticesA[i+1].x, verticesA[i+1].y, 0.0);
    //    glVertex3d(verticesA[i+1].x, verticesA[i+1].y, 0.0);
    //    glVertex3d(verticesA[i+2].x, verticesA[i+2].y, 0.0);
    //    glVertex3d(verticesA[i+2].x, verticesA[i+2].y, 0.0);
    //    glVertex3d(verticesA[i+0].x, verticesA[i+0].y, 0.0);
    //}
    //glEnd();

    // draw selection

    if (selection) {
        glColor3ub(128, 255, 80);
        glBegin(GL_LINES);
        glVertex3d(selectionx0, selectiony+.50, 0.0);
        glVertex3d(selectionx0, selectiony-.50, 0.0);
        glVertex3d(selectionx1, selectiony+.50, 0.0);
        glVertex3d(selectionx1, selectiony-.50, 0.0);
        glEnd();
    }

    glFlush();
    glfwSwapBuffers(g_window);
}

string format(unsigned long long a) {
    stringstream ss;
    ss << a;
    string r = ss.str();
    if (r.size() > 6)
        r = r.substr(0, r.size()-6) + "." + r.substr(r.size()-6);
    else {
        while (r.size() < 6)
            r = "0" + r;
        r = "0." + r;
    }
    return r;
}

void select_task(unsigned long long proc, unsigned long long thread, size_t pos) {
    vector<Entry *> &tasks(tasksperproc[proc][thread]);
    g_seltask = pos;
    selection = true;

    cout << "(" << proc << ", " << thread
            << ") [ " << format(tasks[pos]->start)
            << " " << format(tasks[pos]->start+tasks[pos]->length)
            << " = " << format(tasks[pos]->length) << " ]"
            << " name=[" << tasks[pos]->name << "]"
            << endl;

    selectiony = procthr2coord[proc][thread];
    selectionx0 = (float) tasks[pos]->start;
    selectionx1 = (float) (tasks[pos]->start + tasks[pos]->length);
    //paint();
}

void find_task(size_t rowidx, float time,
               unsigned long long &proc, unsigned long long &thread, size_t &pos) {
    if (rowdata.size() <= rowidx)
        return;

    proc = rowdata[rowidx].first;
    thread = rowdata[rowidx].second;

    vector<Entry *> &tasks(tasksperproc[proc][thread]);

    size_t a = 0;
    size_t b = tasks.size()-1;
    pos = (a+b)/2;

    while (b >= a) {
        pos = (a+b)/2;
        if (tasks[pos]->start > time) {
            if (pos == 0)
                break;
            b = pos-1;
            continue;
        }
        if (tasks[pos]->start + tasks[pos]->length < time) {
            a = pos+1;
            continue;
        }
        break;
    }
    if (pos > 0) {
        if (fabs(time - tasks[pos-1]->start + tasks[pos-1]->length) <
            fabs(time - tasks[pos]->start))
            pos = pos - 1;
    }
    if (pos < tasks.size()-1) {
        if (fabs(time - tasks[pos+1]->start) <
            fabs(time - (tasks[pos]->start+tasks[pos]->length)))
            pos = pos + 1;
    }
}

void mouse_release(int x, int y, int btn) {
    if (drag)
        return;
    if (btn != 1)
        return;

	float xx, yy;
    getCoords(x, y, xx, yy);

    size_t bestidx = 0;
	float best = fabs(rowpos[0] - yy);

    for (size_t i = 0; i < rowpos.size(); ++i) {
		float dist = fabs(rowpos[i] - yy);
        if (dist < best) {
            bestidx = i;
            best = dist;
        }
    }
    g_selrowidx = bestidx;

    unsigned long long proc;
    unsigned long long thread;
    size_t pos;
    find_task(g_selrowidx, xx, proc, thread, pos);

    g_seltask = pos;
    g_seltime = xx;

    select_task(proc, thread, pos);
}

void mouse_click(int x, int y, int btn) {
    m_firstx = x;
    m_firsty = y;
    m_oldx = x;
    m_oldy = y;
    drag = false;
}

void mouse_drag(int dx, int dy, int button) {

	float rx = ( (viewx1-viewx0) / (window_width) );
	float ry = ( (viewy1-viewy0) / (window_height) );

    if (button & 1) {
        m_pos[0] += dx*rx/scalex;
        m_pos[1] += dy*ry/scaley;
    }
    else if (button & 2) {
		float xx, yy;
        getCoords(m_firstx, m_firsty, xx, yy);

        scalex *= exp(dx/100.f);
        scaley *= exp(dy/100.f);
        if (scalex < 0.5) scalex = 0.5;
        if (scaley < 0.5) scaley = 0.5;

		float xx2, yy2;
        getCoords(m_firstx, m_firsty, xx2, yy2);
        m_pos[0] += xx2-xx;
        m_pos[1] += yy2-yy;
    }

	float xx, yy;
    getCoords(window_width/2, window_height/2, xx, yy);
    if (xx < viewx0) m_pos[0] += xx-viewx0;
    if (xx > viewx1) m_pos[0] -= viewx1-xx;
    if (yy < viewy0) m_pos[1] += yy-viewy0;
    if (yy > viewy1) m_pos[1] -= viewy1-yy;
}

void mouse_move(int x, int y, int btn) {
	if (btn == 0) {
		std::cout << "btn 0\n";
		return;
	}
    int dx = x - m_oldx;
    int dy = y - m_oldy;

	if (!drag && abs(x - m_firstx) < 3 && abs(y - m_firsty) < 3) {
		return;
	}

	drag = true;

    mouse_drag(dx, -dy, btn);
    m_oldx = x;
    m_oldy = y;
}

static void key_callback(GLFWwindow * window, int key, int scancode, int action, int mods) {
	if (action != 0) {
		return;
	}
    switch ( key ) {
		case GLFW_KEY_UP: {
			if (g_selrowidx + 1 >= rowdata.size())
				break;
			++g_selrowidx;
			unsigned long long proc;
			unsigned long long thread;
			size_t pos;
			find_task(g_selrowidx, (selectionx0 + selectionx1) / 2.0, proc, thread, pos);
			select_task(proc, thread, pos);
			break;
		}
		case GLFW_KEY_DOWN: {
			if (g_selrowidx == 0)
				break;
			--g_selrowidx;
			unsigned long long proc;
			unsigned long long thread;
			size_t pos;
			find_task(g_selrowidx, (selectionx0 + selectionx1) / 2.0, proc, thread, pos);
			select_task(proc, thread, pos);
			break;
		}
		case GLFW_KEY_LEFT: {
			unsigned long long proc = rowdata[g_selrowidx].first;
			unsigned long long thread = rowdata[g_selrowidx].second;
			if (g_seltask == 0)
				break;
			--g_seltask;
			select_task(proc, thread, g_seltask);
			break;
		}
		case GLFW_KEY_RIGHT: {
			unsigned long long proc = rowdata[g_selrowidx].first;
			unsigned long long thread = rowdata[g_selrowidx].second;
			vector<Entry *> &tasks(tasksperproc[proc][thread]);
			if (g_seltask+1 == tasks.size())
				break;
			++g_seltask;
			select_task(proc, thread, g_seltask);
			break;
		}
		case GLFW_KEY_KP_MULTIPLY:
			m_pos[0] = 0.05 * viewx1;
			m_pos[1] = 0.05 * viewy1;
			scalex = 0.9;
			scaley = 0.9;
			break;
		case GLFW_KEY_ESCAPE:
			exit(0);
    }
}

int glut_btn = 0;
bool pressed = false;
void mouse_button_callback(GLFWwindow * window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT) glut_btn = 1;
	else if (button == GLFW_MOUSE_BUTTON_RIGHT) glut_btn = 2;

	double x, y;
	glfwGetCursorPos(window, &x, &y);

	if (action == GLFW_PRESS) {
		mouse_click(x, y, glut_btn);
		pressed = true;
	}
	else if (action == GLFW_RELEASE) {
		mouse_release(x, y, glut_btn);
		pressed = false;
		glut_btn = 0;
	}

}

void cursor_position_callback(GLFWwindow * window, double x, double y) {
	if (pressed) {
		float xx, yy;
		getCoords(x, y, xx, yy);
		mouse_move(x, y, glut_btn);
	}
}

void split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim))
        elems.push_back(item);
}

static bool ReadMatch(const char *& ptr, const char * str) {
	const int n = strlen(str);
	if (strncmp(ptr, str, n) == 0) {
		ptr += n;
		return true;
	}
	return false;
};

static void ReadUntilNewline(const char *& ptr) {
	while (*ptr != '\n' && *ptr != '\r') {
		++ptr;
	}
};

static void ReadNewline(const char *& ptr) {
	while (*ptr == '\n' || *ptr == '\r') {
		++ptr;
	}
};

static void SkipLine(const char *& ptr) {
	ReadUntilNewline(ptr);
	ReadNewline(ptr);
};

static const char * Find(const char * ptr, const char * end, char c) {
	while (*ptr != c && ptr < end) {
		++ptr;
	}
	return ptr;
}

bool parse(const char *filename) {
    std::ifstream infile(filename, std::ios::binary | std::ios::ate);
	std::cout << filename << std::endl;
    if (infile.fail()) {
        std::cerr << "Read failed" << std::endl;
        return false;
    }

	std::streamsize size = infile.tellg();
	infile.seekg(0, std::ios::beg);

	std::vector<char> buffer(size);
	if (!infile.read(buffer.data(), size))
	{
		std::cerr << "Read failed" << std::endl;
		return false;
	}


	const char * ptr = buffer.data();
	const char * end = buffer.data() + size;

	int numLines = 0;
	for (const char * i = ptr; i < end; ++i) {
		if (*i == '\n') {
			++numLines;
		}
	}

	alltasks.reserve(numLines);

	if (ReadMatch(ptr, "LOG 2")) {
		SkipLine(ptr);
	}
	else if (ReadMatch(ptr, "LOG 3")) {
		SkipLine(ptr);
	}
	while (ptr < end) {
		if (*ptr == '#') {
			SkipLine(ptr);
			continue;
		}

		const char * sep = Find(ptr, end, ':');
		if (sep == end || sep[1] != ' ') {
			std::cerr << "Parse error!" << std::endl;
			return false;
		}

		Entry e;

		const char * sp = Find(ptr, sep, ' ');
		if (sp != sep) {
			e.proc = strtoull(ptr, NULL, 10);
			e.thread = strtoull(sp + 1, NULL, 10);
		}
		else {
			e.proc = 0;
			e.thread = strtoull(ptr, NULL, 10);
		}

		char * next;
		e.start = strtoull(sep + 2, &next, 10);
		if (*next != ' ') {
			std::cerr << "Parse error!" << std::endl;
			return false;
		}
		char * name;
		e.length = strtoull(next + 1, &name, 10);
		ptr = name;
		ReadUntilNewline(ptr);
		e.name = std::string((const char *) name + 1, ptr);

		alltasks.push_back(e);

		ReadNewline(ptr);
	}

    infile.close();

    if (alltasks.empty()) {
        std::cerr << "Empty log" << std::endl;
        return false;
    }

	// fix process ids
	std::sort(alltasks.begin(), alltasks.end(), [](const Entry & a, const Entry & b) -> bool {
		if (a.proc != b.proc) {
			return a.proc < b.proc;
		}
		if (a.thread != b.thread) {
			return a.thread < b.thread;
		}
		return a.start < b.start;
	});

	unsigned long long currentProc = alltasks[0].proc;
	unsigned long long currentThread = alltasks[0].thread;
	unsigned long long procCounter = 0;
	unsigned long long threadCounter = 0;
	tasksperproc.resize(1);
	tasksperproc[0].resize(1);
	tasksperproc[procCounter][threadCounter].push_back(&alltasks[0]);
	alltasks[0].proc = 0;
	alltasks[0].thread = 0;
	for (size_t i = 1; i < alltasks.size(); ++i) {
		if (alltasks[i].proc == currentProc) {
			alltasks[i].proc = procCounter;
			if (alltasks[i].thread == currentThread) {
				alltasks[i].thread = threadCounter;
			}
			else {
				currentThread = alltasks[i].thread;
				alltasks[i].thread = ++threadCounter;
				tasksperproc[procCounter].push_back(std::vector< Entry * >());
			}
		}
		else {
			currentProc = alltasks[i].proc;
			alltasks[i].proc = ++procCounter;
			currentThread = alltasks[i].thread;
			threadCounter = 0;
			alltasks[i].thread = threadCounter;
			tasksperproc.push_back(std::vector< std::vector< Entry * > >());
			tasksperproc[procCounter].push_back(std::vector< Entry * >());
		}
		tasksperproc[procCounter][threadCounter].push_back(&alltasks[i]);
	}

    // store all task names (for coloring later)
    for (size_t i = 0; i < alltasks.size(); ++i) {
        std::string s = alltasks[i].name;
        size_t spos = s.find(' ');
        if (spos != std::string::npos)
            s = s.substr(0, spos);
        if (g_tasknames.find(s) == g_tasknames.end()) {
            g_tasknames[s] = g_tasknames.size();
        }
    }

    // normalize start time
    size_t num_procs = procCounter + 1;

    std::vector<unsigned long long> starttimes(num_procs);
	for (int i = 0; i < tasksperproc.size(); ++i) {
		unsigned long long start = 0;
		bool first = true;
		for (int j = 0; j < tasksperproc[i].size(); ++j) {
			if (tasksperproc[i][j].empty()) {
				continue;
			}
			if (first || tasksperproc[i][j][0]->start < start) {
				first = false;
				start = tasksperproc[i][j][0]->start;
			}
		}
		starttimes[i] = start;
	}

    unsigned long long endtime = 0;
    unsigned long long totaltime = 0;
    for (size_t i = 0; i < alltasks.size(); ++i) {
        alltasks[i].start -= starttimes[alltasks[i].proc];
        if (alltasks[i].start + alltasks[i].length > endtime)
            endtime = alltasks[i].start + alltasks[i].length;
        totaltime += alltasks[i].length;
    }

    cout << "#tasks=" << alltasks.size()
            << " endtime=" << format(endtime)
            << " time=" << format(totaltime)
            << " parallelism=" << std::setprecision(3) << std::fixed
            << totaltime / (float)endtime
            << endl;

	return true;
}

static const char * vertex_shader_text =
"#version 400\n"
"uniform mat4 MVP;\n"
"attribute vec2 vPos;\n"
"in vec3 vCol;\n"
"out vec3 color;\n"
"void main()\n"
"{\n"
"    gl_Position = MVP * vec4(vPos.x, vPos.y, 0.0, 1.0);\n"
"    color = vCol;\n"
"}\n";

static const char * fragment_shader_text =
"#version 400\n"
"in vec3 color;"
"out vec4 gl_FragColor;"
"void main()\n"
"{\n"
"    gl_FragColor = vec4(color, 1.0);\n"
"}\n";

static void error_callback(int error, const char * description)
{
	fprintf(stderr, "Error: %s\n", description);
}

int main(int argc, char *argv[]) {

    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " [filename]" << std::endl;
        return 0;
    }

    if (!parse(argv[1]))
        return 1;

	if (!glfwInit()) {
		std::cerr << "Error initializing glfw" << std::endl;
		return 0;
	}

    setup();

	glfwWindowHint(GLFW_SAMPLES, 4);
	g_window = glfwCreateWindow(1024, 640, "Viewer", NULL, NULL); 
	glfwMakeContextCurrent(g_window);
	gladLoadGL();
	
	glfwSetKeyCallback(g_window, key_callback);
	glfwSetMouseButtonCallback(g_window, mouse_button_callback);
	glfwSetCursorPosCallback(g_window, cursor_position_callback);

	// vertex buffer

	GLuint vertex_buffer;
	glGenBuffers(1, &vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

	GLuint color_buffer;
	glGenBuffers(1, &color_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, color_buffer);
	glBufferData(GL_ARRAY_BUFFER, colA.size() * 3 * sizeof(float), colA.data(), GL_STATIC_DRAW);

	// vertex shader

	GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &vertex_shader_text, NULL);
	glCompileShader(vertex_shader);

	int  success;
	char infoLog[512];
	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(vertex_shader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
	}

	// fragment shader

	GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &fragment_shader_text, NULL);
	glCompileShader(fragment_shader);

	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(fragment_shader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
	}

	// compile

	GLuint program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(program, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
	}

	GLint mvp_location = glGetUniformLocation(program, "MVP");
	GLint vpos_location = glGetAttribLocation(program, "vPos");
	GLint col_location = glGetAttribLocation(program, "vCol");

	// vertex array object

	unsigned int vertex_array;
	glGenVertexArrays(1, &vertex_array);
	glBindVertexArray(vertex_array);
	glEnableVertexAttribArray(vpos_location);
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
	glVertexAttribPointer(vpos_location, 2, GL_FLOAT, GL_FALSE, 0, NULL);

	glEnableVertexAttribArray(col_location);
	glBindBuffer(GL_ARRAY_BUFFER, color_buffer);
	glVertexAttribPointer(col_location, 3, GL_FLOAT, GL_FALSE,	0, NULL);

	// element buffers

	GLuint tri_element_buffer;
	glGenBuffers(1, &tri_element_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tri_element_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, triIndexes.size() * sizeof(triIndexes[0]), triIndexes.data(), GL_STATIC_DRAW);

	GLuint line_element_buffer;
	glGenBuffers(1, &line_element_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, line_element_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, lineIndexes.size() * sizeof(lineIndexes[0]), lineIndexes.data(), GL_STATIC_DRAW);

	glFinish();

	glEnable(GL_MULTISAMPLE);

	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

	while (!glfwWindowShouldClose(g_window)) {
		glfwGetFramebufferSize(g_window, &window_width, &window_height);
		glViewport(0, 0, window_width, window_height);

		glClear(GL_COLOR_BUFFER_BIT);

		if (selection) {
			glColor3ub(128, 255, 80);
			glRectd(selectionx0, selectiony - 0.50, selectionx1, selectiony + 0.50);
			glColor3ub(200, 200, 200);

			float xx, yy, yy2;
			getCoords(0, 0, xx, yy);
			getCoords(0, window_height, xx, yy2);

			glBegin(GL_LINES);
			glVertex3d(selectionx0, yy, 0.0);
			glVertex3d(selectionx0, yy2, 0.0);
			glVertex3d(selectionx1, yy, 0.0);
			glVertex3d(selectionx1, yy2, 0.0);
			glEnd();
		}

		glUseProgram(program);
		mat4x4 m1;
		mat4x4 mvp;
		mat4x4_ortho(m1, viewx0, viewx1, viewy0, viewy1, -1.f, 1.f);
		mat4x4_scale_aniso(mvp, m1, scalex, scaley, 1.0f);
		mat4x4_translate_in_place(mvp, m_pos[0], m_pos[1], 0.f);
		glUniformMatrix4fv(mvp_location, 1, GL_FALSE, (const GLfloat *)mvp);

		glBindVertexArray(vertex_array);
		glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 2);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tri_element_buffer);
		glDrawElements(GL_TRIANGLES, triIndexes.size(), GL_UNSIGNED_INT, 0);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, line_element_buffer);
		glDrawElements(GL_LINES, lineIndexes.size(), GL_UNSIGNED_INT, 0);
		

		if (selection) {
			glColor3ub(128, 255, 80);
			glBegin(GL_LINES);
			glVertex3d(selectionx0, selectiony + .50, 0.0);
			glVertex3d(selectionx0, selectiony - .50, 0.0);
			glVertex3d(selectionx1, selectiony + .50, 0.0);
			glVertex3d(selectionx1, selectiony - .50, 0.0);
			glEnd();
		}


		glfwSwapBuffers(g_window);
		glfwPollEvents();
	}

	glfwDestroyWindow(g_window);
	glfwTerminate();
    return 0;
}
