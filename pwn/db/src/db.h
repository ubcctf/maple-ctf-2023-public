#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef int8_t i8;
typedef int16_t i16;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint64_t u64;
typedef int64_t i64;

#define DB_MAGIC "MAPLDBMS"
#define DB_VERSION 1
#define DB_PAGE_SIZE 0x100
#define DB_MAX_COLUMNS 16
#define DB_MAX_JOINS 16
#define DB_MAX_CODE_LEN 0x400
#define DB_MAX_CMD_LEN 0x1000
#define DB_MAX_ERR_LEN 0x100
#define DB_MAX_INFO_LEN 0x4000
#define DB_MAX_BTREE_DEPTH 16
#define DB_SIZE_THRESHOLD 32
#define DB_GROW_SIZE 8192
#define DB_MAX_BTREE_PAYLOAD                                                   \
    (DB_PAGE_SIZE - offsetof(struct db_page, btree.payload))
#define DB_BTREE_PAYLOAD_OFF (offsetof(struct db_page, btree.payload))
#define DB_MAX_RECORD_LEN ((long)(DB_PAGE_SIZE - DB_BTREE_PAYLOAD_OFF) / 3)
#define DB_REGS 0x20

#define PAGE_HEADER 'M'
#define PAGE_FREE 'f'
#define PAGE_BTREE 'b'

#define TYPE_U8 '8'
#define TYPE_VARINT 'i'
#define TYPE_BYTESTR 'b'

#define RESP_DATA 'D'
#define RESP_END 'E'
#define RESP_ERROR 'X'
#define RESP_INFO 'I'

#define LIST_NIL ((list_t)NULL)

#define INSTR(OP) ((struct db_instr){.op = (OP)})
#define INSTR_R(OP, RD) ((struct db_instr){.op = (OP), .rd = (RD)})
#define INSTR_RR(OP, RD, RS)                                                   \
    ((struct db_instr){.op = (OP), .rd = (RD), .rs = (RS)})
#define INSTR_RRI(OP, RD, RS, IMM)                                             \
    ((struct db_instr){.op = (OP), .rd = (RD), .rs = (RS), .imm = (IMM)})
#define INSTR_O(OP, OFF) ((struct db_instr){.op = (OP), .off = (OFF)})
#define INSTR_RO(OP, RD, OFF)                                                  \
    ((struct db_instr){.op = (OP), .rd = (RD), .off = (OFF)})

#define EMIT_FAIL(instr)                                                       \
    if (!db_compile_emit(comp, (instr)))                                       \
    goto fail

/* On-disk structures */
typedef u32 page_idx;
struct db_header {
    char magic[8];
    u32 version;
    u32 page_total;
    u32 page_alloc;
    page_idx freelist_head;
    page_idx table_btree;
};

typedef char db_page_type;

/* If your silly RISC architecture doesn't support unaligned loads or is
   (whispers) big endian, you'll just have to miss out. */
struct __attribute__((packed)) db_page_free {
    u16 reserved;
    page_idx next;
};
struct __attribute__((packed)) db_page_btree {
    u16 keys;
    u16 size;
    char payload[];
};

struct __attribute__((packed)) db_page {
    db_page_type type;
    u8 reserved;
    union {
        struct db_page_free free;
        struct db_page_btree btree;
    };
};

/* In-memory structures */
struct list {
    void *head;
    struct list *tail;
};
typedef struct list *list_t;

struct db_handle {
    int fd;
    union {
        struct db_header *header;
        void *mapped;
    };
};
typedef struct db_handle *db_handle_t;

typedef char db_scalar_type;
struct db_record_type {
    int len;
    int key_len;
    db_scalar_type col_types[DB_MAX_COLUMNS];
};

struct db_record {
    int len;
    char payload[DB_PAGE_SIZE];
};
struct db_dyn_record {
    struct db_record_type rtype;
    struct db_record rec;
};

