```
Invocation failed Server returned invalid Response.java.lang.RuntimeException: Invocation failed Server returned invalid Response.at git4idea.GitAppUtil.sendXmlRequest(GitAppUtil.java:22)at git4idea.http.GitAskPassApp.main(GitAskPassApp.java:56)Caused by: java.io.IOException: Server returned invalid Response.at org.apache.xmlrpc.LiteXmlRpcTransport.sendRequest(LiteXmlRpcTransport.java:242)at org.apache.xmlrpc.LiteXmlRpcTransport.sendXmlRpc(LiteXmlRpcTransport.java:90)at org.apache.xmlrpc.XmlRpcClientWorker.execute(XmlRpcClientWorker.java:72)at org.apache.xmlrpc.XmlRpcClient.execute(XmlRpcClient.java:194)at org.apache.xmlrpc.XmlRpcClient.execute(XmlRpcClient.java:185)at org.apache.xmlrpc.XmlRpcClient.execute(XmlRpcClient.java:178)at git4idea.GitAppUtil.sendXmlRequest(GitAppUtil.java:19)    ... 1 moreerror: unable to read askpass response from 'C:\Users\renba\AppData\Local\JetBrains\IntelliJIdea2021.2\tmp\intellij-git-askpass-local.sh'bash: /dev/tty: No such device or addresserror: failed to execute prompt script (exit code 1)fatal: could not read Username for 'https://xxxx.git.com': No such file or directory
```

解决方式

git出现fatal: could not read Username for ‘https://git.xxx.com‘: Device not configure  
将项目的.git/config的remote url 改成https://username@\[git地址\]

例子：改前：http://\[git地址\]  例子：https://gitee.com/username/projectNamet.git

改后：http://username@\[git地址\]  例子:https://username@gitee.com/username/projectNamet.git

然后push代码输入用户名密码,也可以在链接中带入密码

https://username:password@gitee.com/username/projectNamet.git

如果密码中有@特殊字符 可以使用%40[转义](https://so.csdn.net/so/search?q=%E8%BD%AC%E4%B9%89&spm=1001.2101.3001.7020)代替

https://username@gitee.com/username/projectNamet.git


https://294722929@qq.com:qq154074@gitee.com/flybigpig/obsidian.git

flybigpig
## [obsidian](https://gitee.com/github-30883884/obsidian "obsidian")[](https://gitee.com/github-30883884/obsidian/recomm_self "自荐")