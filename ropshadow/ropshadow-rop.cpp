
#include "../pin/source/include/pin/pin.H"
#include "utils.cpp"
#include <stdio.h>

#define THRESHOLD_INSTRUCTIONS 5
#define THRESHOLD_GADGETS 3

int gadgets = 0;
int instructions = 0;

void instruction_trace(INS ins, void*)
{	
	// Daca instructiunea este ret
    if (INS_IsRet(ins)) {
    	// Verifica daca numarul de instructiuni din secventa depaseste primul prag
		if (instructions < THRESHOLD_INSTRUCTIONS - 1) {
			// Potential gadget - contorizeaza-l
			gadgets++;
			// Daca numarul de gadget-uri din lant depaseste al doilea prag, se raporteaza
			// atacul si programul se inchide cu eroare
			if (gadgets >= THRESHOLD_GADGETS - 1) {
				die("ROP ATTACK !!!!!");
			}
		}
		else {
			// Lantul de gadget-uri este spart de un o secventa lunga de instructiuni
			// Contorul de gadget-uri este resetat
			gadgets = 0;
		}
		// Reseteaza contorul de instructiuni
		instructions = 0;
	}
	else {
		// Contorizeaza orice instructiune care nu este ret
		instructions++;
	}
}

int main(int argc, char *argv[])
{
	// Initializeaza API-ul Pin
	PIN_Init(argc, argv);
	// Initializeaza simbolurile
	PIN_InitSymbols();

	// Adauga functia de instrumentare care se ruleaza atunci cand o instructiune este executata
	INS_AddInstrumentFunction(instruction_trace, nullptr);
	
	// Porneste programul analizat cu Pin
	PIN_StartProgram();

	return 0;
}