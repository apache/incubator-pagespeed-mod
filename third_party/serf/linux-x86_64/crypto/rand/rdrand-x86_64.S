#if defined(__x86_64__)
.text	

.globl	CRYPTO_rdrand
.hidden CRYPTO_rdrand
.type	CRYPTO_rdrand,@function
.align	16
CRYPTO_rdrand:
.byte	0x48, 0x0f, 0xc7, 0xf0
	.byte	0xf3,0xc3
#endif
