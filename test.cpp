#define TDBASE_IMPLEMENTATION
#include "2dbase.h"


void main(){
	create_window(1024, 768, "hallo", {0, 0}, 1000);

	Vertex vertices[] = {{20, 20}, {20, 511}, {511, 20}, {511, 511}};
	Index indices[] = {0, 1, 2, 1, 3, 2};

	int vertex_count = sizeof(vertices) / sizeof(Vertex);
	int index_count = sizeof(indices) / sizeof(Index);

	while(!render_and_handle_input(vertex_count, vertices, index_count, indices)){}
}