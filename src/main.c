
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "str.h"
#include "debug.h"

#if defined(WINDOWS)
	void OutputDebugStringA(const char *);
	#define NORETURN __declspec(noreturn)
	#define DbgOut OutputDebugStringA
#else
	#define NORETURN 
	#define DbgOut(x)
#endif

#define countof(x) (sizeof(x)/(sizeof((x)[0])))


#define OP_TRACE(...)


void swap_i32(int32_t *px, int32_t *py) {
	int32_t y = *py;
	*py = *px;
	*px = y;
}

void output(const char *message, size_t len) {
	DbgOut(message);
	fwrite(message, len, sizeof(char), stdout);
	fflush(stdout);
}

#define TRACE trace

void trace(const char *format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stdout, format, args);
	va_end(args);
}

#define OPCODES \
	X(halt) \
	X(nop) \
	X(push) \
	X(pop) \
	X(dup) \
	X(xchg) \
	X(gt) \
	X(gte) \
	X(lt) \
	X(lte) \
	X(eq) \
	X(neq) \
	X(and) \
	X(or) \

#define X(y) y,

enum {
	OPCODES
};

#undef X

int ch = -1;
FILE *file;

typedef struct token_t {
	str *text, *type;
} token_t;

void next_char() {
	char tmp;
	ch = fread(&tmp, 1, 1, file) == 1 ? tmp : -1;
}

typedef struct buf_t {
	uint8_t *ptr;
	size_t len;
} buf_t;


#define FL_HALT 0x1

#define VM_STACK_MAX 1024

typedef struct function_t {
	uint32_t name;
	uint8_t *body;
} function_t;

typedef struct module_t {
	str **strings;
	size_t num_strings;
	function_t *functions;
	size_t num_functions;
} module_t;

typedef struct vm_t {
	struct regs_t {
		uint32_t pc, sp, fl;
	} regs;
	const buf_t *bytecode;
	int32_t stack[VM_STACK_MAX];
} vm_t;

void vm_init(vm_t *vm) {
	memset(vm, 0, sizeof(vm_t));
}

NORETURN void vm_panic(vm_t *vm, const char *format, ...) {
	fprintf(stderr, "***** PANIC *****\n");
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "pc: %d, sp: %d\n", vm->regs.pc, vm->regs.sp);
	exit(-1);
}

#define panic_if(vm,condition) \
	do { \
		if (condition) \
			vm_panic(vm, "%s:%ld", __FILE__, __LINE__); \
	} while (0)

void vm_push(vm_t *vm, int32_t value) {
	panic_if(vm, vm->regs.sp >= VM_STACK_MAX); // check for space on stack
	vm->stack[vm->regs.sp++] = value;
}

int32_t vm_pop(vm_t *vm) {
	panic_if(vm, vm->regs.sp == 0);
	panic_if(vm, vm->regs.sp > VM_STACK_MAX);
	return vm->stack[--vm->regs.sp];
}

#define VM_FETCH_IMPL(TYPE,NAME) \
	TYPE NAME(vm_t *vm) { \
		panic_if(vm, vm->regs.pc + 1 + sizeof(TYPE) > vm->bytecode->len); \
		return *(const TYPE *)(vm->bytecode->ptr + vm->regs.pc + 1); \
	}

VM_FETCH_IMPL(uint8_t, vm_fetch_u8);
VM_FETCH_IMPL(uint32_t, vm_fetch_u32);
VM_FETCH_IMPL(int32_t, vm_fetch_i32);



#define NotImplemented() \
	do { \
		TRACE("ERROR: Not implemented: '%s'\n", __FUNCTION__); \
		abort(); \
	} while (0)


bool op_nop(vm_t *vm) {
	vm->regs.pc++;
	return true;
}

bool op_halt(vm_t *vm) {
	vm->regs.fl |= FL_HALT;
	return true;
}


#define COMPARISON(name,oper) \
	bool op_ ## name(vm_t *vm) { \
		panic_if(vm, vm->regs.sp < 2); \
		vm->regs.sp -= 2; \
		int32_t x = vm->stack[vm->regs.sp], y = vm->stack[vm->regs.sp+1]; \
		vm->stack[vm->regs.sp++] = (x oper y); \
		vm->regs.pc++; \
		return true; \
	}

COMPARISON(eq,==);
COMPARISON(neq,!=);
COMPARISON(gt,>);
COMPARISON(lt,<);
COMPARISON(gte,>=);
COMPARISON(lte,<=);
COMPARISON(and,&&);
COMPARISON(or,||);


bool op_dup(vm_t *vm) {
	panic_if(vm, vm->regs.sp == 0);
	panic_if(vm, vm->regs.sp == VM_STACK_MAX);
	int32_t value = vm->stack[vm->regs.sp-1];
	vm->stack[vm->regs.sp++] = value;
	vm->regs.pc += 1;
	return true;
}

bool op_push(vm_t *vm) {
	int32_t operand = vm_fetch_i32(vm);
	OP_TRACE(" %d", operand);
	vm_push(vm, operand);
	vm->regs.pc += 5;
	return true;
}

bool op_pop(vm_t *vm) {
	vm_pop(vm);
	vm->regs.pc++;
	return true;
}

bool op_xchg(vm_t *vm) {
	panic_if(vm, vm->regs.sp < 2);
	swap_i32(&vm->stack[vm->regs.sp-2], &vm->stack[vm->regs.sp-1]);
	vm->regs.pc++;
	return true;
}

