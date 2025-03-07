
CMakeLists.txt文件中有定义：

  

```
# 有关将 CMake 与 Android Studio 配合使用的更多信息，请阅读文档：  
# https://d.android.com/studio/projects/add-native-code.html  
  
# 设置生成本地库所需的最低 CMake 版本。  
  
cmake_minimum_required(VERSION 3.22.1)  
  
# 声明并命名项目。  
  
project("as_jni_project")  
  
# 创建并命名库，将其设置为 STATIC 或 SHARED，并提供其源代码的相对路径。  
# 您可以定义多个库，CMake 会为您构建它们。  
# Gradle 会自动将共享库与您的 APK 打包。  
  
add_library(  
# 设置库的名称。  
as_jni_project  
  
# 将库设置为共享库。  
SHARED  
  
# 提供源文件的相对路径。  
native-lib.cpp)  
  
# 搜索指定的预生成库并将路径存储为变量。  
# 由于 CMake 默认在搜索路径中包含系统库，因此您只需指定要添加的公有 NDK 库的名称。  
# CMake 会在完成构建之前验证库是否存在。  
  
find_library( # Sets the name of the path variable.  
log-lib  
  
# Specifies the name of the NDK library that  
# you want CMake to locate.  
log)  
  
# 指定 CMake 应链接到目标库的库。  
# 可以链接多个库，例如在此生成脚本中定义的库、预生成的第三方库或系统库。  
  
target_link_libraries( # Specifies the target library.  
as_jni_project  
  
# Links the target library to the log library  
# included in the NDK.  
${log-lib})
```

  

可以从CMakeLists.txt文件中的定义看出，将native-lib.cpp文件设置为共享库，库的名称为as_jni_project。所以我们在指定加载模块名称时需要设置为as_jni_project。