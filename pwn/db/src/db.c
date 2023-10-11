#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <unistd.h>

#include "db.h"

struct yy_buffer_state;
typedef struct yy_buffer_state *YY_BUFFER_STATE;

int yyparse();
void yyrestart(FILE *input_file);
void yylex_destroy(void);

YY_BUFFER_STATE yy_scan_buffer(char *base, size_t size);
void yy_delete_buffer(YY_BUFFER_STATE b);
void yy_flush_buffer(YY_BUFFER_STATE b);
void yy_switch_to_buffer(YY_BUFFER_STATE new_buffer);

extern FILE *yyin;
extern int lexer_eof;
extern int parse_error;
static const struct db_record_type db_table_rtype;

db_handle_t db_hdl;
int db_server_mode;

int main(int argc, char **argv) {
    int c, force_repl = 0;
    if (argc < 2)
        return 1;

    while ((c = getopt(argc, argv, "r")) != -1) {
        switch (c) {
        case 'r':
            force_repl = 1;
            break;
        case '?':
            return 1;
        default:
            return 1;
        }
    }

    if (optind >= argc)
        return 1;

    db_hdl = db_open(argv[optind]);

    if (isatty(STDIN_FILENO) || force_repl)
        db_repl();
    else
        db_server();

    db_close(db_hdl);
    yylex_destroy();
}

void db_repl() {
    db_server_mode = 0;
    yyin = stdin;
    while (!lexer_eof) {
        printf("> ");
        fflush(stdout);
        yyparse();
        if (parse_error) {
            parse_error = 0;
            yyrestart(stdin);
        }
    }
}

void db_server() {
    static char cmd_buf[DB_MAX_CMD_LEN];
    db_server_mode = 1;
    u16 cmd_len;
    while (1) {
        if (fread(&cmd_len, sizeof cmd_len, 1, stdin) != 1)
            break;
        if (cmd_len > DB_MAX_CMD_LEN - 2)
            continue;
        if (fread(cmd_buf, 1, cmd_len, stdin) != cmd_len)
            continue;
        cmd_buf[cmd_len] = 0;
        cmd_buf[cmd_len + 1] = 0;
        yy_scan_buffer(cmd_buf, cmd_len + 2);
        while (!lexer_eof)
            yyparse();
        lexer_eof = 0;
        fputc(RESP_END, stdout);
        fflush(stdout);
        yylex_destroy();
    }
}

void __attribute__((noreturn)) db_die(const char *fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
    vfprintf(stderr, fmt, argp);
    va_end(argp);
    exit(1);
}

void db_error(const char *fmt, ...) {
    static char err_buf[DB_MAX_ERR_LEN];
    va_list argp;
    va_start(argp, fmt);
    int len = vsnprintf(err_buf, sizeof err_buf, fmt, argp);
    va_end(argp);
    if (len < 0)
        return;

    if (db_server_mode) {
        u16 len_buf = len;
        fputc(RESP_ERROR, stdout);
        fwrite(&len_buf, sizeof len_buf, 1, stdout);
        fwrite(err_buf, 1, len_buf, stdout);
        fflush(stdout);
    } else {
        fprintf(stderr, "error: %*s\n", len, err_buf);
        fflush(stderr);
    }
}

int db_info_len;
char db_info_buf[DB_MAX_INFO_LEN];

void db_info_send(void) {
    if (db_server_mode) {
        u16 len_buf = db_info_len;
        fputc(RESP_INFO, stdout);
        fwrite(&len_buf, sizeof len_buf, 1, stdout);
    }
    fwrite(db_info_buf, 1, db_info_len, stdout);
    fflush(stdout);
    db_info_len = 0;
}

void db_info(const char *fmt, ...) {
    if (db_info_len >= DB_MAX_INFO_LEN)
        return;
    va_list argp;
    va_start(argp, fmt);
    db_info_len += vsnprintf(db_info_buf + db_info_len,
                             sizeof db_info_buf - db_info_len, fmt, argp);
    va_end(argp);
}

void db_output_row(const struct db_val *val) {
    if (db_server_mode) {
        fputc(RESP_DATA, stdout);
        struct db_dyn_record *rec = val->record;
        fputc(rec->rtype.len, stdout);
        const char *p = rec->rec.payload;
        for (int i = 0; i < rec->rtype.len; i++) {
            fputc(rec->rtype.col_types[i], stdout);
            switch (rec->rtype.col_types[i]) {
            case TYPE_U8:
                fputc(*p++, stdout);
                break;
            case TYPE_VARINT: {
                i32 num = db_varint_decode(&p);
                fwrite(&num, sizeof num, 1, stdout);
                break;
            }
            case TYPE_BYTESTR: {
                u16 len = db_varint_decode(&p);
                fwrite(&len, sizeof len, 1, stdout);
                fwrite(p, 1, len, stdout);
                p += len;
                break;
            }
            }
        }
    } else {
        db_val_dump(val);
        db_info("\n");
        db_info_send();
    }
}

db_handle_t db_open(const char *path) {
    int fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0644);
    int exists = 0;
    if (fd < 0 && errno == EEXIST) {
        fd = open(path, O_RDWR, 0644);
        exists = 1;
    }
    if (fd < 0) {
        perror("open");
        exit(1);
    }
    if (flock(fd, LOCK_EX | LOCK_NB)) {
        perror("flock");
        exit(1);
    }

    union {
        struct db_header hdr;
        char buf[DB_PAGE_SIZE];
    } first_page;
    memset(first_page.buf, 0, DB_PAGE_SIZE);
    struct db_handle *hdl = malloc(sizeof *hdl);
    hdl->fd = fd;
    if (exists) {
        int read_bytes = read(fd, &first_page.hdr, DB_PAGE_SIZE);
        if (read_bytes < 0) {
            perror("read");
            exit(1);
        }
        if (read_bytes != DB_PAGE_SIZE ||
            memcmp(&first_page.hdr.magic, DB_MAGIC,
                   sizeof first_page.hdr.magic) ||
            first_page.hdr.version != DB_VERSION) {
            fprintf(stderr, "bad magic\n");
            exit(1);
        }
    } else {
        first_page.hdr = (struct db_header){
            .magic = DB_MAGIC,
            .version = DB_VERSION,
            .page_total = 1,
            .page_alloc = 1,
        };
        if (write(fd, first_page.buf, DB_PAGE_SIZE) != DB_PAGE_SIZE) {
            perror("write");
            exit(1);
        }
    }
    db_page_map(hdl, &first_page.hdr);

    if (!hdl->header->table_btree)
        hdl->header->table_btree = db_btree_alloc(hdl);

    return hdl;
}

void db_close(db_handle_t hdl) {
    munmap(hdl->mapped, hdl->header->page_total);
    close(hdl->fd);
    free(hdl);
}

void db_table_create(db_handle_t hdl, struct db_table_def *def) {
    struct db_record rec = {.len = 0};
    char col_types[DB_MAX_COLUMNS];
    char col_names[DB_MAX_RECORD_LEN];
    char *p = col_names;
    int n_cols;
    list_t list;

    if (!db_table_def_sort(def))
        return;
    for (n_cols = 0, list = def->col_defs; list; n_cols++, list = list->tail) {
        struct db_col_def *col = (struct db_col_def *)list->head;

        if (n_cols >= DB_MAX_COLUMNS) {
            db_error("too many columns");
            return;
        }
        int l = strlen(col->name) + 1;
        if (p + l - col_names > DB_MAX_RECORD_LEN) {
            db_error("column names too long");
            return;
        }
        memcpy(p, col->name, l);
        p += l;
        col_types[n_cols] = col->type;
    }

    page_idx idx = db_btree_alloc(hdl);

    /*
     * table_name: bytestr
     * idx_name:   bytestr
     * page:       varint
     * key_len:    u8
     * col_types:  bytestr
     * col_names:  bytestr
     */
    if (!db_record_app_bs(&rec, strlen(def->name), def->name))
        return;
    if (!db_record_app_bs(&rec, 0, ""))
        return;
    if (!db_record_app_varint(&rec, idx))
        return;
    if (!db_record_app_u8(&rec, list_length(def->pkey)))
        return;
    if (!db_record_app_bs(&rec, n_cols, col_types))
        return;
    if (!db_record_app_bs(&rec, p - col_names, col_names))
        return;

    page_idx table_btree = hdl->header->table_btree;
    db_cursor_t c = db_cursor_open_page(hdl, table_btree, &db_table_rtype);
    int ret = db_cursor_insert(c, &rec);
    db_cursor_close(c);

    if (ret) {
        db_info("created table \"%s\"\n", def->name);
        db_info_send();
        return;
    }

    db_error("error creating table");
    /* TODO: write a proper btree drop function */
    db_page_free(hdl, idx);
}

