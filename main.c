#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>


// Using this probably isn't the best idea.
#define PATH_BUFFER_LENGTH (FILENAME_MAX + 1)
#define OPTIONS_MAX_LENGTH 4
#define LINE_MAX_LENGTH    1024
#define TOKEN_MAX_LENGTH   256


// Stores command line arguments.
typedef struct {
	// We never have to malloc this,
	// it should always be specified.
	char *input_path;
	char *output_path;
	// If no options are specified, this
	// will just store a null teminator
	char options[OPTIONS_MAX_LENGTH];
} cmd_args;


typedef struct macro macro;
// Compiler macros are stored in a linked list.
typedef struct macro {
	char *name;
	size_t name_len;
	char *value;
	size_t value_len;

	macro *next;
} macro;

typedef enum {
	shader_none = 0,
	shader_vertex = 1,
	shader_pixel = 2
} shader_type;

// Store all the information for a file and our progress
// reading it. We mostly use this so that we don't need
// to pass a billion different variables into functions
// for use with error reporting.
typedef struct {
	const char *path;
	FILE *fp;

	// Current line information.
	char line[LINE_MAX_LENGTH];
	size_t line_len;
	size_t line_num;
} file_data;


// Read the program's command line arguments.
int arguments_read(cmd_args *const args, const int argc, char **argv){
	int i;

	// Initialize the arguments structure.
	args->input_path  = NULL;
	args->output_path = NULL;
	args->options[0]  = '\0';

	// Skip argument zero, since that's the execution path.
	for(i = 1; i < argc; ++i){
		char *cur_arg = argv[i];

		// Add the header option if it was specified.
		if(cur_arg[0] == '-' && strchr(&cur_arg[1], 'h')){
			strcpy(args->options, "-h ");

		// Otherwise, interpret the argument as a path.
		}else if(args->input_path == NULL){
			if(access(cur_arg, F_OK) == 0){
				args->input_path = cur_arg;
			}else{
				printf("Input file at \"%s\" does not exist!\n", cur_arg);
				return(0);
			}
		}else if(args->output_path == NULL){
			args->output_path = cur_arg;
		}
	}

	// The only thing we really need
	// to specify is the input path.
	return(args->input_path != NULL);
}

// Read through the file and return whether
// the shader is a vertex or pixel shader.
shader_type get_shader_type(const char *const input_path){
	shader_type type = shader_none;

	// Open the shader file.
	FILE *const input_file = fopen(input_path, "r");
	if(input_file == NULL){
		printf("Could not open input file at \"%s\".\n", input_path);
	}else{
		char line[LINE_MAX_LENGTH];

		// Read through the input file, looking for the shader type.
		while(fgets(line, LINE_MAX_LENGTH, input_file) != NULL){
			// Check which type of shader we're looking at.
			if(strncmp(line, "vs", 2) == 0){
				type = shader_vertex;
				break;
			}else if(strncmp(line, "ps", 2) == 0){
				type = shader_pixel;
				break;
			}
		}

		fclose(input_file);
	}

	return(type);
}

// Generate the output path of the shader compiler.
int get_compile_path(
	char *compile_path, const cmd_args *const args, const shader_type type
){

	char *start_pos;
	char *ext_pos;
	size_t compile_path_len;

	// Begin with the input filename.
	strcpy(compile_path, args->input_path);
	start_pos = basename(compile_path);
	compile_path_len = strlen(start_pos);
	memmove(compile_path, start_pos, compile_path_len + 1);

	ext_pos = strrchr(compile_path, '.');
	// Figure out where to put the new file extension.
	if(ext_pos == NULL){
		ext_pos = &compile_path[compile_path_len];
	}else{
		compile_path_len = ext_pos - compile_path;
	}

	// Add the file extension. If we're not compiling
	// to a header, this will depend on the shader type.
	if(args->options[0] == '\0'){
		// Make sure the new filename isn't too long.
		if(compile_path_len + 4 > FILENAME_MAX){
			puts(
				"Unable to create temporary file: "
				"input path too long."
			);
			return(1);
		}
		switch(type){
			case shader_none:
				puts(
					"Unable to create temporary file: "
					"invalid shader type."
				);
				return(1);
			break;
			case shader_vertex:
				strcpy(&compile_path[compile_path_len], ".vso");
			break;
			case shader_pixel:
				strcpy(&compile_path[compile_path_len], ".pso");
			break;
		}
	}else{
		// Make sure the new filename isn't too long.
		if(compile_path_len + 2 > FILENAME_MAX){
			puts("Unable to create temporary file: input path too long.");
			return(1);
		}
		strcpy(&compile_path[compile_path_len], ".h");
	}

	return(0);
}


