
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define NORETURN __declspec(noreturn)
#define TRACE printf

enum {
	halt,
	nop,
	push,
	pop,
	dup,
	gt,
	gte,
	lt,
	lte,
	eq,
	neq,
	and,
	or,
};

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

#define VERIFY(x) ((x) || verify_fail(#x))

bool verify_fail(const char *desc) {
	fprintf(stderr, "ERROR: verify() failed: '%s'\n", desc);
	abort();
}

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
		bool b = VERIFY(vm->regs.pc + 1 + sizeof(TYPE) <= vm->bytecode->len); \
		if (b) *valuep = *(const TYPE *)(vm->bytecode->ptr + vm->regs.pc + 1); \
		return b; \
	}

VM_FETCH_IMPL(uint8_t, vm_fetch_u8);

bool vm_fetch_i32(vm_t *vm, int32_t *valuep) {
	bool b = VERIFY(vm->regs.pc + 1 + sizeof(int32_t) <= vm->bytecode->len);
	if (b) *valuep = *(const int32_t *)(vm->bytecode->ptr + vm->regs.pc + 1);
	return b;
}


VM_FETCH_IMPL(uint32_t, vm_fetch_u32);

bool op_push(vm_t *vm) {
	int32_t operand;
	if (vm_fetch_i32(vm, &operand)) {
		TRACE("push %d\n", operand);
		vm_push_i32(vm, operand);
		vm->regs.pc += 5;
		return true;
	}
	return false;
}

bool op_eq(vm_t *vm) {
	if (VERIFY(vm->regs.sp >= 2 * sizeof(int32_t))) {
		const int32_t *operands = ((const int32_t *) &vm->stack[vm->regs.sp]) - 2;
		bool result = operands[0] == operands[1];
		printf("eq? [%s]\n", result ? "true" : "false");
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
		printf("neq? [%s]\n", result ? "true" : "false");
		vm->regs.sp -= 2 * sizeof(int32_t);
		vm_push_i32(vm, result);
		vm->regs.pc += 1;
		return true;
	}
	return false;
}

bool op_halt(vm_t *vm) {
	printf("halt\n");
	vm->regs.fl |= VM_FL_HALT;
	vm->regs.pc += 1;
	return true;
}


bool vm_step(vm_t *vm) {
	if (!VERIFY(vm->regs.pc < vm->bytecode->len))
		return false;
	uint8_t opcode = vm->bytecode->ptr[vm->regs.pc];
	printf("%04x: ", vm->regs.pc);

	switch (opcode) {
		case push: {
			return op_push(vm);
		}
		case eq: {
			return op_eq(vm);
		}
		case neq: {
			return op_neq(vm);
		}
		case halt: {
			return op_halt(vm);
		}
		default: {
			fprintf(stderr, "unrecognized opcode: %d\n", opcode);
			return false;
		}
	}
}

void vm_eval(vm_t *vm) {
	while ((vm->regs.fl & VM_FL_HALT) == 0 && vm_step(vm)) {
		
	}
}


#define OFFSET(p,x) ((void *)(((uint8_t *) p) + (x)))

typedef char char_t;

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
	printf(">>> take_while\n");
	token->text = str_new();
	vm_t vm;
	vm_init(&vm);
	vm.bytecode = bytecode;
	while (ch != -1) {
		vm.regs.pc = 0;
		vm.regs.sp = 0;
		vm.regs.fl = 0;
		//printf("ch = '%c'\n", ch);
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
	printf("<<< take_while\n");
}

bool scan(token_t *token) {
	while (1) {
		token->text = str_new();
		token->type = NULL;
		if (ch == '#') {
			static uint8_t code[] = {
				push, '\n', 0, 0, 0,
				neq,
				halt
			};
			buf_t buf = (buf_t){ code, sizeof(code) };
			take_while(&buf, token);
			printf("skipped over comment '%s'\n", token->text);
		}
		else if (ch == '\n') {
			static uint8_t code[] = {
				push, '\n', 0, 0, 0,
				eq,
				halt
			};
			buf_t buf = (buf_t){ code, sizeof(code) };
			take_while(&buf, token);
		}
		else if (isalpha(ch) || ch == '_') {

		}
		else {
			assert(ch != '#');
			char tmp[2] = { (char) ch, 0 };
			fprintf(stderr, "unrecognized character: '%s' (%d)\n", tmp, ch);
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
			printf("token: { text: '%s', type: %s }\n", token.text, token.type);
		}
		fclose(file);
	}
	return 0;
}