struct db_table *db_table_lookup(db_handle_t hdl, const char *name) {
    struct db_table *table;
    struct db_record rec = {.len = 0};
    struct db_record_type rtype = db_table_rtype;
    rtype.key_len = 1;
    db_record_app_bs(&rec, strlen(name), name);
    db_cursor_t c = db_cursor_open_page(hdl, hdl->header->table_btree, &rtype);
    int ret = db_cursor_lookup(c, &rec);
    if (!ret) {
        db_error("table \"%s\" not found", name);
        table = NULL;
        goto fail;
    }

    table = db_table_load(c);

fail:
    db_cursor_close(c);
    return table;
}

struct db_table *db_table_load(db_cursor_t c) {
    struct db_table *table = malloc(sizeof *table);
    const char *p = db_cursor_get_ptr(c);
    db_varint_next(&p);
    table->name = db_bs_decode(&p);
    table->idx_name = db_bs_decode(&p);
    table->page = db_varint_decode(&p);
    table->rtype.key_len = *p++;
    table->rtype.len = db_varint_decode(&p);
    memcpy(table->rtype.col_types, p, table->rtype.len);
    p += table->rtype.len;
    db_varint_next(&p);
    for (int i = 0; i < table->rtype.len; i++) {
        table->col_names[i] = strdup(p);
        p += strlen(p) + 1;
    }
    return table;
}

void db_table_free(struct db_table *table) {
    free(table->name);
    free(table->idx_name);
    for (int i = 0; i < table->rtype.len; i++)
        free(table->col_names[i]);
    free(table);
}

int db_table_def_sort(struct db_table_def *def) {
    int i;
    list_t pkey_list;
    for (i = 0, pkey_list = def->pkey; pkey_list;
         i++, pkey_list = pkey_list->tail) {
        const char *name = (const char *)pkey_list->head;
        list_t pkey_col;
        int j = list_find((cmp_fn)db_col_def_name_cmp, def->col_defs,
                          (void *)name, &pkey_col);
        if (j < 0) {
            db_error("unknown column %s", name);
            return 0;
        }
        if (list_find((cmp_fn)strcmp, def->pkey, (void *)name, NULL) != i) {
            db_error("duplicate primary key column %s", name);
            return 0;
        }

        list_t col_new = list_nth(def->col_defs, i);
        if (col_new) {
            void *tmp = col_new->head;
            col_new->head = pkey_col->head;
            pkey_col->head = tmp;
        }
    }

    return 1;
}

void db_table_def_free(struct db_table_def *def) {
    if (def->name)
        free(def->name);
    list_free((free_fn)db_col_def_free, def->col_defs);
    list_free(free, def->pkey);
    free(def);
}

int db_col_def_name_cmp(struct db_col_def *def, const char *name) {
    return strcmp(def->name, name);
}

void db_col_def_free(struct db_col_def *def) {
    free(def->name);
    free(def);
}

void db_index_create(db_handle_t hdl, struct db_index_def *def) {
    db_error("unimplemented");
}

void db_index_def_free(struct db_index_def *def) {
    free(def->name);
    free(def->table);
    list_free(free, def->cols);
    free(def);
}

void db_insert(db_handle_t hdl, struct db_insert *insert) {
    struct db_table *table = db_table_lookup(hdl, insert->table);
    if (!table)
        return;
    if (table->rtype.len != list_length(insert->exprs)) {
        db_error("expected %d columns", table->rtype.len);
        goto done;
    }

    struct db_dyn_record drec = {.rtype = table->rtype, .rec = {.len = 0}};
    for (list_t l = insert->exprs; l; l = l->tail) {
        struct db_expr *e = (struct db_expr *)l->head;
        if (e->tag != EXPR_LITERAL)
            goto done;
        if (!db_record_app_val(&drec, &e->literal))
            goto done;
    }

    db_cursor_t c = db_cursor_open_page(hdl, table->page, &table->rtype);
    if (!db_cursor_insert(c, &drec.rec))
        db_error("row primary key already exists");
    db_cursor_close(c);

done:
    db_table_free(table);
}

void db_insert_free(struct db_insert *insert) {
    free(insert->table);
    list_free((free_fn)db_expr_free, insert->exprs);
    free(insert);
}

void db_schema_dump(db_handle_t hdl) {
    db_cursor_t c =
        db_cursor_open_page(hdl, hdl->header->table_btree, &db_table_rtype);
    db_cursor_reset(c);
    int more = !db_cursor_end(c);
    while (more) {
        struct db_table *tab = db_table_load(c);
        db_info("table %s:\n", tab->name);
        for (int i = 0; i < tab->rtype.len; i++) {
            db_info("  %-10s (%c)%s\n", tab->col_names[i],
                    tab->rtype.col_types[i],
                    i < tab->rtype.key_len ? " pkey" : "");
        }
        db_table_free(tab);
        more = db_cursor_next(c);
    }
    db_cursor_close(c);
    db_info_send();
}

struct db_expr *db_expr_make_num(long num) {
    struct db_expr *ret = malloc(sizeof *ret);
    ret->tag = EXPR_LITERAL;
    ret->literal.tag = VAL_NUM;
    ret->literal.num = num;
    return ret;
}

struct db_expr *db_expr_make_string(char *str) {
    struct db_expr *ret = malloc(sizeof *ret);
    ret->tag = EXPR_LITERAL;
    ret->literal.tag = VAL_BLOB;
    ret->literal.len = strlen(str);
    ret->literal.blob = str;
    ret->literal.own = 1;
    return ret;
}

struct db_expr *db_expr_make_ref(char *name, char *table) {
    struct db_expr *ret = malloc(sizeof *ret);
    ret->tag = EXPR_REF;
    ret->ref.name = name;
    ret->ref.table = table;
    return ret;
}

struct db_expr *db_expr_make_binop(int tag, struct db_expr *l,
                                   struct db_expr *r) {
    struct db_expr *ret = malloc(sizeof *ret);
    ret->tag = EXPR_BINOP;
    ret->binop.tag = tag;
    ret->binop.es[0] = l;
    ret->binop.es[1] = r;
    return ret;
}

void db_expr_free(struct db_expr *expr) {
    switch (expr->tag) {
    case EXPR_LITERAL:
        db_val_free(&expr->literal);
        break;
    case EXPR_REF:
        free(expr->ref.name);
        free(expr->ref.table);
        break;
    case EXPR_BINOP:
        db_expr_free(expr->binop.es[0]);
        db_expr_free(expr->binop.es[1]);
        break;
    }
    free(expr);
}

const char *db_binop_strs[] = {
    [BINOP_AND] = "and", [BINOP_OR] = "or",     [BINOP_ADD] = "+",
    [BINOP_SUB] = "-",   [BINOP_MUL] = "*",     [BINOP_DIV] = "/",
    [BINOP_MOD] = "%",   [BINOP_EQ] = "=",      [BINOP_LT] = "<",
    [BINOP_GT] = ">",    [BINOP_NEQ] = "<>",    [BINOP_LEQ] = "<=",
    [BINOP_GEQ] = ">=",  [BINOP_LIKE] = "like",
};
const int db_binop_prec[] = {
    [BINOP_OR] = 0,   [BINOP_AND] = 1, [BINOP_EQ] = 2,  [BINOP_NEQ] = 2,
    [BINOP_LT] = 3,   [BINOP_GT] = 3,  [BINOP_LEQ] = 3, [BINOP_GEQ] = 3,
    [BINOP_LIKE] = 2, [BINOP_ADD] = 4, [BINOP_SUB] = 4, [BINOP_MUL] = 5,
    [BINOP_DIV] = 5,  [BINOP_MOD] = 5,
};

int db_binop_check(struct db_binop *binop, db_scalar_type t1,
                   db_scalar_type t2) {
    if (t1 == 1 || t2 == 1)
        return 1; /* lol */
    switch (binop->tag) {
    case BINOP_AND:
    case BINOP_OR:
    case BINOP_EQ:
    case BINOP_LT:
    case BINOP_GT:
    case BINOP_NEQ:
    case BINOP_LEQ:
    case BINOP_GEQ:
        return 1;
    case BINOP_LIKE:
        if (t1 != TYPE_BYTESTR || t2 != TYPE_BYTESTR) {
            db_error("type error (expected blob)");
            return 0;
        }
        return 1;
    case BINOP_ADD:
    case BINOP_SUB:
    case BINOP_MUL:
    case BINOP_DIV:
    case BINOP_MOD:
        if (t1 != TYPE_VARINT || t2 != TYPE_VARINT) {
            db_error("type error (expected integer)");
            return 0;
        }
        return 1;
    default:
        return 0;
    }
}

