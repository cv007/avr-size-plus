/*

    avr-size-plus

    replacement for avr-size using avr-readelf to gather
    the required information

    suited for avr0/1 series


---FLASH-------------size--address---
available            8192
.text                2946 0x000000
.rodata                86 0x808B82
used                 3032 [ 37%]
free                 5160 [ 63%]

 xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
 xxxx.............................
 .................................

---RAM---------------size--address---
available             512
.data                   4 0x803E00
.bss                   88 0x803E04
.noinit                 4 0x803E5C
used                   96 [ 18%]
free                  416 [ 82%]

 xxxxxxxxxxxxxxxxxx...............
 .................................
 .................................

---EEPROM------------size--address---
available             160 [128/32]
.eeprom                 2 0x801400
.user_signatures        4 0x801300
used                    6 [  3%]
free                  154 [ 97%]

*/

/*
1.20.0420 changed readelf options to add -W for wide output to get full names

*/
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>

#define VERSION "1.20.0420" //1.YY.MMDD

/*-----------------------------------------------------------------------------
    readelf options
    avr-readelf will need to be in same folder as this executable file
-----------------------------------------------------------------------------*/
#define READELF_EXE "./avr-readelf"
#define READELF_HEADER_OPT "-SW"
#define READELF_SYMBOL_OPT "-sW"

/*-----------------------------------------------------------------------------
    vars
-----------------------------------------------------------------------------*/
typedef struct {
    const char* name;
    int idx; //in case we want to match up symbols to sections
    int addr;
    int size;
} section_t;

section_t sections[] = {
    {".text",0,0,0},
    {".rodata",0,0,0},
    {".data",0,0,0},
    {".bss",0,0,0},
    {".noinit",0,0,0},
    {".eeprom",0,0,0},
    {".user_signatures",0,0,0},
    {".fuse",0,0,0},
    {0,0,0,0}
};
#define TEXT_SECTION 0
#define RODATA_SECTION 1
#define DATA_SECTION 2
#define BSS_SECTION 3
#define NOINIT_SECTION 4
#define EEPROM_SECTION 5
#define USER_SIGNATURES_SECTION 6
#define FUSE_SECTION 7

typedef struct {
    const char* name;
    int size;
} region_t;

region_t regions[] = {
    {"__TEXT_REGION_LENGTH__",0},
    {"__DATA_REGION_LENGTH__",0},
    {"__EEPROM_REGION_LENGTH__",0},
    {"__USER_SIGNATURE_REGION_LENGTH__",0},
    {0,0}
};
#define TEXT_REGION 0
#define DATA_REGION 1
#define EEPROM_REGION 2
#define USER_SIGNATURE_REGION 3

int debug;

/*-----------------------------------------------------------------------------
    misc functions
-----------------------------------------------------------------------------*/
//various functions that exit on error
void error(const char* msg) { perror( msg );  exit(1); }
void klose(int fd) { if (close(fd) == -1) error("could not close pipe end" ); }
void mkpipe(int fds[2]) { if (pipe(fds) == -1) error("could not create pipe"); }
void movefd(int fd1, int fd2, int keep) {
  if (dup2(fd1, fd2) == -1) error("could not duplicate pipe end");
  if( ! keep ) klose(fd1);
}
FILE* fd2FILE(int fd, const char* mode){
    FILE *f = fdopen(fd, mode);
    if( 0 == f ) error("could not get FILE stream from descriptor");
    return f;
}
void help(char* argv[]){
    printf( "usage:               [v%s]\r\n"
            "  %s [-d] /full/path/to/myapp.elf\r\n"
            "  -d = optional debug output\r\n",
            VERSION, basename( argv[0] ) );
    exit(1);
}

/*-----------------------------------------------------------------------------
    start avr-readelf process, setup pipe to capture stdout from this process
    stderr also redirected to stdout so we only have to read one pipe
-----------------------------------------------------------------------------*/
int readelf(char* argv[]) {
    int child_out[2]; mkpipe(child_out);
    #define RD 0
    #define WR 1
    pid_t pid = fork();

    if (pid == 0) { //child
        klose(child_out[RD]);
        movefd(child_out[WR], 1, 0); //0 = close source fd
        movefd(1, 2, 1); // 1>&2 //1= do not close source fd
        char* envp[] = { NULL };
        execve(argv[0], argv, envp );
        exit(1);
    } else {
        if( debug ) printf("child pid: %d\r\n", pid );
        klose(child_out[WR]);
        return child_out[RD];
    }
}

/*-----------------------------------------------------------------------------
    parsers
-----------------------------------------------------------------------------*/
//-S option, get section sizes
void find_section(char* str){
    char namebuf[128], buf[128], addrbuf[128], sizebuf[128];
    unsigned int idx, addr, size;
    char* pend = 0;

    if( 6 != sscanf( str, " [ %d] %s %s %s %s %s ",
        &idx, namebuf, buf, addrbuf, buf, sizebuf ) ) return;

    addr = strtol( addrbuf, &pend, 16 );
    if( pend == 0 ) return;
    size = strtol( sizebuf, &pend, 16 );
    if( pend == 0 ) return;

    section_t* ps = &sections[0];
    for( ; ps->name; ps++ ){
        if( strcmp( ps->name, namebuf ) ) continue;
        ps->idx = idx; ps->addr = addr; ps->size = size;
        break;
    }

    if( debug && ps->name ){
        printf("section found: %s %d 0x%06X %d\r\n", ps->name, ps->idx, ps->addr, ps->size);
    }
}