struct db_cursor {
    db_handle_t hdl;
    struct db_record_type rtype;
    page_idx page_stack[DB_MAX_BTREE_DEPTH];
    u16 offset_stack[DB_MAX_BTREE_DEPTH];
    int depth;
};
typedef struct db_cursor *db_cursor_t;

struct db_col_def {
    char *name;
    db_scalar_type type;
};
struct db_table_def {
    char *name;
    list_t col_defs;
    list_t pkey;
};
struct db_index_def {
    char *name;
    char *table;
    list_t cols;
};
struct db_insert {
    char *table;
    list_t exprs;
};

struct db_table {
    char *name;
    char *idx_name;
    page_idx page;
    struct db_record_type rtype;
    char *col_names[DB_MAX_COLUMNS];
};


#define VAL_NULL 0
#define VAL_NUM 'i'
#define VAL_BLOB 'b'
#define VAL_RECORD 'r'
#define VAL_CURSOR 'c'
struct db_val {
    union {
        long num;
        char *blob;
        db_cursor_t cursor;
        struct db_dyn_record *record;
    };
    char tag;
    char own;
    int len;
};

struct db_expr;
struct db_ref {
    char *name;
    char *table;
    int tab_idx;
    int col_idx;
    /* db_scalar_type type; */
};
struct db_binop {
    enum {
        BINOP_AND,
        BINOP_OR,
        BINOP_ADD,
        BINOP_SUB,
        BINOP_MUL,
        BINOP_DIV,
        BINOP_MOD,
        BINOP_EQ,
        BINOP_LT,
        BINOP_GT,
        BINOP_NEQ,
        BINOP_LEQ,
        BINOP_GEQ,
        BINOP_LIKE,
    } tag;
    struct db_expr *es[2];
};
struct db_expr {
    enum { EXPR_LITERAL, EXPR_REF, EXPR_BINOP } tag;
    union {
        struct db_val literal;
        struct db_ref ref;
        struct db_binop binop;
    };
};

struct db_subquery;
struct db_select {
    list_t exprs;
    struct db_subquery *from;
    struct db_expr *where;
    int limit;
    int offset;
    int explain;
};
struct db_subquery {
    enum {
        SUBQUERY_TABLE,
    } tag;
    union {
        char *table;
    };
    struct db_subquery *join;
    struct db_expr *constraint;
};

enum db_opcode {
    OP_RET,
    OP_YLD,
    OP_MOV,
    OP_AND,
    OP_OR,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_EQ,
    OP_LT,
    OP_GT,
    OP_NEQ,
    OP_LEQ,
    OP_GEQ,
    OP_LIKE,
    OP_B,
    OP_BZ,
    OP_BNZ,
    OP_BEND,
    OP_APP,
    OP_PROJ,
    OP_RST,
    OP_FIND,
    OP_NEXT,
    OP_LDR,
    OP_STR,
};
struct db_instr {
    u8 op;
    u8 rd;
    union {
        struct {
            u8 rs;
            u8 imm;
        };
        i16 off;
    };
};

struct db_coroutine {
    int pc;
    struct db_instr code[DB_MAX_CODE_LEN];
    struct db_val regs[DB_REGS];
};

enum db_plan_type {
    PLAN_SCAN,
    PLAN_SEARCH,
};
struct db_plan_step {
    enum db_plan_type type;
    int tab_idx;
    const struct db_table *table;

    /* pk = <expr> */
    const struct db_expr *conj_expr;
    /* just the <expr> */
    const struct db_expr *key_expr;

    list_t cond; /* List of ANDed expressions */
};

struct db_compiler {
    db_handle_t hdl;
    int explain;

    u8 free_reg;
    int off;

    int n_tables;
    struct db_table *tables[DB_MAX_JOINS];

    int loop_offs[DB_MAX_JOINS]; /* Relocate to loop end */
    int loop_skip_offs[DB_MAX_JOINS];

    u8 cursors[DB_MAX_JOINS];
    u8 records[DB_MAX_JOINS];
    u8 empty_rec;