void db_expr_dump(const struct db_expr *expr, int prec) {
    switch (expr->tag) {
    case EXPR_LITERAL:
        db_val_dump(&expr->literal);
        break;
    case EXPR_REF:
        db_ref_dump(&expr->ref);
        break;
    case EXPR_BINOP: {
        int inner = db_binop_prec[expr->binop.tag];
        if (prec >= inner)
            db_info("(");
        db_expr_dump(expr->binop.es[0], inner - 1);
        db_info(" %s ", db_binop_strs[expr->binop.tag]);
        db_expr_dump(expr->binop.es[1], inner);
        if (prec >= inner)
            db_info(")");
        break;
    }
    }
}

void db_ref_dump(const struct db_ref *ref) {
    if (ref->table)
        db_info("%s.", ref->table);
    db_info("%s", ref->name);
}

void db_val_dump(const struct db_val *val) {
    switch (val->tag) {
    case VAL_NULL:
        db_info("NULL");
        break;
    case VAL_NUM:
        db_info("%ld", val->num);
        break;
    case VAL_BLOB:
        db_blob_dump(val->len, val->blob);
        break;
    case VAL_RECORD: {
        const char *p = val->record->rec.payload;
        db_record_dump(&val->record->rtype, &p);
        break;
    }
    case VAL_CURSOR:
        db_info("<cursor: %d>", val->cursor->page_stack[0]);
        break;
    }
}

void db_val_mov(struct db_val *dst, const struct db_val *src) {
    if (dst == src)
        return;
    db_val_free(dst);
    dst->tag = src->tag;
    switch (src->tag) {
    case VAL_NULL:
        break;
    case VAL_NUM:
        dst->num = src->num;
        break;
    case VAL_BLOB:
        dst->len = src->len;
        dst->blob = src->blob;
        dst->own = 0;
        break;
    case VAL_RECORD:
        dst->record = malloc(sizeof *dst->record);
        memcpy(dst->record, src->record, sizeof *dst->record);
        break;
    case VAL_CURSOR:
        dst->cursor = malloc(sizeof *dst->cursor);
        memcpy(dst->cursor, src->cursor, sizeof *dst->cursor);
        break;
    }
}

void db_val_free(struct db_val *val) {
    switch (val->tag) {
    case VAL_NULL:
    case VAL_NUM:
        break;
    case VAL_BLOB:
        if (val->own)
            free(val->blob);
        break;
    case VAL_RECORD:
        free(val->record);
        break;
    case VAL_CURSOR:
        db_cursor_close(val->cursor);
        break;
    }
}

int db_val_cmp(const struct db_val *v1, const struct db_val *v2) {
    if (v1->tag != v2->tag)
        return 1;
    switch (v1->tag) {
    case VAL_NULL:
        return 0;
    case VAL_NUM:
        return v1->num > v2->num ? 1 : v1->num < v2->num ? -1 : 0;
    case VAL_BLOB:
        return memncmp(v1->blob, v2->blob, v1->len, v2->len);
    case VAL_RECORD: {
        const char *p = v2->record->rec.payload;
        return db_record_cmp(&v1->record->rtype, v1->record->rec.payload, &p);
    }
    case VAL_CURSOR:
        return v1->cursor - v2->cursor;
    default:
        return 1;
    }
}

int db_val_nz(struct db_val *val) { return val->tag != VAL_NUM || val->num; }

void db_coroutine_free(struct db_coroutine *co) {
    for (int i = 0; i < DB_REGS; i++)
        db_val_free(co->regs + i);
}

void db_select_free(struct db_select *select) {
    /* I don't care that this cast is UB */
    list_free((free_fn)db_expr_free, select->exprs);
    if (select->from)
        db_subquery_free(select->from);
    if (select->where)
        db_expr_free(select->where);
    free(select);
}

void db_subquery_free(struct db_subquery *subquery) {
    if (!subquery)
        return;
    switch (subquery->tag) {
    case SUBQUERY_TABLE:
        free(subquery->table);
        break;
    }
    if (subquery->join)
        db_subquery_free(subquery->join);
    if (subquery->constraint)
        db_expr_free(subquery->constraint);
    free(subquery);
}

void db_plan_step_free(struct db_plan_step *step) {
    list_free(NULL, step->cond);
}

void db_execute_query(db_handle_t hdl, struct db_select *select) {
    struct db_coroutine co;
    memset(&co, 0, sizeof co);
    if (!db_compile_query(hdl, &co, select))
        return;

    if (!select->explain)
        db_bytecode_exec(hdl, &co, select);
    db_coroutine_free(&co);
}

int db_compile_query(db_handle_t hdl, struct db_coroutine *co,
                     struct db_select *select) {
    struct db_compiler *comp = db_compiler_init(hdl);
    comp->coroutine = co;

    if (!db_compile_lookup_tables(comp, select->from))
        goto fail;
    if (!db_compile_check(comp, select))
        goto fail;
    db_compile_flatten_conj(comp, select);

    comp->empty_rec = db_compile_reg_alloc(comp);
    co->regs[comp->empty_rec] = (struct db_val){
        .tag = VAL_RECORD,
        .record = calloc(1, sizeof(struct db_dyn_record)),
    };

    if (!db_compile_plan(comp, select))
        goto fail;
    if (!db_compile_query_head(comp, select->from))
        goto fail;

    u8 dest = db_compile_reg_alloc(comp);
    u8 rec = db_compile_reg_alloc(comp);
    EMIT_FAIL(INSTR_RR(OP_MOV, rec, comp->empty_rec));
    for (list_t l = select->exprs; l; l = l->tail) {
        if (!db_compile_expr(comp, dest, (struct db_expr *)l->head))
            goto fail;
        if (!db_compile_emit(comp, INSTR_RR(OP_APP, rec, dest)))
            goto fail;
    }
    EMIT_FAIL(INSTR_R(OP_YLD, rec));
    db_compile_reg_free(comp, dest);
    db_compile_reg_free(comp, rec);

    if (!db_compile_query_loop(comp, select->from))
        goto fail;
    if (!db_compile_emit(comp, INSTR(OP_RET)))
        goto fail;

#ifndef DISABLE_EXPLAIN
    if (select->explain) {
        db_info("\ninitial regs:\n");
        for (int i = 0; i < DB_REGS; i++) {
            struct db_val *val = co->regs + i;
            if (val->tag != VAL_NULL) {
                db_info("  r%-2d = ", i);
                db_val_dump(co->regs + i);
                db_info("\n");
            }
        }
        db_compile_disasm(comp);
        db_info_send();
    }
#endif
    db_compiler_free(comp);
    return 1;

fail:
    db_compiler_free(comp);
    db_coroutine_free(co);
    return 0;
}

struct db_compiler *db_compiler_init(db_handle_t hdl) {
    struct db_compiler *ret = calloc(1, sizeof *ret);
    ret->hdl = hdl;
    return ret;
}

void db_compiler_free(struct db_compiler *comp) {
    for (int i = 0; i < comp->plan_len; i++)
        db_plan_step_free(comp->plan + i);
    for (int i = 0; i < comp->n_tables; i++)
        db_table_free(comp->tables[i]);
    list_free(NULL, comp->conj);
    free(comp);
}

int db_compile_plan(struct db_compiler *comp, struct db_select *select) {
    for (int i = 0; i < comp->n_tables; i++) {
        struct db_plan_step best_step, cur_step;
        int best_cost = INT_MAX, cur_cost;
        for (int j = 0; j < comp->n_tables; j++) {
            for (int k = 0; k < comp->plan_len; k++) {
                if (comp->tables[j] == comp->plan[k].table)
                    goto skip_inner;
            }

            cur_cost =
                db_compile_join(comp, select, j, comp->tables[j], &cur_step);
            if (cur_cost < best_cost) {
                best_cost = cur_cost;
                best_step = cur_step;
            }
        skip_inner:;
        }
        comp->plan[comp->plan_len++] = best_step;
    }

    /* Remove expressions used in searches from the filter. */
    for (int i = 0; i < comp->plan_len; i++)
        list_delete(&comp->conj, comp->plan[i].conj_expr);

    for (int i = 0; i < comp->plan_len; i++) {
        struct db_plan_step *step = comp->plan + i;
        list_t *c = &comp->conj;
        while (*c) {
            if (db_compile_can_eval(comp, i + 1, (*c)->head)) {
                /* Move this filter condition to the corresponding step. */
                list_t tmp = *c;
                *c = tmp->tail;
                tmp->tail = step->cond;
                step->cond = tmp;
                continue;
            }
            c = &(*c)->tail;
        }
    }

    if (select->explain)
#ifndef DISABLE_EXPLAIN
        db_compile_explain_plan(comp);
#else
        db_info(":)");
#endif

    return 1;
}

