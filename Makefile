all: gravity

%: %.cpp
	c++ -Wall -std=c++11 -lglfw3 -lGLEW -framework OpenGL -o $@ $^