//-s option, get symbols for memory sizes
void find_symbol(char* str){
    char namebuf[128], buf[128], sizebuf[128];
    unsigned int size;
    char* pend = 0;

    if( 8 != sscanf( str, "%s %s %s %s %s %s %s %s",
        buf, sizebuf, buf, buf, buf, buf, buf, namebuf) ) return;

    size = strtol( sizebuf, &pend, 16 );
    if( pend == 0 ) return;

    region_t* pr = &regions[0];
    for( ; pr->name; pr++ ){
        if( strcmp( pr->name, namebuf ) ) continue;
        pr->size = size;
        break;
    }

    if( debug && pr->name ){
        printf("region size found: %s %d\r\n", pr->name, pr->size);
    }
}

/*-----------------------------------------------------------------------------
    printers
-----------------------------------------------------------------------------*/
void print_header(int n){
    if( n == 0 ) printf( "---FLASH-------------size--address---\r\n" );
    if( n == 1 ) printf( "---RAM---------------size--address---\r\n" );
    if( n == 2 ) printf( "---EEPROM------------size--address---\r\n" );
}
void print_available(int first, int second){
    if( second == 0 ) printf( "%-18s %6d\r\n", "available", first );
    else printf( "%-18s %6d [%d/%d]\r\n", "available", first+second, first, second );
}
void print_percent(int pct){
    if( pct > 99 ) pct = 99;
    printf("\r\n ");
    for( int i = 1; i < 100; i++ ){
        printf( "%c%s", i <= pct ? 'x' : '.', (i == 33) || (i == 66) ? "\r\n " : "" );
    }
    printf("\r\n\r\n");
}
void print_section(int idx){
    if( idx >= sizeof(sections)/sizeof(sections[0]) ) return;
    section_t* ps = &sections[idx];
    printf( "%-18s %6d 0x%06X\r\n", ps->name, ps->size, ps->addr );
}
int print_used_free(int maxsize, int used ){
    if( maxsize == 0 ) return 0;
    int pct = used*100/maxsize;
    printf( "%-18s %6d [%3d%%]\r\n", "used", used, pct );
    printf( "%-18s %6d [%3d%%]\r\n", "free", maxsize-used, 100-pct );
    return pct;
}

/*-----------------------------------------------------------------------------
    main
-----------------------------------------------------------------------------*/
int main(int argc, char* argv[]){

    if( argc < 2 ) help(argv);
    char* elfname = argv[1];
    //avr-size-plus -d /app.elf
    if( (argc == 3) && (argv[1][0] == '-') && (argv[1][1] == 'd') ){
        debug = 1;
        elfname = argv[2];
        printf("debug ON\r\n");
    }

    if( debug ){
        printf("argv[0]: %s  elf file: %s\r\n", argv[0], elfname );
    }

    //so can run ./avr-readelf, which is in this dir
    chdir( dirname(argv[0]) );

    //headers
    char* args[] = { READELF_EXE, READELF_HEADER_OPT, elfname, NULL };
    int child_stdout = readelf( args );
    char buf[256];
    FILE *f = fd2FILE( child_stdout, "r" );

    while( 0 !=fgets( buf, 128, f ) ){
        if( debug ) printf( "%s", buf );
        find_section( buf );
    }

    //symbols
    int rd_total = 0;
    int rd_size;
    args[1] = READELF_SYMBOL_OPT;
    child_stdout = readelf( args );
    f = fd2FILE( child_stdout, "r" );

    while( 0 != fgets( buf, 128, f ) ){
        if( debug ) printf( "%s", buf );
        find_symbol( buf );
    }

    int flash_used = sections[TEXT_SECTION].size + sections[RODATA_SECTION].size;
    int ram_used = sections[DATA_SECTION].size + sections[BSS_SECTION].size + sections[NOINIT_SECTION].size;
    int eeprom_used = sections[EEPROM_SECTION].size + sections[USER_SIGNATURES_SECTION].size;
    int flash_size = regions[TEXT_REGION].size;
    int ram_size = regions[DATA_REGION].size;
    int eeprom_size = regions[EEPROM_REGION].size;
    int user_signatures_size = regions[USER_SIGNATURE_REGION].size;

    if( flash_size ) {
        print_header( 0 );
        print_available( flash_size, 0 );
        print_section( TEXT_SECTION );
        print_section( RODATA_SECTION );
        print_percent( print_used_free( flash_size, flash_used ) );
    }

    if( ram_size ) {
        print_header( 1 );
        print_available( ram_size, 0 );
        print_section( DATA_SECTION );
        print_section( BSS_SECTION );
        print_section( NOINIT_SECTION );
        print_percent( print_used_free( ram_size, ram_used ) );
    }

    if( eeprom_size ) {
        print_header( 2 );
        print_available( eeprom_size, user_signatures_size );
        print_section( EEPROM_SECTION );
        print_section( USER_SIGNATURES_SECTION );
        print_used_free( eeprom_size + user_signatures_size, eeprom_used );
    }

    return 0;

}
