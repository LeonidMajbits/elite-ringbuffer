
build/native/bench_latency:     file format elf64-x86-64


Disassembly of section .init:

Disassembly of section .plt:

Disassembly of section .plt.got:

Disassembly of section .text:

0000000000004370 <bench_ordered_tick>:
    4370:	55                   	push   %rbp
    4371:	48 89 e5             	mov    %rsp,%rbp
    4374:	48 83 ec 10          	sub    $0x10,%rsp
    4378:	0f ae e8             	lfence
    437b:	bf 04 00 00 00       	mov    $0x4,%edi
    4380:	48 8d 75 f0          	lea    -0x10(%rbp),%rsi
    4384:	e8 67 dd ff ff       	call   20f0 <clock_gettime@plt>
    4389:	85 c0                	test   %eax,%eax
    438b:	75 28                	jne    43b5 <bench_ordered_tick+0x45>
    438d:	48 8b 45 f0          	mov    -0x10(%rbp),%rax
    4391:	48 85 c0             	test   %rax,%rax
    4394:	78 1f                	js     43b5 <bench_ordered_tick+0x45>
    4396:	48 ba 09 fa 82 4b 04 	movabs $0x44b82fa09,%rdx
    439d:	00 00 00 
    43a0:	48 39 c2             	cmp    %rax,%rdx
    43a3:	72 28                	jb     43cd <bench_ordered_tick+0x5d>
    43a5:	48 69 c0 00 ca 9a 3b 	imul   $0x3b9aca00,%rax,%rax
    43ac:	48 03 45 f8          	add    -0x8(%rbp),%rax
    43b0:	0f ae e8             	lfence
    43b3:	c9                   	leave
    43b4:	c3                   	ret
    43b5:	48 8d 15 84 aa 00 00 	lea    0xaa84(%rip),%rdx        # ee40 <_IO_stdin_used+0xe40>
    43bc:	be ad 00 00 00       	mov    $0xad,%esi
    43c1:	48 8d 3d e3 9d 00 00 	lea    0x9de3(%rip),%rdi        # e1ab <_IO_stdin_used+0x1ab>
    43c8:	e8 a3 f5 ff ff       	call   3970 <bench_fail>
    43cd:	e8 9e f7 ff ff       	call   3b70 <bench_tick.part.0>

Disassembly of section .fini:
