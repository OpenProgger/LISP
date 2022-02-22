# Lisp Runtime
# r12: Heap ptr
# r13: Environment ptr
# r14: Symbol ptr
# r15: Peek char

.globl _start, gen_closure, apply, get_val, set_var, symbol, car, cdr, cons
_start:
    sub rsp, 4194304
    mov r12, rsp                # Heap: 4 MB
    xor r13, r13                # Reset Environment
    xor r14, r14                # Reset symbol table
    mov r15, 0x20               # Reset peek char
    lea rdi, [rip + proc_atom]  # Get address for atom function
    mov rsi, 0x6D6F7461         # 'atom'
    call gen_proc               # Create atom closure
    lea rdi, [rip + proc_eq]    # Get address for eq function
    mov rsi, 0x7165             # 'eq'
    call gen_proc               # Create eq closure
    lea rdi, [rip + proc_car]   # Get address for car function
    mov rsi, 0x726163           # 'car'
    call gen_proc               # Create car closure
    lea rdi, [rip + proc_cdr]   # Get address for cdr function
    mov rsi, 0x726463           # 'cdr'
    call gen_proc               # Create cdr closure
    lea rdi, [rip + proc_cons]  # Get address for cons function
    mov rsi, 0x736E6F63         # 'cons'
    call gen_proc               # Create cons closure
    lea rdi, [rip + proc_print] # Get address for print function
    mov rsi, 0x746E697270       # 'print'
    call gen_proc               # Create print closure
    lea rdi, [rip + proc_read]  # Get address for read function
    mov rsi, 0x64616572         # 'read'
    call gen_proc               # Create read closure
    call app                    # Run compiled binary
    mov eax, 60                 # Call OS-Exit
    xor rdi, rdi
    syscall

gen_proc:
    push rsi                    # Preserve string buffer
    call gen_closure            # Create closure
    mov rdi, rsp                # Take address to string buffer
    push rax                    # Preserve closure address
    call symbol                 # Create Symbol
    mov rdi, rax                # Take symbol
    pop rsi                     # Take closure address
    pop rax                     # Delete string buffer
    call set_var                # Bind closure to symbol
    ret

gen_closure:                    # Create closure with current environment
    mov rsi, r13                # Catch current environment
    call cons                   # Bind procedure to current environment
    mov cl, 2
    mov [rax], cl               # Apply Function(2) tag
    ret

apply:
    push r13                    # Create new environment frame
    push rsi                    # Preserve parameter
    push rdi                    # Preserve closure
    call cdr                    # Take environment from closure
    pop rdi                     # Take closure
    mov r13, rax                # Set catched environment frame
    call car                    # Get procedure address
    pop rdi                     # Take parameter
    call rax                    # Call procedure
    pop r13                     # Closre environment frame
    ret

set_var:                        # Create new symbol mapping for current Environment Frame
    call cons                   # Create mapping
    mov cl, 3
    mov [rax], cl               # Apply Environment Map(3) tag
    mov rdi, rax                # Apply new mapping
    mov rsi, r13                # Chain to previous mapping
    call cons                   # Create mapping
    mov r13, rax                # Set new head address
    ret

get_val:                        # Get value of given variable
    push rdi                    # Preserve search criteria
    mov rcx, r13                # Set iteration pointer
search_loop:
    test rcx, rcx
    jz search_exit              # End of environment reached(shouldn't happen)
    mov rdi, rcx
    call car                    # Access entry
    mov rdi, rax
    call car                    # Take symbol address
    mov rdi, [rsp]
    cmp rax, rdi
    je search_loop_end          # Exit on match
    mov rdi, rcx
    call cdr                    # Move to next entry
    mov rcx, rax
    jmp search_loop             # Continue search if search criteria is not matching
search_loop_end:
    mov rdi, rcx
    call car                    # Access entry
    mov rdi, rax
    call cdr                    # Return value for symbol
search_exit:
    pop rdi                     # Clear stack
    ret

symbol:                         # Get/Create Symbol
    push rdi                    # Save search criteria
    mov rax, r14                # Start iteration
search_sym_loop:
    test rax, rax
    jz search_sym_loop_end      # Exit if no symbols left
    mov rdi, rax
    push rax                    # Save current iteration
    call car                    # Take symbol address
    mov rsi, rax
    inc rsi                     # Skip tag
    pop rax
    mov rdi, [rsp]              # Take search criteria
    push rax
    call cmp                    # Compare buffers
    test rax, rax
    jz no_match                 # Continue iteration if no match
    pop rax                     # Restore iteration
    mov rdi, rax
    call car                    # Return symbol address
    pop rdi
    ret
