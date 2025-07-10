## Android 源码制作jks签名文件



版权声明：本文为博主原创文章，遵循 [CC 4.0 BY-SA](http://creativecommons.org/licenses/by-sa/4.0/) 版权协议，转载请附上原文出处链接和本声明。

###### 进入进入签名文件目录 /build/target/product/security

```java
第一步： openssl pkcs8 -inform DER -nocrypt -in platform.pk8 -out platform.pem

第二步： openssl pkcs12 -export -in platform.x509.pem -out platform.p12 -inkey platform.pem -password pass:android -name android

第三步： keytool -importkeystore -deststorepass android -destkeystore platform.jks -srckeystore platform.p12 -srcstoretype PKCS12 -srcstorepass android

其中对应：
	storePassword 'android'
	keyAlias 'android'
	keyPassword 'android'
	
```

参考自：[UGG大神博客](https://cczheng.blog.csdn.net/article/details/105069506)