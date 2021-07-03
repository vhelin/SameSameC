
#ifndef _INLINE_ASM_H
#define _INLINE_ASM_H

struct inline_asm *inline_asm_add(void);
struct inline_asm *inline_asm_find(int id);
void inline_asm_free(struct inline_asm *ia);
int inline_asm_add_asm_line(struct inline_asm *ia, char *line, int length, int line_number);

#endif