no_match:
    pop rax                     # Restore iteration
    mov rdi, rax
    call cdr                    # Move to next entry
    jmp search_sym_loop
search_sym_loop_end:
    pop rdi                     # Take string buffer
    call dup                    # Create symbol object
    mov rdi, rax
    mov rsi, r14
    call cons                   # Create entry
    mov r14, rax                # Apply new head of symbols
    mov rdi, rax
    call car                    # Return symbol address
    ret

cmp:                            # Compare 2 strings
    mov al, [rdi]               # Read bytes
    mov bl, [rsi]
    inc rdi                     # Move to next byte
    inc rsi
    cmp al, bl
    jne differ                  # Exit if not equal
    test al, al
    jnz cmp                     # Equality if termination reached
    mov eax, 1                  # Return equal
    ret
differ:
    xor eax, eax                # Return unequal
    ret

dup:                            # Create string object from buffer
    push r12                    # Save new address
    xor eax, eax
    mov [r12], al               # Apply Symbol(0) tag
    inc r12
copy_loop:
    mov al, [rdi]               # Take byte
    inc rdi
    mov [r12], al               # Save byte
    inc r12
    test al, al
    jnz copy_loop               # Terminate on 0
    pop rax                     # Return to new object
    ret

cons:                           # Create list from 2 elements
    mov al, 1
    mov [r12], al               # Apply List(1) tag
    mov [r12 + 1], rdi          # Set first element
    mov [r12 + 9], rsi          # Set second element
    mov rax, r12                # Return address to new object
    add r12, 17
    ret

car:                            # Take first element from list
    mov rax, [rdi + 1]
    ret

cdr:                            # Take elements except of the first one from list
    mov rax, [rdi + 9]
    ret

proc_atom:                      # Check if given item is an atom
    call car                    # Access parameter
    mov bl, [rax]               # Take tag
    cmp bl, 1
    jne is_atom                 # Check for list tag
    xor eax, eax                # Return false on list
    ret
is_atom:
    mov ax, 0x74                # 't'
    push ax
    mov rdi, rsp                # Take string buffer address
    call symbol                 # Create truthly symbol
    pop di                      # Clear stack
    ret

proc_eq:                        # Compare symbols
    push rdi                    # Preserve parameter
    call car                    # Access first parameter
    pop rdi                     # Take parameters
    push rax                    # Preserve first parameter
    call cdr
    mov rdi, rax
    call car                    # Access second parameter
    mov rdi, rax
    pop rsi                     # Take first parameter
    cmp rdi, rsi
    je equal                    # Same address is also equal
    test rdi, rdi
    jz unequal
    test rsi, rsi
    jz unequal
    inc rdi                     # Skip tags
    inc rsi
    call cmp                    # Compare
    test rax, rax
    jnz equal
unequal:
    xor eax, eax                # Return unequal
    ret
equal:
    mov ax, 0x74                # 't'
    push ax
    mov rdi, rsp                # Take string buffer address
    call symbol                 # Create truthly symbol
    pop di                      # Clear stack
    ret

proc_car:                       # car procedure
    call car                    # Access parameter
    mov rdi, rax
    call car                    # Return first element
    ret

proc_cdr:                       # cdr procedure
    call car                    # Access parameter
    mov rdi, rax
    call cdr                    # Restur rest of list
    ret

proc_cons:                      # cons procedure
    push rdi                    # Preserve parameter
    call car                    # Access first parameter
    pop rdi                     # Take parameters
    push rax                    # Preserve first parameter
    call cdr
    mov rdi, rax
    call car                    # Access second parameter
    pop rdi                     # Take first parameter
    mov rsi, rax
    call cons                   # Create new list
    ret

proc_print:                     # print procedure
    call car                    # Access first parameter
    mov rdi, rax
    mov esi, 1                  # Mark as first element
    call print                  # Print object
    xor eax, eax                # Always return empty list
    ret

print:
    test rdi, rdi
    jnz take_tag
    mov rdi, 0x28
    call write
    mov rdi, 0x29
    call write
    xor eax, eax
    ret
take_tag:
    mov bl, [rdi]               # Take tag
    cmp bl, 1
    je is_pair                  # Check for list tag
    inc rdi                     # Skip tag
    call print_symbol           # Print symbol
    ret
is_pair:
    push rdi                    # Preserve parameter
    test rsi, rsi
    jz no_head                  # Check for first element
    mov rdi, 0x28               # '('
    call write