void print_file_error(const file_data *const fd, const char *const error){
	printf("%s(%u)> %s\n", fd->path, fd->line_num, error);
}

int macro_process(const file_data *const fd, macro **list);
// Open and read through a header, adding any macros we encounter.
// Here, "list" points to the head of the global macro list.
int macro_include_header(const file_data *const fd, macro **list){
	char old_dir[PATH_BUFFER_LENGTH];
	char new_dir[PATH_BUFFER_LENGTH];
	char include_path[TOKEN_MAX_LENGTH];
	file_data include;
	int success = 1;

	// Get the current working directory.
	if(getcwd(old_dir, sizeof(old_dir)) == NULL){
		perror("Error calling getcwd()");
	}
	// Headers are usually defined relative to the
	// file they're included in, so we should switch
	// to the directory that our shader file is in.
	strcpy(new_dir, fd->path);
	if(chdir(dirname(new_dir)) == -1){
		printf("Failed to change to subdirectory \"%s\".\n", new_dir);
	}

	// Get the name of the header to include.
	sscanf(fd->line, "#include \"%[^\"]", include_path);
	include.path = include_path;
	// Open the header file.
	include.fp = fopen(include.path, "r");
	if(include.fp == NULL){
		printf("Could not include header file at \"%s\".\n", include.path);
		success = 0;
	}else{
		include.line_num = 0;
		// Register any macros inside this header.
		while(fgets(include.line, LINE_MAX_LENGTH, include.fp) != NULL){
			include.line_len = strlen(include.line);
			++include.line_num;

			// The current line defines a preprocessor directive.
			if(include.line[0] == '#'){
				if(!macro_process(&include, list)){
					print_file_error(
						&include, "Macro registration failure."
					);
				}
			}
		}

		fclose(include.fp);
	}

	// After including the file, we should
	// return to our original directory.
	if(chdir(old_dir) == -1){
		printf("Failed to change to old directory \"%s\".\n", old_dir);
	}

	return(success);
}

// Insert a new macro into the list in order of name length.
void macro_insertion_sort(macro **list, macro *const node){
	macro *cur_macro = *list;
	// Search through the list until we find a macro
	// whose name is shorter than our new node's.
	while(cur_macro != NULL && node->name_len < cur_macro->name_len){
		list = &cur_macro->next;
		cur_macro = *list;
	}

	// Make the new node point to the current macro.
	node->next = cur_macro;
	// Set the "next" pointer of the
	// previous macro to our new one.
	*list = node;
}

// Register a new preprocessor macro. Here, "list"
// points to the head of the global macro list.
int macro_register(const file_data *const fd, macro **list){
	char name[TOKEN_MAX_LENGTH];
	size_t name_len;
	char value[TOKEN_MAX_LENGTH];
	size_t value_len;

	macro *new_macro;

	// Read the macro's name and value.
	sscanf(fd->line, "#define %s %s", name, value);
	name_len  = strlen(name);
	value_len = strlen(value);

	// Allocate the new macro instance. Note that we
	// also allocate memory for the name and value
	// strings, as well as their NULL terminators.
	new_macro = malloc(sizeof(*new_macro) + name_len + value_len + 2);
	if(new_macro == NULL){
		print_file_error(fd, "Memory allocation failure.");
		return(0);
	}

	// Set the name and value pointers.
	new_macro->name      = (char *)&new_macro[1];
	new_macro->name_len  = name_len;
	new_macro->value     = &new_macro->name[name_len+1];
	new_macro->value_len = value_len;
	new_macro->next      = NULL;

	// Copy the name and value over.
	strcpy(new_macro->name, name);
	strcpy(new_macro->value, value);

	// Insertion sort the new macro into the global list by the
	// the length of its name. This ensures that we don't replace
	// using macros whose names are substrings of other macros.
	macro_insertion_sort(list, new_macro);

	printf(
		"%s(%u)> Registered macro: %s -> %s\n",
		fd->path, fd->line_num, new_macro->name, new_macro->value
	);
	return(1);
}

// Interpret a preprocessor directive. Note that "list"
// points to the head of the global macro list.
int macro_process(const file_data *const fd, macro **list){
	if(strncmp(&fd->line[1], "include", 7) == 0){
		return(macro_include_header(fd, list));
	}else if(strncmp(&fd->line[1], "define", 6) == 0){
		return(macro_register(fd, list));
	}

	print_file_error(fd, "Unrecognized preprocessor directive.");
	return(0);
}


