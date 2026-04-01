/***************************************************************************************
* make by mazhenxin
***************************************************************************************/

#include <ftrace.h>
#include <debug.h>

#include <elf.h>
#include <stdio.h>

#define MAX_FTRACE_INDENT 120

typedef struct {
  uint64_t start;
  uint64_t end;
  char *name;
} FuncSym;

static FuncSym *funcs = NULL;
static size_t nr_funcs = 0;
static int call_depth = 0;
static bool ftrace_ready = false;

static void append_func_sym(uint64_t start, uint64_t size, const char *name) {
  if (size == 0 || name == NULL || name[0] == '\0') return;

  FuncSym *new_buf = realloc(funcs, (nr_funcs + 1) * sizeof(FuncSym));
  Assert(new_buf != NULL, "ftrace: out of memory while expanding symbol table");
  funcs = new_buf;

  size_t nlen = strlen(name);
  funcs[nr_funcs].name = malloc(nlen + 1);
  Assert(funcs[nr_funcs].name != NULL, "ftrace: out of memory while copying symbol name");
  memcpy(funcs[nr_funcs].name, name, nlen + 1);

  funcs[nr_funcs].start = start;
  funcs[nr_funcs].end = start + size;
  nr_funcs++;
}

__attribute__((unused)) static const char *lookup_func_name(uint64_t addr) {
  for (size_t i = 0; i < nr_funcs; i++) {
    if (addr >= funcs[i].start && addr < funcs[i].end) {
      return funcs[i].name;
    }
  }
  return "???";
}

static void build_indent(char *buf, size_t size, int depth) {
  int n = depth * 2;
  if (n < 0) n = 0;
  if (n > MAX_FTRACE_INDENT) n = MAX_FTRACE_INDENT;
  if ((size_t)n >= size) n = (int)size - 1;
  memset(buf, ' ', n);
  buf[n] = '\0';
}

static void load_symtab32(FILE *fp, const Elf32_Ehdr *ehdr) {
  Assert(ehdr->e_shentsize == sizeof(Elf32_Shdr), "ftrace: unexpected ELF32 section header size");

  Elf32_Shdr *shdr = malloc(ehdr->e_shnum * sizeof(Elf32_Shdr));
  Assert(shdr != NULL, "ftrace: out of memory while reading ELF32 section headers");

  fseek(fp, ehdr->e_shoff, SEEK_SET);
  size_t nread = fread(shdr, sizeof(Elf32_Shdr), ehdr->e_shnum, fp);
  Assert(nread == ehdr->e_shnum, "ftrace: failed to read ELF32 section headers");

  for (int i = 0; i < ehdr->e_shnum; i++) {
    if (shdr[i].sh_type != SHT_SYMTAB || shdr[i].sh_entsize != sizeof(Elf32_Sym)) continue;

    Assert(shdr[i].sh_link < (uint32_t)ehdr->e_shnum, "ftrace: invalid ELF32 strtab link");
    Elf32_Shdr strsec = shdr[shdr[i].sh_link];
    Assert(strsec.sh_type == SHT_STRTAB, "ftrace: ELF32 linked section is not a string table");

    char *strtab = malloc(strsec.sh_size);
    Assert(strtab != NULL, "ftrace: out of memory while reading ELF32 strtab");
    fseek(fp, strsec.sh_offset, SEEK_SET);
    nread = fread(strtab, 1, strsec.sh_size, fp);
    Assert(nread == strsec.sh_size, "ftrace: failed to read ELF32 strtab");

    size_t nsym = shdr[i].sh_size / sizeof(Elf32_Sym);
    Elf32_Sym *symtab = malloc(nsym * sizeof(Elf32_Sym));
    Assert(symtab != NULL, "ftrace: out of memory while reading ELF32 symtab");
    fseek(fp, shdr[i].sh_offset, SEEK_SET);
    nread = fread(symtab, sizeof(Elf32_Sym), nsym, fp);
    Assert(nread == nsym, "ftrace: failed to read ELF32 symtab");

    for (size_t j = 0; j < nsym; j++) {
      if (ELF32_ST_TYPE(symtab[j].st_info) != STT_FUNC) continue;
      if (symtab[j].st_name >= strsec.sh_size) continue;
      append_func_sym(symtab[j].st_value, symtab[j].st_size, strtab + symtab[j].st_name);
    }

    free(symtab);
    free(strtab);
  }

  free(shdr);
}

