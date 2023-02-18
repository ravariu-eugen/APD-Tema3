build:
	mpicc tema3.c -o tema3 -lm

run:
	mpirun --oversubscribe -np 12 tema3 120 2

clear:
	rm tema3