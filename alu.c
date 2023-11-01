#include <stddef.h>
#include <stdlib.h>
#include <iso646.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

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

#define return_e(ret, A, msg)        \
    do                               \
    {                                \
        printf("[ERROR] %s\n", msg); \
        A->error = msg;              \
        return ret;                  \
    } while (0)

#define check(A, ret)       \
    if (A == null or errno) \
        return ret;

/**
 *
 * @category Typedefs
 *
 */

typedef double alu_Number;
typedef char *alu_String;

typedef enum _alu_Type
{
    ALU_NULL = 0,
    ALU_NUMBER,
    ALU_STRING,
    ALU_BOOL,
} alu_Type;

typedef enum _alu_Eval
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

typedef struct s_stack
{
    void *data;
    struct s_stack *next;
} alu_Stack;

typedef struct
{
    char *error;
    alu_Stack *stack;
    alu_Stack *garbage;
    alu_Stack *regs;
    alu_Stack *instructions;
    size_t pc;
} alu_State;

typedef struct
{
    size_t index;
    alu_Variable *var;
} alu_Register;

/**
 *
 * @category C functions
 *
 */

#define SIGNIFY(n) ((n > 0) - (n < 0))

int faststrcmp(const char *s1, const char *s2)
{
    for (; *s1 and (*s1 == *s2); ++s1, ++s2)
        ;
    return SIGNIFY((unsigned char)*s1 - (unsigned char)*s2);
}

// Return a pointer to the last element of the stack.
alu_Stack *Stack_top(alu_Stack *from)
{
    if (from == null)
        return null;
    for (; from->next != null;)
        from = from->next;
    return from;
}

// Pushes a slate into a stack.
void Stack_push(alu_Stack **stack, void *data)
{
    alu_Stack *slate = (alu_Stack *)malloc(sizeof(alu_Stack));
    if (slate == null)
        return;
    slate->next = null;
    slate->data = data;
    if (*stack == null)
        *stack = slate;
    else
        Stack_top(*stack)->next = slate;
}

// Get the stack len from a pointer.
size_t Stack_len(alu_Stack *from)
{
    size_t n = 0;
    if (from == null)
        return n;
    for (; from->next != null; ++n)
        from = from->next;
    return n + 1;
}

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
 * @category Alu variable
 *
 */

// Create a `alu_Variable`.
alu_Variable *Alu_newvariable(alu_Type type, void *data)
{
    alu_Variable *var = (alu_Variable *)malloc(sizeof(alu_Variable));
    if (var == null or data == null or type == ALU_NULL)
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
    s = ((s == 0 and src->type == ALU_STRING) ? ((strlen(src->data) + 1) * sizeof(char)) : s);
    if (s == 0)
        return null;
    dest->data = malloc(s);
    dest->type = src->type;
    if (dest->data != null)
        memcpy(dest->data, src->data, s);
    return dest;
}

// Converts a variable of any type to a string.
void Alu_tostring(alu_Variable *var)
{
    if (var->type == ALU_STRING or var->type == ALU_NULL)
        return;
}

/**
 *
 * @category Alu Stack Manipulation
 *
 */

