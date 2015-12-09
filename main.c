#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>

#ifndef LITTLE_ENDIAN
    #define LITTLE_ENDIAN 0
#endif
#ifndef BIG_ENDIAN
    #define BIG_ENDIAN 1
#endif

#ifndef FORCE_INLINE
    #ifdef MSVC
        #define FORCE_INLINE __forceinline
    #else
        #define FORCE_INLINE __inline__ __attribute__((always_inline))
    #endif
#endif

#ifdef NDEBUG
#define LOG(fmt, ...) ((void)0)
#define REG(x) ((void)0)
#else
#define LOG(...) fprintf(stdout, __VA_ARGS__)
#define REG(x) reg_string(x)

char* reg_string(unsigned short r)
{
    static char str[6];
    switch (r)
    {
        case 32768:
            sprintf(str, "r0");
            break;
        case 32769:
            sprintf(str, "r1");
            break;
        case 32770:
            sprintf(str, "r2");
            break;
        case 32771:
            sprintf(str, "r3");
            break;
        case 32772:
            sprintf(str, "r4");
            break;
        case 32773:
            sprintf(str, "r5");
            break;
        case 32774:
            sprintf(str, "r6");
            break;
        case 32775:
            sprintf(str, "r7");
            break;
        default:
            sprintf(str, "");
            break;
    }

    return str;
}
#endif

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed char i8;
typedef signed short i16;
typedef signed int i32;
typedef int (*instruction_ptr)(u8* buffer);

struct Stack
{
    u16* data;
    u16 pointer;
};
void stack_init(struct Stack* stack)
{
    stack->data = malloc(0xFFFF);
    stack->pointer = 0;
}
void stack_deinit(struct Stack* stack)
{
    free(stack->data);
}
void stack_push(struct Stack* stack, u16 value)
{
    stack->data[stack->pointer] = value;
    stack->pointer = stack->pointer + 1;
}

u16 stack_pop(struct Stack* stack)
{
    u16 value = stack->data[stack->pointer - 1];
    stack->pointer = stack->pointer - 1;

    return value;
}

void print_bytes(unsigned char* buffer, int length)
{
    int i;
    for (i = 0; i < length; i++)
    {
        fprintf(stdout, "%#x\n", *(buffer + i));
    }
}

struct Memory
{
    u8* mem;
    u16 size;
};
void mem_init(struct Memory* mem)
{
    mem->size = 0x7FFF;
    mem->mem = malloc(sizeof(u16) * mem->size);
    memset(mem->mem, 0, sizeof(u16) * mem->size);
}
void mem_deinit(struct Memory* mem)
{
    free(mem->mem);
    mem->size = 0;
}
void mem_set(struct Memory* mem, u16 addr, u16 value)
{
    addr = addr * 2;
#if BIG_ENDIAN
    mem->mem[addr] = value;
    mem->mem[addr + 1] = value >> 8;
#elif LITTLE_ENDIAN
    mem->mem[addr] = value >> 8;
    mem->mem[addr + 1] = value;
#else
#   error Mixed Endianess not supported
#endif
}

/* declared here, because it's used here */
u8* get_next_pair(u8* buffer, u16* pair);
u16 mem_get(struct Memory* mem, u16 addr)
{
    u16 pair;

    addr = addr * 2;
    get_next_pair(mem->mem + addr, &pair);
    return pair;
}

/* Stack and registers are static */
struct Stack stack;
struct Memory memory;
u16 rPC = 0;
u16 r0 = 0;
u16 r1 = 0;
u16 r2 = 0;
u16 r3 = 0;
u16 r4 = 0;
u16 r5 = 0;
u16 r6 = 0;
u16 r7 = 0;

/* main procedures */
int parse(char const* filename);
int operate(u16 opcode, u8* buffer);

/* assistant procedures */
int is_valid(u16 opcode);
int is_literal(u16 opcode);
int is_register(u16 opcode);
u16* get_register(u16 opcode);
instruction_ptr get_instruction(u16 opcode);

