#include "scrym.h"

#ifdef HAVE_FL_RESERVED2
#define SCRYM_MARK_PART_1 FL_RESERVED1
#define SCRYM_MARK_PART_2 FL_RESERVED2
#else
#define SCRYM_MARK_PART_1 FL_RESERVED
#define SCRYM_MARK_PART_2 FL_FINALIZE
#endif
#define SCRYM_MARK (SCRYM_MARK_PART_1 | SCRYM_MARK_PART_2)
#define SCRYM_DESCRIPTOR_CHUNK_SIZE 4000
#define SCRYM_CACHE_MIN 4

#define other_mark_part(mask) (mask ^ SCRYM_MARK)

typedef struct scrym_descriptor_chunk {
    struct scrym_descriptor_chunk *next;
    size_t index;
    int mark_mask;
    VALUE descriptors[SCRYM_DESCRIPTOR_CHUNK_SIZE];
} scrym_descriptor_chunk_t;

typedef struct scrym_descriptor {
    scrym_descriptor_chunk_t *marked;
    scrym_descriptor_chunk_t *expired;
    scrym_descriptor_chunk_t *cache;
    size_t cache_size;
    size_t unused_cache_size;
    int mark_mask;
} scrym_descriptor_t;

static scrym_descriptor_t * global_desc = NULL;
static VALUE rb_mScrym = Qnil;
static VALUE rb_cScrymMutator = Qnil;
static VALUE rb_cScrymDescriptor = Qnil;

/*---- descriptor ----*/

static scrym_descriptor_chunk_t *
scrym_alloc_descriptor_chunk(void)
{
    scrym_descriptor_chunk_t *res;

    res = xmalloc(sizeof(scrym_descriptor_chunk_t));
    return res;
}

static void
scrym_cache_descriptor_chunk(scrym_descriptor_t *desc, scrym_descriptor_chunk_t *chunk)
{
    chunk->next = desc->cache;
    desc->cache = chunk;
    desc->cache_size++;
}

static void
scrym_shrink_descriptor_chunk_cache(scrym_descriptor_t *desc)
{
    scrym_descriptor_chunk_t *chunk;

    if (desc->unused_cache_size > (desc->cache_size/2) &&
        desc->unused_cache_size > SCRYM_CACHE_MIN) {
        chunk = desc->cache;
        desc->cache = desc->cache->next;
        desc->cache_size--;
        free(chunk);
    }
    desc->unused_cache_size = desc->cache_size;
}

static void
scrym_new_descriptor_chunk(scrym_descriptor_t *desc)
{
    scrym_descriptor_chunk_t *next;

    assert(desc->marked == NULL ||
           desc->marked->index == SCRYM_DESCRIPTOR_CHUNK_SIZE);
    if (desc->cache_size > 0) {
        next = desc->cache;
        desc->cache = desc->cache->next;
        desc->cache_size--;
        if (desc->unused_cache_size > desc->cache_size)
            desc->unused_cache_size = desc->cache_size;
    }
    else {
        next = scrym_alloc_descriptor_chunk();
    }
    next->next = desc->marked;
    next->mark_mask = desc->mark_mask;
    next->index = 0;
    desc->marked = next;
}

static void
scrym_add_descriptor(scrym_descriptor_t *desc, VALUE data)
{
    if (desc->marked == NULL ||
        desc->marked->index == SCRYM_DESCRIPTOR_CHUNK_SIZE) {
        scrym_new_descriptor_chunk(desc);
    }
    desc->marked->descriptors[desc->marked->index++] = data;
}

static void
scrym_descriptor_chunk_foreach(scrym_descriptor_chunk_t *chunk,
                             void (*func)(scrym_descriptor_chunk_t *, VALUE))
{
    size_t i;

    for(i=0; i < chunk->index; i++) {
        func(chunk, chunk->descriptors[i]);
    }
}

static void
scrym_init(scrym_descriptor_t *desc)
{
    int i;

    desc->mark_mask = SCRYM_MARK_PART_1;

    for(i=0; i < SCRYM_CACHE_MIN; i++) {
        scrym_cache_descriptor_chunk(desc, scrym_alloc_descriptor_chunk());
    }
    desc->unused_cache_size = desc->cache_size;
}


/* ---- mark for avoiding garbage collect ---- */

static void
scrym_descriptor_obj_mark(scrym_descriptor_chunk_t *chunk, VALUE obj)
{
    rb_gc_mark(obj);
}

static void
scrym_descriptor_mark(scrym_descriptor_t *desc)
{
    scrym_descriptor_chunk_t *tmp;

    tmp = desc->marked;
    while (tmp != NULL) {
        scrym_descriptor_chunk_foreach(tmp, scrym_descriptor_obj_mark);
        tmp = tmp->next;
    }

    tmp = desc->expired;
    while (tmp != NULL) {
        scrym_descriptor_chunk_foreach(tmp, scrym_descriptor_obj_mark);
        tmp = tmp->next;
    }
}

