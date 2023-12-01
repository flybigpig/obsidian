# removeView



````
java.lang.IllegalStateException: The specified child already has a parent. You must call removeView() on the child's parent first.

````



##### 报错原因

根据The specified child already has a parent. You must call removeView() on the child’s parent first.日志信息，不难读懂大致意思是子view已经拥有一个父布局，***我们需要先让该子view的父布局调用一下 removeView()方法***。也就是说一个子view只能拥有一个父view，这种情况往往会出现在动态添加view上，我们添加子view的时候，并不知道子view是不是已经拥有一个父view，如果说已经存在一个父view那么就会报以上错误。

