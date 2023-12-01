1、shell介绍

shell 俗称叫做壳，计算机的壳层，和内核是相对的，用于和用户交互，接收用户指令，调用相应的程序。



![img](https:////upload-images.jianshu.io/upload_images/17552351-8f65f82591fb8ee4.png?imageMogr2/auto-orient/strip|imageView2/2/w/500/format/webp)

因此，把shell分为2大类

[1.1、图形界面shell（Graphical User Interface shell 即 GUI shell）](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.zjiecode.com%2F2019%2F04%2F15%2Fshell%2F%231-1%E3%80%81%E5%9B%BE%E5%BD%A2%E7%95%8C%E9%9D%A2shell%EF%BC%88Graphical-User-Interface-shell-%E5%8D%B3-GUI-shell%EF%BC%89)

也就是用户使用GUI和计算机核交互的shell，比如Windows下使用最广泛的Windows Explorer（Windows资源管理器），Linux下的X Window，以及各种更强大的CDE、GNOME、KDE、 XFCE。

他们都是GUI Shell。

[1.2、命令行式shell（Command Line Interface shell ，即CLI shell）](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.zjiecode.com%2F2019%2F04%2F15%2Fshell%2F%231-2%E3%80%81%E5%91%BD%E4%BB%A4%E8%A1%8C%E5%BC%8Fshell%EF%BC%88Command-Line-Interface-shell-%EF%BC%8C%E5%8D%B3CLI-shell%EF%BC%89)

也就是通过命令行和计算机交互的shell。

Windows NT 系统下有 cmd.exe（命令提示字符）和近年来微软大力推广的 Windows PowerShell。

Linux下有bash / sh / ksh / csh／zsh等

一般情况下，习惯把命令行shell（CLI shell）直接称做shell，以后，如果没有特别说明，shell就是指 CLI shell，后文也是主要讲Linux下的 CLI shell。

[2、交互方式](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.zjiecode.com%2F2019%2F04%2F15%2Fshell%2F%232%E3%80%81%E4%BA%A4%E4%BA%92%E6%96%B9%E5%BC%8F)

根据交互方式的不一样，命令行式shell（CLI shell），又分为交互式shell和非交互式shell。

[2.1、交互式shell](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.zjiecode.com%2F2019%2F04%2F15%2Fshell%2F%232-1%E3%80%81%E4%BA%A4%E4%BA%92%E5%BC%8Fshell)

交互式模式就是shell等待你的输入，并且执行你提交的命令，然后马上给你反馈。这种也是我们大多数时候使用的。

[2.2、非交互式shell](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.zjiecode.com%2F2019%2F04%2F15%2Fshell%2F%232-2%E3%80%81%E9%9D%9E%E4%BA%A4%E4%BA%92%E5%BC%8Fshell)

非交互式shell，就是把shell放在写在一个文件里面，执行的时候，不与用户交互，从前往后依次执行，执行到文件结尾时，shell也就终止了。

[3、shell的种类](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.zjiecode.com%2F2019%2F04%2F15%2Fshell%2F%233%E3%80%81shell%E7%9A%84%E7%A7%8D%E7%B1%BB)

在Linux下 ，各种shell百花齐放，种类繁多，不同的shell，也有不同的优缺点。

我们要查看当前系统下支持的shell，可以读取/etc/shells文件。



![img](https:////upload-images.jianshu.io/upload_images/17552351-2d79e879e976f6eb.png?imageMogr2/auto-orient/strip|imageView2/2/w/882/format/webp)

[3.1、bash](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.zjiecode.com%2F2019%2F04%2F15%2Fshell%2F%233-1%E3%80%81bash)

Bourne Again Shell 用来替代Bourne shell，也是目前大多数Linux系统默认的shell。

[3.2、sh](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.zjiecode.com%2F2019%2F04%2F15%2Fshell%2F%233-2%E3%80%81sh)

Bourne Shell 是一个比较老的shell，目前已经被/bin/bash所取代，在很多linux系统上，sh已经是一个指向bash的链接了。

下面是CentOS release 6.5 的系统



![img](https:////upload-images.jianshu.io/upload_images/17552351-7afb12bfabd4d5a4.png?imageMogr2/auto-orient/strip|imageView2/2/w/868/format/webp)

[3.3、csh／tcsh](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.zjiecode.com%2F2019%2F04%2F15%2Fshell%2F%233-3%E3%80%81csh%EF%BC%8Ftcsh)

C shell 使用的是“类C”语法,csh是具有C语言风格的一种shell，tcsh是增强版本的csh，目前csh已经很少使用了。

[3.4、ksh](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.zjiecode.com%2F2019%2F04%2F15%2Fshell%2F%233-4%E3%80%81ksh)

最早，bash交互体验很好，csh作为非交互式使用很爽，ksh就吸取了2者的优点。

[3.5、zsh](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.zjiecode.com%2F2019%2F04%2F15%2Fshell%2F%233-5%E3%80%81zsh)

zsh网上说的目前使用的人很少，但是感觉使用的人比较多。

zsh本身是不兼容bash的，但是他可以使用仿真模式（emulation mode）来模拟bash等，基本可以实现兼容。

在交互式的使用中，目前很多人都是zsh，因为zsh拥有很强大的提示和插件功能，炫酷吊炸天。推荐在终端的交互式使用中使用zsh，再安利一个插件Oh My Zsh

其实我个人的理解是，在终端中使用shell，基本上只是调用各种命令，比如：curl cat ls等等，基本不会使用到zsh的编程，所以终端中使用zsh是可以的。但是在写shell脚本的时候，需要考虑兼容性，

最主流的还是bash shell，所以，后文我们介绍的shell脚本也是bash shell的。

[4、shell脚本](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.zjiecode.com%2F2019%2F04%2F15%2Fshell%2F%234%E3%80%81shell%E8%84%9A%E6%9C%AC)

[4.1、基础](https://links.jianshu.com/go?to=https%3A%2F%2Fblog.zjiecode.com%2F2019%2F04%2F15%2Fshell%2F%234-1%E3%80%81%E5%9F%BA%E7%A1%80)

 4.2、Shell 变量

shell的使用比较简单，就像这样，并且没有数据类型的概念，所有的变量都可以当成字符串来处理：

```
#!/bin/bashmyName="tom"
youName="cat"
```

不需要申明，直接写就可以了，但是有几个点需要特别注意：

- 等号两边不能有空格！！！特别要注意，非常容易写错
- 命名只能使用英文字母，数字和下划线，首个字符不能以数字开头。
- 中间不能有空格，可以使用下划线（_）。
- 不能使用标点符号。
- 不能使用bash里的关键字。

使用变量

```
ABC="tom"
echo $ABC #使用变量前面加$美元符号
echo "ABC=$ABC" #可以直接在字符串里面引用
echo "ABC=${ABC}" #但是建议把变量名字用{}包起来
```

只读变量

```
ABC="tom"
echo "ABC=${ABC}"
readOnly ABC #设置只读
ABC="CAT" #会报错，因为设置了只读，不能修改
```

![readonly](https://oscimg.oschina.net/oscnet/9b7245a65e0825b8deb2424437011e61f25.jpg)

删除变量

```
ABC="tom"
echo "ABC=${ABC}"
unset ABC #删除
echo "ABC=$ABC"  
echo "ABC=${ABC}"
```

从这个例子当中，我们也发现，使用一个不存在的变量，shell不会报错，只是当作空来处理。

## 4.3、Shell 的字符串

使用字符串

```
NAME="tom"
A=my #你甚至可以不用引号，但是字符串当中不能有空格，这种方式也不推荐
B='my name is ${NAME}' #变量不会被解析
C="my name is ${NAME}" #变量会解析
echo $A
echo $B
echo $C
```

执行结果

![run_1](https://oscimg.oschina.net/oscnet/4da8758bd71f034b44c14f1ef2fca9e0cac.jpg)

我们可以发现，这个字符串的单双号和PHP的处理非常类似，单引号不解析变量，双引号可以解析变量。但是都可以处理转义符号。

```
A='my\nname\nis\ntom'
B="my\nname\nis\ntom"
echo $A
echo $B
```

执行结果

![run_2](https://oscimg.oschina.net/oscnet/8ff82a4f650ba4fc880765ecef9f8b5715c.jpg)

拼接字符串 其实shell拼接字符串，大概就是2种

- 一是直接在双引号内应用变量，类似模版字符串
- 二是直接把字符串写在一起，不需要类似Java链接字符串的“+” 和PHP链接字符串的“.”

```
NAME="TOM"# 使用双引号拼接echo "hello, "$NAME" !" #直接写在一起，没有字符串连接符
echo "hello, ${NAME} !" #填充模版# 使用单引号拼接echo 'hello, '$NAME' !' #直接写在一起，没有字符串连接符
echo 'hello, ${NAME} !' #上面已经提高过，单引号里面的变量是不会解析的
```

![run_3](https://oscimg.oschina.net/oscnet/1c09b9bd68aa837218e8a5e176cd36d9d4c.jpg)

强大的字符串处理 shell中简单的处理字符串，可以直接使用各种标记，只是比较难记忆，要用的时候，可以查一下。

```
ABC="my name is tom,his name is cat"
echo "字符串长度=${#ABC}" # 取字符串长度
echo "截取=${ABC:11}" # 截取字符串， 从11开始到结束
echo "截取=${ABC:11:3}" # 截取字符串， 从11开始3个字符串
echo "默认值=${XXX-default}" #如果XXX不存在，默认值是default
echo "默认值=${XXX-$ABC}" #如果XXX不存在，默认值是变量ABC
echo "从开头删除最短匹配=${ABC#my}" # 从开头删除 my 匹配的最短字符串
echo "从开头删除最长匹配=${ABC##my*tom}" # 从开头删除 my 匹配的最长字符串
echo "从结尾删除最短匹配=${ABC%cat}" # 从结尾删除 cat 匹配的最短字符串
echo "从结尾删除最长匹配=${ABC%%,*t}" # 从结尾删除 ,*t 匹配的最长字符串
echo "替换第一个=${ABC/is/are}" #替换第一个is
echo "替换所有=${ABC//is/are}" #替换所有的is
```

运行结构

![run_4](https://oscimg.oschina.net/oscnet/e514b2d7c19daf65449e6496da03ddc9471.jpg)

这里只是介绍了比较常用的一些字符串处理，实际shell支持的还有很多。

## 4.4、数组

Bash Shell 也是支持数组的，与绝大部分语言一样，数组下标从0开始。不过需要注意的是，它只支持一维数组。 定义一个数组，用小括号阔气来，当中用“空格”分割，就像下面这样：

```
array=("item0" "item1" "item2")
```

也可以根据下标来定义元素

```
array[0]="new_item0"
array[1]="new_item1"
array[2]="new_item2"
array[4]="new_item4" #数组下标可以是不连续的
```

读取数组元素，和变量类似

```
echo ${array[0]}
echo "array[0]=${array[0]}"
```

获取数组所有的元素

```
echo "数组的元素为: ${array[*]}"
echo "数组的元素为: ${array[@]}"
```

获取数组的长度

```
echo "数组的长度为: ${#array[*]}"
echo "数组的长度为: ${#array[@]}"
```

## .5、输入输出

### 4.5.1、echo

在上文中，其实我们已经到多次，就是：echo “字符串” 来输出，一个很简单的例子

```
echo "Hello world!"
```

如果当中包含特殊符号，可以使用转义等：

```
echo "Hello \nworld!"
echo "\"Hello\""
echo '"Hello"' #当然，也可以这样,单引号不转义，上文提到过
echo `date` #打印执行date的结果
echo -n "123" #加-n  表示不在末尾输出换行
echo "456"
echo -e "\a处理特殊符号" #-e 处理特殊符号
```

-n 让echo输出结束以后，在默认不输出换行符 -e 让echo处理特殊符号，比如：

| 符号 | 作用                           |
| :--- | :----------------------------- |
| \a   | 发出警告声                     |
| \b   | 删除前一个字符                 |
| \c   | 后不加上换行符号               |
| \f   | 换行但光标仍旧停留在原来的位置 |
| \n   | 换行且光标移至行首             |
| \r   | 光标移至行首，但不换行         |
| \t   | 插入tab                        |

上面的特殊符号，写到mac的shell脚本里面要注意，执行的时候，要用bash执行才有效 ，sh无效。

当然，你也可以玩一点更有趣的，就是我们随时在终端中看到的五颜六色的文字：

```
echo -e "\033[31m 红色前景 \033[0m 缺省颜色" 
echo -e "\033[41m 红色背景 \033[0m 缺省颜色"
```

![color](https://img-blog.csdnimg.cn/20190502204336569.png)

其中 \033[是一个特殊标记，表示终端转义开始， 31m表示使用红色字体，你也可以使用其他颜色，[30-39]是前景颜色，[40-49]是背景颜色。 \033[0m回复到缺省设置 还可以有一些其他的动作

```
echo -e "\033[2J" #清除屏幕
echo -e "\033[0q" #关闭所有的键盘指示灯
echo -e "\033[1q" #设置"滚动锁定"指示灯(Scroll Lock)
echo -e "\033[2q" #设置"数值锁定"指示灯(Num Lock)
echo -e "\033[1m" #设置高亮度
echo -e "\033[4m" #下划线
echo -e "\033[7m" #反显
echo -e "\033[y;xH" #设置光标位置
```

其他更多的特殊码请自行查询。

### 4.5.2、read

有输出，必然有输入，read命令接收标准输入的输入。

```
read name
echo "my name is ${name}"
```

可以使用-p给一个输入提示

```
read -p "please input your name:" name
echo "my name is ${name}"
```

如果没有指定输入的变量，会把输入放在环境标量REPLY中

```
read -p "please input your name:"
echo "my name is ${REPLY}"
```

计时输入，如果一段时间没有输入 ，就直接返回,使用-t 加时间

```
read -t 3 -p "please input your name in 3 senconds:" name
```

指定输入字符个数，使用-n ，后面的是输入字符个数

```
read -n 1 -p "Are you sure [Y/N]?" isYes
```

默读（输入不再监视器上显示），加一个-s参数。

```
read  -s  -p "Enter your password:" password
```

### 4.5.3、printf

echo已经比较强大，但是有的时候，我们需要用到字符串模版输出，printf就比较好用了，他类似C里面的printf程序。 语法是：printf format-string [arguments...] 比如我们要输出一个表格

```
printf "%-10s %-8s %-4s\n" 姓名 性别 体重kg  
printf "%-10s %-8s %-4.2f\n" 郭靖 男 66.1234 
printf "%-10s %-8s %-4.2f\n" 杨过 男 48.6543 
printf "%-10s %-8s %-4.2f\n" 郭芙 女 47.9876
```

运行结果

![run_5](https://oscimg.oschina.net/oscnet/387589ec24cf6990550da6bebfcf7242536.jpg)

%s %c %d %f都是格式替代符 %-10s 指一个宽度为10个字符（-表示左对齐，没有则表示右对齐），至少显示10字符宽度，如果不足则自动以空格填充，超过不限制。 %-4.2f 指格式化为小数，其中.2指保留2位小数。

### 4.5.4、重定向

大多数 UNIX 系统命令从你的终端接受输入并将所产生的输出发送回到您的终端。一个命令通常从一个叫标准输入的地方读取输入，默认情况下，这恰好是你的终端。同样，一个命令通常将其输出写入到标准输出，默认情况下，这也是你的终端。

| 命令            | 作用                                               |
| :-------------- | :------------------------------------------------- |
| command > file  | 将输出重定向到 file。                              |
| command < file  | 将输入重定向到 file。                              |
| command >> file | 将输出以追加的方式重定向到 file。                  |
| n > file        | 将文件描述符为 n 的文件重定向到 file。             |
| n >> file       | 将文件描述符为 n 的文件以追加的方式重定向到 file。 |
| n >& m          | 将输出文件 m 和 n 合并。                           |
| n <& m          | 将输入文件 m 和 n 合并。                           |
| << tag          | 将开始标记 tag 和结束标记 tag 之间的内容作为输入。 |

需要注意的是文件描述符 0 通常是标准输入（STDIN），1 是标准输出（STDOUT），2 是标准错误输出（STDERR）。

输出到文件

```
echo "test">text.txt #直接输出
echo "test">>text.txt #追加在text.txt后面
```

重定向输入

```
read a <<EOF
"测试"
EOF
echo "a=$a"
```

来个比较过分的

```
cat  < 1.sh > text.txt
```

把1.sh文件的内容出入到cat，然后cat在输出到text.txt中，相当于，把1.sh的内容输出到text.txt中了

还有一种用法，把标准错误直接输出到标准输出，并且输出到文件file

```
command > file 2>&1
```

/dev/null 文件 这个是一个特殊文件，他是一个黑洞，写入到它的内容都会被丢弃，如果我们不关心程序的输出，可以这样

```
command > /dev/null 2>&1
```

## 4.6、条件判断（if）

和其他语言一样，shell也有条件判断

单分支：

```
if condition
then
    command1 
    command2
    ...
fi
```

双分支：

```
if condition
then
    command1 
    command2
    ...
else
    command
fi
```

多分支：

```
if condition1
then
    command1
elif condition2 
then 
    command2
else
    commandN
fi
```

比如

```
if [ "2" == "2" ]; then # "2" 的2边都有空格，不能省略 ，写在一行，条件后面加一个分号
    echo "2==2"
else
    echo "2!=2"
fi
```

需要特别注意：[ "2" == "2" ] 其中的"=="两边都有空格，不能省略，否则结果不正确。 判断普通文件是否存在

```
if [ -f "1.sh" ]; then # 判断一个普通文件是否存在
    echo "1.sh 存在"
fi
```

判断目录是否存在

```
if [ -d "1.sh" ]; then # 判断一个目录是否存在
    echo "1.sh 存在"
fi
```

判断字符串长度为0

```
a=""
if [ -z $a ]; then
    echo "a为空"
fi
```

## 4.7、()、(())、[]、[[]]和{}

在shell中，有几个符号要非常注意，用的也比较多，不要搞混了，搞混了，逻辑运算很容易出错

### 4.7.1、单小括号()

- 命令组 括号中的命令将会新开一个子shell顺序执行，所以括号中的变量不能够被脚本余下的部分使用。括号中多个命令之间用分号隔开，最后一个命令可以没有分号，各命令和括号之间不必有空格。

  ```
  a="123"
  (echo "123";a="456";echo "a=$a")
  echo "a=$a
  ```

  ![命令组](https://oscimg.oschina.net/oscnet/d8fef37b27dc75dd8602671e7b840a487dc.jpg)

- 命令替换 发现了$(cmd)结构，便将$(cmd)中的cmd执行一次，得到其标准输出，再将此输出放到原来命令。

- 用于初始化数组 如：array=(a b c d)

### 4.7.2、双小括号(())

- 运算扩展，比如，你可以

  ```
  a=$((4+5)) 
  echo "a=$a"
  ```

- 做数值运算，重新定义变量

  ```
  a=5
  ((a++))
  echo "a=$a"
  ```

- 用于算术运算比较

  ```
  if ((1+1>1));then
    echo "1+1>1"
  fi
  ```

### 4.7.3、单中括号[]

- 用于字符串比较 需要注意，用于字符串比较，运算符只能是 ==和!=，需要注意，运算符号2边必须有空格，不然结果不正确！！！比如：

  ```
  if [ "2" == "2" ]; then # "2" 的2边都有空格，不能省略
    echo "2==2"
  else
    echo "2!=2"
  fi
  ```

- 用于整数比较 需要注意，整数比较，只能用-eq，-gt这种形式，不能直接使用大于(>)小于(<)符号。只能用于整形。

  ```
  if [ 2 -eq 2 ]; then
     echo "2==2"
  else
    echo "2!=2"
  fi
  ```

  符号表

  | 符号 | 运算     |
  | :--- | :------- |
  | -eq  | 等于     |
  | -ne  | 不等于   |
  | -gt  | 大于     |
  | -ge  | 大于等于 |
  | -lt  | 小于     |
  | -le  | 小于等于 |

- 多个逻辑组合 -a 表示and 与运算 -o 表示or 或运算

  ```
  if [ "2" == "2" -a "1" == "1" ]; then #注意，在这里，不能是[ "2" == "2" ] -a [ "1" == "1" ] 会报错
    echo "ok"
  fi
  ```

### 4.7.4、双中括号[[]]

[[是 bash 程序语言的关键字。并不是一个命令，[[ ]] 结构比[ ]结构更加通用。

- 字符串匹配时甚至支持简单的正则表达式

  ```
  if [[ "123" == 12* ]]; then #右边是正则不需要引号
    echo "ok"
  fi
  ```

- 支持对数字的判断，是支持浮点型的，并且可以直接使用<、>、==、!=符号

  ```
  if [[  2.1 > 1.1 ]]; then
    echo "ok"
  fi
  ```

- 多个逻辑判断 可以直接使用&&、||做逻辑运算，并且可以在多个[[]]之间进行运算

  ```
  if [[  1.1 > 1.1 ]] || [[ 1.1 == 1.1 ]]; then
    echo "ok"
  fi
  ```

### 4.7.5、大括号{}

- 统配扩展

  ```
  touch new_{1..5}.txt #创建new_1.txt	new_2.txt	new_3.txt	new_4.txt	new_5.txt  5个文件
  ```

## 4.8、循环

### 4.8.1、for循环

语法格式为：

```
for a in "item1" "item2" "item3"
do
    echo $a
done
```

输出当前目录下 .sh结尾的文件

```
for a in `ls ./`
do
    if [[ $a == *.sh ]]
  then
        echo $a
  fi
done
```

### 4.8.2、for循环

语法

```
while condition
do
    command
done
```

我们要输出1-10000

```
int=1;while(($int<=10000))do
    echo $int
    ((int++))
done
```

### 4.8.3、until循环

语法

```
until condition
do
    command
done
```

用法类似，这里不再赘述。 循环中 continue命令与break作用和其他语言中类似。

## 4.9、case

case和其他语言switch类型，多分支，选择一个匹配。匹配发现取值符合某一模式后，其间所有命令开始执行直至 ;;，有点类型Java的break。如果无一匹配模式，使用星号 * 捕获该值，再执行后面的命令。

```
echo '输入 1 到 4 之间的数字:'
echo '你输入的数字为:'
read aNum
case $aNum in
    1)  echo '你选择了 1'
    ;;
    2)  echo '你选择了 2'
    ;;
    3)  echo '你选择了 3'
    ;;
    4)  echo '你选择了 4'
    ;;
    *)  echo '你没有输入 1 到 4 之间的数字'
    ;;
esac
```

## 4.10、函数

shell也可以用户定义函数，然后在shell脚本中可以随便调用。 注意：所有函数在使用前必须定义。这意味着必须将函数放在脚本开始部分，直至shell解释器首次发现它时，才可以使用。调用函数仅使用其函数名即可。 语法格式如下：

```
[function] funname()
{

    cmd....
    [return int]
}
```

一个最简单的函数

```
Line(){
  echo "--------分割线--------"
}
echo "123"
Line
echo "456"
```

在Shell中，调用函数时可以向其传递参数。 在函数体内部，通过 $n 的形式来获取参数的值，例如，$1表示第一个参数，$2表示第二个参数... 调用的时候 ，函数名，参数直接用空格分割开。 带参数的函数示例：

```
out(){
    echo "1-->$1"
    echo "2-->$2"
}
out 1 2 #调用的之后
```

还有一些其他的特殊符号需要注意

| 符号 | 作用                                                         |
| :--- | :----------------------------------------------------------- |
| $#   | 传递到脚本的参数个数                                         |
| $*   | 以一个单字符串显示所有向脚本传递的参数                       |
| $$   | 脚本运行的当前进程ID号                                       |
| $!   | 后台运行的最后一个进程的ID号                                 |
| $@   | 与$*相同，但是使用时加引号，并在引号中返回每个参数。         |
| $?   | 显示最后命令的退出状态。0表示没有错误，其他任何值表明有错误。 |

所以我们可以写一个代码参数，返回值的函数

```
out(){
    echo "全部参数$*"
    for item in $*
    do
        echo "$item"
    done
    return $# #这类返回参数个数，返回值必须是整数
}
out this is perfect
echo "函数返回值:$?"
```

## 4.11、shell传递参数

我们可以在执行 Shell 脚本时，向脚本传递参数，脚本内获取参数的格式为：$n。n 代表一个数字，1 为执行脚本的第一个参数，2 为执行脚本的第二个参数，以此类推…… 除了参数可以使用特殊符号，也可以使用上文中函数所使用的特殊符号，这里不再赘述

```
echo "执行的文件名：$0";
echo "全部参数:$*"
echo "参数个数:$#"
echo "第一个参数为：$1";
echo "第二个参数为：$2";
echo "第三个参数为：$3";
```

![params](https://oscimg.oschina.net/oscnet/f54c763021e15bd30781675373517947b7b.jpg)

# 5、其他实用技巧

shell脚本，他本身的功能并不强大，强大的是他可以调用其他程序，而在Linux下，系统自带的就有非常多的强大工具可以调用。

## 5.1、后台执行

后台执行一个脚本只需要在后面加上&符号即可，我们先用之前学习的，写一个脚本，1s输出一个数字

```
#!/bin/bashint=1
while :
do
    echo $int
    ((int++))
    sleep 1s #睡眠一秒
done
```

我们执行sh d.sh & 我们发现，的确会后台输出，但是会输出到当前控制台，我们可以用之前学的重定向，把输出重定向到文件

sh d.sh > out.log 2>&1 &

这样就把输出和错误重新定向到out.log文件了 但是，我们发现，关闭终端以后，文件就不输出了。 当我们端口连接远程主机的session或者关闭当前终端的时候， 会产生一个SIGHUP信号 ，导致程序退出，我们可以使用nuhup来忽略这个信号 ，达到真正的后台。

nuhup sh d.sh > out.log 2>&1 &

这样启动程序，就可以打到真正后台运行了。 那么问题来了，我们验证程序在后台运行呢？要怎嚒结束后台程序呢？请继续看。

## 5.2、cat

在本文中，我们已经多次用到cat，他的作用就是读取文件输出到标准输出上，也就是我们的终端。 语法是：

cat [option] file

我们也可以使用：cat -n file ,来输出行号。

## 5.2、tail

类似上面的例子，我们要验证程序是不是在后台，每一秒输出一个数字到文件，使用cat读取，需要不断的多次查看，一次cat只能输出一次。 tail非常适合查看这种日志类文件，他的作用是读取文件末尾几行输出到标准输出上。 tail out.log 默认显示10行，可以使用参数-n指定行数 tail -20 out.log 显示文件末尾20行 tail -f out.log 持续监控文件out.log，如果有变化，他会试试的显示在我们的屏幕上面。

## 5.3、ps

ps，查询进程 这个命令参数比较多，列举几个比较常用的

| 参数 | 作用                                       |
| :--- | :----------------------------------------- |
| a    | 显示终端上的所有进程，包括其他用户的进程。 |
| u    | 显示面向用户的格式信息。                   |
| x    | 显示没有控制终端的进程。                   |

一般查询，使用 ps aux就可以了，查询出来比较多，可以筛选一下。 这里我们使用 ps u 就可以查询出我们刚才开启的后台进程了。

![ps](https://oscimg.oschina.net/oscnet/5982437f7b5b60869e78b3fd6abe07eb303.jpg)

我们看到我们刚才启动的程序PID为7523， 使用kill命令就可以杀死他了

## 5.4、kill

kill命令比较简单，就是根据PID结束一个程序，比如我们已经查询到，我们开的后台进行是7523，要结束他可以使用： kill 7523 以上是常用用法，其实kill是给程序发送一个信号，上面的程序给会程序发送一个SIGTERM信号，程序收到这个信号，完成资源的释放，就退出了。 但是也有程序不听话，收到信号就是不退出，这个时候，就要强制他退出，使用9号命令（SIGKILL），强制杀死他。 简单的说 kill PID 是告诉程序，你应该退出了，请自己退出。 kill -9 PID ，是直接告诉程序，你被终结了，这个命令信号，不能被抓取或者忽略。

# 6、总结

- shell使用的比较少，但是特别强大；
- shell对语法比较敏感，并且应为解释器很多，每个解释器语法标准也可能不完全一致；
- 使用到的编号、编码、参数特别多，并且都是简写，很多记不住。其实不用死记硬背，记住有这个功能就可以了，需要用到的时候再查询。