static void
scrym_descriptor_free(scrym_descriptor_t *desc)
{
    scrym_descriptor_chunk_t *free, *tmp;

    tmp = desc->marked;
    while (tmp != NULL) {
        free = tmp;
        tmp = tmp->next;
        xfree(free);
    }

    tmp = desc->expired;
    while (tmp != NULL) {
        free = tmp;
        tmp = tmp->next;
        xfree(free);
    }

    tmp = desc->cache;
    while (tmp != NULL) {
        free = tmp;
        tmp = tmp->next;
        xfree(free);
    }

    xfree(desc);
}


/* ---- mark ---- */

static VALUE
scrym_mark(VALUE self, VALUE obj)
{
    if (IMMEDIATE_P(obj)) {
        rb_raise(rb_eArgError, "can not accept an immediate value");
        return;
    }
    if (BUILTIN_TYPE(obj) == T_NONE) {
        rb_raise(rb_eArgError, "can not accept an node");
        return;
    }
    if (RBASIC(obj)->flags & global_desc->mark_mask) {
        return;
    }
    scrym_add_descriptor(global_desc, obj);
    RBASIC(obj)->flags |= global_desc->mark_mask;
    return Qnil;
}

/* ---- collect ---- */


/* The original code is gc.c on trunk */
static void
scrym_obj_free(VALUE obj)
{
    switch (BUILTIN_TYPE(obj)) {
      case T_NIL:
      case T_FIXNUM:
      case T_TRUE:
      case T_FALSE:
	rb_bug("scrym_obj_free() called for broken object");
	break;
    }

    if (FL_TEST(obj, FL_EXIVAR)) {
	rb_free_generic_ivar((VALUE)obj);
	FL_UNSET(obj, FL_EXIVAR);
    }

    switch (BUILTIN_TYPE(obj)) {
      case T_OBJECT:
	if (!(RBASIC(obj)->flags & ROBJECT_EMBED) &&
            ROBJECT(obj)->as.heap.ivptr) {
	    xfree(ROBJECT(obj)->as.heap.ivptr);
	}
	break;
      case T_MODULE:
      case T_CLASS:
	rb_clear_cache_by_class((VALUE)obj);
        if (RCLASS(obj)->m_tbl) {
            rb_free_m_table(RCLASS(obj)->m_tbl);
        }
	if (RCLASS_IV_TBL(obj)) {
	    st_free_table(RCLASS_IV_TBL(obj));
	}
	if (RCLASS_CONST_TBL(obj)) {
	    rb_free_const_table(RCLASS_CONST_TBL(obj));
	}
	if (RCLASS_IV_INDEX_TBL(obj)) {
	    st_free_table(RCLASS_IV_INDEX_TBL(obj));
	}
        xfree(RCLASS(obj)->ptr);
	break;
      case T_STRING:
	rb_str_free(obj);
	break;
      case T_ARRAY:
	rb_ary_free(obj);
	break;
      case T_HASH:
	if (RHASH(obj)->ntbl) {
	    st_free_table(RHASH(obj)->ntbl);
	}
	break;
      case T_REGEXP:
	if (RREGEXP(obj)->ptr) {
	    onig_free(RREGEXP(obj)->ptr);
	}
	break;
      case T_DATA:
	if (DATA_PTR(obj)) {
	    if (RTYPEDDATA_P(obj)) {
		RDATA(obj)->dfree = RTYPEDDATA(obj)->type->function.dfree;
	    }
	    if (RDATA(obj)->dfree == (RUBY_DATA_FUNC)-1) {
		xfree(DATA_PTR(obj));
	    }
	}
	break;
      case T_MATCH:
        if ((R_CAST(RMatch)(obj))->rmatch) {
            struct rmatch *rm = (R_CAST(RMatch)(obj))->rmatch;
	    onig_region_free(&rm->regs, 0);
            if (rm->char_offset)
		xfree(rm->char_offset);
	    xfree(rm);
	}
	break;
      case T_FILE:
	if (RFILE(obj)->fptr) {
	    rb_io_fptr_finalize(RFILE(obj)->fptr);
	}
	break;
      case T_RATIONAL:
      case T_COMPLEX:
	break;
      case T_ICLASS:
	/* iClass shares table with the module */
	xfree(RCLASS(obj)->ptr);
	break;

      case T_FLOAT:
	break;

      case T_BIGNUM:
	if (!(RBASIC(obj)->flags & RBIGNUM_EMBED_FLAG) && RBIGNUM_DIGITS(obj)) {
	    xfree(RBIGNUM_DIGITS(obj));
	}
	break;

      case T_STRUCT:
	if ((RBASIC(obj)->flags & RSTRUCT_EMBED_LEN_MASK) == 0 &&
	    RSTRUCT(obj)->as.heap.ptr) {
	    xfree(RSTRUCT(obj)->as.heap.ptr);
	}
	break;

      default:
	rb_bug("scrym_collect(): unknown data type 0x%x(%p) 0x%"PRIxVALUE,
	       BUILTIN_TYPE(obj), (void*)obj, RBASIC(obj)->flags);
    }

    return 0;
}


