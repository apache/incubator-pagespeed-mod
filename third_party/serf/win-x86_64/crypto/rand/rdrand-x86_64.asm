default	rel
%define XMMWORD
%define YMMWORD
%define ZMMWORD
section	.text code align=64


global	CRYPTO_rdrand

ALIGN	16
CRYPTO_rdrand:
	mov	QWORD[8+rsp],rdi	;WIN64 prologue
	mov	QWORD[16+rsp],rsi
	mov	rax,rsp
$L$SEH_begin_CRYPTO_rdrand:
	mov	rdi,rcx


DB	0x48,0x0f,0xc7,0xf0
	mov	rdi,QWORD[8+rsp]	;WIN64 epilogue
	mov	rsi,QWORD[16+rsp]
	DB	0F3h,0C3h		;repret
