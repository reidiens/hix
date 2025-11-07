#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#define MIN_COLS_32_CHAR    148  
#define MIN_COLS_28_CHAR    130
#define MIN_COLS_24_CHAR    112
#define MIN_COLS_20_CHAR    94
#define MIN_COLS_16_CHAR    76
#define MIN_COLS_12_CHAR    58
#define MIN_COLS_8_CHAR     40
#define MIN_COLS_4_CHAR     22

typedef unsigned char uchar;
typedef unsigned short ushort;

ushort  get_current_columns(int *err);
char*   get_fname(int *err); 
size_t  get_fsz(FILE *fp, int *err);
int     read_file(FILE *fp, uchar *fb, size_t fsz, int *err);
void    print_hex_line(uchar *buf, ushort chars);
void    dump_hex(uchar *buf, ushort chars, size_t bufsz);
size_t  strl(char *buf);
ushort  find_max_chars(unsigned short availcol);
int     eval_args(int argc, char **argv, char **fname);

bool need_fname_from_user = false;
bool powers_of_2_only = false;

int main(int argc, char **argv) {
    char *fname = NULL;
    int err = 0;
    err = eval_args(argc, argv, &fname);
    if (err == -1) return err; 
    if (need_fname_from_user) {
        if ((fname = get_fname(&err)) == NULL)
            return err;
    }
    struct stat fstat;
    if (stat(fname, &fstat) == -1) {
        err = errno;
        perror("hix: stat");
        if (need_fname_from_user) free(fname);
        return err;
    }
    if (S_ISDIR(fstat.st_mode)) {
        fprintf(stderr, "hix: \"%s\" is a directory\n", fname);
        if (need_fname_from_user) free(fname);
        return -1;
    }
    FILE *fp = fopen(fname, "rb");
    if (!fp) {
        err = errno;
        perror("hix: fopen");
        if (need_fname_from_user) free(fname);
        fclose(fp);
        return err;
    }
    size_t fsz = get_fsz(fp, &err);
    if (fsz == 0 && err != 0) {
        if (need_fname_from_user) free(fname);
        fclose(fp);
        return err;
    }
    else if (fsz == 0 && err == 0) {
        fprintf(stderr, "hix: File is empty\n");
        if (need_fname_from_user) free(fname);
        fclose(fp);
        return -1;
    }
    uchar *fb = malloc(fsz * sizeof(uchar));
    if (!fb) {
        err = errno;
        perror("hix: malloc");
        fclose(fp);
        return err;
    }
    if (read_file(fp, fb, fsz, &err) == -1) {
        if (need_fname_from_user) free(fname);
        free(fb);
        fclose(fp);
        return err;
    }
    ushort availcol = get_current_columns(&err);
    if (availcol == 0 && err != 0) {
        if (need_fname_from_user) free(fname);
        free(fb);
        fclose(fp);
        return err;
    }
    ushort chars = find_max_chars(availcol);
    dump_hex(fb, chars, fsz);
    if (need_fname_from_user) free(fname);
    free(fb);
    fclose(fp);
    return 0;
}

char* get_fname(int *err) {
    char *ret = malloc(PATH_MAX * sizeof(char));
    if (!ret) {
        *err = errno;
        perror("hix: malloc");
        return NULL;
    }
    puts("What's the path to the file?");
    if (fgets(ret, PATH_MAX, stdin) == NULL) {
        if (ferror(stdin)) {
            *err = errno;
            perror("hix: fgets");
            free(ret);
            return NULL;
        }
    }
    if (ret[strl(ret) - 1] == '\n') ret[strl(ret) - 1] = '\0';
    return ret;
}

size_t get_fsz(FILE *fp, int *err) {
    size_t ret = 0;
    if (!fp) {
        fprintf(stderr, "hix: get_fsz: Bad file pointer\n");
        *err = -1;
        return ret;
    }
    if (fseek(fp, 0, SEEK_END) == -1) {
        *err = errno;
        perror("hix: fseek");
        return ret;
    }
    if ((ret = ftell(fp)) == (long)-1) {
        *err = errno;
        perror("hix: ftell");
        ret = 0;
        return ret;
    }
    if (fseek(fp, 0, SEEK_SET) == -1) {
        *err = errno;
        perror("hix: fseek");
        ret = 0;
        return ret;
    }
    return ret;
}