#ifndef DISABLE_EXPLAIN
void db_compile_explain_plan(struct db_compiler *comp) {
    db_info("query plan:\n");
    for (int i = 0; i < comp->n_tables; i++) {
        struct db_plan_step *step = comp->plan + i;
        switch (step->type) {
        case PLAN_SCAN:
            db_info("  scan %s\n", step->table->name);
            break;
        case PLAN_SEARCH:
            db_info("  search %s using %s = ", step->table->name,
                    step->table->col_names[0]);
            db_expr_dump(step->key_expr, -1);
            db_info("\n");
            break;
        }
        for (list_t c = step->cond; c; c = c->tail) {
            db_info("    filter ");
            db_expr_dump(c->head, -1);
            db_info("\n");
        }
    }
}
#endif

int db_compile_check(struct db_compiler *comp, struct db_select *select) {
    for (list_t l = select->exprs; l; l = l->tail) {
        struct db_expr *e = (struct db_expr *)l->head;
        if (!db_compile_check_expr(comp, e))
            return 0;
    }
    if (select->where && !db_compile_check_expr(comp, select->where))
        return 0;

    for (struct db_subquery *q = select->from; q; q = q->join) {
        switch (q->tag) {
        case SUBQUERY_TABLE:
            if (q->constraint && !db_compile_check_expr(comp, q->constraint))
                return 0;
            break;
        }
    }

    return 1;
}

db_scalar_type db_compile_check_expr(struct db_compiler *comp,
                                     struct db_expr *expr) {
    switch (expr->tag) {
    case EXPR_LITERAL:
        switch (expr->literal.tag) {
        case VAL_NUM:
            return TYPE_VARINT;
        case VAL_BLOB:
            return TYPE_BYTESTR;
        default:
            return 0;
        }
        break;
    case EXPR_REF:
        if (!db_compile_resolve_ref(comp, &expr->ref))
            return 0;
        return 1;
    case EXPR_BINOP: {
        db_scalar_type ts[2];
        for (int i = 0; i < 2; i++) {
            ts[i] = db_compile_check_expr(comp, expr->binop.es[i]);
            if (!ts[i])
                return 0;
        }
        return db_binop_check(&expr->binop, ts[0], ts[1]) ? TYPE_VARINT : 0;
    }
    }
    return 0;
}

int db_compile_resolve_ref(struct db_compiler *comp, struct db_ref *ref) {
    int found = 0;
    for (int i = 0; i < comp->n_tables; i++) {
        struct db_table *tab = comp->tables[i];
        if (ref->table && strcmp(tab->name, ref->table))
            continue;
        for (int j = 0; j < tab->rtype.len; j++) {
            if (!strcmp(tab->col_names[j], ref->name)) {
                if (found) {
                    db_error("ambiguous variable %s", ref->name);
                    return 0;
                }
                ref->tab_idx = i;
                ref->col_idx = j;
                found = 1;
            }
        }
    }

    if (found)
        return 1;

    if (ref->table)
        db_error("unbound variable %s.%s", ref->table, ref->name);
    else
        db_error("unbound variable %s", ref->name);
    return 0;
}

void db_compile_flatten_conj(struct db_compiler *comp,
                             struct db_select *select) {
    /* Just assume left associated AND clause */
    if (select->where) {
        struct db_expr *expr;
        for (expr = select->where;
             expr->tag == EXPR_BINOP && expr->binop.tag == BINOP_AND;
             expr = expr->binop.es[0])
            comp->conj = list_cons(expr->binop.es[1], comp->conj);
        comp->conj = list_cons(expr, comp->conj);
    }
    for (struct db_subquery *subq = select->from; subq; subq = subq->join)
        if (subq->constraint)
            comp->conj = list_cons(subq->constraint, comp->conj);
}

/*
 * Since we're going to use a greedy algorithm to find the query plan, the
 * actual cost doesn't matter as much as the ordering.  In order of decreasing
 * preference, the ways to compute a join between relations A -> B are:
 *
 *   Use a column from A to do a primary key lookup in B
 *   Use a column from A to do an index lookup in B      (TODO)
 *   Scan B with a WHERE clause including columns from B
 *   Scan B
 */
int db_compile_join(struct db_compiler *comp, const struct db_select *select,
                    int tab_idx, const struct db_table *table,
                    struct db_plan_step *step) {
    memset(step, 0, sizeof *step);
    step->table = table;
    step->tab_idx = tab_idx;
    /* TODO: lmao what the fuck is this hack */
    comp->plan[comp->plan_len] = *step;

    int cost = 100, found_idx = 0;

    for (list_t l = comp->conj; l; l = l->tail) {
        struct db_expr *e = l->head;
        if (db_compile_can_eval(comp, comp->plan_len + 1, e)) {
            cost = cost > 2 ? cost - 1 : cost;
        }

        if (e->tag != EXPR_BINOP || e->binop.tag != BINOP_EQ)
            continue;
        for (int i = 0; i < 2; i++) {
            struct db_expr *c = e->binop.es[i], *key = e->binop.es[1 - i];

            if (c->tag != EXPR_REF)
                continue;
            /*
             *  col_idx == 0 guarantees it's in the primary key.  Remove
             *  key_len == 1 constraint after we support using partial indexes.
             */
            if (comp->tables[c->ref.tab_idx] == table &&
                table->rtype.key_len == 1 && c->ref.col_idx == 0 &&
                db_compile_can_eval(comp, comp->plan_len, key)) {
                found_idx = 1;
                step->type = PLAN_SEARCH;
                step->conj_expr = e;
                step->key_expr = key;
                cost -= 90;
            }
        }
    }

    if (!found_idx)
        step->type = PLAN_SCAN;
    return cost;
}

/* Can this expression be evaluated under the current query plan? */
int db_compile_can_eval(struct db_compiler *comp, int plan_len,
                        struct db_expr *expr) {
    switch (expr->tag) {
    case EXPR_LITERAL:
        return 1;
    case EXPR_REF: {
        struct db_table *tab = comp->tables[expr->ref.tab_idx];
        int in_plan = 0;
        for (int i = 0; i < plan_len; i++)
            if ((in_plan = comp->plan[i].table == tab))
                break;
        return in_plan;
    }
    case EXPR_BINOP:
        return db_compile_can_eval(comp, plan_len, expr->binop.es[0]) &&
               db_compile_can_eval(comp, plan_len, expr->binop.es[1]);
    }
    return 0;
}

int db_compile_lookup_tables(struct db_compiler *comp,
                             struct db_subquery *subq) {
    for (comp->n_tables = 0; subq; comp->n_tables++, subq = subq->join) {
        if (comp->n_tables >= DB_MAX_JOINS) {
            db_error("too many joins");
            return 0;
        }

        switch (subq->tag) {
        case SUBQUERY_TABLE: {
            struct db_table *table = db_table_lookup(comp->hdl, subq->table);
            if (!table)
                return 0;
            comp->tables[comp->n_tables] = table;
            break;
        }
        }
    }
    return 1;
}