/* instruction procedures */
int instruction_halt(u8* buffer);
int instruction_set(u8* buffer);
int instruction_push(u8* buffer);
int instruction_pop(u8* buffer);
int instruction_eq(u8* buffer);
int instruction_gt(u8* buffer);
int instruction_jmp(u8* buffer);
int instruction_jt(u8* buffer);
int instruction_jf(u8* buffer);
int instruction_add(u8* buffer);
int instruction_mult(u8* buffer);
int instruction_mod(u8* buffer);
int instruction_and(u8* buffer);
int instruction_or(u8* buffer);
int instruction_not(u8* buffer);
int instruction_rmem(u8* buffer);
int instruction_wmem(u8* buffer);
int instruction_call(u8* buffer);
int instruction_ret(u8* buffer);
int instruction_out(u8* buffer);
int instruction_in(u8* buffer);
int instruction_noop(u8* buffer);

int main(int argc, char* argv[])
{
    int ret;
    if (argc < 2)
    {
        fprintf(stdout, "Usage: %s <file>\n", argv[0]);
        return 0;
    }

    stack_init(&stack);
    mem_init(&memory);

    ret = parse(argv[1]);

    mem_deinit(&memory);
    stack_deinit(&stack);

    return ret;
}

int parse(char const* filename)
{
    FILE* file;
    long filesize;
    int ret;
    size_t size_read;
    u16 opcode;
    u8* buffer;

    file = fopen(filename, "rb");
    if (!file)
    {
        fprintf(stderr, "Error: Opening File: %s\n", filename);
        return 1;
    }

    if (fseek(file, 0, SEEK_END))
    {
        fprintf(stderr, "Error: Opening File: %s\n", filename);
        fclose(file);
        return 1;
    }
    if ((filesize = ftell(file)) == EOF)
    {
        fprintf(stderr, "Error: Opening File: %s\n", filename);
        fclose(file);
        return 1;
    }
    if (fseek(file, 0, SEEK_SET))
    {
        fprintf(stderr, "Error: Opening File: %s\n", filename);
        fclose(file);
        return 1;
    }

    size_read = fread(memory.mem, 1, filesize, file);
    if (size_read != filesize)
    {
        fprintf(stderr, "Error: Reading File: %s\n", filename);
        fclose(file);
        return 1;
    }

    buffer = memory.mem;
    rPC = 0;
    for (;;)
    {
        get_next_pair(buffer + rPC, &opcode);
        ret = operate(opcode, buffer + rPC + 2);
        if (ret == -1) { goto end; }

        rPC = ret;
    }

end:
    fclose(file);
    return 0;
}

int operate(u16 opcode, unsigned char* buffer)
{
    instruction_ptr instr;
    int next_instr;

    instr = get_instruction(opcode);
    if (instr == NULL) { LOG("opcode: %d\n", opcode); }
    assert(instr != NULL);
    next_instr = instr(buffer);
    if (next_instr == -1) { return -1; }

    return next_instr;
}

int is_valid(u16 opcode)
{
    return opcode < 32776;
}

int is_literal(u16 opcode)
{
    return opcode < 32768;
}

int is_register(u16 opcode)
{
    return opcode >= 32768 && opcode <= 32775;
}

u16* get_register(u16 opcode)
{
    switch (opcode)
    {
        case 32768:
            return &r0;
        case 32769:
            return &r1;
        case 32770:
            return &r2;
        case 32771:
            return &r3;
        case 32772:
            return &r4;
        case 32773:
            return &r5;
        case 32774:
            return &r6;
        case 32775:
            return &r7;
        default:
            return NULL;
    }
}

instruction_ptr get_instruction(u16 opcode)
{
    switch (opcode)
    {
        case 0:
            return &instruction_halt;
        case 1:
            return &instruction_set;
        case 2:
            return &instruction_push;
        case 3:
            return &instruction_pop;
        case 4:
            return &instruction_eq;
        case 5:
            return &instruction_gt;
        case 6:
            return &instruction_jmp;
        case 7:
            return &instruction_jt;
        case 8:
            return &instruction_jf;
        case 9:
            return &instruction_add;
        case 10:
            return &instruction_mult;
        case 11:
            return &instruction_mod;
        case 12:
            return &instruction_and;
        case 13:
            return &instruction_or;
        case 14:
            return &instruction_not;
        case 15:
            return &instruction_rmem;
        case 16:
            return &instruction_wmem;
        case 17:
            return &instruction_call;
        case 18:
            return &instruction_ret;
        case 19:
            return &instruction_out;
        case 20:
            return &instruction_in;
        case 21:
            return &instruction_noop;
        default:
            return NULL;
    }

    return NULL;
}

