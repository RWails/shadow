#include "sneaky_time.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <elf.h>
#include <sys/auxv.h>

using clock_gettime_t = int (*)(clockid_t clock_id, struct timespec *tp);
static void *_vdso_sym(const char *name);

static clock_gettime_t _CLOCK_GETTIME_FN = nullptr;

__attribute__((constructor))
static void _init_clock_gettime_fn() {
  _CLOCK_GETTIME_FN = reinterpret_cast<clock_gettime_t>(_vdso_sym("clock_gettime"));
  assert(_CLOCK_GETTIME_FN);
}

static void* _vdso_sym(const char* name) {
  // in order to semantically inline getauxval one would have to bypass glibc,
  // as the auxval is passed
  //  by the kernel to the bin's entrypoint used by the kernel
  auto vdso_addr = reinterpret_cast<std::uint8_t*>(getauxval(AT_SYSINFO_EHDR));

  auto elf_header = (Elf64_Ehdr*)vdso_addr;
  auto section_header = (Elf64_Shdr*)(vdso_addr + elf_header->e_shoff);

  char* dynstr = 0;

  for (int i = 0; i < elf_header->e_shnum; i++) {
    auto& s = section_header[i];
    auto& ss_ = section_header[elf_header->e_shstrndx];
    auto name = (char*)(vdso_addr + ss_.sh_offset + s.sh_name);
    if (std::strcmp(name, ".dynstr") == 0) {
      dynstr = (char*)(vdso_addr + s.sh_offset);
      break;
    }
  }

  void* ret = NULL;

  for (int i = 0; i < elf_header->e_shnum; i++) {
    auto& s = section_header[i];
    auto& ss_ = section_header[elf_header->e_shstrndx];
    auto name = (char*)(vdso_addr + ss_.sh_offset + s.sh_name);
    if (std::strcmp(name, ".dynsym") == 0) {
      for (int si = 0; si < (s.sh_size / s.sh_entsize); si++) {
        auto& sym = ((Elf64_Sym*)(vdso_addr + s.sh_offset))[si];
        auto name = dynstr + sym.st_name;
        if (std::strcmp(name, "clock_gettime") == 0) {
          ret = (vdso_addr + sym.st_value);
          break;
        }
      }
      if (ret) break;
    }
  }
  return ret;
}

extern "C" {

int sneaky_gettime(clockid_t clock_id, struct timespec *tp) {
  assert(tp && _CLOCK_GETTIME_FN);
  return _CLOCK_GETTIME_FN(clock_id, tp);
}

}
