

# 最终可执行文件hello，依赖于hello.o
hello: hello.o
	gcc hello.o -o hello          # ← Tab 开头，不是空格！

# 目标文件hello.o，依赖于hello.s
hello.o: hello.s
	gcc -c hello.s -o hello.o      # ← Tab 开头

# 汇编文件hello.s，依赖于hello.i
hello.s: hello.i
	gcc -S hello.i -o hello.s      # ← Tab 开头

# 预处理文件hello.i，依赖于hello.c
hello.i: hello.c
	gcc -E hello.c -o hello.i      # ← Tab 开头


.PHONY: clean
clean:
	rm -f hello.i hello.s hello.o hello