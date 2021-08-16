
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

void OutputDebugStringA(const char *);

#define countof(x) (sizeof(x)/(sizeof((x)[0])))
#define NORETURN __declspec(noreturn)

#define OFFSET(p,x) ((void *)(((uint8_t *) p) + (x)))

#if 1
typedef char char_t;
#define DbgOut OutputDebugStringA
#endif

#define VERIFY(x) ((x) || verify_fail(#x))

bool verify_fail(const char_t *desc) {
	fprintf(stderr, "ERROR: verify() failed: '%s'\n", desc);
	abort();
}

void output(const char_t *message, size_t len) {
	DbgOut(message);
	fwrite(message, sizeof(char_t), len, stdout);
	fflush(stdout);
}

#define TRACE trace

void trace(const char *format, ...) {
	va_list args;
	va_start(args, format);
	char_t buf[8], *ptr = buf;
	int buf_count = countof(buf), count = _vsnprintf_s(ptr, buf_count, _TRUNCATE, format, args);
	if (count < 0) {
		count = _vscprintf(format, args);
		if (VERIFY(count >= 0)) {
			ptr = calloc(((size_t) count) + 1, sizeof(char_t));
			if (VERIFY(ptr)) {
				count = _vsnprintf_s(ptr, ((size_t) count) + 1, _TRUNCATE, format, args);
				assert(count >= 0);
			}
		}
	}
	va_end(args);
	if (count > 0) output(ptr, count);
	if (ptr != buf) free(ptr);
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
	char *text, *type;
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


bool op_and(vm_t *vm) {
	NotImplemented();
}

bool op_dup(vm_t *vm) {
	assert(vm->regs.sp >= sizeof(int32_t));
	const int32_t *src = (const int32_t *)(stack_top(vm) - sizeof(int32_t));
	TRACE("dup [=%d]", *src);
	vm_push_i32(vm, *src);
	vm->regs.pc += 1;
	return true;
}

bool op_gt(vm_t *vm) {
	NotImplemented();
}

bool op_gte(vm_t *vm) {
	NotImplemented();
}

bool op_lt(vm_t *vm) {
	NotImplemented();
}

bool op_lte(vm_t *vm) {
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
		TRACE("or? [=%s]", x || y ? "true" : "false");
		vm_push_i32(vm, x || y);
		vm->regs.pc += 1;
	}
	return b;
}

bool op_xchg(vm_t *vm) {
	assert(vm->regs.sp >= 2 * sizeof(int32_t));
	int32_t *x = (int32_t *)(vm->stack + vm->regs.sp - 2 * sizeof(int32_t));
	int32_t tmp = x[0];
	TRACE("xchg [%d <-> %d]", x[0], x[1]);
	x[0] = x[1];
	x[1] = tmp;
	vm->regs.pc += 1;
	return true;
}

bool op_push(vm_t *vm) {
	int32_t operand;
	if (vm_fetch_i32(vm, &operand)) {
		TRACE("push %d", operand);
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
		TRACE("eq? [=%s]", result ? "true" : "false");
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
		TRACE("neq? [%s]", result ? "true" : "false");
		vm->regs.sp -= 2 * sizeof(int32_t);
		vm_push_i32(vm, result);
		vm->regs.pc += 1;
		return true;
	}
	return false;
}

bool op_halt(vm_t *vm) {
	TRACE("halt");
	vm->regs.fl |= VM_FL_HALT;
	vm->regs.pc += 1;
	return true;
}


bool vm_step(vm_t *vm) {
	if (!VERIFY(vm->regs.pc < vm->bytecode->len))
		return false;
	uint8_t opcode = vm->bytecode->ptr[vm->regs.pc];
	TRACE("%04x: ", vm->regs.pc);

	switch (opcode) {
#define X(opcode) \
		case opcode: { \
			bool result = op_ ## opcode(vm); \
			TRACE("\n"); \
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

void vm_eval(vm_t *vm) {
	while ((vm->regs.fl & VM_FL_HALT) == 0 && vm_step(vm)) {
		TRACE("[ ");
		const int32_t *stack = (const int32_t *) vm->stack;
		for (size_t i = 0; i < vm->regs.sp / sizeof(int32_t); i++) {
			TRACE("%d ", stack[i]);
		}
		TRACE("]\n");
	}
}




size_t str_length(const char_t *s) {
	void *ptr = ((uint8_t *) s) - sizeof(size_t);
	return *(size_t *) ptr;
}

char_t *str_alloc(size_t len) {
	size_t sz = sizeof(size_t) + (len + 1) * sizeof(char_t);
	void *block = malloc(sz);
	if (VERIFY(block)) {
		memset(block, 0, sz);
		*((size_t *) block) = len;
		return OFFSET(block, sizeof(size_t));
	}
	else {
		return NULL;
	}
}

char_t *str_new() {
	return str_alloc(0);
}

void str_delete(char_t *str) {
	void *ptr = OFFSET(str, -((int) sizeof(size_t)));
	free(ptr);
}

char_t *str_concat(const char_t *p, const char_t *q) {
	size_t len_p = str_length(p), len_q = str_length(q);
	char_t *r = str_alloc(len_p + len_q);
	for (size_t i = 0; i < len_p; i++)
		r[i] = p[i];
	for (size_t j = 0; j < len_q; j++)
		r[len_p + j] = q[j];
	return r;
}

char_t *str_append(const char_t *p, char_t q) {
	size_t len_p = str_length(p), len = len_p + 1;
	char_t *r = str_alloc(len);
	r[len - 1] = q;
	return r;
}

void take_while(const buf_t *bytecode, token_t *token) {
	TRACE(">>> take_while\n");
	token->text = str_new();
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
		uint32_t result = -1;
		if (vm_pop_i32(&vm, &result) && result) {
			char_t *tmp = str_append(token->text, (char_t) ch);
			if (VERIFY(tmp)) {
				str_delete(token->text);
				token->text = tmp;
			}
			next_char();
		}
		else {
			break;
		}
	}
	TRACE("<<< take_while\n");
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
	TRACE("skipped over comment '%s'\n", token.text);
}

bool scan(token_t *token) {
	while (1) {
		token->text = str_new();
		token->type = NULL;
		if (ch == '#') {
			scan_comment();
		}
		else if (isspace(ch)) {
			static uint8_t code[] = {
				dup,
				push, '\n', 0, 0, 0,
				eq,
				xchg,
				push, ' ', 0, 0, 0,
				eq,
				or,
				halt
			};
			buf_t buf = (buf_t){ code, sizeof(code) };
			take_while(&buf, token);
			TRACE("skipped space: '%s'\n", token->text);
		}
		else if (isalpha(ch) || ch == '_') {
			return false;
		}
		else {
			assert(ch != '#');
			char tmp[2] = { (char) ch, 0 };
			TRACE("unrecognized character: '%s' (%d)\n", tmp, ch);
			return false;
		}
	}
	return false;
}

int main(void) {
	fopen_s(&file, "test.txt", "rb");
	if (file) {
		next_char();
		token_t token;
		while (scan(&token)) {
			TRACE("token: { text: '%s', type: %s }\n", token.text, token.type);
		}
		fclose(file);
	}
	return 0;
}