int db_compile_query_head(struct db_compiler *comp, struct db_subquery *subq) {
    for (int i = 0; i < comp->plan_len; i++) {
        u8 cursor_reg = db_compile_reg_alloc(comp),
           rec_reg = db_compile_reg_alloc(comp);
        struct db_plan_step *step = comp->plan + i;
        comp->cursors[step->tab_idx] = cursor_reg;
        comp->records[step->tab_idx] = rec_reg;
    }

    for (int i = 0; i < comp->plan_len; i++) {
        struct db_plan_step *step = comp->plan + i;
        u8 cursor_reg = comp->cursors[step->tab_idx],
           rec_reg = comp->records[step->tab_idx];

        struct db_record_type rtype = step->table->rtype;
        rtype.key_len = 1;
        db_cursor_t c =
            db_cursor_open_page(comp->hdl, step->table->page, &rtype);
        comp->coroutine->regs[cursor_reg] =
            (struct db_val){.tag = VAL_CURSOR, .cursor = c};

        switch (step->type) {
        case PLAN_SCAN:
            EMIT_FAIL(INSTR_R(OP_RST, cursor_reg));
            break;
        case PLAN_SEARCH: {
            u8 key_reg = db_compile_reg_alloc(comp);
            if (!db_compile_expr(comp, key_reg, step->key_expr))
                goto fail;
            u8 rec_reg = db_compile_reg_alloc(comp);
            EMIT_FAIL(INSTR_RR(OP_MOV, rec_reg, comp->empty_rec));
            EMIT_FAIL(INSTR_RR(OP_APP, rec_reg, key_reg));
            db_compile_reg_free(comp, key_reg);
            EMIT_FAIL(INSTR_RR(OP_FIND, cursor_reg, rec_reg));
            db_compile_reg_free(comp, rec_reg);
            break;
        }
        }

        comp->loop_offs[i] = comp->off;
        EMIT_FAIL(INSTR_RO(OP_BEND, cursor_reg, 0));
        EMIT_FAIL(INSTR_RR(OP_LDR, rec_reg, cursor_reg));

        u8 cond_reg = db_compile_reg_alloc(comp);
        for (list_t c = step->cond; c; c = c->tail) {
            if (!db_compile_expr(comp, cond_reg, c->head))
                goto fail;
            EMIT_FAIL(INSTR_RO(OP_BZ, cond_reg, -i - 1));
        }
        db_compile_reg_free(comp, cond_reg);
    }

    return 1;

fail:
    return 0;
}

int db_compile_query_loop(struct db_compiler *comp, struct db_subquery *subq) {
    for (int i = comp->plan_len - 1; i >= 0; i--) {
        switch (comp->plan[i].type) {
        case PLAN_SCAN:
            comp->loop_skip_offs[i] = comp->off;
            EMIT_FAIL(INSTR_R(OP_NEXT, comp->cursors[comp->plan[i].tab_idx]));
            EMIT_FAIL(INSTR_O(OP_B, comp->loop_offs[i]));
            break;
        case PLAN_SEARCH:
            break;
        }
        comp->coroutine->code[comp->loop_offs[i]].off = comp->off;
    }

    for (int i = 0; i < comp->off; i++) {
        struct db_instr *instr = comp->coroutine->code + i;
        if (instr->op == OP_B || instr->op == OP_BZ || instr->op == OP_BNZ ||
            instr->op == OP_BEND) {
            if (instr->off < 0)
                instr->off = comp->loop_skip_offs[-instr->off - 1];
        }
    }

    return 1;
fail:
    return 0;
}

int db_compile_ref(struct db_compiler *comp, u8 dest,
                   const struct db_ref *ref) {
    return db_compile_emit(
        comp,
        INSTR_RRI(OP_PROJ, dest, comp->records[ref->tab_idx], ref->col_idx));
}

u8 binop_ops[] = {
    [BINOP_AND] = OP_AND, [BINOP_OR] = OP_OR,     [BINOP_ADD] = OP_ADD,
    [BINOP_SUB] = OP_SUB, [BINOP_MUL] = OP_MUL,   [BINOP_DIV] = OP_DIV,
    [BINOP_MOD] = OP_MOD, [BINOP_EQ] = OP_EQ,     [BINOP_LT] = OP_LT,
    [BINOP_GT] = OP_GT,   [BINOP_NEQ] = OP_NEQ,   [BINOP_LEQ] = OP_LEQ,
    [BINOP_GEQ] = OP_GEQ, [BINOP_LIKE] = OP_LIKE,
};

int db_compile_expr(struct db_compiler *comp, u8 dest,
                    const struct db_expr *expr) {
    switch (expr->tag) {
    case EXPR_LITERAL: {
        int r = db_compile_expr_alloc(comp, expr);
        EMIT_FAIL(INSTR_RR(OP_MOV, dest, r));
        return 1;
    }
    case EXPR_REF:
        return db_compile_ref(comp, dest, &expr->ref);
    case EXPR_BINOP: {
        if (!db_compile_expr(comp, dest, expr->binop.es[0]))
            return 0;
        int r = db_compile_expr_alloc(comp, expr->binop.es[1]);
        if (r < 0)
            return 0;
        EMIT_FAIL(INSTR_RR(binop_ops[expr->binop.tag], dest, r));
        db_compile_reg_free(comp, r);
        return 1;
    }
    }

fail:
    return 0;
}


int db_compile_expr_alloc(struct db_compiler *comp, const struct db_expr *expr) {
    u8 r;
    if (expr->tag != EXPR_LITERAL) {
        r = db_compile_reg_alloc(comp);
        if (!db_compile_expr(comp, r, expr))
            goto fail;
        return r;
    }
    int i;
    for (i = 0; i < DB_REGS; i++) {
        if (!db_val_cmp(&comp->coroutine->regs[i], &expr->literal))
            return i;
    }
    r = db_compile_reg_alloc(comp);
    if (r < DB_REGS)
        db_val_mov(&comp->coroutine->regs[r], &expr->literal);
    return r;
 fail:
    return -1;
}


#ifndef DISABLE_EXPLAIN
#define INSTR_RS (1 << 0)
#define INSTR_RD (1 << 1)
#define INSTR_IMM (1 << 2)
#define INSTR_OFF (1 << 3)
struct db_instr_info {
    const char *mnemonic;
    char flags;
};
const struct db_instr_info db_instr_info[] = {
    [OP_RET] = {"ret", 0},
    [OP_YLD] = {"yld", INSTR_RD},
    [OP_MOV] = {"mov", INSTR_RD | INSTR_RS},
    [OP_AND] = {"and", INSTR_RD | INSTR_RS},
    [OP_OR] = {"or", INSTR_RD | INSTR_RS},
    [OP_ADD] = {"add", INSTR_RD | INSTR_RS},
    [OP_SUB] = {"sub", INSTR_RD | INSTR_RS},
    [OP_MUL] = {"mul", INSTR_RD | INSTR_RS},
    [OP_DIV] = {"div", INSTR_RD | INSTR_RS},
    [OP_MOD] = {"mod", INSTR_RD | INSTR_RS},
    [OP_EQ] = {"eq", INSTR_RD | INSTR_RS},
    [OP_LT] = {"lt", INSTR_RD | INSTR_RS},
    [OP_GT] = {"gt", INSTR_RD | INSTR_RS},
    [OP_NEQ] = {"neq", INSTR_RD | INSTR_RS},
    [OP_LEQ] = {"leq", INSTR_RD | INSTR_RS},
    [OP_GEQ] = {"geq", INSTR_RD | INSTR_RS},
    [OP_LIKE] = {"like", INSTR_RD | INSTR_RS},
    [OP_B] = {"b", INSTR_OFF},
    [OP_BZ] = {"bz", INSTR_RD | INSTR_OFF},
    [OP_BNZ] = {"bnz", INSTR_RD | INSTR_OFF},
    [OP_BEND] = {"bend", INSTR_RD | INSTR_OFF},
    [OP_APP] = {"app", INSTR_RD | INSTR_RS},
    [OP_PROJ] = {"proj", INSTR_RD | INSTR_RS | INSTR_IMM},
    [OP_RST] = {"rst", INSTR_RD},
    [OP_FIND] = {"find", INSTR_RD | INSTR_RS},
    [OP_NEXT] = {"next", INSTR_RD},
    [OP_LDR] = {"ldr", INSTR_RD | INSTR_RS},
    [OP_STR] = {"str", INSTR_RD | INSTR_RS},
};

static void db_compile_disasm_instr(int pc, struct db_instr instr) {
    struct db_instr_info info = db_instr_info[instr.op];
    db_info("%3x: %-4s ", pc, info.mnemonic);
    if (info.flags & INSTR_RD)
        db_info("r%d", instr.rd);
    if (info.flags & INSTR_RS)
        db_info(", r%d", instr.rs);
    if (info.flags & INSTR_IMM)
        db_info(", %d", instr.imm);
    if (info.flags & INSTR_OFF) {
        if (info.flags & INSTR_RD)
            db_info(", ");
        if (instr.off < 0)
            db_info("[reloc %d]", -instr.off);
        else
            db_info("%x", instr.off);
    }
    db_info("\n");
}

void db_compile_disasm(struct db_compiler *comp) {
    db_info("\nbytecode:\n");
    for (int pc = 0; pc < comp->off; pc++)
        db_compile_disasm_instr(pc, comp->coroutine->code[pc]);
}
#endif