// For each macro in the list specified, replace any
// occurrences of its name with its value in the string.
void macro_replace_line(file_data *const fd, const macro *const list){
	const macro *cur_macro = list;
	while(cur_macro != NULL){
		char *const macro_pos = strstr(fd->line, cur_macro->name);
		// If the current macro doesn't appear in
		// the string, move on to the next one.
		if(macro_pos == NULL){
			cur_macro = cur_macro->next;

		// Otherwise, replace this occurrence!
		}else{
			// Shift everything after the macro
			// so that the value fits snugly.
			memmove(
				&macro_pos[cur_macro->value_len],
				&macro_pos[cur_macro->name_len],
				(size_t)(&fd->line[fd->line_len] - macro_pos)
			);
			// Insert the macro's value in!
			strncpy(macro_pos, cur_macro->value, cur_macro->value_len);
			// Update the line's length.
			fd->line_len += cur_macro->value_len - cur_macro->name_len;

			// Note that we should return to the beginning of
			// the list just in case we've added some new macros.
			cur_macro = list;
		}
	}
}

// Create a duplicate of the input shader but
// with all of the preprocessor macros replaced.
int macro_replace_file(
	const char *const input_path, const char *const output_path
){

	file_data input;

	// Open the shader file.
	input.path = input_path;
	input.fp = fopen(input.path, "r");
	if(input.fp == NULL){
		printf("Could not open input file at \"%s\".\n", input.path);
		return(0);
	}else{
		// Open a temporary output file. This file will be a copy
		// of the shader, but with all of the macros replaced.
		FILE *const output_file = fopen(output_path, "w");
		if(output_file == NULL){
			printf("Could not open output file at \"%s\".\n", output_path);
			return(0);
		}else{
			// Linked list of macros that we have registered.
			// These are stored such that macros with the largest
			// name are at the front of the list, as this ensures
			// that we don't accidentally replace using macros
			// that are substrings of other macros.
			macro *macro_list = NULL;

			input.line_num = 0;
			// Copy each line of the shader to the temporary file.
			while(fgets(input.line, LINE_MAX_LENGTH, input.fp) != NULL){
				input.line_len = strlen(input.line);
				++input.line_num;

				// Before writing the line to our temporary
				// file, we should substitute in any macros.
				macro_replace_line(&input, macro_list);

				// The current line defines a preprocessor directive.
				if(input.line[0] == '#'){
					if(!macro_process(&input, &macro_list)){
						print_file_error(
							&input, "Macro registration failure."
						);
					}
				}else{
					// Write the updated line to our output file!
					fwrite(
						input.line, sizeof(*input.line),
						input.line_len, output_file
					);
				}
			}

			// Now that we've finished writing to the
			// output file, we can free our macros.
			while(macro_list != NULL){
				macro *const macro_next = macro_list->next;
				free(macro_list);
				macro_list = macro_next;
			}

			fclose(output_file);
		}

		fclose(input.fp);
	}

	return(1);
}


// Compile the shader depending on what type it is.
int shader_compile(
	const char *const compile_path,
	const char *const options,
	const shader_type type
){

	char cmd[LINE_MAX_LENGTH];

	switch(type){
		case shader_none:
			puts(
				"Unable to compile shader: "
				"invalid shader type."
			);
			return(0);
		break;
		// Vertex shader -- run "vsa.exe".
		case shader_vertex:
			sprintf(
				cmd, "vsa.exe %s\"%s\"",
				options, compile_path
			);
		break;
		// Pixel shader -- run "psa.exe".
		case shader_pixel:
			sprintf(
				cmd, "psa.exe %s\"%s\"",
				options, compile_path
			);
		break;
	}

	if(system(cmd) != 0){
		puts("Shader compilation failed.");
		return(0);
	}

	return(1);
}


// Generate the new first line for the
// compiled header file and return its size.
size_t output_generate_line(
	char *const line, const char *const input_path, const shader_type type
){

	char input_name[PATH_BUFFER_LENGTH];
	char *temp_pos;

	// Get the name of the input file.
	strcpy(input_name, input_path);
	// Delete everything after the file extension.
	temp_pos = strrchr(input_name, '.');
	if(temp_pos != NULL){
		*temp_pos = '\0';
	}
	// Remove the file path.
	temp_pos = basename(input_name);
	memmove(input_name, basename(input_name), strlen(temp_pos) + 1);
	// Capitalize the first letter of the input name.
	input_name[0] = toupper(input_name[0]);

	// Generate the corrected first line of the header file.
	if(type == shader_vertex){
		sprintf(line, "DWORD dw%sVertexShader[] = {\r\n", input_name);
	}else{
		sprintf(line, "DWORD dw%sPixelShader[] = {\r\n", input_name);
	}

	return(strlen(line));
}