    /* List of ANDed expresions from join conds and WHERE clause. */
    list_t conj;
    int plan_len;
    struct db_plan_step plan[DB_MAX_JOINS];

    struct db_coroutine *coroutine;
};

enum db_vm_status {
    VM_ERROR,
    VM_OK,
    VM_RET,
    VM_YLD,
};

db_handle_t db_open(const char *path);
void db_close(db_handle_t hdl);

void db_repl(void);
void db_server(void);

void __attribute__((noreturn)) db_die(const char *fmt, ...);
void db_error(const char *fmt, ...);
void db_info_send(void);
void db_info(const char *fmt, ...);
void db_output_row(const struct db_val *val);

void db_table_create(db_handle_t hdl, struct db_table_def *def);
struct db_table *db_table_lookup(db_handle_t hdl, const char *name);
struct db_table *db_table_load(db_cursor_t c);
void db_table_free(struct db_table *table);

int db_table_def_sort(struct db_table_def *def);
void db_table_def_free(struct db_table_def *def);

int db_col_def_name_cmp(struct db_col_def *def, const char *name);
void db_col_def_free(struct db_col_def *def);

void db_index_create(db_handle_t hdl, struct db_index_def *def);
void db_index_def_free(struct db_index_def *def);

void db_insert(db_handle_t hdl, struct db_insert *insert);
void db_insert_free(struct db_insert *insert);

void db_schema_dump(db_handle_t hdl);

struct db_expr *db_expr_make_num(long num);
struct db_expr *db_expr_make_string(char *blob);
struct db_expr *db_expr_make_ref(char *name, char *table);
struct db_expr *db_expr_make_binop(int tag, struct db_expr *l,
                                   struct db_expr *r);
int db_binop_check(struct db_binop *binop, db_scalar_type t1,
                   db_scalar_type t2);

void db_expr_free(struct db_expr *expr);
void db_expr_dump(const struct db_expr *expr, int prec);
void db_ref_dump(const struct db_ref *ref);
void db_val_dump(const struct db_val *val);
void db_val_mov(struct db_val *dst, const struct db_val *src);
void db_val_free(struct db_val *val);
int db_val_cmp(const struct db_val *v1, const struct db_val *v2);
int db_val_nz(struct db_val *val);
void db_coroutine_free(struct db_coroutine *co);
void db_select_free(struct db_select *select);
void db_subquery_free(struct db_subquery *subquery);
void db_plan_step_free(struct db_plan_step *step);

void db_execute_query(db_handle_t hdl, struct db_select *select);
int db_compile_query(db_handle_t hdl, struct db_coroutine *co,
                     struct db_select *select);
struct db_compiler *db_compiler_init(db_handle_t hdl);
void db_compiler_free(struct db_compiler *comp);
int db_compile_plan(struct db_compiler *comp, struct db_select *select);
void db_compile_explain_plan(struct db_compiler *comp);
int db_compile_check(struct db_compiler *comp, struct db_select *select);
db_scalar_type db_compile_check_expr(struct db_compiler *comp,
                                     struct db_expr *expr);
int db_compile_resolve_ref(struct db_compiler *comp, struct db_ref *ref);
void db_compile_flatten_conj(struct db_compiler *comp,
                             struct db_select *select);
int db_compile_join(struct db_compiler *comp, const struct db_select *select,
                    int tab_idx, const struct db_table *table,
                    struct db_plan_step *step);
int db_compile_can_eval(struct db_compiler *comp, int plan_len,
                        struct db_expr *expr);
int db_compile_lookup_tables(struct db_compiler *comp,
                             struct db_subquery *subq);
int db_compile_query_head(struct db_compiler *comp, struct db_subquery *subq);
int db_compile_query_loop(struct db_compiler *comp, struct db_subquery *subq);
int db_compile_ref(struct db_compiler *comp, u8 dest, const struct db_ref *ref);
int db_compile_expr(struct db_compiler *comp, u8 dest,
                    const struct db_expr *expr);
