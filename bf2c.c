#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEADER "#include <stdio.h>\n#include <stdlib.h>\n"
#define MAX_MEM 1048575
#define MIN_MEM 255

char* change_file_extension(const char *filename, const char *ext) {
    const char *last_slash = strrchr(filename, '/');
    if (last_slash == NULL) {
        last_slash = filename;
    } else {
        last_slash++;
    }
    const char *last_dot = strrchr(last_slash, '.');
    if (last_dot == NULL) {
        last_dot = filename + strlen(filename);
    }
    size_t new_name_length = (last_dot - last_slash) + strlen(ext);
    char *new_name = (char *)malloc(new_name_length + 1);
    if (new_name == NULL) {
        perror("malloc");
        return NULL;
    }
    strncpy(new_name, last_slash, last_dot - last_slash);
    new_name[last_dot - last_slash] = '\0';
    strcat(new_name, ext);
    return new_name;
}

void flush_ops(FILE *c_fp, int *ptr_net, int *mem_net);

void flush_ops(FILE *c_fp, int *ptr_net, int *mem_net) {
    if (*ptr_net != 0) {
        if (*ptr_net > 0) {
            fprintf(c_fp, "ptr += %d;\n    ", *ptr_net);
        } else {
            fprintf(c_fp, "ptr -= %d;\n    ", -(*ptr_net));
        }
        *ptr_net = 0;
    }
    if (*mem_net != 0) {
        if (*mem_net > 0) {
            fprintf(c_fp, "mem[ptr] += %d;\n    ", *mem_net);
        } else {
            fprintf(c_fp, "mem[ptr] -= %d;\n    ", -(*mem_net));
        }
        *mem_net = 0;
    }
}

int main(int argc, char* argv[]) {
    if (argc % 2 == 1) {
        fprintf(stderr, "Error: missing parameter\n");
        return 1;
    }
    FILE *fp = fopen(argv[1], "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: No find file\n");
        return 2;
    }
    char MemNum[8] = "30000";
    char *ExportPath = change_file_extension(argv[1], ".c");
    if (ExportPath == NULL) {
        fprintf(stderr, "Error: Malloc failed for ExportPath\n");
        fclose(fp);
        return 4;
    }
    int ModifyExportPath = 0;

    for (int i = 2; i < argc; i+=2) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-mem") == 0) {
                int MemNumTest = atoi(argv[i+1]);
                if (MemNumTest > MAX_MEM || MemNumTest < MIN_MEM) {
                    fprintf(stderr, "Error: This number is inappropriate\n");
                    fclose(fp);
                    if (ModifyExportPath == 0) free(ExportPath);
                    return 3;
                }
                snprintf(MemNum, sizeof(MemNum), "%d", MemNumTest);
            }
            if (strcmp(argv[i], "-o") == 0) {
                ModifyExportPath=1;
                free(ExportPath);
                ExportPath = argv[i+1];
            }
        }
    }

    char memDef[64] = "unsigned char mem[";
    strcat(memDef, MemNum);
    strcat(memDef, "] = {0};\nint ptr = 0;\nint main() {\n    ");
    char end[] = "    return 0;\n}";

    FILE *c_fp = fopen(ExportPath, "w");
    if (c_fp == NULL) {
        fprintf(stderr, "Error: Failed to create file → %s\n", ExportPath);
        if (ModifyExportPath == 0) free(ExportPath);
        fclose(fp);
        return 5;
    }

    fprintf(c_fp, "%s", HEADER);
    fprintf(c_fp, "%s", memDef);

    int ch;
    int bracket_stack = 0;
    int ptr_net = 0, mem_net = 0;

    while ((ch = fgetc(fp)) != EOF) {
        switch (ch) {
            case '>':
                if (mem_net != 0) {
                    flush_ops(c_fp, &ptr_net, &mem_net);
                }
                ptr_net++;
                break;
            case '<':
                if (mem_net != 0) {
                    flush_ops(c_fp, &ptr_net, &mem_net);
                }
                ptr_net--;
                break;
            case '+':
                if (ptr_net != 0) {
                    flush_ops(c_fp, &ptr_net, &mem_net);
                }
                mem_net++;
                break;
            case '-':
                if (ptr_net != 0) {
                    flush_ops(c_fp, &ptr_net, &mem_net);
                }
                mem_net--;
                break;
            case '.':
                flush_ops(c_fp, &ptr_net, &mem_net);
                fprintf(c_fp, "putchar(mem[ptr]);\n    ");
                break;
            case ',':
                flush_ops(c_fp, &ptr_net, &mem_net);
                fprintf(c_fp, "mem[ptr] = getchar();\n    ");
                break;
            case '[':
                flush_ops(c_fp, &ptr_net, &mem_net);
                bracket_stack++;
                fprintf(c_fp, "while (mem[ptr]) {\n        ");
                break;
            case ']':
                flush_ops(c_fp, &ptr_net, &mem_net);
                if (bracket_stack == 0) {
                    fprintf(stderr, "Error: Mismatched ']' in BF file\n");
                    fclose(c_fp); fclose(fp);
                    if (ModifyExportPath == 0) free(ExportPath);
                    return 6;
                }
                bracket_stack--;
                fprintf(c_fp, "}\n    ");
                break;
            default: 
                break;
        }
    }

    flush_ops(c_fp, &ptr_net, &mem_net);

    if (bracket_stack != 0) {
        fprintf(stderr, "Error: Mismatched '[' (missing %d ']')\n", bracket_stack);
        fclose(c_fp); fclose(fp);
        if (ModifyExportPath == 0) free(ExportPath);
        return 6;
    }

    fprintf(c_fp, "%s", end);
    fclose(c_fp);
    fclose(fp);
    if (ModifyExportPath == 0) free(ExportPath);
    return 0;
}
