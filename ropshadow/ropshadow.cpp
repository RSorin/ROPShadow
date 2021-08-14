
#include <unwind.h>
#include "../pin/source/include/pin/pin.H"
#include <stack>
#include "utils.cpp"

// Constante pentru argumetele functiilor de instrumentare on_call_args si on_ret_args
// IARG_INST_PTR - adresa instructiunii curente
// IARG_RETURN_IP - adresa de intoarcere a functiei (la inceputul functiei)
// IARG_BRANCH_TARGET_ADDR - adresa tinta a branch-ului
// IARG_THREAD_ID - ID-ul firului de executie curent
#define on_call_args IARG_INST_PTR, IARG_RETURN_IP , IARG_THREAD_ID
#define on_ret_args  IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR, IARG_THREAD_ID

void log(THREADID thread_id, CallFrame frame)
{
	// Afiseaza ID-ul firului de executie si frame-ul
	print_indent(thread_id);
	cout << "t" << thread_id << ": " << frame << "\n";
}

class CallStack
{
	public:
		// Structura pentru contextul exceptiei
		_Unwind_Context *handler_context = nullptr;

		inline size_t size() const {
			return frames.size();
		}

		inline void push(CallFrame c) {
			// Adauga un frame in stiva auxiliara
			frames.push(c);
		}

		inline CallFrame pop() {
			// Scoate frame-ul din varful stivei auxiliare
			if (frames.size() <= 0) {
				cout << "pop(): stack empty" << endl;	
			}
			auto top = frames.top();
			frames.pop();
			// Returneaza frame-ul scos
			return top;
		}

		static void destroy(void *cs)
		{
			delete (CallStack*)cs;
		}

	private:
		// Stiva auxiliara cu frame-uri
		stack<CallFrame> frames;
};


namespace ShadowStack
{
	// Cheia pentru stocarea locala a firului de executie
	// Fiecare fir de executie va avea asociat o stiva auxiliara in stocarea lui locala
	// Pe baza cheii se obtine stiva
	TLS_KEY tls_call_stack;

	void on_call(const ADDRINT call_ins, const ADDRINT target_addr, THREADID thread_id)
	{
		// Obtine stiva asociata firului de executie curent
		CallStack *stack = (CallStack*)PIN_GetThreadData(tls_call_stack, thread_id);
		// Creaza un nou frame ce contine adresa instructiunii call si adresa tintei acesteia
		CallFrame frame = {call_ins, target_addr};
		// Pune frame-ul in stiva
		stack->push(frame);

		// Afiseaza ID-ul firului de executie si frame-ul dintr-o zona critica
		do_lock(log, frame);
		indent();
	}

	void on_ret(const ADDRINT ret_ins, const ADDRINT ret_addr, THREADID thread_id)
	{
		// Obtine stiva asociata firului de executie curent
		CallStack *stack = (CallStack*)PIN_GetThreadData(tls_call_stack, thread_id);
		// Formateaza output-ul
		unindent();


		// Verifica daca adresa de intoarcere se afla in stiva auxiliara
		ADDRINT saved_ret_addr = stack->pop().target_addr;
		while (saved_ret_addr != ret_addr && stack->size() != 0) {
		 	lock_printf("t%d: " RED "skipping a frame" RESET "\n", thread_id);
			unindent();
			saved_ret_addr = stack->pop().target_addr;
		}

		// Logheaza adresa instructiunii ret si adresa de intoarcere
		lock_printf("t%d: %p: ret (to " GREEN "%p" RESET ")\n",
			thread_id, (void*)ret_ins, (void*)ret_addr);

		// Daca adresa de intoarcere nu corespunde cu vreo adresa din stiva auxiliara
		// atunci se raporteaza atacul si procesul analizat este oprit
		if (stack->size() == 0 && saved_ret_addr != ret_addr)
			die("RETURN ADDRESS REWRITING DETECTED!!!!!");		
		
	}

	void on_interrupt(THREADID thread_id, CONTEXT_CHANGE_REASON reason,
		const CONTEXT *original_context, CONTEXT *signal_context, int32_t info, void*)
	{
		// Verifica tipul semnalului
		if (reason == CONTEXT_CHANGE_REASON_SIGNAL) {
			// Obtine stiva asociata firului de executie curent
			CallStack *stack = (CallStack*)PIN_GetThreadData(tls_call_stack, thread_id);

			lock_printf(BLUE "t%d: client program received signal %d " RESET "\n",
				thread_id, info);

			// Se extrage instruction pointer-ul si stack pointer-ul din context pentru a forma un cadru
			auto signal_context_sp = (ADDRINT*)(PIN_GetContextReg(signal_context, REG_STACK_PTR));
			auto signal_context_ip = PIN_GetContextReg(signal_context, REG_INST_PTR);

			// Se creeaza un cadru si se adauga in stiva auxliara
			CallFrame frame = {*signal_context_sp, signal_context_ip};
			stack->push(frame);
		} else if (reason == CONTEXT_CHANGE_REASON_FATALSIGNAL) {
			// Afiseaza faptul ca s-a primit un semnal fatal
			lock_printf(RED "t%d: client program received fatal signal %d " RESET "\n",
				thread_id, info);
		} else {
			// Semnal necuonscut
			lock_printf(BLUE "t%d: unknown context change (%d)" RESET "\n", thread_id, (int)reason);
		}
	}

