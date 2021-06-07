
#ifndef _SOURCE_LINE_MANAGER_H
#define _SOURCE_LINE_MANAGER_H

int load_source_file(char *file_name, int file_id);
int source_files_free(void);
int get_source_line(int file_id, int line_number, FILE *file_out);

#endif