u8* get_next_pair(u8* buffer, u16* pair)
{
#if BIG_ENDIAN
    *pair = (*(buffer + 1) << 8) + *buffer;
#elif LITTLE_ENDIAN
    *pair = (*buffer << 8) + *(buffer + 1);
#endif

    return buffer + 2;
}

int instruction_halt(u8* buffer)
{
    return -1;
}

int instruction_set(u8* buffer)
{
    u16 next;
    u16* a;

    buffer = get_next_pair(buffer, &next);
    LOG("set: %d", next);
    a = get_register(next);
    buffer = get_next_pair(buffer, &next);
    if (is_register(next)) next = *get_register(next);
    LOG(" %d\n", next);

    *a = next;

    return rPC + 2 + 4;
}

int instruction_push(u8* buffer)
{
    u16 next;

    buffer = get_next_pair(buffer, &next);
    if (is_register(next)) { next = *get_register(next); }
    stack_push(&stack, next);
    LOG("push: %d\n", next);

    return rPC + 2 + 2;
}

int instruction_pop(u8* buffer)
{
    u16 next;
    u16* a;

    buffer = get_next_pair(buffer, &next);
    a = get_register(next);
    *a = stack_pop(&stack);

    LOG("pop: %d\n", *a);
    return rPC + 2 + 2;
}

int instruction_eq(u8* buffer)
{
    u16 next;
    u16* a;
    u16 b, c;

    buffer = get_next_pair(buffer, &next);
    LOG("%d: eq: %s", rPC, REG(next));
    a = get_register(next);
    buffer = get_next_pair(buffer, &b);
    LOG(" %s", REG(b));
    if (is_register(b)) { b = *get_register(b); }
    LOG("(%d)", b);
    buffer = get_next_pair(buffer, &c);
    LOG(" %s", REG(c));
    if (is_register(c)) { c = *get_register(c); }
    LOG("(%d)\n", c);

    if (b == c) { *a = 1; }
    else { *a = 0; }

    return rPC + 2 + 6;
}

int instruction_gt(u8* buffer)
{
    u16 next;
    u16* a;
    u16 b, c;

    buffer = get_next_pair(buffer, &next);
    LOG("gt: %d", next);
    a = get_register(next);
    buffer = get_next_pair(buffer, &b);
    if (is_register(b)) { b = *get_register(b); }
    buffer = get_next_pair(buffer, &c);
    if (is_register(c)) { c = *get_register(c); }

    if (b > c) { *a = 1; }
    else { *a = 0; }

    LOG(" %d %d\n", b, c);
    return rPC + 2 + 6;
}

int instruction_jmp(u8* buffer)
{
    u16 next;

    buffer = get_next_pair(buffer, &next);
    if (is_register(next)) next = *get_register(next);
    LOG("jmp: %d\n", next);

    return (next * 2);
}

int instruction_jt(u8* buffer)
{
    u16 a, b;

    buffer = get_next_pair(buffer, &a);
    LOG("jt: %d", a);
    if (is_register(a)) { a = *get_register(a); }

    buffer = get_next_pair(buffer, &b);
    if (is_register(b)) { b = *get_register(b); }

    LOG(" %d\n", b);
    if (a != 0) { return (b * 2); }
    else { return rPC + 2 + 4; }
}

int instruction_jf(u8* buffer)
{
    u16 a, b;

    buffer = get_next_pair(buffer, &a);

    LOG("jf: %d", a);
    if (is_register(a)) { a = *get_register(a); }

    buffer = get_next_pair(buffer, &b);
    if (is_register(b)) { b = *get_register(b); }

    LOG(" %d\n", b);
    if (a == 0) { return (b * 2); }
    else { return rPC + 2 + 4; }
}

int instruction_add(u8* buffer)
{
    u16 next;
    u16* a;
    u16 b, c;

    buffer = get_next_pair(buffer, &next);
    a = get_register(next);

    LOG("add: %d ", next);
    buffer = get_next_pair(buffer, &b);
    if (is_register(b)) { b = *get_register(b); }

    buffer = get_next_pair(buffer, &c);
    if (is_register(c)) { c = *get_register(c); }

    *a = (b + c) % 32768;
    LOG("%d %d\n", b, c);

    return rPC + 2 + 6;
}

