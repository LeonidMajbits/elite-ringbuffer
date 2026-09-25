
build/native/bench_latency:     file format elf64-x86-64


Disassembly of section .init:

Disassembly of section .plt:

Disassembly of section .plt.got:

Disassembly of section .text:

0000000000006c70 <bench_send>:
    6c70:	55                   	push   %rbp
    6c71:	48 89 e5             	mov    %rsp,%rbp
    6c74:	41 57                	push   %r15
    6c76:	49 89 f7             	mov    %rsi,%r15
    6c79:	41 56                	push   %r14
    6c7b:	4c 8d b5 70 ff ff ff 	lea    -0x90(%rbp),%r14
    6c82:	41 55                	push   %r13
    6c84:	4c 8d 6d 80          	lea    -0x80(%rbp),%r13
    6c88:	41 54                	push   %r12
    6c8a:	49 89 fc             	mov    %rdi,%r12
    6c8d:	53                   	push   %rbx
    6c8e:	4c 89 cb             	mov    %r9,%rbx
    6c91:	48 81 ec 88 00 00 00 	sub    $0x88,%rsp
    6c98:	48 89 95 68 ff ff ff 	mov    %rdx,-0x98(%rbp)
    6c9f:	48 89 8d 60 ff ff ff 	mov    %rcx,-0xa0(%rbp)
    6ca6:	4c 89 85 58 ff ff ff 	mov    %r8,-0xa8(%rbp)
    6cad:	eb 07                	jmp    6cb6 <bench_send+0x46>
    6caf:	90                   	nop
    6cb0:	48 83 03 01          	addq   $0x1,(%rbx)
    6cb4:	f3 90                	pause
    6cb6:	4c 89 f2             	mov    %r14,%rdx
    6cb9:	4c 89 ee             	mov    %r13,%rsi
    6cbc:	4c 89 e7             	mov    %r12,%rdi
    6cbf:	e8 fc 0d 00 00       	call   7ac0 <elite_write_reserve>
    6cc4:	83 f8 02             	cmp    $0x2,%eax
    6cc7:	74 e7                	je     6cb0 <bench_send+0x40>
    6cc9:	85 c0                	test   %eax,%eax
    6ccb:	0f 85 9f 00 00 00    	jne    6d70 <bench_send+0x100>
    6cd1:	83 bd 78 ff ff ff 3f 	cmpl   $0x3f,-0x88(%rbp)
    6cd8:	0f 86 92 00 00 00    	jbe    6d70 <bench_send+0x100>
    6cde:	48 8b 85 70 ff ff ff 	mov    -0x90(%rbp),%rax
    6ce5:	4c 89 fa             	mov    %r15,%rdx
    6ce8:	4c 89 f9             	mov    %r15,%rcx
    6ceb:	4d 89 f8             	mov    %r15,%r8
    6cee:	48 8b 9d 58 ff ff ff 	mov    -0xa8(%rbp),%rbx
    6cf5:	48 f7 d1             	not    %rcx
    6cf8:	4c 89 ee             	mov    %r13,%rsi
    6cfb:	4c 89 e7             	mov    %r12,%rdi
    6cfe:	48 c1 c2 11          	rol    $0x11,%rdx
    6d02:	48 89 48 08          	mov    %rcx,0x8(%rax)
    6d06:	48 8b 8d 68 ff ff ff 	mov    -0x98(%rbp),%rcx
    6d0d:	48 31 da             	xor    %rbx,%rdx
    6d10:	4c 89 38             	mov    %r15,(%rax)
    6d13:	48 89 50 28          	mov    %rdx,0x28(%rax)
    6d17:	48 f7 d2             	not    %rdx
    6d1a:	48 89 48 10          	mov    %rcx,0x10(%rax)
    6d1e:	48 8b 8d 60 ff ff ff 	mov    -0xa0(%rbp),%rcx
    6d25:	48 89 50 30          	mov    %rdx,0x30(%rax)
    6d29:	48 ba 15 7c 4a 7f b9 	movabs $0x9e3779b97f4a7c15,%rdx
    6d30:	79 37 9e 
    6d33:	4c 31 fa             	xor    %r15,%rdx
    6d36:	48 89 48 18          	mov    %rcx,0x18(%rax)
    6d3a:	b9 07 00 00 00       	mov    $0x7,%ecx
    6d3f:	48 89 50 38          	mov    %rdx,0x38(%rax)
    6d43:	ba 40 00 00 00       	mov    $0x40,%edx
    6d48:	48 89 58 20          	mov    %rbx,0x20(%rax)
    6d4c:	e8 af 10 00 00       	call   7e00 <elite_write_commit>
    6d51:	85 c0                	test   %eax,%eax
    6d53:	75 33                	jne    6d88 <bench_send+0x118>
    6d55:	48 c1 e8 20          	shr    $0x20,%rax
    6d59:	83 f8 02             	cmp    $0x2,%eax
    6d5c:	75 2a                	jne    6d88 <bench_send+0x118>
    6d5e:	48 81 c4 88 00 00 00 	add    $0x88,%rsp
    6d65:	5b                   	pop    %rbx
    6d66:	41 5c                	pop    %r12
    6d68:	41 5d                	pop    %r13
    6d6a:	41 5e                	pop    %r14
    6d6c:	41 5f                	pop    %r15
    6d6e:	5d                   	pop    %rbp
    6d6f:	c3                   	ret
    6d70:	48 8d 15 59 8d 00 00 	lea    0x8d59(%rip),%rdx        # fad0 <_IO_stdin_used+0x1ad0>
    6d77:	be 88 01 00 00       	mov    $0x188,%esi
    6d7c:	48 8d 3d 28 74 00 00 	lea    0x7428(%rip),%rdi        # e1ab <_IO_stdin_used+0x1ab>
    6d83:	e8 e8 cb ff ff       	call   3970 <bench_fail>
    6d88:	48 8d 15 69 8d 00 00 	lea    0x8d69(%rip),%rdx        # faf8 <_IO_stdin_used+0x1af8>
    6d8f:	be 89 01 00 00       	mov    $0x189,%esi
    6d94:	48 8d 3d 10 74 00 00 	lea    0x7410(%rip),%rdi        # e1ab <_IO_stdin_used+0x1ab>
    6d9b:	e8 d0 cb ff ff       	call   3970 <bench_fail>

Disassembly of section .fini:
