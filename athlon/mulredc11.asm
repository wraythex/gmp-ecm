# mp_limb_t mulredc11(mp_limb_t * z, const mp_limb_t * x, const mp_limb_t * y,
#                 const mp_limb_t *m, mp_limb_t inv_m);
#
#  Stack:
#    inv_m    ## parameters
#    m
#    y
#    x
#    z							(4*(2k+7))%esp
#    ???   (1 limb???)
#    ebp      ## pushed registers                  (4*(2k+5))%esp
#    edi
#    esi
#    ebx
#    ...      ## counter (1 mp_limb_t)             (4*(2k+1))%esp
#    ...      ## tmp space (2*k+1 mp_limb_t)

include(`config.m4')
	TEXT
	GLOBL GSYM_PREFIX`'mulredc11
	TYPE(GSYM_PREFIX`'mulredc11,`function')

GSYM_PREFIX`'mulredc11:
	pushl	%ebp
	pushl	%edi
	pushl	%esi
	pushl	%ebx
	subl	$96, %esp
	movl	%esp, %edi
### set tmp[0..2k+1[ to 0
	movl	$0, (%edi)
	movl	$0, 4(%edi)
	movl	$0, 8(%edi)
	movl	$0, 12(%edi)
	movl	$0, 16(%edi)
	movl	$0, 20(%edi)
	movl	$0, 24(%edi)
	movl	$0, 28(%edi)
	movl	$0, 32(%edi)
	movl	$0, 36(%edi)
	movl	$0, 40(%edi)
	movl	$0, 44(%edi)
	movl	$0, 48(%edi)
	movl	$0, 52(%edi)
	movl	$0, 56(%edi)
	movl	$0, 60(%edi)
	movl	$0, 64(%edi)
	movl	$0, 68(%edi)
	movl	$0, 72(%edi)
	movl	$0, 76(%edi)
	movl	$0, 80(%edi)
	movl	$0, 84(%edi)
	movl	$0, 88(%edi)
###########################################
	movl	$11, 92(%esp)

	.align 32
Loop:
	## compute u and store in %ebp
	movl	120(%esp), %eax
	movl	124(%esp), %esi
	movl	(%eax), %eax
	mull	(%esi)
	addl	(%edi), %eax
	mull	132(%esp)
	movl    %eax, %ebp
	movl	128(%esp), %esi
### addmul1: src[0] is (%esi)
###          dst[0] is (%edi)
###          mult is %ebp
###          k is 11
###          kills %eax, %ebx, %ecx, %edx
###   dst[0,k[ += mult*src[0,k[  plus carry put in ecx or ebx
	movl	(%esi), %eax
	mull	%ebp
	movl	%eax, %ebx
	movl	%edx, %ecx
	movl	4(%esi), %eax

	mull	%ebp
	addl	%ebx, (%edi)
	movl	$0, %ebx
	adcl	%eax, %ecx
	movl	8(%esi), %eax
	adcl	%edx, %ebx

	mull	%ebp
	addl	%ecx, 4(%edi)
	movl	$0, %ecx
	adcl	%eax, %ebx
	movl	12(%esi), %eax
	adcl	%edx, %ecx

	mull	%ebp
	addl	%ebx, 8(%edi)
	movl	$0, %ebx
	adcl	%eax, %ecx
	movl	16(%esi), %eax
	adcl	%edx, %ebx

	mull	%ebp
	addl	%ecx, 12(%edi)
	movl	$0, %ecx
	adcl	%eax, %ebx
	movl	20(%esi), %eax
	adcl	%edx, %ecx

	mull	%ebp
	addl	%ebx, 16(%edi)
	movl	$0, %ebx
	adcl	%eax, %ecx
	movl	24(%esi), %eax
	adcl	%edx, %ebx

	mull	%ebp
	addl	%ecx, 20(%edi)
	movl	$0, %ecx
	adcl	%eax, %ebx
	movl	28(%esi), %eax
	adcl	%edx, %ecx

	mull	%ebp
	addl	%ebx, 24(%edi)
	movl	$0, %ebx
	adcl	%eax, %ecx
	movl	32(%esi), %eax
	adcl	%edx, %ebx

	mull	%ebp
	addl	%ecx, 28(%edi)
	movl	$0, %ecx
	adcl	%eax, %ebx
	movl	36(%esi), %eax
	adcl	%edx, %ecx

	mull	%ebp
	addl	%ebx, 32(%edi)
	movl	$0, %ebx
	adcl	%eax, %ecx
	movl	40(%esi), %eax
	adcl	%edx, %ebx
	mull	%ebp
	addl	%ecx, 36(%edi)
	adcl	%ebx, %eax
	adcl	$0, %edx
	addl	%eax, 40(%edi)
	adcl	$0, %edx
### carry limb is in %edx
	addl	%edx, 44(%edi)
	adcl	$0, 48(%edi)
	movl	120(%esp), %eax
	movl	(%eax), %ebp
	movl	124(%esp), %esi
### addmul1: src[0] is (%esi)
###          dst[0] is (%edi)
###          mult is %ebp
###          k is 11
###          kills %eax, %ebx, %ecx, %edx
###   dst[0,k[ += mult*src[0,k[  plus carry put in ecx or ebx
	movl	(%esi), %eax
	mull	%ebp
	movl	%eax, %ebx
	movl	%edx, %ecx
	movl	4(%esi), %eax

	mull	%ebp
	addl	%ebx, (%edi)
	movl	$0, %ebx
	adcl	%eax, %ecx
	movl	8(%esi), %eax
	adcl	%edx, %ebx

	mull	%ebp
	addl	%ecx, 4(%edi)
	movl	$0, %ecx
	adcl	%eax, %ebx
	movl	12(%esi), %eax
	adcl	%edx, %ecx

	mull	%ebp
	addl	%ebx, 8(%edi)
	movl	$0, %ebx
	adcl	%eax, %ecx
	movl	16(%esi), %eax
	adcl	%edx, %ebx

	mull	%ebp
	addl	%ecx, 12(%edi)
	movl	$0, %ecx
	adcl	%eax, %ebx
	movl	20(%esi), %eax
	adcl	%edx, %ecx

	mull	%ebp
	addl	%ebx, 16(%edi)
	movl	$0, %ebx
	adcl	%eax, %ecx
	movl	24(%esi), %eax
	adcl	%edx, %ebx

	mull	%ebp
	addl	%ecx, 20(%edi)
	movl	$0, %ecx
	adcl	%eax, %ebx
	movl	28(%esi), %eax
	adcl	%edx, %ecx

	mull	%ebp
	addl	%ebx, 24(%edi)
	movl	$0, %ebx
	adcl	%eax, %ecx
	movl	32(%esi), %eax
	adcl	%edx, %ebx

	mull	%ebp
	addl	%ecx, 28(%edi)
	movl	$0, %ecx
	adcl	%eax, %ebx
	movl	36(%esi), %eax
	adcl	%edx, %ecx

	mull	%ebp
	addl	%ebx, 32(%edi)
	movl	$0, %ebx
	adcl	%eax, %ecx
	movl	40(%esi), %eax
	adcl	%edx, %ebx
	mull	%ebp
	addl	%ecx, 36(%edi)
	adcl	%ebx, %eax
	adcl	$0, %edx
	addl	%eax, 40(%edi)
	adcl	$0, %edx
### carry limb is in %edx
   addl    %edx, 44(%edi)
   adcl    $0, 48(%edi)

	addl	$4, 120(%esp)
	addl	$4, %edi
	decl	92(%esp)
	jnz	Loop
###########################################
### Copy result in z
	movl	116(%esp), %ebx
	movl	(%edi), %eax
	movl	%eax, (%ebx)
	movl	4(%edi), %eax
	movl	%eax, 4(%ebx)
	movl	8(%edi), %eax
	movl	%eax, 8(%ebx)
	movl	12(%edi), %eax
	movl	%eax, 12(%ebx)
	movl	16(%edi), %eax
	movl	%eax, 16(%ebx)
	movl	20(%edi), %eax
	movl	%eax, 20(%ebx)
	movl	24(%edi), %eax
	movl	%eax, 24(%ebx)
	movl	28(%edi), %eax
	movl	%eax, 28(%ebx)
	movl	32(%edi), %eax
	movl	%eax, 32(%ebx)
	movl	36(%edi), %eax
	movl	%eax, 36(%ebx)
	movl	40(%edi), %eax
	movl	%eax, 40(%ebx)
	movl	44(%edi), %eax	# carry
	addl    $96, %esp
	popl	%ebx
	popl	%esi
	popl	%edi
	popl	%ebp
	ret