// Compute the size of the final output file.
// This assumes we're at the beginning of the file.
size_t output_get_size(FILE *const output, const size_t first_line_len){
	size_t output_size = first_line_len;

	// Skip past the first line.
	while(fgetc(output) != '\n');
	// Count the remaining characters.
	while(fgetc(output) != EOF){
		++output_size;
	}
	rewind(output);

	return(output_size);
}

// Rename the compiled shader's variable
// to include the original filename.
int output_update(
	const char *const input_path, const char *const output_path,
	const shader_type type
){

	FILE *output_file = fopen(output_path, "rb");
	if(output_file == NULL){
		printf("Could not open output file at \"%s\".\n", output_path);
		return(1);
	}else{
		char first_line[LINE_MAX_LENGTH];
		// Determine what the first line of
		// the file should be replaced with.
		const size_t first_line_len = output_generate_line(
			first_line, input_path, type
		);
		const size_t output_size = output_get_size(
			output_file, first_line_len
		);
		char *const output_data = malloc(output_size);
		if(output_data == NULL){
			puts("Failed to allocate memory for output file.");
			return(1);
		}

		// Copy the file's contents to the buffer.
		memcpy(output_data, first_line, first_line_len);
		while(fgetc(output_file) != '\n');
		fread(
			&output_data[first_line_len], sizeof(*output_data),
			output_size - first_line_len, output_file
		);

		// We want to completely clear the file rather than edit it,
		// as otherwise we'll have characters left over at the end.
		output_file = freopen(output_path, "wb", output_file);
		if(output_file == NULL){
			free(output_data);
			printf("Could not change mode of output file.");
			return(1);
		}else{
			// Write the buffer to the output file!
			fwrite(
				output_data, sizeof(*output_data),
				output_size, output_file
			);
			fclose(output_file);
		}

		free(output_data);
	}

	puts("\nShader compiled and updated successfully!");
	return(0);
}


int main(int argc, char **argv){
	cmd_args args;
	shader_type type;
	char compile_path[PATH_BUFFER_LENGTH];

	// Read and validate the command line arguments.
	if(!arguments_read(&args, argc, argv)){
		puts(
			"Invalid command line arguments specified. "
			"Please use the following format:\n"
			"\tnvasm-new.exe -[OPTIONS] \"input_path\" \"output_path\"\n"
			"Valid command line options include:\n"
			"\th\t:\tCompile to a header file."
		);
		return(1);
	}

	// (!!) Delete the output file if it exists. (!!)
	remove(args.output_path);

	// Determine the type of shader we're looking at.
	type = get_shader_type(args.input_path);
	if(type == shader_none){
		puts("Invalid shader format specified.");
		return(1);
	}

	// We need to use a temporary, intermediate file to store the
	// result of replacing all of our preprocessor directives.
	// This will automatically be replaced with the compiled shader.
	get_compile_path(compile_path, &args, type);

	// Replace all of the macros in in the shader
	// with their values, and then compile it.
	if(
		!macro_replace_file(args.input_path, compile_path) ||
		!shader_compile(compile_path, args.options, type)
	){
		return(1);
	}

	// Now that the shader has been compiled, we should rename
	// the output to the path specified. If no path was specified,
	// we still need to know what the output file is called.
	// If no output path was specified, use the default one.
	if(args.output_path == NULL){
		args.output_path = compile_path;

	// Otherwise, rename the output file to the desired path.
	}else{
		char output_dir[PATH_BUFFER_LENGTH];
		// Create the output directory before renaming the file.
		strcpy(output_dir, args.output_path);
		mkdir(dirname(args.output_path));

		if(rename(compile_path, args.output_path) != 0){
			puts(
				"Failed to rename output file. "
				"Using default output file."
			);
			args.output_path = compile_path;
		}
	}

	// If the output is a header file, we should rename the
	// variable so it isn't the same as every other header.
	if(args.options[0] != '\0'){
		return(output_update(args.input_path, args.output_path, type));
	}
	return(0);
}