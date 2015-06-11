#if defined(__x86_64__)
.text	

.globl	_CRYPTO_rdrand
.private_extern _CRYPTO_rdrand

.p2align	4
_CRYPTO_rdrand:
.byte	0x48, 0x0f, 0xc7, 0xf0
	.byte	0xf3,0xc3
#endif
