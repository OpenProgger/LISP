#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Required Object types at compile-time
enum Type {
    SYMBOL,
    CELL
};

// Generic Object Declaration
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

// Last entry of the internal symbol list
Object* symbols;

// Peeked integer from input
int peek;

// Fetch Token from stdin
void gettoken(char* token) {
    int index = 0;

    // Skip any whitespaces and newline
    while(peek == ' ' || peek == '\n' || peek == '\t')
        peek = getchar();

    if (peek == '(' || peek == ')') {
        // Fetch only one char
        token[index++] = peek;
        peek = getchar();
    } else {

        // Read symbol name until a terminating char is reached
        while(peek != 0 && peek != ' ' && peek != '\n' && peek != '\t' && peek != '(' && peek != ')') {

            // Take terminating char if escaped
            if(peek == '\\') {
                peek = getchar();

                // Handle newline
                if (peek == 'n')
                    peek = '\n';
            }

            token[index++] = peek;
            peek = getchar();
        }
    }

    // Terminate token buffer
    token[index] = '\0';
}

// Create list object from given objects
Object* cons(Object* _car, Object* _cdr) {
    Object *_pair = calloc(1, sizeof(Object));
    *_pair = (Object) {.type = CELL, .list = {.car = _car, .cdr = _cdr}};
    return _pair;
}

// Get address of given string from symbol table or create a new one
Object* symbol(char *sym) {
    Object* _pair = symbols;
    for (;_pair; _pair = _pair->list.cdr)
        if (strncmp(sym, _pair->list.car->symbol, 64)==0)
            return _pair->list.car;

    // No matching string found, create new one and append to list
    Object *symbol = calloc(1, sizeof(Object));
    *symbol = (Object) {.type = SYMBOL, .symbol = strdup(sym)};
    symbols = cons(symbol, symbols);
    return symbol;
}

// Parse list from tokens
Object* getlist() {

    // Fetch first element for list
    char token[64] = {0};
    gettoken(token);

    // Return if end of list reached
    if (token[0] == ')')
        return 0;

    // Parse two elements and create new list from them
    Object* car = token[0] == '(' ? getlist() : symbol(token);
    return cons(car, getlist());
}

// Codegen for fetching symbols
int getsymbol(Object* sym, int label) {

    // Assembly code to fetch symbols at runtime
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

// Recursive function to bind values to their parameters
int applylist(Object* exp, int label) {
    if(exp) {

        // Take value from parameter
        printf("push rdi\n");
        printf("call car\n");
        printf("mov rdi, rax\n");
        printf("push rdi\n");

        // Fetch symbol name
        int new_label = getsymbol(exp->list.car, label);
        printf("mov rdi, rax\n");
        printf("pop rsi\n");

        // Create variable binding
        printf("call set_var\n");
        printf("pop rdi\n");
        printf("call cdr\n");
        printf("mov rdi, rax\n");
        return applylist(exp->list.cdr, new_label);
    }
    return label;
}

// Codegen for constants
int gendata(Object* exp, int label) {
    if(!exp) {

        // NULL if no data available
        printf("xor eax, eax\n");
        return label;
    } else if(exp->type == SYMBOL) {

        // Just fetch the symbol
        return getsymbol(exp, label);
    } else {

        // Process first element
        int new_label = gendata(exp->list.car, label);
        printf("push rax\n");

        // Process second element
        int gen_label = gendata(exp->list.cdr, new_label);
        printf("mov rsi, rax\n");
        printf("pop rdi\n");
        printf("call cons\n");
        return gen_label;
    }
}

int compile(Object* exp, int label);

// Generate conditionals with unique labels
int gencond(Object* exp, int label, int prev_label, int endlabel) {
    if(exp) {

        // Conditional label
        printf("L%d:\n", prev_label);
        int comp_label = compile(exp->list.car->list.car, label);

        // Check condition
        printf("test rax, rax\n");

        // Jump to next condition or to end if last condition
        printf("jz L%d\n", exp->list.cdr ? comp_label : endlabel);
        int comp2_label = compile(exp->list.car->list.cdr->list.car, comp_label + 1);

        // Jump to end after branch finished
        printf("jmp L%d\n", endlabel);
        return gencond(exp->list.cdr, comp2_label, comp_label, endlabel);
    }
    printf("L%d:\n", endlabel);
    return label;
}

// Evaluate parameter list
int evlist(Object* exp, int label, _Bool first) {
    if(exp) {

        // Save entry to first parameter on first run
        if(first) {
            int comp_label = compile(exp->list.car, label);
            printf("mov rdi, rax\n");
            printf("xor esi, esi\n");
            printf("call cons\n");
            printf("push rax\n");
            printf("add rax, 9\n");
            return evlist(exp->list.cdr, comp_label, 0);
        } else {

            // Chain remaining parameters to each other
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

    // Take parameter list and closure data to call it
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

        // NULL if no expression present
        printf("xor eax, eax\n");
        return label;
    } else if ( exp->type == SYMBOL ) {

        // Resolve symbol
        int symbol_label = getsymbol(exp, label);
        printf("mov rdi, rax\n");
        printf("call get_val\n");
        return symbol_label;
    } else if (exp->list.car == symbol("quote")) {

        // Generate constant data
        return gendata(exp->list.cdr->list.car, label);
    } else if (exp->list.car == symbol("cond")) {

        // Generate condition
        return gencond(exp->list.cdr, label + 2, label + 1, label);
    } else if (exp->list.car == symbol("lambda")) {

        // Create entry for function
        printf("call L%d\n", label);

        // Apply parameters
        int apply_label = applylist(exp->list.cdr->list.car, label + 1);
        int comp_label = compile(exp->list.cdr->list.cdr->list.car, apply_label);
        printf("ret\n");

        // Skip label to prevent early execution
        printf("L%d:\n", label);

        // Take address of function entry
        printf("pop rdi\n");
        printf("call gen_closure\n");
        return comp_label;
    } else {

        // Generate function call
        int comp_label = compile(exp->list.car, label);
        printf("push rax\n");
        return evlist(exp->list.cdr, comp_label, 1);
    }
}

int main() {

    // Prefix for GAS
    printf(".globl app\napp:\n");

    // Trigger parsing
    peek = getchar();
    char token[64] = {0};
    gettoken(token);

    // Generate Assembler output
    compile(token[0] == '(' ? getlist() : symbol(token), 0);
    printf("ret\n");
    return 0;
}