no_head:
    mov rdi, [rsp]
    call car                    # Access parameter
    mov rdi, rax
    mov esi, 1
    call print                  # Print first element
    pop rdi                     # Take parameter
    call cdr                    # Take second parameter
    test rax, rax
    jz end_list                 # Don't print if empty
    push rax                    # Preserve element
    mov rdi, 0x20               # ' '
    call write
    pop rdi                     # Take element
    xor esi, esi
    call print
    ret
end_list:
    mov rdi, 0x29               # ')'
    call write
    ret

print_symbol:                   # Print symbol name
    xor eax, eax                # Clear register for usage
    mov al, [rdi]               # Read byte
    test al, al
    jz print_symbol_end         # Terminate on 0
    push rdi                    # Preserve buffer address
    mov edi, eax
    call write                  # Print char
    pop rdi                     # Restore buffer address
    inc rdi                     # Move to next char
    jmp print_symbol
print_symbol_end:
    ret

proc_read:                      # Read expression
    sub rsp, 32                 # Make 32-byte string buffer
    mov rdi, rsp
    call gettoken               # Get first token
    mov al, [rsp]
    cmp al, 0x28                # '('
    jne read_token              # Parse list on paranthese
    add rsp, 32                 # Clear buffer
    call getlist                # Parse list
    ret
read_token:
    mov rdi, rsp
    call symbol                 # Make symbol from token
    add rsp, 32                 # Clear buffer
    ret

gettoken:                       # Get next token
    push rdi
    xor rcx, rcx                # Reset index
whitespace_loop:
    cmp r15, 0x20               # ' '
    je skip_byte                # Skip byte on whitespace
    cmp r15, 0x9                # '\t'
    je skip_byte
    cmp r15, 0xA
    jne whitespace_loop_end
skip_byte:
    push rcx
    call read                   # Read next byte
    pop rcx
    mov r15, rax                # Save peek char
    jmp whitespace_loop
whitespace_loop_end:
    cmp r15, 0x28               # '('
    je peek_one                 # Save only one byte on list
    cmp r15, 0x29               # ')'
    je peek_one
read_byte_loop:
    test r15, r15
    jz terminate_token          # Exit on null
    cmp r15, 0x20               # ' '
    je terminate_token          # Exit on non-symbol char
    cmp r15, 0x9                # '\t'
    je terminate_token
    cmp r15, 0xA                # '\n'
    je terminate_token
    cmp r15, 0x28               # '('
    je terminate_token
    cmp r15, 0x29               # ')'
    je terminate_token
    cmp r15, 0x5C               # '\'
    jne no_escape
    push rcx
    call read
    pop rcx
    cmp rax, 0x6E               # 'n'
    jne no_newline
    mov rax, 0xA                # '\n'
no_newline:
    mov r15, rax
no_escape:
    mov rdi, [rsp]              # Take string buffer
    mov [rdi + rcx], r15b       # Save peek char
    inc rcx                     # Increment index
    push rcx                    # Preserve counter
    call read                   # Read next byte
    pop rcx                     # Restore counter
    mov r15, rax                # Save new peek char
    jmp read_byte_loop
peek_one:
    mov rdi, [rsp]              # Take string buffer
    mov [rdi + rcx], r15b       # Save peek char
    inc rcx                     # Increment index
    push rcx                    # Preserve counter
    call read                   # Read next byte
    pop rcx                     # Restore counter
    mov r15, rax                # Save new peek char
terminate_token:
    xor eax, eax
    pop rdi
    mov [rdi + rcx], al         # Save termination byte
    ret

getlist:                        # Parse list
    sub rsp, 32                 # Make 32-byte string buffer
    mov rdi, rsp
    call gettoken               # Get next token
    mov al, [rsp]               # Take first byte
    cmp al, 0x29                # ')'
    jne make_list               # Check if list has ended
    add rsp, 32
    xor eax, eax                # Return empty list
    ret
make_list:
    cmp al, 0x28                # '('
    jne take_token              # Check for next list
    add rsp, 32
    call getlist                # Parse list
    jmp take_token_end
take_token:
    mov rdi, rsp
    call symbol                 # Create symbol
    add rsp, 32
take_token_end:
    push rax                    # Preserve first element
    call getlist                # Parse next list
    pop rdi                     # Take first element
    mov rsi, rax
    call cons                   # Create and return new list
    ret

write:                          # OS-Write
    push di
    mov eax, 1
    mov edi, eax
    mov rsi, rsp
    mov edx, eax
    syscall
    pop di
    ret

read:                           # OS-Read
    xor eax, eax
    xor edi, edi
    push rax
    mov rsi, rsp
    mov edx, 1
    syscall
    pop rax
    ret
