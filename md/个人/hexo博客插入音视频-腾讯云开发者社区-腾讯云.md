> iframe、aplayer、dplayer

## aplayer本地音频

![](https://ask.qcloudimg.com/raw/yehe-b343db5317ff8/6vm09rbsz6.png)

1.博客中如果要插入本地音频，需要先安装hexo-tag-aplayer，在你的cmd输入

```
npm install --save hexo-tag-aplayer
```

2.然后确保你的hexo的配置文件\_config.yml里

打开这个可以让你new新的文章时生成同名文件夹

当然如果你要使用别的路径，可以忽略这步

3.把音频文件放到同名文件夹里，然后在文章插入以下语句

```
{% aplayer "No_Time_for_Caution" "Hans_Zimmer" "No_Time_for_Caution-Hans_Zimmer-24026258.mp3" "https://img4.kuwo.cn/star/albumcover/300/50/69/4184500136.jpg" %}
```

第三个引号是文件名，是本地的，图片也可以是本地的

其他详细参数参考[https://github.com/grzhan/hexo-tag-aplayer#usage](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Fgithub.com%2Fgrzhan%2Fhexo-tag-aplayer%23usage&objectId=2199563&objectType=1&isNewArticle=undefined)

![](https://ask.qcloudimg.com/raw/yehe-b343db5317ff8/sysko5bfja.png)

```
<iframe frameborder="no" border="0" marginwidth="0" marginheight="0" width=430 height=86 src="//music.163.com/outchain/player?type=2&id=2919622&auto=0&height=66"></iframe>
```

width 宽度

height 高度

type 歌曲(1), 歌单(2), 电台(3)

id 歌曲ID号

auto 自动播放(1), 0手动播放(0)

## iframe网络视频

复制嵌入代码

![](https://developer.qcloudimg.com/http-save/yehe-5555998/691d69e7aab865c45590896bc1e2fd41.png)

https://youtu.be/m4iRwp\_FWxI

```
<iframe width="951" height="535" src="https://www.youtube.com/embed/m4iRwp_FWxI" frameborder="0" allow="accelerometer; autoplay; encrypted-media; gyroscope; picture-in-picture" allowfullscreen></iframe>
```

## dplayer本地视频

和applayer一样

1.博客中如果要插入本地视频，需要先安装hexo-tag-dplayer，在你的cmd输入

```
npm install hexo-tag-dplayer --save
```

2.然后确保你的hexo的配置文件\_config.yml里

打开这个可以让你new新的文章时生成同名文件夹

当然如果你要使用别的路径，可以忽略这步

3.把视频文件放到同名文件夹里，然后在文章插入以下语句

```
{% dplayer "url=1.mp4"  "pic=1.png" "loop=yes" "theme=#FADFA3" "autoplay=false" "token=tokendemo" %}
```

第一个引号是文件名

其他详细参数参考[https://github.com/NextMoe/hexo-tag-dplayer#usage](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Fgithub.com%2FNextMoe%2Fhexo-tag-dplayer%23usage&objectId=2199563&objectType=1&isNewArticle=undefined)

![](https://ask.qcloudimg.com/raw/yehe-b343db5317ff8/zkfshav4ji.png)

本文参与 [腾讯云自媒体同步曝光计划](https://cloud.tencent.com/developer/support-plan)，分享自作者个人站点/博客。

原始发表：2020-08-07，

如有侵权请联系 [cloudcommunity@tencent.com](mailto:cloudcommunity@tencent.com) 删除