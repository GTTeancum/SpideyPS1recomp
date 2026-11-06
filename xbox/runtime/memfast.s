# memcpy and memset for the console, in place of nxdk's (pdclib's), which move one byte
# per loop step. Whole words with the string instructions, then the remaining bytes.
# Assembly, so link-time optimisation cannot turn them into calls to themselves.

    # No exception handlers here: tell the linker so (/safeseh), as compiled objects do.
    .def @feat.00
    .scl 3
    .type 0
    .endef
    .globl @feat.00
    .set @feat.00, 1

    .text

    .globl _memcpy
    .p2align 4
_memcpy:                        # void *memcpy(void *dst, const void *src, size_t n)
    pushl %edi
    pushl %esi
    movl 12(%esp), %edi
    movl 16(%esp), %esi
    movl 20(%esp), %ecx
    movl %edi, %eax
    movl %ecx, %edx
    shrl $2, %ecx
    rep movsl
    movl %edx, %ecx
    andl $3, %ecx
    rep movsb
    popl %esi
    popl %edi
    ret

    .globl _memset
    .p2align 4
_memset:                        # void *memset(void *dst, int c, size_t n)
    pushl %edi
    movl 8(%esp), %edi
    movzbl 12(%esp), %eax
    imull $0x01010101, %eax, %eax
    movl 16(%esp), %ecx
    movl %ecx, %edx
    shrl $2, %ecx
    rep stosl
    movl %edx, %ecx
    andl $3, %ecx
    rep stosb
    movl 8(%esp), %eax
    popl %edi
    ret