static void
scrym_expire(scrym_descriptor_chunk_t *chunk, VALUE obj)
{
    RBASIC(obj)->flags &= ~(chunk->mark_mask);
    if (!(RBASIC(obj)->flags & other_mark_part(chunk->mark_mask))) {
        scrym_obj_free(obj);
        rb_gc_force_recycle(obj);
    }
}

static int
scrym_expire_chunk(void)
{
    scrym_descriptor_chunk_t *next;

    if (global_desc->expired == NULL) {
        return NULL;
    }
    scrym_descriptor_chunk_foreach(global_desc->expired, scrym_expire);
    next = global_desc->expired->next;
    scrym_cache_descriptor_chunk(global_desc, global_desc->expired);
    global_desc->expired = next;
    return Qtrue;
}

static VALUE
scrym_collect(VALUE self)
{
    while(scrym_expire_chunk());
    global_desc->expired = global_desc->marked;
    global_desc->marked = NULL;
    global_desc->mark_mask = other_mark_part(global_desc->mark_mask);
    scrym_shrink_descriptor_chunk_cache(global_desc);
    return Qnil;
}

#ifdef SCRYM_TEST
static VALUE
scrym_test(VALUE self) {
    size_t i;

    {
        puts("scrym_init");
        assert(global_desc->expired == NULL);
        assert(global_desc->marked == NULL);
        assert(global_desc->cache != NULL);
    }

    {
        size_t j;
        puts("scrym_add_descriptor");
        for(i = 0; i < SCRYM_CACHE_MIN; i++) {
            for(j = 0; j < SCRYM_DESCRIPTOR_CHUNK_SIZE; j++) {
                scrym_add_descriptor(global_desc, j);
            }
        }
        for(j = 0; j < SCRYM_DESCRIPTOR_CHUNK_SIZE; j++) {
            assert(global_desc->marked->descriptors[j] == (VALUE)j);
        }
        assert(global_desc->marked->next->next->next != NULL);
        assert(global_desc->marked->next->next->next->descriptors[SCRYM_DESCRIPTOR_CHUNK_SIZE] != SCRYM_DESCRIPTOR_CHUNK_SIZE-1);
        assert(global_desc->marked->next->next->next->next == NULL);
        assert(global_desc->expired == NULL);
        assert(global_desc->cache == NULL);
        assert(global_desc->cache_size == 0);
        assert(global_desc->unused_cache_size == 0);
        global_desc->cache = global_desc->marked;
        global_desc->marked = NULL;
        global_desc->cache_size = SCRYM_CACHE_MIN;
    }

    {
        VALUE str = rb_str_new_cstr("test");
        VALUE str2 = rb_str_new_cstr("test2");
        puts("scrym_collect");
        assert(scrym_expire_chunk() == Qfalse);
        assert((RBASIC(str)->flags & SCRYM_MARK) == 0);

        puts("  scrym_mark1");
        scrym_mark(Qnil, str);
        scrym_mark(Qnil, str);
        assert(global_desc->marked->index == 1);
        assert(RBASIC(str)->flags & global_desc->marked->mark_mask);
        assert(!(RBASIC(str)->flags & other_mark_part(global_desc->marked->mark_mask)));

        puts("  scrym_mark2");
        scrym_mark(Qnil, str2);
        assert(global_desc->marked->index == 2);
        assert(RBASIC(str2)->flags & global_desc->marked->mark_mask);

        puts("  collect1");
        scrym_collect(Qnil);
        scrym_mark(Qnil, str2);

        puts("  collect2");
        scrym_collect(Qnil);
        assert(RBASIC(str)->flags == 0);
        assert(RBASIC(str2)->flags != 0);
        assert(global_desc->marked == NULL);
        assert(global_desc->expired != NULL);

        puts("  collect3");
        scrym_collect(Qnil);
        assert(RBASIC(str2)->flags == 0);
        assert(global_desc->expired == NULL);
        assert(global_desc->cache_size == SCRYM_CACHE_MIN);
    }
}
#endif

void
Init_scrym_ext(void)
{
    VALUE desc;

    rb_mScrym = rb_define_module("Scrym");
    rb_cScrymMutator = rb_define_class_under(rb_mScrym, "Mutator", rb_cObject);
    rb_define_singleton_method(rb_cScrymMutator, "mark", scrym_mark, 1);
    rb_define_singleton_method(rb_cScrymMutator, "collect", scrym_collect, 0);
    rb_cScrymDescriptor = rb_define_class_under(rb_mScrym, "Descriptor", rb_cObject);

    global_desc = (scrym_descriptor_t *)xmalloc(sizeof(scrym_descriptor_t));
    MEMZERO(global_desc, scrym_descriptor_t, 1);
    scrym_init(global_desc);
    desc = Data_Wrap_Struct(rb_cScrymDescriptor, scrym_descriptor_mark, scrym_descriptor_free, global_desc);
    rb_iv_set(rb_cScrymMutator, "@__descriptor", desc);

#ifdef SCRYM_TEST
    rb_define_singleton_method(rb_cScrymMutator, "test", scrym_test, 0);
#endif
}
