#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <sys/ioctl.h>

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

int main(int argc, char **argv) {
    char *fname = NULL;
    int err = 0;
    bool got_name = false;
    if (argc < 2) {
        if ((fname = get_fname(&err)) == NULL) return err;
        got_name = true;
    } 
    else fname = argv[1];
    FILE *fp = fopen(fname, "rb");
    if (!fp) {
        err = errno;
        perror("hix: fopen");
        if (got_name) free(fname);
        fclose(fp);
        return err;
    }
    size_t fsz = get_fsz(fp, &err);
    if (fsz == 0 && err != 0) {
        if (got_name) free(fname);
        fclose(fp);
        return err;
    }
    else if (fsz == 0 && err == 0) {
        fprintf(stderr, "hix: File is empty\n");
        if (got_name) free(fname);
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
        if (got_name) free(fname);
        free(fb);
        fclose(fp);
        return err;
    }
    ushort availcol = get_current_columns(&err);
    if (availcol == 0 && err != 0) {
        if (got_name) free(fname);
        free(fb);
        fclose(fp);
        return err;
    }
    ushort chars = find_max_chars(availcol);
    dump_hex(fb, chars, fsz);
    if (got_name) free(fname);
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
    else if (availcol >= MIN_COLS_8_CHAR && availcol < MIN_COLS_12_CHAR) return 8;
    else if (availcol >= MIN_COLS_12_CHAR && availcol < MIN_COLS_16_CHAR) return 12;
    else if (availcol >= MIN_COLS_16_CHAR && availcol < MIN_COLS_20_CHAR) return 16;
    else if (availcol >= MIN_COLS_20_CHAR && availcol < MIN_COLS_24_CHAR) return 20;
    else if (availcol >= MIN_COLS_24_CHAR && availcol < MIN_COLS_28_CHAR) return 24;
    else if (availcol >= MIN_COLS_28_CHAR && availcol < MIN_COLS_32_CHAR) return 28;
    else if (availcol >= MIN_COLS_32_CHAR) return 32;
    else return 16;
}