static void load_symtab64(FILE *fp, const Elf64_Ehdr *ehdr) {
  Assert(ehdr->e_shentsize == sizeof(Elf64_Shdr), "ftrace: unexpected ELF64 section header size");

  Elf64_Shdr *shdr = malloc(ehdr->e_shnum * sizeof(Elf64_Shdr));
  Assert(shdr != NULL, "ftrace: out of memory while reading ELF64 section headers");

  fseek(fp, ehdr->e_shoff, SEEK_SET);
  size_t nread = fread(shdr, sizeof(Elf64_Shdr), ehdr->e_shnum, fp);
  Assert(nread == ehdr->e_shnum, "ftrace: failed to read ELF64 section headers");

  for (int i = 0; i < ehdr->e_shnum; i++) {
    if (shdr[i].sh_type != SHT_SYMTAB || shdr[i].sh_entsize != sizeof(Elf64_Sym)) continue;

    Assert(shdr[i].sh_link < (uint32_t)ehdr->e_shnum, "ftrace: invalid ELF64 strtab link");
    Elf64_Shdr strsec = shdr[shdr[i].sh_link];
    Assert(strsec.sh_type == SHT_STRTAB, "ftrace: ELF64 linked section is not a string table");

    char *strtab = malloc(strsec.sh_size);
    Assert(strtab != NULL, "ftrace: out of memory while reading ELF64 strtab");
    fseek(fp, strsec.sh_offset, SEEK_SET);
    nread = fread(strtab, 1, strsec.sh_size, fp);
    Assert(nread == strsec.sh_size, "ftrace: failed to read ELF64 strtab");

    size_t nsym = shdr[i].sh_size / sizeof(Elf64_Sym);
    Elf64_Sym *symtab = malloc(nsym * sizeof(Elf64_Sym));
    Assert(symtab != NULL, "ftrace: out of memory while reading ELF64 symtab");
    fseek(fp, shdr[i].sh_offset, SEEK_SET);
    nread = fread(symtab, sizeof(Elf64_Sym), nsym, fp);
    Assert(nread == nsym, "ftrace: failed to read ELF64 symtab");

    for (size_t j = 0; j < nsym; j++) {
      if (ELF64_ST_TYPE(symtab[j].st_info) != STT_FUNC) continue;
      if (symtab[j].st_name >= strsec.sh_size) continue;
      append_func_sym(symtab[j].st_value, symtab[j].st_size, strtab + symtab[j].st_name);
    }

    free(symtab);
    free(strtab);
  }

  free(shdr);
}

void init_ftrace(const char *elf_file) {
  if (elf_file == NULL) {
    Log("ftrace: no ELF file is provided, disabled");
    return;
  }

  FILE *fp = fopen(elf_file, "rb");
  Assert(fp != NULL, "ftrace: can not open ELF file '%s'", elf_file);

  unsigned char e_ident[EI_NIDENT] = {};
  size_t nread = fread(e_ident, 1, EI_NIDENT, fp);
  Assert(nread == EI_NIDENT, "ftrace: failed to read ELF ident from '%s'", elf_file);

  Assert(e_ident[EI_MAG0] == ELFMAG0 && e_ident[EI_MAG1] == ELFMAG1 &&
         e_ident[EI_MAG2] == ELFMAG2 && e_ident[EI_MAG3] == ELFMAG3,
         "ftrace: '%s' is not a valid ELF file", elf_file);

  fseek(fp, 0, SEEK_SET);
  if (e_ident[EI_CLASS] == ELFCLASS32) {
    Elf32_Ehdr ehdr32 = {};
    nread = fread(&ehdr32, 1, sizeof(ehdr32), fp);
    Assert(nread == sizeof(ehdr32), "ftrace: failed to read ELF32 header from '%s'", elf_file);
    load_symtab32(fp, &ehdr32);
  } else if (e_ident[EI_CLASS] == ELFCLASS64) {
    Elf64_Ehdr ehdr64 = {};
    nread = fread(&ehdr64, 1, sizeof(ehdr64), fp);
    Assert(nread == sizeof(ehdr64), "ftrace: failed to read ELF64 header from '%s'", elf_file);
    load_symtab64(fp, &ehdr64);
  } else {
    panic("ftrace: unsupported ELF class %u in '%s'", e_ident[EI_CLASS], elf_file);
  }

  fclose(fp);

  Assert(nr_funcs > 0, "ftrace: no function symbol found in ELF '%s'", elf_file);
  ftrace_ready = true;
  Log("ftrace: loaded %zu function symbols from '%s'", nr_funcs, elf_file);
}

void ftrace_trace_call(vaddr_t pc, vaddr_t target) {
  if (!ftrace_ready) return;

  char indent[MAX_FTRACE_INDENT + 1] = {};
  build_indent(indent, sizeof(indent), call_depth);
  log_write("[FUNC]:"FMT_WORD ": %scall [%s@" FMT_WORD "]\n", pc, indent, lookup_func_name(target), target);
  call_depth++;
}

void ftrace_trace_ret(vaddr_t pc) {
  if (!ftrace_ready) return;

  if (call_depth > 0) call_depth--;
  char indent[MAX_FTRACE_INDENT + 1] = {};
  build_indent(indent, sizeof(indent), call_depth);
  log_write("[FUNC]:"FMT_WORD ": %sret  [%s]\n", pc, indent, lookup_func_name(pc));
}
