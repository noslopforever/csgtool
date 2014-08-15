#include <string.h>
#include "dbg.h"

#include "util.h"
#include "commands.h"
#include "mesh.h"
#include "bsp.h"
#include "bsp_mesh.h"
#include "export.h"

typedef bsp_node_t* (*bsp_binary_op)(bsp_node_t *, bsp_node_t *);

// A generalization of a binary CSG operation being performed on an STL
// in `path1` and `path1` defined by an operation `op` from` bsp.h
// Result is freshly allocated, and needs to be freed with `free_bsp_tree()`
bsp_node_t* bsp_binary_operation(char *path1, char *path2, bsp_binary_op op) {
	mesh_t *file1 = NULL;
	bsp_node_t *bsp1 = NULL;

	mesh_t *file2 = NULL;
	bsp_node_t *bsp2 = NULL;

	bsp_node_t *result = NULL;

	// Read 1
	file1 = mesh_read_file(path1);
	check(file1 != NULL, "Failed to read mesh from '%s'", path1);
	log_info("Loaded file: %s %d facets", path1, file1->poly_count(file1));
	bsp1 = mesh_to_bsp(file1);
	assert_mem(bsp1);

	// Read 2
	file2 = mesh_read_file(path2);
	check(file2 != NULL, "Failed to read mesh from '%s'", path2);
	log_info("Loaded file: %s %d facets", path2, file2->poly_count(file2));
	bsp2 = mesh_to_bsp(file2);
	assert_mem(bsp2);

	// Operate
	result = op(bsp1, bsp2);

	if(file1 != NULL) file1->destroy(file1);
	if(file2 != NULL) file2->destroy(file2);
	if(bsp1 != NULL) free_bsp_tree(bsp1);
	if(bsp2 != NULL) free_bsp_tree(bsp2);
	return result;
error:
	if(file1 != NULL) file1->destroy(file1);
	if(file2 != NULL) file2->destroy(file2);
	if(bsp1 != NULL) free_bsp_tree(bsp1);
	if(bsp2 != NULL) free_bsp_tree(bsp2);
	if(result != NULL) free_bsp_tree(result);
	return NULL;
}

// Constructor for commands named after the CSG functions they perform.
// Produces a function named `cmd_<name>(int argc, char **argv) that reads
// two files and an optional output path and calls a matching function
// bsp_<name>(bsp_node_t*,bsp_node_t*) and writes the resulting mesh
// to disk as either `./out.stl` or the value of argv[2]
// Uses the above `bsp_binary_operation(..)` wrapper to do most of the
// heavy lifting.
#define MAKE_CSG_COMMAND(name)                                                        \
int cmd_##name(int argc, char **argv) {                                               \
	bsp_node_t *result = NULL;                                                        \
	mesh_t *out = NULL;                                                               \
	char *out_path = "./out.stl";                                                     \
                                                                                      \
	check(argc >= 2, "At least two input files required.");                           \
	if(argc > 2) out_path = argv[2];                                                  \
                                                                                      \
	result = bsp_binary_operation(argv[0], argv[1], bsp_##name);                      \
	check(result != NULL, "Binary operation" #name "failed.");                        \
	out = NEW(bsp_mesh_t, "BSP", result);                                             \
    log_info("Writing output to %s", out_path);                                       \
	check(out->write(out, out_path, "STL") == 0, "Failed to write STL to %s", out_path); \
                                                                                      \
	out->destroy(out);                               				                  \
	return 0;                                                                         \
error:                                                                                \
	if(out != NULL) out->destroy(out);                                                \
	else if(result != NULL) free_bsp_tree(result);                                    \
	return -1;                                                                        \
}

// Each MAKE_BSP_COMMAND(name) results in a function named
// cmd_<name>(int argc, argv) which calls bsp_<name>() with
// two trees built from files in argv[0] and argv[1]
MAKE_CSG_COMMAND(intersect);
MAKE_CSG_COMMAND(union);
MAKE_CSG_COMMAND(subtract);

// Commands that exist only when built with `DEBUG` defined
// these generally don't do anything useful
#ifdef DEBUG
int cmd_DEBUG_bsp(int argc, char **argv) {
	const char *suffix = ".bsp.stl";
	char *name = argv[0];
	char *out_name = NULL;
	mesh_t *in = NULL;
	mesh_t *out = NULL;
	bsp_node_t *bsp = NULL;
	check(argc >= 1, "Too few args");
	assert_mem(out_name = calloc(strlen(name) + strlen(suffix) + 1, 1));
	check(sprintf(out_name, "%s%s", name, suffix) == (strlen(name) + strlen(suffix)), "Failed to build out name.");

	check(in = mesh_read_file(name), "Failed to READ");
	check(bsp = in->to_bsp(in), "Failed to BSP");
	check(out = bsp_to_mesh(bsp, 0), "Failed to BSP->mesh wrap");
	bsp = NULL; // Make it obvs that out now holds the ref

	log_info("Read: [%s]", argv[0]);
	log_info("Poly Count: [%d]", in->poly_count(in));
	log_info("BSP: [%p]", out);
	log_info("BSP Poly Count: [%d]", out->poly_count(out));
	log_info("Write: [%s](%d)", out_name, out->write(out, out_name, "STL"));

	if(out != NULL) out->destroy(out);
	if(in != NULL) in->destroy(in);
	if(out_name != NULL) free(out_name);
	return 0;
error:
	if(out != NULL) out->destroy(out);
	if(in != NULL) in->destroy(in);
	if(out_name != NULL) free(out_name);
	return -1;
}
#endif

// Available commands
const cmd_t commands[] = {
	{"intersect", "Intersect two geometries", cmd_intersect},
	{"subtract",  "Subtract two geometries",  cmd_subtract},
	{"union",     "Union two geometries",     cmd_union},

#ifdef DEBUG
	{"bsp",       "Identity through BSP",     cmd_DEBUG_bsp},
#endif

	{NULL, NULL, NULL}
};

// Search for a command by name.
cmd_fun_t cmd_find(const char *name) {
	for(cmd_t *c = (cmd_t*)commands; c->name != NULL; c++) {
		if(0 == strcmp(c->name, name)) return c->fun;
	}
	return NULL;
}
