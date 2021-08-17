
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


#define VM_FL_HALT 0x1

#define VM_STACK_MAX 1024

typedef struct vm_t {
	struct regs_t {
		uint32_t pc, sp, fl;
	} regs;
	const buf_t *bytecode;
	uint8_t stack[VM_STACK_MAX];
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

bool vm_push_i32(vm_t *vm, int32_t value) {
	bool b = VERIFY(vm->regs.sp + sizeof(int32_t) <= VM_STACK_MAX);
	if (b) {
		*(int32_t *)(vm->stack + vm->regs.sp) = value;
		vm->regs.sp += sizeof(int32_t);
	}
	return b;
}

bool vm_pop_i32(vm_t *vm, int32_t *valuep) {
	bool b = VERIFY(vm->regs.sp >= sizeof(int32_t));
	if (b) {
		vm->regs.sp -= sizeof(int32_t);
		*valuep = *(const int32_t *)(vm->stack + vm->regs.sp);
	}
	return b;
}


#define VM_FETCH_IMPL(TYPE,NAME) \
	bool NAME(vm_t *vm, TYPE *valuep) { \
		size_t pc = vm->regs.pc; \
		bool b = VERIFY(pc + 1 + sizeof(TYPE) <= vm->bytecode->len); \
		if (b) *valuep = *(const TYPE *)(vm->bytecode->ptr + vm->regs.pc + 1); \
		return b; \
	}

VM_FETCH_IMPL(uint8_t, vm_fetch_u8);
VM_FETCH_IMPL(uint32_t, vm_fetch_u32);
VM_FETCH_IMPL(int32_t, vm_fetch_i32);

/*
bool vm_fetch_i32(vm_t *vm, int32_t *valuep) {
	size_t pc = vm->regs.pc;
	bool b = VERIFY(pc + 1 + sizeof(int32_t) <= vm->bytecode->len);
	if (b) *valuep = *(const int32_t *)(vm->bytecode->ptr + vm->regs.pc + 1);
	return b;
}
*/

static uint8_t *stack_top(vm_t *vm) {
	return vm->stack + vm->regs.sp;
}

#define NotImplemented() \
	do { \
		TRACE("ERROR: Not implemented: '%s'\n", __FUNCTION__); \
		exit(-1); \
	} while (0)


bool op_dup(vm_t *vm) {
	assert(vm->regs.sp >= sizeof(int32_t));
	const int32_t *src = (const int32_t *)(stack_top(vm) - sizeof(int32_t));
	OP_TRACE("dup [=%d]", *src);
	vm_push_i32(vm, *src);
	vm->regs.pc += 1;
	return true;
}

bool op_gt(vm_t *vm) {
	NotImplemented();
}

#define COMPARISON(name,oper) \
bool op_ ## name(vm_t *vm) { \
	assert(vm->regs.sp >= 2 * sizeof(int32_t)); \
	int32_t x, y; \
	vm_pop_i32(vm, &y); \
	vm_pop_i32(vm, &x); \
	int32_t result = x oper y; \
	OP_TRACE(#name "? [=%s]", result ? "true" : "false"); \
	vm_push_i32(vm, result); \
	vm->regs.pc += 1; \
	return true; \
}

COMPARISON(gte,>=);
COMPARISON(lte,<=);
COMPARISON(and,&&);

bool op_lt(vm_t *vm) {
	NotImplemented();
}

bool op_nop(vm_t *vm) {
	NotImplemented();
}

bool op_or(vm_t *vm) {
	assert(vm->regs.sp >= 2 * sizeof(int32_t));
	int32_t x, y;
	bool b = VERIFY(vm_pop_i32(vm, &x) && vm_pop_i32(vm, &y));
	if (b) {
		OP_TRACE("or? [=%s]", x || y ? "true" : "false");
		vm_push_i32(vm, x || y);
		vm->regs.pc += 1;
	}
	return b;
}

bool op_xchg(vm_t *vm) {
	assert(vm->regs.sp >= 2 * sizeof(int32_t));
	int32_t *x = (int32_t *)(vm->stack + vm->regs.sp - 2 * sizeof(int32_t));
	int32_t tmp = x[0];
	OP_TRACE("xchg [%d <-> %d]", x[0], x[1]);
	x[0] = x[1];
	x[1] = tmp;
	vm->regs.pc += 1;
	return true;
}

bool op_push(vm_t *vm) {
	int32_t operand;
	if (vm_fetch_i32(vm, &operand)) {
		OP_TRACE("push %d", operand);
		vm_push_i32(vm, operand);
		vm->regs.pc += 5;
		return true;
	}
	return false;
}

bool op_pop(vm_t *vm) {
	NotImplemented();
}

bool op_eq(vm_t *vm) {
	if (VERIFY(vm->regs.sp >= 2 * sizeof(int32_t))) {
		const int32_t *operands = ((const int32_t *) &vm->stack[vm->regs.sp]) - 2;
		bool result = operands[0] == operands[1];
		OP_TRACE("eq? [=%s]", result ? "true" : "false");
		vm->regs.sp -= 2 * sizeof(int32_t);
		vm_push_i32(vm, result);
		vm->regs.pc += 1;
		return true;
	}
	return false;
}

bool op_neq(vm_t *vm) {
	if (VERIFY(vm->regs.sp >= 2 * sizeof(int32_t))) {
		const int32_t *operands = ((const int32_t *) &vm->stack[vm->regs.sp]) - 2;
		bool result = operands[0] != operands[1];
		OP_TRACE("neq? [%s]", result ? "true" : "false");
		vm->regs.sp -= 2 * sizeof(int32_t);
		vm_push_i32(vm, result);
		vm->regs.pc += 1;
		return true;
	}
	return false;
}

bool op_halt(vm_t *vm) {
	OP_TRACE("halt");
	vm->regs.fl |= VM_FL_HALT;
	vm->regs.pc += 1;
	return true;
}


bool vm_step(vm_t *vm) {
	if (!VERIFY(vm->regs.pc < vm->bytecode->len))
		return false;
	uint8_t opcode = vm->bytecode->ptr[vm->regs.pc];
	OP_TRACE("%04x: ", vm->regs.pc);

	switch (opcode) {
#define X(opcode) \
		case opcode: { \
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
	const int32_t *stack = (const int32_t *) vm->stack;
	for (size_t i = 0; i < vm->regs.sp / sizeof(int32_t); i++) {
		TRACE("%d ", stack[i]);
	}
	TRACE("]\n");
}

void vm_eval(vm_t *vm) {
	while ((vm->regs.fl & VM_FL_HALT) == 0 && vm_step(vm)) {
		//print_stack(vm);
	}
}

bool take(token_t *token) {
	if (ch == -1) return false;
	str *tmp = token->text;
	token->text = str_append(token->text, (uchar_t) ch);
	free(tmp);
	next_char();
	return true;
}

int take_while(const buf_t *bytecode, token_t *token) {
	//TRACE(">>> take_while\n");
	token->text = str_new(0);
	vm_t vm;
	vm_init(&vm);
	vm.bytecode = bytecode;
	while (ch != -1) {
		vm.regs.pc = 0;
		vm.regs.sp = 0;
		vm.regs.fl = 0;
		//TRACE("ch = '%c'\n", ch);
		vm_push_i32(&vm, ch);
		vm_eval(&vm);
		int32_t result = -1;
		if (vm_pop_i32(&vm, &result) && result) {
			str *tmp = str_append(token->text, (uchar_t) ch);
			str_delete(token->text);
			token->text = tmp;
			next_char();
		}
		else {
			break;
		}
	}
	//TRACE("<<< take_while\n");
	return str_num_chars(token->text);
}

void print_str(const str *s, FILE *f) {
	if (VERIFY(s)) {
		size_t offset = 0;
		uchar_t ch;
		while (str_next_char(s, &ch, &offset))
			fputc(ch, f);
	}
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
			static uint8_t code[] = {
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
			buf_t buf = (buf_t){ code, sizeof(code) };
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

int main(int argc, char *argv[]) {
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