int db_compile_expr_alloc(struct db_compiler *comp, const struct db_expr *expr);
void db_compile_disasm(struct db_compiler *comp);
int db_compile_emit(struct db_compiler *comp, struct db_instr instr);
u8 db_compile_reg_alloc(struct db_compiler *comp);
void db_compile_reg_free(struct db_compiler *comp, u8 reg);

int db_bytecode_exec(db_handle_t hdl, struct db_coroutine *coroutine,
                     const struct db_select *select);
enum db_vm_status db_bytecode_step(struct db_coroutine *coroutine,
                                   struct db_val **out);
void db_bytecode_cmp(struct db_val *dst, struct db_val *src, enum db_opcode op);

void db_page_map(db_handle_t hdl, struct db_header *hdr);
void db_page_grow(db_handle_t hdl, u32 num);
page_idx db_page_alloc(db_handle_t hdl);
void db_page_free(db_handle_t hdl, page_idx idx);
struct db_page *db_page_get(db_handle_t hdl, page_idx page);

page_idx db_btree_alloc(db_handle_t hdl);

db_cursor_t db_cursor_open_page(db_handle_t hdl, page_idx tree,
                                const struct db_record_type *rtype);
void db_cursor_close(db_cursor_t cursor);
struct db_page *db_cursor_get_page(db_cursor_t cursor);
char *db_cursor_get_ptr(db_cursor_t cursor);
char *db_cursor_get_end(db_cursor_t cursor);
int db_cursor_find_leaf(db_cursor_t cursor);
void db_cursor_reset(db_cursor_t cursor);
void db_cursor_reset_end(db_cursor_t cursor);
int db_cursor_next(db_cursor_t cursor);
int db_cursor_end(db_cursor_t cursor);
int db_cursor_lookup(db_cursor_t cursor, const struct db_record *rec);
int db_cursor_insert(db_cursor_t cursor, const struct db_record *rec);
void db_cursor_load(db_cursor_t cursor, struct db_record *out);

void db_blob_dump(int len, const char *blob);
int db_blob_like(const char *data, const char *data_end, const char *pat,
                 const char *pat_end);
void db_scalar_dump(db_scalar_type type, const char **buf);
void db_scalar_next(db_scalar_type type, const char **buf);
int db_scalar_cmp(db_scalar_type type, const char **s1, const char **s2);

void db_record_dump(const struct db_record_type *rtype, const char **buf);
void db_record_next(const struct db_record_type *rtype, const char **buf);
int db_record_cmp(const struct db_record_type *rtype, const char *r1,
                  const char **r2);
void db_record_insert(char *buf, int size, int offset,
                      const struct db_record *rec, page_idx right_idx);
void db_record_find_median(const struct db_record_type *rtype, char *buf,
                           int size, int keys, char **median, int *median_idx);

int db_record_app_val(struct db_dyn_record *dst, const struct db_val *val);
int db_record_app_u8(struct db_record *rec, u8 data);
int db_record_app_bs(struct db_record *rec, int len, const char *data);
int db_record_app_varint(struct db_record *rec, i32 data);

struct db_val db_record_proj(const struct db_record_type *rtype,
                             const struct db_record *rec, int i);

char *db_bs_decode(const char **buf);

void db_varint_next(const char **buf);
int db_varint_size(i32 n);
i32 db_varint_decode(const char **buf);
int db_varint_encode(char *buf, i32 n);

int memncmp(const char *s1, const char *s2, size_t len1, size_t len2);

list_t list_cons(void *head, list_t tail);
typedef void (*free_fn)(void *);
void list_free(free_fn f, list_t list);
list_t list_reverse(list_t list);
typedef int (*cmp_fn)(const void *, const void *);
int list_find(cmp_fn cmp, list_t list, const void *elem, list_t *out);
list_t list_nth(list_t list, int i);
int list_length(list_t list);
void list_delete(list_t *list, const void *elem);

extern db_handle_t db_hdl;
extern int db_server_mode;
