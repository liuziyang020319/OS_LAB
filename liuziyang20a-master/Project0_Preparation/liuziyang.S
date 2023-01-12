.global main
main:
	li s1,1
	li s2,101
	li s3,0
	bge s1, s2, end
loop:
	add s3,s3,s1
	addi s1,s1,0x01
	blt s1,s2,loop
end:
	j end