int db_compile_emit(struct db_compiler *comp, struct db_instr instr) {
    if (comp->off >= DB_MAX_CODE_LEN) {
        db_error("exceeded code size limit");
        return 0;
    }
    comp->coroutine->code[comp->off++] = instr;
    return 1;
}

u8 db_compile_reg_alloc(struct db_compiler *comp) { return comp->free_reg++; }

void db_compile_reg_free(struct db_compiler *comp, u8 reg) {}

int db_bytecode_exec(db_handle_t hdl, struct db_coroutine *coroutine,
                     const struct db_select *select) {
    enum db_vm_status status = VM_OK;
    struct db_val *val;
    int skipped = 0, read = 0;
    while ((status == VM_OK || status == VM_YLD) &&
           (select->limit < 0 || read < select->limit)) {
        status = db_bytecode_step(coroutine, &val);
        if (status == VM_YLD) {
            if (select->offset < 0 || skipped++ >= select->offset) {
                db_output_row(val);
                read++;
            }
        }
    }

    if (status == VM_ERROR)
        return 0;

    return 1;
}

enum db_vm_status db_bytecode_step(struct db_coroutine *coroutine,
                                   struct db_val **out) {
    struct db_instr instr = coroutine->code[coroutine->pc];
    struct db_val *dst = coroutine->regs + instr.rd,
                  *src = coroutine->regs + instr.rs;
    switch (instr.op) {
    case OP_RET:
        return VM_RET;
    case OP_YLD:
        *out = dst;
        coroutine->pc++;
        return VM_YLD;
    case OP_MOV:
        db_val_mov(dst, src);
        break;
    case OP_AND: {
        int ret = db_val_nz(dst) && db_val_nz(src);
        db_val_free(dst);
        dst->tag = VAL_NUM;
        dst->num = ret;
        break;
    }
    case OP_OR: {
        int ret = db_val_nz(dst) || db_val_nz(src);
        db_val_free(dst);
        dst->tag = VAL_NUM;
        dst->num = ret;
        break;
    }
    case OP_ADD:
        dst->num += src->num;
        break;
    case OP_SUB:
        dst->num -= src->num;
        break;
    case OP_MUL:
        dst->num *= src->num;
        break;
    case OP_DIV:
        if (!src->num) {
            db_error("division by zero");
            return VM_ERROR;
        }
        dst->num /= src->num;
        break;
    case OP_MOD:
        dst->num %= src->num;
        break;
    case OP_EQ:
    case OP_LT:
    case OP_GT:
    case OP_NEQ:
    case OP_LEQ:
    case OP_GEQ:
        db_bytecode_cmp(dst, src, instr.op);
        break;
    case OP_LIKE: {
        int cmp = db_blob_like(dst->blob, dst->blob + dst->len, src->blob,
                               src->blob + src->len);
        db_val_free(dst);
        dst->tag = VAL_NUM;
        dst->num = cmp;
        break;
    }
    case OP_B:
        coroutine->pc = instr.off;
        return VM_OK;
    case OP_BZ:
        if (!db_val_nz(dst)) {
            coroutine->pc = instr.off;
            return VM_OK;
        }
        break;
    case OP_BNZ:
        if (db_val_nz(dst)) {
            coroutine->pc = instr.off;
            return VM_OK;
        }
        break;
    case OP_BEND:
        if (db_cursor_end(dst->cursor)) {
            coroutine->pc = instr.off;
            return VM_OK;
        }
        break;
    case OP_APP:
        if (!db_record_app_val(dst->record, src))
            return VM_ERROR;
        break;
    case OP_PROJ: {
        struct db_val old = *dst;
        *dst =
            db_record_proj(&src->record->rtype, &src->record->rec, instr.imm);
        db_val_free(&old);
        break;
    }
    case OP_RST:
        db_cursor_reset(dst->cursor);
        break;
    case OP_FIND:
        if (!db_cursor_lookup(dst->cursor, &src->record->rec))
            db_cursor_reset_end(dst->cursor);
        break;
    case OP_NEXT:
        db_cursor_next(dst->cursor);
        break;
    case OP_LDR: {
        db_cursor_t c = src->cursor;
        struct db_val v = {
            .tag = VAL_RECORD,
            .record = malloc(sizeof *v.record),
        };
        v.record->rtype = c->rtype;
        db_cursor_load(c, &v.record->rec);
        db_val_free(dst);
        *dst = v;
        break;
    }
    case OP_STR:
        return VM_ERROR;
    default:
        return VM_ERROR;
    }

    coroutine->pc++;
    return VM_OK;
}

void db_bytecode_cmp(struct db_val *dst, struct db_val *src,
                     enum db_opcode op) {
    int cmp = db_val_cmp(dst, src);
    db_val_free(dst);
    dst->tag = VAL_NUM;
    switch (op) {
    case OP_EQ:
        dst->num = cmp == 0;
        break;
    case OP_LT:
        dst->num = cmp < 0;
        break;
    case OP_GT:
        dst->num = cmp > 0;
        break;
    case OP_NEQ:
        dst->num = cmp != 0;
        break;
    case OP_LEQ:
        dst->num = cmp <= 0;
        break;
    case OP_GEQ:
        dst->num = cmp >= 0;
        break;
    default:
        __builtin_unreachable();
        break;
    }
}

