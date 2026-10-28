	@ Distinguish address operands from fixed-width scalar words.
	.ifndef NATIVE_POINTER_SIZE
	.set NATIVE_POINTER_SIZE, 4
	.endif
	.macro data_ptr values:vararg
	.if NATIVE_POINTER_SIZE == 8
	.quad \values
	.else
	.4byte \values
	.endif
	.endm

	.macro native64_pad bytes:req
	.if NATIVE_POINTER_SIZE == 8
	.space \bytes
	.endif
	.endm

	.macro inc x:req
	.set \x, \x + 1
	.endm

	.macro enum_start x=0
	.set __enum__, \x
	.endm

	.macro enum constant:req
	.equiv \constant, __enum__
	inc __enum__
	.endm