	void on_call_exception(THREADID thread_id, _Unwind_Context *context)
	{
		CallStack *stack = (CallStack*)PIN_GetThreadData(tls_call_stack, thread_id);
		// Adauga structura de context in structura stivei auxiliare aferente
		// firului de executie curent
		stack->handler_context = context;
	}

	void on_ret_exception(THREADID thread_id)
	{
		CallStack *stack = (CallStack*)PIN_GetThreadData(tls_call_stack, thread_id);
		// Zona critica
		PIN_LockClient();

		// Obtine adresa din catch folosind structura de context a firului
		// de executie curent
		ADDRINT catch_addr = ADDRINT(_Unwind_GetIP(stack->handler_context));
		// Se formeaza un nou cadru cu adresa catch-ului
		// Cadul format nu are o adresa ca a doua componenta deoarece
		// dupa ce se executa directiva catch, exectia se continua secvential
		CallFrame top_frame_copy = stack->pop();
		CallFrame handler = { catch_addr, 0 };
		// Adauga adresa catch-ului inainte varfului stivei deoarece aceasta functie
		// se executa intainte instructiunii ret
		stack->push(handler);
		stack->push(top_frame_copy);

		// Sfarsit zona critica
		PIN_UnlockClient();

		lock_printf(BLUE "t%d: catch handler @ %p" RESET "\n", thread_id, (void*)catch_addr);
	}

	void on_thread_start(THREADID thread_id, CONTEXT*, int, void*)
	{
		// Seteaza stiva auxiliara pentru firul de executie atunci cand acesta porneste
		PIN_SetThreadData(tls_call_stack, new CallStack, thread_id);
	}

	void find_exception_context(IMG img, void*)
	{
		// Gaseste functia cu numele _Unwind_RaiseException_Phase2
		// Aceasta are ca prim argument contextul exceptiei in care este stocata
		// adresa functiei care gestioneaza exceptia
		auto rtn = RTN_FindByName(img, "_Unwind_RaiseException_Phase2");
		if (RTN_Valid(rtn))
		{
			RTN_Open(rtn);

			// Insereaza un apel la functia de instrumentare on_call_exception
			// avand ca argumente ID-ul firului de executie si contextul exceptiei
			RTN_InsertCall(rtn, IPOINT_BEFORE,
				AFUNPTR(&on_call_exception),
				IARG_THREAD_ID,
				IARG_FUNCARG_ENTRYPOINT_VALUE, 1, // _Unwind_Context*
				IARG_END);
			// Insereaza un apel la functia de instrumentare on_ret_exception
			// avand ca argumente ID-ul firului de executie
			RTN_InsertCall(rtn, IPOINT_AFTER,
				AFUNPTR(&on_ret_exception),
				IARG_THREAD_ID,
				IARG_END);

			RTN_Close(rtn);
		}
	}

	void function_trace(RTN rtn, void*)
	{
		// Verifica daca rutina este valida
		if (RTN_Valid(rtn))
		{
			RTN_Open(rtn);

			// Insereaza apelul functiei de instrumentare on_call la inceputul functiei
			// avand ca argumente cele definite in constanta specifica
			RTN_InsertCall(rtn, IPOINT_BEFORE,
				AFUNPTR(&on_call),
				on_call_args,
				IARG_END);

			// Insereaza apelul functiei de instrumentare on_ret inainte de executia
			// instructiunii ret, avand ca argumete cele definite in constanta specifica
			INS instr;
			for(instr = RTN_InsHead(rtn); INS_Valid(instr) && !INS_IsRet(instr); instr = INS_Next(instr));
			
			if (INS_Valid(instr) && INS_IsRet(instr)) {
				INS_InsertCall(instr, IPOINT_BEFORE, AFUNPTR(&on_ret), on_ret_args, IARG_END);
			}

			RTN_Close(rtn);
		}
	}
}


int main(int argc, char *argv[])
{

	// Initializeaza API-ul Pin
	PIN_Init(argc, argv);
	// Initializeaza simbolurile
	PIN_InitSymbols();
	// Creaza stiva auxiliara in zona de stocare locala a firului de executie curent
	ShadowStack::tls_call_stack = PIN_CreateThreadDataKey(&CallStack::destroy);
	// Initializeaza lacatul furnizat de Pin pentru a gestiona interactiunea
	// intre firele de executie
	PIN_InitLock(&prlock);

	// Adauga functia care se ruleaza cand un fir de executie porneste
	PIN_AddThreadStartFunction(ShadowStack::on_thread_start, nullptr);
	// Adauga functia de instrumentare care se ruleaza atunci cand o rutina este apelata
	RTN_AddInstrumentFunction(ShadowStack::function_trace, nullptr);
	// Adauga functia de instrumentare care se ruleaza atunci cand o exceptie este lasata
	IMG_AddInstrumentFunction(ShadowStack::find_exception_context, nullptr);
	// Adauga functia de instrumentare care se ruleaza atunci cand un semnal apare si schimba contextul
	PIN_AddContextChangeFunction(ShadowStack::on_interrupt, nullptr);

	// Porneste programul analizat cu Pin
	PIN_StartProgram();

	return 0;
}