bool vm_step(vm_t *vm) {
	panic_if(vm, vm->regs.pc >= vm->bytecode->len);
	uint8_t opcode = vm->bytecode->ptr[vm->regs.pc];
	OP_TRACE("%04x: ", vm->regs.pc);

	switch (opcode) {
#define X(opcode) \
		case opcode: { \
			OP_TRACE(#opcode); \
			bool result = op_ ## opcode(vm); \
			OP_TRACE("\n"); \
			return result; \
		}
		OPCODES
#undef X
		default: {
			TRACE("unrecognized opcode: %d\n", opcode);
			return false;
		}
	}
}

static void print_stack(vm_t *vm) {
	TRACE("[ ");
	if (vm->regs.sp > 0) {
		TRACE("%d", vm->stack[0]);
		for (size_t i = 1; i < vm->regs.sp; i++) {
			TRACE(", %d", vm->stack[i]);
		}
	}
	TRACE(" ]\n");
}

void vm_eval(vm_t *vm) {
	OP_TRACE(">>> vm_eval\n");
	//print_stack(vm);
	while ((vm->regs.fl & FL_HALT) == 0 && vm_step(vm)) {
		//print_stack(vm);
	}
	//print_stack(vm);
	OP_TRACE("<<< vm_eval\n");
}

bool take(token_t *token) {
	if (ch == -1) return false;
	str *tmp = token->text;
	token->text = str_append(token->text, (uchar_t) ch);
	free(tmp);
	next_char();
	return true;
}

bool run_program(vm_t *vm) {
	vm->regs.pc = 0;
	vm->regs.sp = 0;
	vm->regs.fl = 0;
	//TRACE("ch = '%c'\n", ch);
	vm_push(vm, ch);
	vm_eval(vm);
	return vm_pop(vm) != 0;
}

void print_str(const str *s, FILE *f) {
	if (VERIFY(s)) {
		size_t offset = 0;
		uchar_t ch;
		while (str_next_char(s, &ch, &offset))
			fputc(ch, f);
	}
}

static uint8_t is_space[] = {
	dup,
	dup,
	push, '\n', 0, 0, 0,
	eq,
	xchg,
	push, ' ', 0, 0, 0,
	eq,
	or,
	xchg,
	push, '\t', 0, 0, 0,
	eq,
	or,
	halt
};

int take_while(const buf_t *bytecode, token_t *token) {
	TRACE(">>> take_while\n");
	token->text = str_new(0);
	vm_t vm;
	vm_init(&vm);
	vm.bytecode = bytecode;
	while (ch != -1 && run_program(&vm)) {
		str *text = str_append(token->text, (uchar_t) ch);
		str_delete(token->text);
		token->text = text;
		next_char();
	}
	TRACE("<<< take_while: ");
	print_str(token->text, stdout);
	TRACE("\n");
	return str_num_chars(token->text);
}

void scan_comment() {
	static uint8_t code[] = {
		push, '\n', 0, 0, 0,
		neq,
		halt
	};
	token_t token;
	buf_t buf = (buf_t){ code, sizeof(code) };
	take_while(&buf, &token);
	fputs("skipped over comment '", stdout);
	print_str(token.text, stdout);
	fputs("'\n", stdout);
}

bool scan_word(token_t *token) {
	uint8_t code[] = {
		dup,
		dup,
		push, 'a', 0, 0, 0,
		gte,
		xchg,
		push, 'z', 0, 0, 0,
		lte,
		and,
		xchg,
		push, '_', 0, 0, 0,
		eq,
		or,
		halt
	};
	buf_t is_alpha = { .ptr=code, .len=sizeof(code) };
	bool b = take_while(&is_alpha, token) > 0;
	token->type = str_new("word");
	return b;
}

bool scan(token_t *token) {
	while (ch != -1) {
		token->text = str_new(0);
		token->type = NULL;
		if (ch == '#') {
			scan_comment();
		}
		else if (isspace(ch)) {
			buf_t buf = (buf_t){ is_space, sizeof(is_space) };
			take_while(&buf, token);
		}
		else if (isalpha(ch) || ch == '_') {
			return scan_word(token);
		}
		else {
			bool b = take(token);
			uchar_t ch;
			size_t offset = 0;
			str_next_char(token->text, &ch, &offset);
			token->type = str_new((char[]){ ch, 0 });
			return b;
		}
	}
	return false;
}

void print_token(const token_t *token, FILE *f) {
	fputs("token: { text: '", f);
	print_str(token->text, f);
	fputs("', type: '", f);
	print_str(token->type, f);
	fputs("' }\n", stdout);
}

#define TEST_ASSERT assert

int32_t call_function(vm_t *vm, uint8_t *body, size_t len, int32_t *params, size_t num_params) {
	buf_t buf = (buf_t) { body, len };
	vm->bytecode = &buf;
	for (size_t i = 0; i < num_params; i++)
		vm_push(vm, params[i]);
	vm_eval(vm);
	return vm_pop(vm);
}

void run_tests() {
	for (size_t i = 0; i < 255; i++) {
		vm_t vm;
		vm_init(&vm);
		int b = call_function(&vm, is_space, sizeof(is_space), (int32_t[]) { i }, 1);
		printf("is_space(%ld) -> %d\n", i, b);
		fflush(stdout);
		if (b) {
			if (i != ' ' && i != '\t' && i != '\n') {
				assert(false);
			}
		}
		else {
			if (i == ' ' || i == '\t' || i == '\n') {
				assert(false);
			}
		}
	}
}

int main(int argc, char *argv[]) {
	run_tests();
	file = fopen("test.txt", "rb");
	if (file) {
		next_char();
		token_t token;
		while (scan(&token)) {
			print_token(&token, stdout);
		}
		fclose(file);
	}
	return 0;
}