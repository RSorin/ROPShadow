#include <string>
#include <iostream>
#include <cassert>
#include <cstdio>
#include <cstdarg>
#include <unistd.h>
#include <fcntl.h>
#include "../pin/source/include/pin/pin.H"


using namespace std;

// terminal colors
#define RED    "\e[41m"
#define GREEN  "\e[32m"
#define YELLOW "\e[33m"
#define BLUE   "\e[44m"
#define RESET  "\e[0m"

extern int numtabs[128];

void print_indent(int tid);
// Defineste functii de identare si de stergere a identarii cu tab-uri
#define indent() (++numtabs[PIN_ThreadId()])
#define unindent() (--numtabs[PIN_ThreadId()])

int lock_printf (const char *fmt, ...) __attribute__(( format(printf, 1, 2) ));



// Structura unui cadru
struct CallFrame {
        ADDRINT call_ins; // adresa instructiunii call/ret
        ADDRINT target_addr; // addresa tinta a instructiunii call/ret (pentru ret e adresa de intoarcere)
};

// Supraincarcarea operatorului de scriere pentru a afisa un cadru
// Cand se afiseaza un cadru, se afiseaza adresa instruciunii, adresa tintei si numele functiei tinta
inline std::ostream& operator<<(std::ostream& os, const CallFrame& c) {
    return os << YELLOW << (void*)c.call_ins << RESET ": "
            "call " << (void*)c.target_addr <<
            " <" << RTN_FindNameByAddress((ADDRINT)c.target_addr) << ">";
}

// Lacat furnizat de PIN
extern PIN_LOCK prlock;

// Executa o functie data ca argument intr-o zona critica
// Functia executata in zona critica ia ca argumente ID-ul firului
// de executie si o structura de cadru
void do_lock(void (*func)(THREADID, CallFrame), CallFrame frame){
    int tid = PIN_ThreadId();
    PIN_GetLock(&prlock, tid);
    func(tid,frame);
    PIN_ReleaseLock(&prlock);
}


void die (string msg) __attribute__(( noreturn ));

PIN_LOCK prlock;



int numtabs[128] = {0};

// Afiseaza identarea in functie de ID-ul firului de executie
void print_indent(int tid) {
    for (int i = 0; i < numtabs[tid]; ++i)
        putchar(' ');
}

// Afiseaza un string formatat pe ecran in mod thread-safe 
int lock_printf(const char *fmt, ...) {

    va_list args;
    va_start(args, fmt);
    PIN_GetLock(&prlock, PIN_ThreadId());
    print_indent(PIN_ThreadId());
    int ret = vprintf(fmt, args);
    PIN_ReleaseLock(&prlock);
    va_end(args);
    return ret;
}

// Afiseaza mesajul de eroare si se inchide
void die(string msg) {
    fprintf(stderr, RED "%s" RESET "\n", msg.c_str());
    exit(1);
}