int read_file(FILE *fp, uchar *fb, size_t fsz, int *err) {
    if (!fp) {
        *err = -1;
        fprintf(stderr, "hix: read_file: Bad file pointer\n");
        return -1;
    }
    if (!fb) {
        *err = -1;
        fprintf(stderr, "hix: read_file: Bad file buffer pointer\n");
        return -1;
    }
    fread(fb, fsz, 1, fp);
    if (ferror(fp)) {
        *err = errno;
        perror("hix: fread");
        return -1;
    } 
    return 0;
}

void print_hex_line(uchar *buf, ushort chars) {
    if (!buf) {
        fprintf(stderr, "hix: print_hex_line: Bad buffer\n");
        return;
    }
    for (int i = 0; i < chars; i++) {
        printf("%.2X ", buf[i]);
        if ((i + 1) % 4 == 0) printf("  ");
    }
    printf("    ");
    for (int i = 0; i < chars; i++) printf("%c", (buf[i] < 32 || buf[i] == 127) ? '.' : buf[i]);
    printf("\n");
}

void dump_hex(uchar *buf, ushort chars, size_t bufsz) {
    if (!buf) {
        fprintf(stderr, "hix: dump_hex: Bad buffer\n");
        return;
    }
    uchar *temp = buf;
    int lines = bufsz / chars;
    int leftover = bufsz % chars; 
    if (lines) 
        for (int i = 0; i < lines; i++) print_hex_line(temp + (i * chars), chars);
    if (leftover) {
        uchar *temp2 = buf + ((lines * chars) - 1);
        for (int i = 0; i < leftover; i++) {
            printf("%.2X ", temp2[i]);
            if ((i + 1) % 4 == 0) printf("  ");
        }
        int a, b;
        a = leftover / 4;
        b = leftover - a;
        int cols_til_chars = ((chars / 4) * 14) + 4;
        int spaces = cols_til_chars - ((b * 3) + (a * 5));
        for (int i = 0; i < spaces; i++) printf(" ");
        for (int i = 0; i <= leftover; i++) printf("%c", (temp2[i] < 32 || temp2[i] == 127) ? '.' : temp2[i]);
        printf("\n");
    }
    for (int i = 0; i < (((chars / 4) * 14) + 4) + (chars - 3); i++) printf(" ");
    printf("EOF\n");
}

size_t strl(char *buf) {
    if (!buf) return 0;
    size_t ret = 0;
    for (char *p = buf; *p; p++, ++ret)
        if (*p == '\0') break;
    return ret;
}
 
ushort get_current_columns(int *err) {
    if (isatty(fileno(stdout))) return MIN_COLS_32_CHAR;
    struct winsize w;
    if (ioctl(0, TIOCGWINSZ, &w) == -1) {
        *err = errno;
        perror("hix: ioctl");
        return 0;
    }
    return w.ws_col;
}

ushort find_max_chars(unsigned short availcol) {
    if (availcol < MIN_COLS_8_CHAR) return 4;
    else if (availcol >= MIN_COLS_8_CHAR && availcol < MIN_COLS_12_CHAR)
        return 8;
    else if (availcol >= MIN_COLS_12_CHAR && availcol < MIN_COLS_16_CHAR)
        return (powers_of_2_only) ? 8 : 12;
    else if (availcol >= MIN_COLS_16_CHAR && availcol < MIN_COLS_20_CHAR)
        return 16;
    else if (availcol >= MIN_COLS_20_CHAR && availcol < MIN_COLS_24_CHAR)
        return (powers_of_2_only) ? 16 : 20;
    else if (availcol >= MIN_COLS_24_CHAR && availcol < MIN_COLS_28_CHAR)
        return (powers_of_2_only) ? 16 : 24;
    else if (availcol >= MIN_COLS_28_CHAR && availcol < MIN_COLS_32_CHAR)
        return (powers_of_2_only) ? 16 : 28;
    else if (availcol >= MIN_COLS_32_CHAR)
        return 32;
    else return 16;
}

int eval_args(int argc, char **argv, char **fname) {
    if (!fname) return -1;
    if (argc < 2) {
        need_fname_from_user = true;
        return 0;
    }
    bool got_fname = false;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (argv[i][1] == 'p' && argv[i][2] == '\0') powers_of_2_only = true;
            else {
                fprintf(stderr, "hix: Unrecognized option: \"%s\"\n", argv[i]);
                return -1;
            }
        }
        else {
            if (!got_fname) {
                *fname = argv[i]; 
                got_fname = true;
            }
        }
    }
    if (*fname == NULL) need_fname_from_user = true;
    return 0;
}