void db_page_map(db_handle_t hdl, struct db_header *hdr) {
    hdl->mapped = mmap(NULL, hdr->page_total * DB_PAGE_SIZE,
                       PROT_READ | PROT_WRITE, MAP_SHARED, hdl->fd, 0);
    if (hdl->mapped == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
}

void db_page_grow(db_handle_t hdl, u32 num) {
    struct db_header hdr;
    memcpy(&hdr, hdl->header, sizeof hdr);
    if (hdr.page_total + num < hdr.page_total)
        exit(1);
    if (hdl->mapped && munmap(hdl->mapped, hdr.page_total * DB_PAGE_SIZE)) {
        perror("munmap");
        exit(1);
    }
    if (ftruncate(hdl->fd, (hdr.page_total + num) * DB_PAGE_SIZE) < 0) {
        perror("ftruncate");
        exit(1);
    }
    hdr.page_total += num;
    db_page_map(hdl, &hdr);
    hdl->header->page_total = hdr.page_total;

    u32 i, total = hdl->header->page_total;
    for (i = total - num; i < total; i++) {
        struct db_page *page = db_page_get(hdl, i);
        page->type = PAGE_FREE;
        page->free.next = i == total - 1 ? hdl->header->freelist_head : i + 1;
    }
    hdl->header->freelist_head = total - num;
}

page_idx db_page_alloc(db_handle_t hdl) {
    page_idx head = hdl->header->freelist_head;
    if (head) {
        struct db_page *head_page = db_page_get(hdl, head);
        assert(head_page->type == PAGE_FREE);
        hdl->header->freelist_head = head_page->free.next;
        hdl->header->page_alloc++;
        return head;
    } else {
        db_page_grow(hdl, hdl->header->page_total > DB_SIZE_THRESHOLD
                              ? DB_GROW_SIZE
                              : 1);
        return db_page_alloc(hdl);
    }
}

void db_page_free(db_handle_t hdl, page_idx idx) {
    struct db_page *page = db_page_get(hdl, idx);
    assert(page->type != PAGE_HEADER && page->type != PAGE_FREE);
    page->type = PAGE_FREE;
    page->free.next = hdl->header->freelist_head;
    hdl->header->freelist_head = idx;
    hdl->header->page_alloc--;
}

struct db_page *db_page_get(db_handle_t hdl, page_idx page) {
    assert(page < hdl->header->page_total);
    return (struct db_page *)((char *)hdl->mapped + page * DB_PAGE_SIZE);
}

page_idx db_btree_alloc(db_handle_t hdl) {
    page_idx idx = db_page_alloc(hdl);
    struct db_page *page = db_page_get(hdl, idx);
    page->type = PAGE_BTREE;
    page->btree.keys = 0;
    page->btree.size = 1; /* Initial, leftmost 0 page_idx */
    page->btree.payload[0] = 0;
    return idx;
}

db_cursor_t db_cursor_open_page(db_handle_t hdl, page_idx tree,
                                const struct db_record_type *rtype) {
    struct db_cursor *c = calloc(1, sizeof *c);
    c->hdl = hdl;
    c->page_stack[0] = tree;
    c->rtype = *rtype;
    return c;
}

void db_cursor_close(db_cursor_t cursor) { free(cursor); }

struct db_page *db_cursor_get_page(db_cursor_t cursor) {
    page_idx idx = cursor->page_stack[cursor->depth];
    return db_page_get(cursor->hdl, idx);
}

char *db_cursor_get_ptr(db_cursor_t cursor) {
    return db_cursor_get_page(cursor)->btree.payload +
           cursor->offset_stack[cursor->depth];
}

char *db_cursor_get_end(db_cursor_t cursor) {
    struct db_page_btree *btree = &db_cursor_get_page(cursor)->btree;
    return btree->payload + btree->size;
}

int db_cursor_find_leaf(db_cursor_t cursor) {
    const char *p = db_cursor_get_ptr(cursor);
    page_idx next = db_varint_decode(&p);
    if (!next)
        return 0;
    while (next) {
        cursor->page_stack[++cursor->depth] = next;
        cursor->offset_stack[cursor->depth] = 0;
        p = db_cursor_get_ptr(cursor);
        next = db_varint_decode(&p);
    }
    return 1;
}

void db_cursor_reset(db_cursor_t cursor) {
    cursor->depth = 0;
    cursor->offset_stack[0] = 0;
    db_cursor_find_leaf(cursor);
}

void db_cursor_reset_end(db_cursor_t cursor) {
    cursor->depth = 0;
    cursor->offset_stack[0] = db_cursor_get_page(cursor)->btree.size;
}

int db_cursor_next(db_cursor_t cursor) {
    const struct db_page_btree *btree = &db_cursor_get_page(cursor)->btree;
    const char *p = db_cursor_get_ptr(cursor), *end = db_cursor_get_end(cursor);

    db_varint_next(&p);
    db_record_next(&cursor->rtype, &p);
    cursor->offset_stack[cursor->depth] = p - btree->payload;
    if (db_cursor_find_leaf(cursor))
        return 1;

    while (db_varint_next(&p), p >= end && cursor->depth) {
        cursor->depth--;
        p = db_cursor_get_ptr(cursor);
        end = db_cursor_get_end(cursor);
    }

    return p < end;
}

int db_cursor_end(db_cursor_t cursor) {
    if (cursor->depth)
        return 0;
    const char *p = db_cursor_get_ptr(cursor);
    db_varint_next(&p);
    return p >= db_cursor_get_end(cursor);
}

int db_cursor_lookup(db_cursor_t cursor, const struct db_record *rec) {
    cursor->depth = 0;
    while (1) {
        struct db_page_btree *btree = &db_cursor_get_page(cursor)->btree;

        const char *p_before = btree->payload;
        const char *p = p_before;

        int cmp = 1;
        page_idx left_idx = 0;
        for (int i = 0; cmp > 0 && (p_before = p); i++) {
            left_idx = db_varint_decode(&p);
            if (i >= btree->keys)
                break;
            cmp = db_record_cmp(&cursor->rtype, rec->payload, &p);
        }

        /* p_before points to the left ptr of the first key gte the record. */
        cursor->offset_stack[cursor->depth] = p_before - btree->payload;
        if (!cmp)
            return 1;
        if (!left_idx)
            return 0;

        cursor->offset_stack[++cursor->depth] = 0;
        cursor->page_stack[cursor->depth] = left_idx;
    }
}

int db_cursor_insert(db_cursor_t cursor, const struct db_record *rec) {
    int found = db_cursor_lookup(cursor, rec);
    if (found)
        return 0;

    /* The cursor points to the page_idx the key will be inserted after. */
    page_idx right_idx = 0;
    struct db_record key = *rec;
    char page_buf[DB_PAGE_SIZE * 2];

    while (1) {
        struct db_page *page = db_cursor_get_page(cursor);
        struct db_page_btree *btree = &page->btree;
        int offset = cursor->offset_stack[cursor->depth];

        int idx_len = db_varint_size(right_idx);

        int new_size = btree->size + key.len + idx_len;
        int new_keys = btree->keys + 1;

        if (new_size <= (long)DB_MAX_BTREE_PAYLOAD) {
            /* We can insert in-place. */
            db_record_insert(btree->payload, btree->size, offset, &key,
                             right_idx);
            btree->size = new_size;
            btree->keys = new_keys;
            return 1;
        }

        if (cursor->depth >= DB_MAX_BTREE_DEPTH - 1) {
            db_error("max btree depth exceeded");
            return 0;
        }

        memcpy(page_buf, btree->payload, btree->size);
        char *page_buf_end = page_buf + new_size;
        db_record_insert(page_buf, btree->size, offset, &key, right_idx);

        char *median;
        int median_idx;
        db_record_find_median(&cursor->rtype, page_buf, new_size, new_keys,
                              &median, &median_idx);

        const char *right = median;
        db_record_next(&cursor->rtype, &right);

        /* Greater keys are always evicted to a new page. */
        right_idx = db_btree_alloc(cursor->hdl);
        struct db_page_btree *right_btree =
            &db_page_get(cursor->hdl, right_idx)->btree;
        int right_size = page_buf_end - right;
        memcpy(right_btree->payload, right, right_size);
        right_btree->size = right_size;
        right_btree->keys = new_keys - median_idx - 1;

        /* Lesser keys stay in the original page, unless this is the root. */
        int left_size = median - page_buf;
        struct db_page_btree *left_btree;
        if (cursor->depth == 0) {
            page_idx left_idx = db_btree_alloc(cursor->hdl);
            left_btree = &db_page_get(cursor->hdl, left_idx)->btree;
            btree = &db_cursor_get_page(cursor)->btree;
            btree->keys = 0;
            btree->size = db_varint_encode(btree->payload, left_idx);
            cursor->offset_stack[0] = 0;
        } else {
            left_btree = &db_cursor_get_page(cursor)->btree;
            cursor->depth--;
        }

        memcpy(left_btree->payload, page_buf, left_size);
        left_btree->size = left_size;
        left_btree->keys = median_idx;

        key.len = right - median;
        memcpy(key.payload, median, key.len);
    }
}

void db_cursor_load(db_cursor_t cursor, struct db_record *out) {
    const char *end = db_cursor_get_ptr(cursor);
    db_varint_next(&end);
    const char *start = end;
    db_record_next(&cursor->rtype, &end);
    out->len = end - start;
    assert(out->len <= DB_MAX_RECORD_LEN);
    memcpy(out->payload, start, out->len);
}

int db_blob_like(const char *data, const char *data_end, const char *pat,
                 const char *pat_end) {
    if (pat >= pat_end)
        return data >= data_end;
    else if (data >= data_end)
        return *pat == '%' && db_blob_like(data, data_end, pat + 1, pat_end);
    else if (*pat == '%')
        return db_blob_like(data + 1, data_end, pat, pat_end) ||
               db_blob_like(data, data_end, pat + 1, pat_end);
    else if (tolower(*pat) == tolower(*data))
        return db_blob_like(data + 1, data_end, pat + 1, pat_end);
    else
        return 0;
}

void db_blob_dump(int len, const char *blob) {
    db_info("'");
    for (int i = 0; i < len; i++) {
        u8 c = *blob++;
        if (c == '\'')
            db_info("''");
        else if (c == '\\')
            db_info("\\\\");
        else if (isprint(c) || c == ' ')
            db_info("%c", c);
        else
            db_info("\\x%02x", c);
    }
    db_info("'");
}

void db_scalar_dump(db_scalar_type type, const char **buf) {
    int len;
    switch (type) {
    case TYPE_U8:
        db_info("%du8", *(*buf)++);
        break;
    case TYPE_VARINT:
        db_info("%d", db_varint_decode(buf));
        break;
    case TYPE_BYTESTR:
        len = db_varint_decode(buf);
        db_blob_dump(len, *buf);
        *buf += len;
        break;
    }
}

void db_scalar_next(db_scalar_type type, const char **buf) {
    int len;
    switch (type) {
    case TYPE_U8:
        (*buf)++;
        break;
    case TYPE_VARINT:
        db_varint_next(buf);
        break;
    case TYPE_BYTESTR:
        len = db_varint_decode(buf);
        *buf += len;
        break;
    }
}

int db_scalar_cmp(db_scalar_type type, const char **s1, const char **s2) {
    switch (type) {
    case TYPE_U8: {
        u8 x1 = *(*s1)++, x2 = *(*s2)++;
        return (int)x1 - x2;
    }
    case TYPE_VARINT: {
        i32 x1 = db_varint_decode(s1), x2 = db_varint_decode(s2);
        return x1 - x2;
    }
    case TYPE_BYTESTR: {
        i32 len1 = db_varint_decode(s1), len2 = db_varint_decode(s2);
        int ret = memncmp(*s1, *s2, len1, len2);
        *s1 += len1;
        *s2 += len2;
        return ret;
    }
    default:
        __builtin_unreachable();
    }
}

void db_record_dump(const struct db_record_type *rtype, const char **buf) {
    db_info("{");
    for (int i = 0; i < rtype->len; i++) {
        db_scalar_dump(rtype->col_types[i], buf);
        if (i < rtype->len - 1)
            db_info(", ");
    }
    db_info("}");
}

void db_record_next(const struct db_record_type *rtype, const char **buf) {
    for (int i = 0; i < rtype->len; i++) {
        db_scalar_next(rtype->col_types[i], buf);
    }
}

int db_record_cmp(const struct db_record_type *rtype, const char *r1,
                  const char **r2) {
    int i, ret;
    for (i = 0, ret = 0; i < rtype->key_len && !ret; i++)
        ret = db_scalar_cmp(rtype->col_types[i], &r1, r2);
    for (; i < rtype->len; i++) {
        db_scalar_next(rtype->col_types[i], r2);
    }
    return ret;
}

void db_record_insert(char *buf, int size, int offset,
                      const struct db_record *rec, page_idx right_idx) {
    /* Insert after the left_idx. */
    char *p = buf + offset;
    char *end = buf + size;
    db_varint_next((const char **)&p);

    char idx_buf[8];
    int idx_len = db_varint_encode(idx_buf, right_idx);

    /* Make space for the record. */
    int insert_len = rec->len + idx_len;
    memmove(p + insert_len, p, end - p);

    memcpy(p, rec->payload, rec->len);
    memcpy(p + rec->len, idx_buf, idx_len);
}

void db_record_find_median(const struct db_record_type *rtype, char *buf,
                           int size, int keys, char **median, int *median_idx) {
    const char *p = buf;
    int i;
    db_varint_next(&p);
    for (i = 0; i < keys / 2; i++) {
        db_record_next(rtype, &p);
        db_varint_next(&p);
    }
    *median = (char *)p;
    *median_idx = i;
}

static int db_record_append_check(struct db_record *rec, int extra) {
    if (rec->len + extra > DB_MAX_RECORD_LEN) {
        db_error("exceeded max record length");
        return 0;
    }
    return 1;
}

int db_record_app_val(struct db_dyn_record *dst, const struct db_val *val) {
    struct db_record_type *rtype = &dst->rtype;
    if (rtype->len >= DB_MAX_COLUMNS) {
        db_error("too many columns");
        return 0;
    }
    switch (val->tag) {
    case VAL_NUM:
        rtype->col_types[rtype->len++] = TYPE_VARINT;
        rtype->key_len++;
        return db_record_app_varint(&dst->rec, val->num);
    case VAL_BLOB:
        rtype->col_types[rtype->len++] = TYPE_BYTESTR;
        rtype->key_len++;
        return db_record_app_bs(&dst->rec, val->len, val->blob);
    default:
        db_error("type cannot be appended to record");
        return 0;
    }
}

int db_record_app_u8(struct db_record *rec, u8 data) {
    if (!db_record_append_check(rec, 1))
        return 0;
    rec->payload[rec->len++] = data;
    return 1;
}

int db_record_app_bs(struct db_record *rec, int len, const char *data) {
    char buf[8];
    int ilen = db_varint_encode(buf, len);
    if (!db_record_append_check(rec, ilen + len))
        return 0;
    char *p = rec->payload + rec->len;
    rec->len += ilen + len;
    memcpy(p, buf, ilen);
    memcpy(p + ilen, data, len);
    return 1;
}

int db_record_app_varint(struct db_record *rec, i32 data) {
    char buf[8];
    int len = db_varint_encode(buf, data);
    if (!db_record_append_check(rec, len))
        return 0;
    ;
    char *p = rec->payload + rec->len;
    rec->len += len;
    memcpy(p, buf, len);
    return 1;
}

struct db_val db_record_proj(const struct db_record_type *rtype,
                             const struct db_record *rec, int i) {
    const char *p = rec->payload;
    for (int j = 0; j < i; j++) {
        db_scalar_next(rtype->col_types[j], &p);
    }

    struct db_val val;
    char type = rtype->col_types[i];
    switch (type) {
    case TYPE_U8:
        val.tag = VAL_NUM;
        val.num = *p;
        break;
    case TYPE_VARINT:
        val.tag = VAL_NUM;
        val.num = db_varint_decode(&p);
        break;
    case TYPE_BYTESTR:
        val.tag = VAL_BLOB;
        val.len = db_varint_decode(&p);
        val.blob = malloc(val.len);
        val.own = 1;
        memcpy(val.blob, p, val.len);
        break;
    }

    return val;
}

char *db_bs_decode(const char **buf) {
    i32 len = db_varint_decode(buf);
    char *ret = malloc(len + 1);
    memcpy(ret, *buf, len);
    ret[len] = 0;
    *buf += len;
    return ret;
}

static const struct db_record_type db_table_rtype = {
    .len = 6,
    .key_len = 2,
    .col_types = {TYPE_BYTESTR, TYPE_BYTESTR, TYPE_VARINT, TYPE_U8,
                  TYPE_BYTESTR, TYPE_BYTESTR},
};

/*
 * VARINT format (signed, little endian):
 *                                                bits
 *   -------1                                     7    =  7
 *   ------10 --------                            6+ 8 = 14
 *   -----100 -------- --------                   5+16 = 21
 *   ----1000 -------- -------- --------          4+24 = 28
 *   ---10000 -------- -------- -------- xxx----- 3+29 = 32
 *
 *   "x" bits are unused.
 */

void db_varint_next(const char **buf) {
    if (!**buf) {
        (*buf)++;
    } else {
        int shift = __builtin_ctz(**buf) + 1;
        (*buf) += shift;
    }
}

int db_varint_size(i32 n) {
    if (!n)
        return 1;
    return (38 - __builtin_clrsb(n)) / 7;
}

i32 db_varint_decode(const char **buf) {
    u64 un;
    memcpy(&un, *buf, sizeof un); /* TODO fix this or not? */
    if (!(un & 0xff)) {
        (*buf)++;
        return 0;
    }
    int shift = __builtin_ctz(un & 0xff) + 1;
    int sbits = 64 - 7 * shift;
    u64 ret = (un >> shift) << sbits;
    *buf += shift;
    return ((i64)ret >> sbits);
}

int db_varint_encode(char *buf, i32 n) {
    if (!n) {
        *buf = 0;
        return 1;
    }
    int bits = 32 - __builtin_clrsb(n);
    int shift = (bits + 6) / 7;
    u64 un = ((u64)n << shift) | (1 << (shift - 1));
    memcpy(buf, &un, sizeof un);
    return shift;
}

int memncmp(const char *s1, const char *s2, size_t len1, size_t len2) {
    size_t len = len1 > len2 ? len2 : len1;
    int ret = memcmp(s1, s2, len);
    return ret ? ret : (ssize_t)len1 - (ssize_t)len2;
}

list_t list_cons(void *head, list_t tail) {
    list_t ret = malloc(sizeof *ret);
    ret->head = head;
    ret->tail = tail;
    return ret;
}

void list_free(free_fn f, list_t list) {
    while (list) {
        list_t tail = list->tail;
        if (f)
            f(list->head);
        free(list);
        list = tail;
    }
}

list_t list_reverse(list_t list) {
    list_t prev = LIST_NIL;
    while (list) {
        list_t tmp = list->tail;
        list->tail = prev;
        prev = list;
        list = tmp;
    }
    return prev;
}

int list_find(cmp_fn cmp, list_t list, const void *elem, list_t *out) {
    for (int i = 0; list; i++, list = list->tail) {
        if (!cmp(list->head, elem)) {
            if (out)
                *out = list;
            return i;
        }
    }
    return -1;
}

list_t list_nth(list_t list, int i) {
    for (; i > 0 && list; i--)
        list = list->tail;
    return list;
}

int list_length(list_t list) {
    int i = 0;
    for (i = 0; list; i++, list = list->tail)
        ;
    return i;
}

void list_delete(list_t *list, const void *elem) {
    for (list_t *l = list; *l; l = &(*l)->tail) {
        if ((*l)->head == elem) {
            list_t tmp = *l;
            *l = (*l)->tail;
            free(tmp);
            break;
        }
    }
}
