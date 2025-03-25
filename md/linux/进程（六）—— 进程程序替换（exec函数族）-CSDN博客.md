fork创建子进程以后，子进程继承父进程的一部分代码和数据，如果我们希望子进程和父进程执行不同的代码，我们可以通过判断 fork函数 的返回值来判断当前进程是父进程还是子进程，进而分配不同的任务。

![](https://i-blog.csdnimg.cn/blog_migrate/6c9ad94dba016e86231090d975cc072a.png)

exec 函数族的作用是替换原本要执行程序，转而去执行其他程序，可以让我们更加灵活的给子进程分配任务。 

___

**目录**

[一、exec函数族](https://blog.csdn.net/challenglistic/article/details/123901218#t0)

[1、execl / execlp](https://blog.csdn.net/challenglistic/article/details/123901218#t1)

[(1) execl函数参数解析](https://blog.csdn.net/challenglistic/article/details/123901218#t2)

[(2) execlp 函数参数解析](https://blog.csdn.net/challenglistic/article/details/123901218#t3)

[2、execv / execvp / execvpe](https://blog.csdn.net/challenglistic/article/details/123901218#t4)

[(1) execv函数参数解析](https://blog.csdn.net/challenglistic/article/details/123901218#t5)

[(2) execvp函数参数解析](https://blog.csdn.net/challenglistic/article/details/123901218#t6)

[(3) execvpe函数参数解析](https://blog.csdn.net/challenglistic/article/details/123901218#t7)

[二、exec 函数族的应用场景](https://blog.csdn.net/challenglistic/article/details/123901218#t8)

[三、进程程序替换原理](https://blog.csdn.net/challenglistic/article/details/123901218#t9)

___

## 一、[exec函数](https://so.csdn.net/so/search?q=exec%E5%87%BD%E6%95%B0&spm=1001.2101.3001.7020)族

exec函数族的命名存在规律性，exec之后的字母代表了当前函数传递参数的类型。

-   “l”：表示list，传递的是形参列表；
-   “v”：表示vector，传递的是形参数组；
-   “p”：表示path，说明当前函数会去环境变量中搜索替换程序文件；
-   “e”：表示environment，说明当前函数搜索替换程序程序时，搜索的是自定义环境变量。

### 1、[execl](https://so.csdn.net/so/search?q=execl&spm=1001.2101.3001.7020) / execlp

execl 和 execlp函数的声明如下：

![](https://i-blog.csdnimg.cn/blog_migrate/e6f85089df7ab18f68e8fb53cebfbbf7.png)

#### (1) execl函数参数解析

execl函数是去指定路径下面搜索并执行相应的程序。execl 的最后一个参数必须是NULL

-   **参数 path**：替换程序所在路径。（如果是自己编写的替换程序，替换程序必须包含main函数）
-   **参数arg**：传递给替换程序的参数。arg 会被传递给替换程序 main 函数的 argv，main函数中的 argv 根据NULL来判断参数是否传递结束。

```
int main(int argc, char** argv){}
```

-   **返回值**：调用失败返回 -1，errno 会被设置（可以根据错误码对应到错误信息）

```
const char* path = "/bin/ls";if(execl(path, "ls", "-a", "-l", NULL)<0){    perror("execl");}
```

#### (2) execlp 函数参数解析

execlp函数是去当前Shell 的环境变量PATH中搜索并执行相应的程序。execlp 的最后一个参数必须是NULL

-   **参数 file**：替换程序的名称。因为是去环境变量PATH中搜索，所以不需要明确的路径，只需要程序名称。（如果是自己编写的替换程序，替换程序必须包含main函数）
-   **参数arg**：传递给替换程序的参数。同 execl 函数。
-   **返回值**：调用失败返回 -1，errno 会被设置（可以根据错误码对应到错误信息）

```
const char* file= "ls";       if(execl(file, "ls", "-a", "-l", NULL)<0){    perror("execlp");}
```

### 2、execv / execvp / execvpe

execv 的作用和 execl 的作用是一样的，execv中的“v”表示vector，传递的是一个数组；execl中的“l”表示list，传递的是参数列表。两者的区别仅仅只是传递参数的方式不同。

![](https://i-blog.csdnimg.cn/blog_migrate/44c21d1e6e4ff8df24fe521132ab3985.png)

#### (1) execv函数参数解析

execv函数是去指定路径下面搜索并执行相应的程序，传入的是一个参数数组，而不是参数列表。

-   **参数 path**：替换程序所在路径。（如果是自己编写的替换程序，替换程序必须包含main函数）
-   **参数argv**：传递给替换程序的参数数组。argv 的最后一个参数必须是NULL(原因参考execl)
-   **返回值**：调用失败返回 -1，errno 会被设置（可以根据错误码对应到错误信息）

```
const char* path = "/bin/ls";char* argv[] = {"ls", "-a", "-l", NULL};    if(execv(path, argv)<0){    perror("execv");}
```

#### (2) execvp函数参数解析

execvp函数是去当前Shell 的环境变量PATH中搜索并执行相应的替换程序。

-   **参数 file**：替换程序的名称。因为是去环境变量PATH中搜索，所以不需要明确的路径，只需要程序名称。（如果是自己编写的替换程序，替换程序必须包含main函数）
-   **参数argv**：传递给替换程序的参数数组。argv 的最后一个参数必须是NULL(原因参考execl)
-   **返回值**：调用失败返回 -1，errno 会被设置（可以根据错误码对应到错误信息）

```
const char* file= "ls";char* argv[] = {"ls", "-a", "-l", NULL};    if(execvp(file, argv)<0){    perror("execvp");}
```

#### (3) execvpe函数参数解析

execvpe函数比execvp函数多了一个“e”，“e”表示environment，execvp函数是在环境变量PATH中搜索并执行替换程序；而execvpe函数则是使用自定义的环境变量，也就是相当于我们自己提供替换程序的搜索路径（搜索路径可以有多个）

-   **参数 file**：替换程序的名称。因为是去环境变量PATH中搜索，所以不需要明确的路径，只需要程序名称。（如果是自己编写的替换程序，替换程序必须包含main函数）
-   **参数argv**：传递给替换程序的参数数组。argv 的最后一个参数必须是NULL(原因参考execl)
-   **参数envp**：自定义的环境变量数组。
-   **返回值**：调用失败返回 -1，errno 会被设置（可以根据错误码对应到错误信息）

```
const char* file= "ls";char* argv[] = {"ls", "-a", "-l", NULL};    char* const envp[]={"PATH=/bin:/usr/bin", "TERM=console", NULL};    if(execvpe(file, argv, envp) < 0) {perror("execvpe");}
```

## 二、exec 函数族的应用场景

exec函数执行的时候有如下特点：

-   进程间具有独立性，子进程的程序替换不会影响到父进程
-   进程替换成功以后，就不会执行后续代码，即后续的语句不会执行

### 1、单进程

如果是单进程调用了exec函数，那么exec函数之后的代码都不会被执行。

![](https://i-blog.csdnimg.cn/blog_migrate/cd1d1a46dc7e2bd99b6e048d68dd62db.png)

### 2、父子进程

如果是父子进程中，让子进程调用了 exec 函数，那么子进程就会转而去执行 exec函数指向的程序，但是父进程不会受到影响。

![](https://i-blog.csdnimg.cn/blog_migrate/53ecffeb39193ee8c3e0bf7bcff55fa6.png)

## 三、进程程序替换原理

一个完整的进程 = 进程控制块(task\_struct) + 虚拟内存 + 页表 + 物理内存中的代码和数据

![](https://i-blog.csdnimg.cn/blog_migrate/0f322d3617916caec3cc619f9cf04ab1.png)

由这个图可以知道，**进程程序替换**，替换的是**物理内存**中的**代码段**和**数据段**的内容

现在假设磁盘上有一段程序B

![](https://i-blog.csdnimg.cn/blog_migrate/4760c236fb4da63cbbacbc7b2e0e614b.png)

我们要用程序B的代码和数据去**替换**上面这个进程的代码和数据

![](https://i-blog.csdnimg.cn/blog_migrate/1d29353b9e8c61823ab20257ac255413.png)

修改的只是物理内存中的代码和数据，PCB、虚拟内存并没有受到影响，所以这里我们需要知道的是，**进程程序替换不会创建新的进程**！！！