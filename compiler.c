#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum Type {
	SYMBOL,
	CELL
};

typedef struct Object {
	enum Type type;
	union {
		struct {
			struct Object* cdr;
			struct Object* car;
		} list;
		char* symbol;
	};
} Object;
Object* symbols;
int peek;

void gettoken(char* token) {
	int index = 0;
	while(peek == ' ' || peek == '\n' || peek == '\t') { peek = getchar(); }
	if (peek == '(' || peek == ')') {
		token[index++] = peek;
		peek = getchar();
	} else {
		while(peek != 0 && peek != ' ' && peek != '\n' && peek != '\t' && peek != '(' && peek != ')') {
			if(peek == '\\') {
				peek = getchar();
				if (peek == 'n') peek = '\n';
			}
			token[index++] = peek;
			peek = getchar();
		}
	}
	token[index] = '\0';
}

Object* cons(Object* _car, Object* _cdr) {
	Object *_pair = calloc(1, sizeof(Object));
	*_pair = (Object) {.type = CELL, .list = {.car = _car, .cdr = _cdr}};
	return _pair;
}

Object* symbol(char *sym) {
	Object* _pair = symbols;
	for (;_pair; _pair = _pair->list.cdr)
		if (strncmp(sym, _pair->list.car->symbol, 64)==0) return _pair->list.car;
	Object *symbol = calloc(1, sizeof(Object));
	*symbol = (Object) {.type = SYMBOL, .symbol = strdup(sym)};
	symbols = cons(symbol, symbols);
	return symbol;
}

Object* getlist() {
	char token[64] = {0};
	gettoken(token);
	if (token[0] == ')') return 0;
	Object* car = token[0] == '(' ? getlist() : symbol(token);
	return cons(car, getlist());
}

int getsymbol(Object* sym, int label) {
	printf("push rdi\n");
	printf("jmp L%d_skip\n", label);
	printf("L%d_str:\n", label);
	printf(".asciz \"%s\"\n", sym->symbol);
	printf("L%d_skip:\n", label);
	printf("lea rdi, [rip + L%d_str]\n", label);
	printf("call symbol\n");
	printf("pop rdi\n");
	return label + 1;
}

int applylist(Object* exp, int label) {
	if(exp) {
		printf("push rdi\n");
		printf("call car\n");
		printf("mov rdi, rax\n");
		printf("push rdi\n");
		int new_label = getsymbol(exp->list.car, label);
		printf("mov rdi, rax\n");
		printf("pop rsi\n");
		printf("call set_var\n");
		printf("pop rdi\n");
		printf("call cdr\n");
		printf("mov rdi, rax\n");
		return applylist(exp->list.cdr, new_label);
	}
	return label;
}

int gendata(Object* exp, int label) {
	if(!exp) {
		printf("xor eax, eax\n");
		return label;
	} else if(exp->type == SYMBOL) {
		return getsymbol(exp, label);
	} else {
		int new_label = gendata(exp->list.car, label);
		printf("push rax\n");
		int gen_label = gendata(exp->list.cdr, new_label);
		printf("mov rsi, rax\n");
		printf("pop rdi\n");
		printf("call cons\n");
		return gen_label;
	}
}

int compile(Object* exp, int label);

int gencond(Object* exp, int label, int prev_label, int endlabel) {
	if(exp) {
		printf("L%d:\n", prev_label);
		int comp_label = compile(exp->list.car->list.car, label);
		printf("test rax, rax\n");
		printf("jz L%d\n", exp->list.cdr ? comp_label : endlabel);
		int comp2_label = compile(exp->list.car->list.cdr->list.car, comp_label + 1);
		printf("jmp L%d\n", endlabel);
		return gencond(exp->list.cdr, comp2_label, comp_label, endlabel);
	}
	printf("L%d:\n", endlabel);
	return label;
}

int evlist(Object* exp, int label, _Bool first) {
	if(exp) {
		if(first) {
			int comp_label = compile(exp->list.car, label);
			printf("mov rdi, rax\n");
			printf("xor esi, esi\n");
			printf("call cons\n");
			printf("push rax\n");
			printf("add rax, 9\n");
			return evlist(exp->list.cdr, comp_label, 0);
		} else {
			printf("push rax\n");
			int comp_label = compile(exp->list.car, label);
			printf("mov rdi, rax\n");
			printf("xor esi, esi\n");
			printf("call cons\n");
			printf("pop rdi\n");
			printf("mov [rdi], rax\n");
			printf("add rax, 9\n");
			return evlist(exp->list.cdr, comp_label, 0);
		}
	}
	printf("xor esi, esi\n");
	if(!first) {
		printf("pop rsi\n");
	}
	printf("pop rdi\n");
	printf("call apply\n");
	return label;
}

int compile(Object* exp, int label) {
	if(!exp) {
		printf("xor eax, eax\n");
		return label;
	} else if ( exp->type == SYMBOL ) {
		int symbol_label = getsymbol(exp, label);
		printf("mov rdi, rax\n");
		printf("call get_val\n");
		return symbol_label;
	} else if (exp->list.car == symbol("quote")) {
		return gendata(exp->list.cdr->list.car, label);
	} else if (exp->list.car == symbol("cond")) {
		return gencond(exp->list.cdr, label + 2, label + 1, label);
	} else if (exp->list.car == symbol("lambda")) {
		printf("call L%d\n", label);
		int apply_label = applylist(exp->list.cdr->list.car, label + 1);
		int comp_label = compile(exp->list.cdr->list.cdr->list.car, apply_label);
		printf("ret\n");
		printf("L%d:\n", label);
		printf("pop rdi\n");
		printf("call gen_closure\n");
		return comp_label;
	} else {
		int comp_label = compile(exp->list.car, label);
		printf("push rax\n");
		return evlist(exp->list.cdr, comp_label, 1);
	}
}

int main() {
	printf(".globl app\napp:\n");
	peek = getchar();
	char token[64] = {0};
	gettoken(token);
	compile(token[0] == '(' ? getlist() : symbol(token), 0);
	printf("ret\n");
	return 0;
}