int instruction_mult(u8* buffer)
{
    u16 next;
    u16* a;
    u16 b, c;

    buffer = get_next_pair(buffer, &next);
    a = get_register(next);

    LOG("mult: %d ", next);
    buffer = get_next_pair(buffer, &b);
    if (is_register(b)) { b = *get_register(b); }

    buffer = get_next_pair(buffer, &c);
    if (is_register(c)) { c = *get_register(c); }

    *a = (b * c) % 32768;
    LOG("%d %d\n", b, c);

    return rPC + 2 + 6;
}

int instruction_mod(u8* buffer)
{
    u16 next;
    u16* a;
    u16 b, c;

    buffer = get_next_pair(buffer, &next);
    a = get_register(next);

    LOG("mod: %d ", next);
    buffer = get_next_pair(buffer, &b);
    if (is_register(b)) { b = *get_register(b); }

    buffer = get_next_pair(buffer, &c);
    if (is_register(c)) { c = *get_register(c); }

    *a = (b % c);
    LOG("%d %d\n", b, c);

    return rPC + 2 + 6;
}

int instruction_and(u8* buffer)
{
    u16 next;
    u16* a;
    u16 b, c;

    buffer = get_next_pair(buffer, &next);

    LOG("and: %d", next);
    a = get_register(next);

    buffer = get_next_pair(buffer, &b);
    if (is_register(b)) { b = *get_register(b); }

    buffer = get_next_pair(buffer, &c);
    if (is_register(c)) { c = *get_register(c); }

    LOG(" %d %d\n", b, c);
    *a = b & c;

    return rPC + 2 + 6;
}

int instruction_or(u8* buffer)
{
    u16 next;
    u16* a;
    u16 b, c;

    buffer = get_next_pair(buffer, &next);

    LOG("or: %d", next);
    a = get_register(next);

    buffer = get_next_pair(buffer, &b);
    if (is_register(b)) { b = *get_register(b); }

    buffer = get_next_pair(buffer, &c);
    if (is_register(c)) { c = *get_register(c); }

    LOG(" %d %d\n", b, c);
    *a = b | c;

    return rPC + 2 + 6;
}

int instruction_not(u8* buffer)
{
    u16 next;
    u16* a;
    u16 b;

    buffer = get_next_pair(buffer, &next);

    LOG("not: %d", next);
    a = get_register(next);

    buffer = get_next_pair(buffer, &b);
    if (is_register(b)) { b = *get_register(b); }

    LOG(" %d\n", b);
    *a = ~b;
    *a ^= (1 << 15);

    return rPC + 2 + 4;
}

int instruction_rmem(u8* buffer)
{
    u16 next;
    u16 *a, b;

    buffer = get_next_pair(buffer, &next);
    LOG("%d: rmem: %d", rPC, next);
    a = get_register(next);

    buffer = get_next_pair(buffer, &b);
    LOG(" %d\n", b);
    if (is_register(b)) b = *get_register(b);

    *a = mem_get(&memory, b);

    return rPC + 2 + 4;
}

int instruction_wmem(u8* buffer)
{
    u16 a, b;

    buffer = get_next_pair(buffer, &a);
    LOG("wmem: %d", a);
    if (is_register(a)) a = *get_register(a);

    buffer = get_next_pair(buffer, &b);
    LOG(" %d\n", b);
    if (is_register(b)) b = *get_register(b);

    mem_set(&memory, a, b);

    return rPC + 2 + 4;
}

int instruction_call(u8* buffer)
{
    u16 a;

    buffer = get_next_pair(buffer, &a);
    if (is_register(a)) { a = *get_register(a); }

    LOG("call: %d\n", a);
    stack_push(&stack, (rPC + 2 + 2) / 2);

    return (a * 2);
}

int instruction_ret(u8* buffer)
{
    u16 addr;

    LOG("ret\n");
    addr = stack_pop(&stack);

    return (addr * 2);
}

int instruction_out(u8* buffer)
{
    u16 next;

    buffer = get_next_pair(buffer, &next);
    if (is_register(next)) next = *get_register(next);
    fprintf(stdout, "%c", next);

    return rPC + 2 + 2;
}

int instruction_in(u8* buffer)
{
    char in;
    u16 addr;
    u16* reg;

    reg = NULL;
    get_next_pair(buffer, &addr);
    reg = get_register(addr);
    LOG("in: %d\n", addr);

    in = getchar();
    *reg = in;

    return rPC + 2 + 2;
}

int instruction_noop(u8* buffer)
{
    return rPC + 2;
}