/// Removes the first element of the stack.
/// `[a, b, c] -> [b, c]`
void Alu_popk(alu_State *A)
{
    alu_Stack *n = null;
    if (A->stack == null)
        return;
    n = A->stack->next;
    remove(((alu_Variable *)A->stack->data)->data);
    remove(A->stack->data);
    remove(A->stack);
    A->stack = n;
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
void Alu_push(alu_State *A, void *ptr, size_t s, alu_Type t)
{
    alu_Variable *var = null;
    alu_Number *data = null;
    check(A, );
    data = malloc(s);
    if (data != null)
    {
        memset(data, 0, s);
        memcpy(data, ptr, s);
    }
    var = Alu_newvariable(t, data);
    if (var == null)
        return;
    Stack_push(&A->stack, var);
}

/// Push a number in the stack.
/// `Code -> Stack`
void Alu_pushnumber(alu_State *A, alu_Number num)
{
    Alu_push(A, &num, sizeof(alu_Number), ALU_NUMBER);
}

/// Push a boolean in the stack.
/// `Code -> Stack`
void Alu_pushbool(alu_State *A, _Bool b)
{
    Alu_push(A, &b, sizeof(_Bool), ALU_BOOL);
}

/// Push a string in the stack.
/// `Code -> Stack`
void Alu_pushstring(alu_State *A, const alu_String str)
{
    Alu_push(A, str, ((strlen(str) + 1) * sizeof(char)), ALU_STRING);
}

/// Pop a value from the stack and return it.
/// `Stack -> Code`
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
alu_Variable *Alu_get(alu_State *A, size_t index)
{
    alu_Stack *link = null;
    check(A, null);
    link = A->stack;
    for (; index; link = (link ? link->next : null))
        --index;
    if (link == null)
        return_e(null, A, "Reach end of stack");
    return ((alu_Variable *)link->data);
}

/// Get a alu_Number from the Stack.
/// `Stack -> Code`
alu_Number Alu_getnumber(alu_State *A, size_t index)
{
    alu_Variable *var = Alu_get(A, index);
    if (var == null)
        return 0;
    return *((alu_Number *)var->data);
}

/// Get a boolean from the stack.
/// `Stack -> Code`
_Bool Alu_getbool(alu_State *A, size_t index)
{
    alu_Variable *var = Alu_get(A, index);
    if (var == null)
        return 0;
    return *((_Bool *)var->data);
}

/// Get a string from the stack.
/// `Stack -> Code`
alu_String Alu_getstring(alu_State *A, size_t index)
{
    alu_Variable *var = Alu_get(A, index);
    if (var == null)
        return 0;
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
        return_e(, A, "Too few elements in the stack to add up.");
    a = Alu_get(A, 0);
    b = Alu_get(A, 1);
    if (a->type != b->type)
        return_e(, A, "Elements types missmatch");
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
void Alu_load(alu_State *A, size_t registerIndex)
{
    alu_Variable *var = null;
    alu_Register *reg = null;

    if (Stack_len(A->stack) < 1)
        return_e(, A, "Too few elements in the stack to add up.");
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
    if (reg == null)
    {
        reg = malloc(sizeof(alu_Register));
        reg->var = var;
        reg->index = registerIndex;
        Stack_push(&A->regs, reg);
        return;
    }
    reg->var = var;
}

/// Get the deep register and push it in the stack.
/// `Deep -> Stack`
void Alu_unload(alu_State *A, size_t registerIndex)
{
    alu_Variable *var = null;

    for (alu_Stack *r = A->regs; r != null; r = r->next)
        if (((alu_Register *)r->data)->index == registerIndex)
        {
            var = ((alu_Register *)r->data)->var;
            break;
        }
    if (var == null)
        return_e(, A, "Reach end of registers.");
    Stack_push(&A->stack, Alu_cpyvar(var));
}

/// Time to abandonned register ! (General Grievous)
/// 
/// Basically removes the register value and push it into
/// the stack.
/// `Deep -> Stack`
void Alu_defunload(alu_State *A, size_t registerIndex)
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
        return_e(, A, "Reach end of registers.");
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

// Creates an `alu_State`.
alu_State *Alu_newstate(void)
{
    alu_State *A = (alu_State *)malloc(sizeof(alu_State));
    if (A == null)
        return null;
    memset(A, 0, sizeof(alu_State));
    signal(SIGINT, __Alu_sighandler);
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
    alu_String *str = null;
    while (A->instructions != null)
    {
        tmp = A->instructions->next;
        remove(str);
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
        fprintf(stderr, "\n| [ERROR] Program ends with an error:\n| %s\n", A->error);
        res = 1;
    }
    Alu_stackclose(A);
    Alu_garbageclose(A);
    Alu_registerclose(A);
    remove(A);
    return res;
}

/// Evaluate stack[0] and stack[1] and compared with eval.
/// Pushes true or false in the stack.
void Alu_eval(alu_State *A, alu_Eval eval)
{
    char ev = 0, cmpres = 0;
    alu_Variable *a = null, *b = null;

    if (Stack_len(A->stack) < 1)
        return_e(, A, "Too few elements in the stack to add up.");
    a = Alu_get(A, 0);
    b = Alu_get(A, 1);
    if (a->type != b->type)
    {
        Alu_stackclose(A);
        Alu_pushbool(A, false);
        return;
    }
    if (a->type == ALU_STRING)
        cmpres = faststrcmp(a->data, b->data);
    else
        cmpres = (*((alu_Number *)a->data) - *((alu_Number *)b->data));
    ev |= (cmpres == 0) ? EVAL_EQUALS : 0;
    ev |= ((cmpres < 0) ? EVAL_SMALLER : ((cmpres > 0) ? EVAL_GREATER : 0));
    Alu_stackclose(A);
    Alu_pushbool(A, ev & eval);
}

/// Feed the state with raw instructions.
void __Alu_feed(alu_State *A, const char *begin, size_t n)
{
    A->instructions = null;
}

/* Main */

int main(void)
{
    alu_State *A = Alu_newstate();

    // let a = "Hello"
    Alu_pushstring(A, "Hello");
    Alu_load(A, 0);

    // let b = a + "World"
    Alu_unload(A, 0);
    Alu_pushstring(A, "World");
    Alu_sumstack(A);
    Alu_load(A, 1);

    // b = b + a
    Alu_unload(A, 1);
    Alu_unload(A, 0);
    Alu_sumstack(A);
    Alu_load(A, 1);

    // <cprintf>(a, b) // Hello, HelloWorldHello
    Alu_unload(A, 0);
    Alu_unload(A, 1);
    printf("%s, %s\n", Alu_getstring(A, 0), Alu_getstring(A, 1));
    Alu_stackclose(A);
    
    // let foo = 3 + 10
    Alu_pushnumber(A, 3);
    Alu_pushnumber(A, 10);
    Alu_sumstack(A);
    Alu_load(A, 2);

    // <cprintf>(foo > 6) // true
    Alu_unload(A, 2);
    Alu_pushnumber(A, 6);
    Alu_eval(A, EVAL_GREATER);
    printf("%s\n", Alu_getbool(A, 0) ? "true" : "false");
    return Alu_close(A);
}
