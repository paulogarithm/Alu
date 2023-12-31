/**
 * Copyright (C) 2023 Paul Parisot
 *
 * This software has no warranty.
 * See the LICENSE file for more informations.
 */

#include <stddef.h>
#include <stdlib.h>
#include <iso646.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <stdio.h>

/**
 *
 * @category Defines
 *
 */

#define null NULL
#define false (_Bool)0
#define true (_Bool)1

#define remove(pointer)  \
    if (pointer != null) \
        free(pointer);

#define raise(errnum, val)                    \
    {                                         \
        printf("[ERROR] in %s (%s:%d) %d \n", \
               __FUNCTION__,                  \
               __FILE__, __LINE__,            \
               errnum);                       \
        return val;                           \
    }                                         \
    while (0)                                 \
        ;

#define debug(A, msg, ...) \
    if (A->verbose)        \
    printf(msg, ##__VA_ARGS__)

#define ALU_VER_MAJ 0
#define ALU_VER_MIN 2
#define ALU_VER_NUM (ALU_VER_MAJ * 100 + ALU_VER_MIN)

#define ALU_SIGNATURE "\x1b\xca\xca"

/**
 *
 * @category Typedefs
 *
 */

typedef uint8_t alu_Byte;
typedef double alu_Number;
typedef char *alu_String;
typedef uint32_t alu_Size;

typedef enum
{
    AERR_IDK = 0, // Basic error.
    AERR_NOMEM,   // No memory left.
    AERR_STKLN,   // Stack too small.
    AERR_NOREG,   // No such register.
    AERR_NOSTK,   // No such element in stack.
    AERR_NOFND,   // Not found.
    AERR_TYPES,   // Invalid combination of types.
    AERR_OUTJM,   // Out jump the conditions.
    AERR_NOFIL,   // File doesnt exist.

    AERR_CREAD, // C Invalid read.
    AERR_CSTAT, // C Stat failure.
} alu_Errno;

typedef enum
{
    ALU_NULL = 0, // Nothing.
    ALU_NUMBER,   // A double.
    ALU_STRING,   // An allocated pointer to a character array.
    ALU_BOOL,     // A value which is either true or false.
    ALU_ABSTRACT, // A non-allocated C pointer.
    ALU_INST,     // A pointer to the first instruction.
} alu_Type;

typedef enum
{
    // General
    OP_HALT = 0x00,
    OP_RET,

    // Jumps
    OP_JMP,  // Jump
    OP_JTR,  // Jump if s0 == true
    OP_JFA,  // Jump if s0 == false
    OP_JEM,  // Jump if stack is empty
    OP_JNEM, // Jump if something is in the stack

    // Stack push
    OP_PUSHNUM,
    OP_PUSHSTR,
    OP_PUSHBOOL,
    OP_PUSHDEF,

    // Stack Manip
    OP_SUMSTACK,
    OP_STACKCLOSE,
    OP_EVAL,
    OP_SUPER,

    // Functions
    OP_CALL,

    // Registers
    OP_LOAD,
    OP_UNLOAD,
    OP_DEFUNLOAD,

    // End
    OP_END
} alu_Opcode;

typedef enum
{
    EVAL_EQUALS = (1 << 0),
    EVAL_SMALLER = (1 << 1),
    EVAL_GREATER = (1 << 2)
} alu_Eval;

typedef struct
{
    void *data;
    alu_Type type;
} alu_Variable;

typedef struct s_stack2
{
    void *data;
    struct s_stack2 *next;
    struct s_stack2 *previous;
    struct s_stack2 *top;
} alu_Stack;

typedef struct
{
    alu_String error;
    alu_Stack *stack;
    alu_Stack *garbage;
    alu_Stack *regs;
    alu_Stack *instructions;

    alu_Size seed;
    _Bool verbose;
} alu_State;

typedef struct
{
    alu_Size index;
    alu_Variable *var;
} alu_Register;

typedef void (*func0_t)(void *A);                   // 0
typedef void (*func1_t)(void *A, alu_Size);         // 1
typedef void (*func2_t)(void *A, alu_Number);       // 2
typedef void (*func3_t)(void *A, const alu_String); // 3
typedef void (*func4_t)(void *A, alu_Byte);         // 4

typedef struct
{
    void *func;
    alu_Byte argument;
} alu_StructOpcode;

typedef struct
{
    alu_String name;
    void *f;
} alu_Def;

/**
 *
 * @category Static arrays & Declarations
 *
 */

/* Op Code functions */

void Alu_stackclose(alu_State *A);
void Alu_sumstack(alu_State *A);
void Alu_load(alu_State *A, alu_Size);
void Alu_unload(alu_State *A, alu_Size);
void Alu_wait(alu_State *A, alu_Size);
void Alu_pushnumber(alu_State *A, alu_Number);
void Alu_pushstring(alu_State *A, const alu_String);
void Alu_pushbool(alu_State *A, _Bool);
void Alu_eval(alu_State *A, alu_Byte);
void Alu_pushdef(alu_State *A, alu_String str);
void Alu_call(alu_State *A);
void Alu_super(alu_State *A);

static const alu_StructOpcode F[] = {
    [OP_HALT] = {null, 0},
    [OP_STACKCLOSE] = {Alu_stackclose, 0},
    [OP_SUMSTACK] = {Alu_sumstack, 0},
    [OP_CALL] = {Alu_call, 0},
    [OP_SUPER] = {Alu_super, 0},
    [OP_LOAD] = {Alu_load, 1},
    [OP_UNLOAD] = {Alu_unload, 1},
    [OP_PUSHNUM] = {Alu_pushnumber, 2},
    [OP_PUSHSTR] = {Alu_pushstring, 3},
    [OP_PUSHDEF] = {Alu_pushdef, 3},
    [OP_PUSHBOOL] = {Alu_pushbool, 4},
    [OP_EVAL] = {Alu_eval, 4},
};

/* String Conversion Functions */

void __Alu_btoa(alu_Variable *var);
void __Alu_nulltoa(alu_Variable *var);
void __Alu_ntoa(alu_Variable *var);
void __Alu_abstracttoa(alu_Variable *var);

static const void *CONVERT_STRING[] = {
    [ALU_NULL] = __Alu_nulltoa,
    [ALU_NUMBER] = __Alu_ntoa,
    [ALU_STRING] = null,
    [ALU_BOOL] = __Alu_btoa,
    [ALU_ABSTRACT] = __Alu_abstracttoa,
};

/* Call Def Functions */

void Alu_print(alu_State *);
void Alu_wait(alu_State *, alu_Size);

static const alu_Def DEF[] = {
    {"print", Alu_print},
    {"wait", Alu_wait},
    {null, null},
};

/**
 *
 * @category My C functions
 *
 */

// Cuts a string from `from` to `to`.
char *strcut(const char *str, size_t from, size_t to)
{
    size_t size = to - from;
    char *buf = (char *)malloc((size + 1) * sizeof(char));
    if (buf == null)
        raise(AERR_NOMEM, null);
    memset(buf, 0, size + 1);
    if (buf == null)
        return null;
    // for (size_t n = 0; str[n + from] != '\0' and n < size; ++n)
    for (size_t n = 0; n < size; ++n)
        buf[n] = str[n + from];
    buf[size] = '\0';
    return buf;
}

/// Reads the int value from a byte array.
/// `00 00 0c 7a -> (int) 3194`
int bytesint(const alu_Byte *bytes)
{
    int result = 0;
    for (unsigned short i = 0; i < sizeof(int); ++i)
        result = (result << 8) | bytes[i];
    return result;
}

/// Reads the double value from a byte array.
double bytesdouble(const unsigned char *bytes)
{
    union
    {
        double d;
        uint64_t u;
    } value;
    value.u = 0;
    int shift = 0;
    for (int i = sizeof(double) - 1; i >= 0; i--)
        value.u |= ((uint64_t)bytes[i] << (shift++ * 8));
    return value.d;
}

void strrev(char *str)
{
    char swap = '\0';
    for (size_t a = 0, b = strlen(str) - 1; a < b; ++a, --b)
    {
        swap = str[a];
        str[a] = str[b];
        str[b] = swap;
    }
}

/**
 *
 * @category Stack2 functions
 *
 */

// Push data in a stack.
void Stack_push(alu_Stack **stack, void *data)
{
    alu_Stack *slate = (alu_Stack *)malloc(sizeof(alu_Stack));
    if (slate == null)
        raise(AERR_NOMEM,);
    memset(slate, 0, sizeof(alu_Stack));
    slate->previous = (*stack != null ? (*stack)->top : null);
    slate->next = null;
    slate->data = data;
    if (*stack == null)
        *stack = slate;
    else
        (*stack)->top->next = slate;
    (*stack)->top = slate;
}

// Get the stack len from a pointer.
alu_Size Stack_len(alu_Stack *from)
{
    alu_Size n = 0;
    if (from == null)
        return n;
    for (; from->next != null; ++n)
        from = from->next;
    return n + 1;
}

// View pointers in the stack.
void Stack_view(alu_Stack *from)
{
    printf("[");
    while (from)
    {
        printf("(%p)", from);
        from = from->next;
        if (from != null)
            printf(" -> ");
    }
    printf("]\n");
}

/**
 *
 * @category Alu Random
 *
 */

// Handles a signal
void __Alu_sighandler(int sig)
{
    if (sig == SIGINT)
        errno = 1;
}

// Get the size from an `alu_Type`.
size_t Alu_sizeoftype(alu_Type t)
{
    switch (t)
    {
    case ALU_BOOL:
        return sizeof(_Bool);
    case ALU_NUMBER:
        return sizeof(alu_Number);
    case ALU_STRING:
    case ALU_NULL:
    default:
        return 0;
    }
}

// Return the pointer depending on the type.
void *Alu_alloctype(alu_Type t)
{
    size_t n = Alu_sizeoftype(t);
    if (n == 0)
        return null;
    return malloc(n);
}

/**
 *
 * @category Alu string casting
 *
 */

void __Alu_abstracttoa(alu_Variable *var)
{
    char *hex = "0123456789abcdef";
    unsigned short base = 16;
    uintptr_t ptr = (uintptr_t)var->data;
    size_t nblen = 0;
    alu_String str = null;

    remove(var->data);
    var->data = null;
    for (uintptr_t n = ptr; n; n /= base)
        ++nblen;
    str = (char *)malloc(sizeof(char) * (nblen + 2 + 1));
    if (str == null)
        return;
    memset(str, 0, (nblen + 2 + 1));
    size_t index = 0;
    for (; ptr; ptr /= base)
        str[index++] = hex[ptr % base];
    strcat(str, "x0");
    strrev(str);
    var->data = str;
}

/// Converts a bool to an alu_String
void __Alu_btoa(alu_Variable *var)
{
    _Bool value = *((_Bool *)var->data);
    remove(var->data);
    var->data = strdup(value ? "true" : "false");
}

/// Converts null to string
void __Alu_nulltoa(alu_Variable *var)
{
    var->data = strdup("null");
}

/// Fill string with the double.
void __Alu_ntoa_fillbuf(alu_String buf, size_t *infos)
{
    size_t index = 0;
    if (infos[2])
    {
        for (; infos[2]; infos[2] /= 10)
            buf[index++] = '0' + (infos[2] % 10);
        ((alu_String)buf)[index++] = '.';
    }
    for (; infos[1]; infos[1] /= 10)
        buf[index++] = '0' + (infos[1] % 10);
    if (infos[3])
        ((alu_String)buf)[index++] = '-';
    remove(infos);
    strrev(buf);
}

/// Get a pointer to :
/// [0] = total, [1] = int, [2] = fract, [3] = signed
size_t *__Alu_ntoa_infos(alu_Number num, size_t precision)
{
    size_t *part = (size_t *)malloc(sizeof(size_t) * 4);
    if (part == null)
        return null;
    memset(part, 0, sizeof(size_t) * 4);
    num *= ((num < 0) ? -((short)(part[0] += ++part[3])) : 1);
    for (size_t n = (part[1] = (size_t)num); n > 0; n /= 10)
        ++(part[0]);
    num -= (short)num;
    for (; precision--;
         part[2] *= ((!(int)(num * 10)) ? 1 : 10), part[2] += (num *= 10))
        num -= (short)num;
    part[0] += (part[2] != 0);
    for (size_t n = part[2]; n > 0; n /= 10)
        ++(part[0]);
    return part;
}

// Converts an alu number into a string.
void __Alu_ntoa(alu_Variable *var)
{
    alu_Number num = *((alu_Number *)var->data);
    size_t *infos = __Alu_ntoa_infos(num, 6);
    remove(var->data);
    var->data = null;
    if (infos == null)
        raise(AERR_NOMEM, );
    var->data = (alu_String)malloc(sizeof(char) * (infos[0] + 1));
    if (var->data == null)
    {
        remove(infos);
        raise(AERR_NOMEM, );
    }
    memset(var->data, 0, (infos[0] + 1));
    __Alu_ntoa_fillbuf(var->data, infos);
}

// Converts a variable of any type to a string.
void Alu_vartostring(alu_Variable *var)
{
    if (var->type == ALU_STRING)
        return;
    ((void (*)(alu_Variable *))CONVERT_STRING[var->type])(var);
    var->type = ALU_STRING;
}

// Converts stack[0] into a string.
void Alu_tostring(alu_State *A)
{
    if (A->stack == null)
        raise(AERR_STKLN, );
    Alu_vartostring(A->stack->data);
}

/**
 *
 * @category Alu variable
 *
 */

// Create a `alu_Variable`.
alu_Variable *Alu_newvariable(alu_Type type, void *data)
{
    alu_Variable *var = (alu_Variable *)malloc(sizeof(alu_Variable));
    if (var == null)
        raise(AERR_NOMEM, null);
    if (data == null or type == ALU_NULL)
        return null;
    var->type = type;
    var->data = data;
    return var;
}

/// Returns a copy of this variable.
alu_Variable *Alu_cpyvar(alu_Variable *src)
{
    alu_Variable *dest = (alu_Variable *)malloc(sizeof(alu_Variable));
    size_t s = Alu_sizeoftype(src->type);
    s = ((s == 0 and src->type == ALU_STRING) ?
    ((strlen(src->data) + 1) * sizeof(char)) : s);
    if (s == 0)
        return null;
    dest->data = malloc(s);
    dest->type = src->type;
    if (dest->data != null)
        memcpy(dest->data, src->data, s);
    return dest;
}

/**
 *
 * @category Alu Stack Manipulation
 *
 */

/// Removes stack[0] from the stack.
/// `[a, b, c] -> [b, c]`
void Alu_popk(alu_State *A)
{
    alu_Stack *next = null;
    alu_Variable *var = null;
    if (A->stack == null)
        return;
    next = A->stack->next;
    var = A->stack->data;
    if (var->type != ALU_NULL and var->type != ALU_ABSTRACT)
        remove(var->data);
    remove(var);
    remove(A->stack);
    A->stack = next;
}

/// Clears the stack of the `alu_State`.
/// `[a, b, c] -> []`
void Alu_stackclose(alu_State *A)
{
    while (A->stack != null)
        Alu_popk(A);
}

/// Pushes an allocated version of `ptr` of type `t` in the stack.
/// `Code -> Stack`
void Alu_push(alu_State *A, const void *ptr, size_t s, alu_Type t)
{
    alu_Variable *var = null;
    void *data = null;
    data = malloc(s);
    if (data != null)
    {
        memset(data, 0, s);
        memcpy(data, ptr, s);
    }
    else
    {
        errno = ENOMEM;
        return;
    }
    var = Alu_newvariable(t, data);
    if (var == null)
        return;
    Stack_push(&A->stack, var);
}

/// Push a number in the stack.
void Alu_pushnumber(alu_State *A, alu_Number num)
{
    Alu_push(A, &num, sizeof(alu_Number), ALU_NUMBER);
}

/// Push a boolean in the stack.
void Alu_pushbool(alu_State *A, _Bool b)
{
    Alu_push(A, &b, sizeof(_Bool), ALU_BOOL);
}

/// Push a string in the stack.
void Alu_pushstring(alu_State *A, const alu_String str)
{
    Alu_push(A, str, ((strlen(str) + 1) * sizeof(char)), ALU_STRING);
}

/// Pushes a C pointer to the stack.
void Alu_pushabstract(alu_State *A, void *pointer)
{
    alu_Variable *var = Alu_newvariable(ALU_ABSTRACT, pointer);
    if (var == null)
        return;
    Stack_push(&A->stack, var);
}

/// Pushes a fefault functions
void Alu_pushdef(alu_State *A, alu_String str)
{
    for (size_t n = 0; DEF[n].name != null; ++n)
        if (strcmp(DEF[n].name, str) == 0)
            return Alu_pushabstract(A, DEF[n].f);
    raise(AERR_NOFND, );
}

/// Pop a value from the stack and return it.
alu_Variable *Alu_pop(alu_State *A)
{
    alu_Stack *link = null;
    alu_Variable *var = null;
    if (A->stack == null)
        return null;
    link = A->stack;
    A->stack = link->next;
    var = (alu_Variable *)link->data;
    remove(link);
    Stack_push(&A->garbage, var);
    return var;
}

// Get a variable from the stack index.
alu_Variable *Alu_get(alu_State *A, alu_Size index)
{
    alu_Stack *link = null;
    link = A->stack;
    for (; index; link = (link ? link->next : null))
        --index;
    if (link == null)
        raise(AERR_NOSTK, null);
    return ((alu_Variable *)link->data);
}

/// Get a alu_Number from the Stack.
/// `Stack -> Code`
alu_Number Alu_getnumber(alu_State *A, alu_Size index)
{
    alu_Variable *var = Alu_get(A, index);
    if (var == null)
        return 0;
    return *((alu_Number *)var->data);
}

/// Get a boolean from the stack.
/// `Stack -> Code`
_Bool Alu_getbool(alu_State *A, alu_Size index)
{
    alu_Variable *var = Alu_get(A, index);
    if (var == null)
        return 0;
    return *((_Bool *)var->data);
}

/// Get a string from the stack.
/// `Stack -> Code`
alu_String Alu_getstring(alu_State *A, alu_Size index)
{
    alu_Variable *var = Alu_get(A, index);
    if (var == null)
        return "";
    return (alu_String)var->data;
}

// Process the sum of 2 variables
static void *Alu_sumvar(alu_Variable *a, alu_Variable *b)
{
    void *data = Alu_alloctype(a->type);
    size_t len = 0;
    if ((data == null) and (a->type != ALU_STRING))
        return null;
    switch (a->type)
    {
    case ALU_NUMBER:
        *(alu_Number *)data = (*(alu_Number *)a->data + *(alu_Number *)b->data);
        break;
    case ALU_BOOL:
        *(_Bool *)data = (*(_Bool *)a->data + *(_Bool *)b->data);
        break;
    case ALU_STRING:
        len = strlen((alu_String)a->data) + strlen((alu_String)b->data);
        data = malloc(sizeof(char) * (len + 1));
        if (data == null)
            return null;
        memset(data, 0, len + 1);
        strcpy(data, a->data);
        strcat(data, b->data);
        break;
    default:
        remove(data)
            data = null;
        return null;
    }
    return data;
}

/// Sum stack[0] and stack[1] and pushes the result into the stack.
void Alu_sumstack(alu_State *A)
{
    alu_Variable *a = null, *b = null;
    alu_Type t = ALU_NULL;
    void *data = null;
    if (Stack_len(A->stack) < 2)
        raise(AERR_STKLN, );
    a = Alu_get(A, 0);
    b = Alu_get(A, 1);
    if (a->type != b->type)
        raise(AERR_TYPES, );
    t = a->type;
    data = Alu_sumvar(a, b);
    Alu_stackclose(A);
    if (t == ALU_STRING)
        Alu_pushstring(A, data);
    else
        Alu_push(A, data, Alu_sizeoftype(t), t);
    remove(data);
}

/**
 *
 * @category Registers methods
 *
 */

/// Clears all registers of the `alu_State`.
/// `[A, B, C] -> []`
void Alu_registerclose(alu_State *A)
{
    alu_Stack *tmp = null;
    alu_Register *reg = null;
    while (A->regs != null)
    {
        tmp = A->regs->next;
        reg = A->regs->data;
        remove(reg->var->data);
        remove(reg->var);
        remove(reg);
        remove(A->regs);
        A->regs = tmp;
    }
}

/// Set the value of stack[0] as a deep register.
/// `Stack -> Deep`
void Alu_load(alu_State *A, alu_Size registerIndex)
{
    alu_Variable *var = null;
    alu_Register *reg = null;

    if (Stack_len(A->stack) < 1)
        raise(AERR_STKLN, );
    var = Alu_cpyvar(A->stack->data);
    Alu_stackclose(A);
    for (alu_Stack *r = A->regs; r != null; r = r->next)
        if (((alu_Register *)r->data)->index == registerIndex)
        {
            reg = r->data;
            remove(reg->var->data);
            remove(reg->var);
            break;
        }
    if (reg != null)
    {
        reg->var = var;
        return;
    }
    reg = (alu_Register *)malloc(sizeof(alu_Register));
    if (reg == null)
        raise(AERR_NOMEM, );
    reg->var = var;
    reg->index = registerIndex;
    Stack_push(&A->regs, reg);
}

/// Get the deep register and push it in the stack.
/// `Deep -> Stack`
void Alu_unload(alu_State *A, alu_Size registerIndex)
{
    alu_Variable *var = null;
    for (alu_Stack *r = A->regs; r != null; r = r->next)
        if (((alu_Register *)r->data)->index == registerIndex)
        {
            var = ((alu_Register *)r->data)->var;
            break;
        }
    if (var == null)
        raise(AERR_NOREG, );
    Stack_push(&A->stack, Alu_cpyvar(var));
}

/// Time to abandonned register ! (General Grievous)
///
/// Basically removes the register value and push it into
/// the stack.
/// `Deep -> Stack`
void Alu_defunload(alu_State *A, alu_Size registerIndex)
{
    alu_Stack *rgStack = null;
    alu_Variable *var = null;
    for (alu_Stack *r = A->regs; r != null; r = r->next)
        if (((alu_Register *)r->data)->index == registerIndex)
        {
            var = ((alu_Register *)r->data)->var;
            break;
        }
    if (var == null)
        raise(AERR_NOREG, );
    Stack_push(&A->stack, var);
    rgStack = A->regs;
    A->regs = A->regs->next;
    remove(rgStack->data);
    remove(rgStack);
}

/**
 *
 * @category Alu methods
 *
 */

static alu_Size __Alu_seedgen(alu_State *A)
{
    size_t bigseed = 0;
    bigseed += (size_t)&A + (size_t)time(NULL) + (size_t)&__Alu_seedgen;
    return (alu_Size)bigseed;
}

// Creates an `alu_State`.
alu_State *Alu_newstate(void)
{
    alu_State *A = (alu_State *)malloc(sizeof(alu_State));
    if (A == null)
        raise(AERR_NOMEM, null);
    memset(A, 0, sizeof(alu_State));
    signal(SIGINT, __Alu_sighandler);
    A->seed = __Alu_seedgen(A);
    return A;
}

// Clears the garbage.
void Alu_garbageclose(alu_State *A)
{
    alu_Stack *tmp = null;
    alu_Variable *var = null;
    while (A->garbage != null)
    {
        tmp = A->garbage->next;
        var = A->garbage->data;
        if (var->type != ALU_ABSTRACT and var->type != ALU_NULL)
            remove(var->data);
        remove(var);
        remove(A->garbage);
        A->garbage = tmp;
    }
}

/// Close the `instructions`.
void Alu_instructionclose(alu_State *A)
{
    alu_Stack *tmp = null;
    while (A->instructions != null)
    {
        tmp = A->instructions->next;
        remove(A->instructions->data);
        remove(A->instructions);
        A->instructions = tmp;
    }
}

/// Close an `alu_State`.
/// Returns the status code.
int Alu_close(alu_State *A)
{
    int res = 0;
    if (A == null)
        return 1;
    if (A->error or res)
    {
        fprintf(stderr,
                "| [ERROR] Program ends with an error:\n| %s\n", A->error);
        res = 1;
    }
    Alu_stackclose(A);
    Alu_garbageclose(A);
    Alu_instructionclose(A);
    Alu_registerclose(A);
    remove(A);
    return res;
}

/// Evaluate stack[0] and stack[1] and compared with eval.
/// Pushes true or false in the stack.
void Alu_eval(alu_State *A, alu_Byte eval)
{
    int8_t ev = 0, cmpres = 0;
    alu_Variable *a = null, *b = null;

    if (Stack_len(A->stack) < 1)
        raise(AERR_STKLN, );
    a = Alu_get(A, 0);
    b = Alu_get(A, 1);
    if (a->type != b->type)
    {
        Alu_stackclose(A);
        Alu_pushbool(A, false);
        return;
    }
    if (a->type == ALU_STRING)
        cmpres = strcmp(a->data, b->data);
    else
        cmpres = (*((alu_Number *)a->data) - *((alu_Number *)b->data));
    ev |= (cmpres == 0) ? EVAL_EQUALS : 0;
    ev |= ((cmpres < 0) ? EVAL_SMALLER : ((cmpres > 0) ? EVAL_GREATER : 0));
    Alu_stackclose(A);
    Alu_pushbool(A, ev & eval);
}

/// Returns the number of bytes there is from the OP code to the end
/// of an instruction.
size_t __Alu_readop(alu_Opcode op, const char *ptr)
{
    size_t size = 0;
    if ((op >= OP_JMP) and (op <= OP_JNEM))
        return sizeof(alu_Size);
    switch (F[op].argument)
    {
    case 1:
        return sizeof(alu_Size);
    case 2:
        return sizeof(alu_Number);
    case 3:
        while (ptr[size] != '\0')
            ++size;
        return size;
    case 4:
        return sizeof(alu_Byte);
    default:
        return 0;
    }
}

/// Feed the state instruction with a raw instruction string.
void Alu_feed(alu_State *A, const alu_String ptr)
{
    alu_Byte op = 0x00;
    char *str = null;
    size_t n = 0, readlen = 0;
    debug(A, "=== Begin of instructions ===\n");
    do
    {
        op = (alu_Byte)ptr[n];
        if ((op == OP_HALT) or (op > OP_END))
            break;
        readlen = __Alu_readop(op, &ptr[n]);
        str = strcut(ptr, n, n + readlen + 1);
        if (str == null)
            break;
        n += readlen + 1;
        debug(A, "Get: ");
        for (size_t i = 0; i <= readlen; ++i)
            debug(A, "%02x ", str[i]);
        debug(A, "\n");
        Stack_push(&A->instructions, str);
    } while (true);
    debug(A, "Get: 00\n===  End of instructions  ===\n\n");
}

// Execute the opcode instruction.
void __Alu_executeop(alu_State *A, alu_Opcode op, const alu_Byte *instructions)
{
    alu_String str = null;
    switch (F[op].argument)
    {
    case 0: // A
        ((func0_t)F[op].func)(A);
        break;
    case 1: // A, alu_Size
        ((func1_t)F[op].func)(A, (alu_Size)bytesint(instructions + 1));
        break;
    case 2: // A, alu_Number
        ((func2_t)F[op].func)(A, (alu_Number)bytesdouble(instructions + 1));
        break;
    case 3: // A, alu_String
        str = strcut(
            (char *)instructions, 1, strlen((char *)instructions + 1) + 1);
        ((func3_t)F[op].func)(A, str);
        remove(str);
        break;
    case 4:
        ((func4_t)F[op].func)(A, *(instructions + 1));
    default:
        break;
    }
}

// Returns true if the jump instruction is valid.
_Bool __Alu_needtojump(alu_State *A, alu_Opcode op)
{
    alu_Variable *var = null;

    if (op == OP_JMP)
        return true;
    if (A->stack == null and op == OP_JEM)
        return true;
    if (A->stack == null)
        return false;
    var = ((alu_Variable *)A->stack->data);
    switch (op)
    {
    case OP_JFA:
        return (var->type == ALU_BOOL) and (*((_Bool *)var->data) == false);
    case OP_JTR:
        return (var->type == ALU_BOOL) and (*((_Bool *)var->data) == true);
    default:
        return true;
    }
}

// Execute an OP_JUMP action.
void Alu_jump(alu_State *A, alu_Opcode op, alu_Stack **iptr)
{
    if (not __Alu_needtojump(A, op))
    {
        debug(A, "Dont jump\n");
        *iptr = (*iptr)->next;
        Alu_popk(A);
        return;
    }
    int jumps = bytesint(((alu_Byte *)(*iptr)->data) + 1);
    jumps += (jumps > 0 ? 1 : -1);
    debug(A, "Jump %d instructions\n", jumps);
    Alu_popk(A);
    if (jumps >= 0)
        for (; jumps-- and (*iptr != null); *iptr = (*iptr)->next)
            ;
    else
        for (; jumps++ and (*iptr != null); *iptr = (*iptr)->previous)
            debug(A, "> %p\n", (*iptr)->previous);
    if (*iptr == null)
        raise(AERR_OUTJM, );
}

// Executes the instruction set.
void Alu_execute(alu_State *A)
{
    alu_Stack *instruction = A->instructions;
    alu_Byte op = 0x00;
    while ((instruction != null) and (not errno))
    {
        op = ((alu_Byte *)instruction->data)[0];
        if (op == OP_RET)
            return;
        debug(A, "Executes %02x\n", op);
        if ((op >= OP_JMP) and (op <= OP_JNEM))
        {
            Alu_jump(A, op, &instruction);
            continue;
        }
        __Alu_executeop(A, op, (alu_Byte *)instruction->data);
        instruction = instruction->next;
    }
}

// Start a program.
void Alu_start(alu_State *A, alu_String input)
{
    input += strlen(ALU_SIGNATURE);
    Alu_feed(A, input);
    alu_Stack *inst = A->instructions;
    unsigned int n = 0;
    while (inst != null)
    {
        inst = inst->next;
        ++n;
    }
    debug(A, "There is %d instructions\n", n);
    Alu_execute(A);
}

// Start a program by filename.
void Alu_startfile(alu_State *A, const alu_String filename)
{
    int fd = open(filename, O_RDONLY);
    char *buffer = null;
    struct stat st = {0};
    if (fd == -1)
        raise(AERR_NOFIL, );
    if (stat(filename, &st) == -1)
        raise(errno, );
    buffer = malloc(sizeof(char) * (st.st_size + 1));
    if (buffer == null)
        raise(AERR_NOMEM, );
    memset(buffer, 0, st.st_size);
    if (read(fd, buffer, st.st_size) == -1)
    {
        remove(buffer);
        raise(errno, );
    }
    Alu_start(A, buffer);
    remove(buffer);
}

/**
 *
 * @category Alu functions
 *
 */

// Print stuff in stack, and empty it !
void Alu_print(alu_State *A)
{
    while (A->stack != null)
    {
        Alu_tostring(A);
        puts(((alu_Variable *)A->stack->data)->data);
        Alu_popk(A);
    }
}

// Waits `ms` milliseconds.
void Alu_wait(alu_State __attribute__((unused))*A, alu_Size ms)
{
    alu_Size a = 0, b = 0;
    a = clock();
    while (true)
    {
        b = ((clock() - a) / 1000);
        if (b >= ms)
            break;
    }
}

/// Execute the function in stack[0].
void Alu_call(alu_State *A)
{
    alu_Variable *var = null;
    func0_t fptr = null;
    if (A->stack == null)
        raise(AERR_NOSTK, );
    var = Alu_pop(A);
    if (var->type == ALU_ABSTRACT)
    {
        fptr = var->data;
        fptr(A);
    }
    else
        raise(AERR_TYPES, )
}

// Set the head element to the top.
void Alu_super(alu_State *A)
{
    alu_Stack *super = null, *degrade = null;
    if (Stack_len(A->stack) < 2)
        raise(AERR_STKLN,);
    super = A->stack->top;
    degrade = A->stack;
    degrade->top = null;
    super->top = super->previous;
    super->previous->next = null;
    super->previous = null;
    super->next = degrade;
    degrade->previous = super;
    A->stack = super;
}

/* Main */

int main(void)
{
    alu_State *A = Alu_newstate();
    // char input[] = {
    //     OP_PUSHNUM,     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    //     OP_LOAD,        0, 0, 0, 0,

    //     OP_UNLOAD,      0, 0, 0, 0,
    //     OP_PUSHNUM,     0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    //     OP_SUMSTACK,
    //     OP_LOAD,        0, 0, 0, 0,
    //     OP_UNLOAD,      0, 0, 0, 0,
    //     OP_PUSHNUM,     0x40, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    //     OP_EVAL,        (EVAL_GREATER | EVAL_EQUALS),
    //     OP_JFA,         0xff, 0xff, 0xff, 0xfa,
    //     OP_UNLOAD,      0, 0, 0, 0,
    //     OP_RET,
    //     OP_HALT,
    // };

    // char input[] = {
    //     OP_PUSHNUM,     0x40, 0x5f, 0x53, 0x33, 0x33, 0x33, 0x33, 0x34,
    //     OP_PUSHNUM,     0x40, 0x5f, 0x53, 0x33, 0x33, 0x33, 0x33, 0x34,
    //     OP_PUSHNUM,     0xc0, 0xe3, 0x47, 0xae, 0x14, 0x7a, 0xe1, 0x48,
    //     OP_SUMSTACK,
    //     OP_HALT,
    // };

    // char input[] = {
    //     0x1b, 0xca, 0xca,
    //     OP_PUSHNUM,     0x40, 0x5f, 0x53, 0x33, 0x33, 0x33, 0x33, 0x34,
    //     OP_PUSHDEF,     'p', 'r', 'i', 'n', 't', '\0',
    //     OP_SUPER,
    //     OP_CALL,
    //     OP_HALT,
    // };
    Alu_startfile(A, "samples/file.alc");
    return Alu_close(A);
}
