## 一、swiper安装使用

 1、安装，注意标明版本

```
npm install swiper@3 --save-dev
```

 

2、main.js引入

[![img](https://app.yinxiang.com/shard/s28/nl/23628363/b47595fd-b94d-4dff-bccc-9706339df960/res/9b53860e-c059-491a-97ba-dec5c497e2ac/copycode.gif?resizeSmall&width=832)](https://app.yinxiang.com/shard/s28/nl/23628363/b47595fd-b94d-4dff-bccc-9706339df960?content=#)

```
import Vue from 'vue'
import App from './App.vue'
import router from './router'
import store from './store'

import 'swiper/dist/css/swiper.min.css'
import 'swiper/dist/js/swiper.min'

Vue.config.productionTip = false

new Vue({
  router,
  store,
  render: h => h(App)
}).$mount('#app')
```

[![img](https://app.yinxiang.com/shard/s28/nl/23628363/b47595fd-b94d-4dff-bccc-9706339df960/res/9b53860e-c059-491a-97ba-dec5c497e2ac/copycode.gif?resizeSmall&width=832)](https://app.yinxiang.com/shard/s28/nl/23628363/b47595fd-b94d-4dff-bccc-9706339df960?content=#)

 

3、使用地方代码

[![img](https://app.yinxiang.com/shard/s28/nl/23628363/b47595fd-b94d-4dff-bccc-9706339df960/res/9b53860e-c059-491a-97ba-dec5c497e2ac/copycode.gif?resizeSmall&width=832)](https://app.yinxiang.com/shard/s28/nl/23628363/b47595fd-b94d-4dff-bccc-9706339df960?content=#)

```
<template>
  <div class="about">
   <div class="swiper-container">
    <div class="swiper-wrapper">
      <div class="swiper-slide">Slide 1</div>
      <div class="swiper-slide">Slide 2</div>
      <div class="swiper-slide">Slide 3</div>
    </div>
    <!-- 如果需要分页器 -->
    <div class="swiper-pagination"></div>

    <!-- 如果需要导航按钮 -->
    <div class="swiper-button-prev"></div>
    <div class="swiper-button-next"></div>

    <!-- 如果需要滚动条 -->
<!--    <div class="swiper-scrollbar"></div>-->
  </div>
  </div>
</template>

<script>
  import Swiper from 'swiper'
  export default {
    name:'About',
    data(){
      return {
      }
    },
    mounted() {
      new Swiper ('.swiper-container', {
        loop: true,
        // 如果需要分页器
        pagination: '.swiper-pagination',
        // 如果需要前进后退按钮
        nextButton: '.swiper-button-next',
        prevButton: '.swiper-button-prev',
        // 如果需要滚动条
        // scrollbar: '.swiper-scrollbar',
        //如果需要自动切换海报
        // autoplay: {
        //   delay: 1000,//时间 毫秒
        //   disableOnInteraction: false,//用户操作之后是否停止自动轮播默认true
        // },
      })
    }
  }
</script>

<style lang="less" scoped>
.swiper-container{
  height: 500px;
  width: 100%;
  .swiper-wrapper{
    .swiper-slide{
      width: 100%;
      height: 100%;
      background-color: #42b983;
      text-align: center;
      line-height: 500px;
    }
  }
}
</style>
```

[![img](https://app.yinxiang.com/shard/s28/nl/23628363/b47595fd-b94d-4dff-bccc-9706339df960/res/9b53860e-c059-491a-97ba-dec5c497e2ac/copycode.gif?resizeSmall&width=832)](https://app.yinxiang.com/shard/s28/nl/23628363/b47595fd-b94d-4dff-bccc-9706339df960?content=#)

 

4、完成效果



![img](https://app.yinxiang.com/shard/s28/nl/23628363/b47595fd-b94d-4dff-bccc-9706339df960/res/04b9cc87-698a-4c2a-9843-3e249f699c28/2081619-20200825171037034-2141031618.png?resizeSmall&width=832)



 

 

## 二、vue-awesome-swiper安装使用

1、安装

```
//需要指定版本
npm install vue-awesome-swiper@3 --save-dev

npm install swiper@3 --save-dev
```

2、局部使用

[![img](https://app.yinxiang.com/shard/s28/nl/23628363/b47595fd-b94d-4dff-bccc-9706339df960/res/9b53860e-c059-491a-97ba-dec5c497e2ac/copycode.gif?resizeSmall&width=832)](https://app.yinxiang.com/shard/s28/nl/23628363/b47595fd-b94d-4dff-bccc-9706339df960?content=#)

```
<template>
  <div class="recommendPage">
    <swiper :options="swiperOption" ref="mySwiper">
      <swiper-slide>I'm Slide 1</swiper-slide>
      <swiper-slide>I'm Slide 2</swiper-slide>
      <swiper-slide>I'm Slide 3</swiper-slide>
      <swiper-slide>I'm Slide 4</swiper-slide>
      <div class="swiper-pagination" slot="pagination"></div>
      <div class="swiper-button-prev" slot="button-prev"></div>
      <div class="swiper-button-next" slot="button-next"></div>
    </swiper>
  </div>
</template>

<script>
// 引入插件
import { swiper, swiperSlide } from "vue-awesome-swiper";
import "swiper/dist/css/swiper.css";

export default {
  name: 'Home',
  components: {
    swiper,
    swiperSlide
  },
  data() {
    return {
      swiperOption: {
        loop: true,
        autoplay: {
          delay: 3000,
          stopOnLastSlide: false,
          disableOnInteraction: false
        },
        // 显示分页
        pagination: {
          el: ".swiper-pagination",
          clickable: true //允许分页点击跳转
        },
        // 设置点击箭头
        navigation: {
          nextEl: ".swiper-button-next",
          prevEl: ".swiper-button-prev"
        }
      }
    };
  },
  computed: {
    swiper() {
      return this.$refs.mySwiper.swiper;
    }
  },
  mounted() {
    // current swiper instance
    // 然后你就可以使用当前上下文内的swiper对象去做你想做的事了
    console.log("this is current swiper instance object", this.swiper);
    // this.swiper.slideTo(3, 1000, false);
  }
}
</script>

<style scoped>
  .recommendPage .swiper-container{
    position: relative;
    width: 100%;
    height: 200px;
    background: pink;
  }
  .recommendPage .swiper-container .swiper-slide{
    width: 100%;
    line-height: 200px;
    background: yellowgreen;
    color: #000;
    font-size: 16px;
    text-align: center;
  }
</style>
```

[![img](https://app.yinxiang.com/shard/s28/nl/23628363/b47595fd-b94d-4dff-bccc-9706339df960/res/9b53860e-c059-491a-97ba-dec5c497e2ac/copycode.gif?resizeSmall&width=832)](https://app.yinxiang.com/shard/s28/nl/23628363/b47595fd-b94d-4dff-bccc-9706339df960?content=#)

3、完成后效果



![img](https://app.yinxiang.com/shard/s28/nl/23628363/b47595fd-b94d-4dff-bccc-9706339df960/res/90da69f9-9685-40d3-a19b-ec94851594a7/2081619-20200825163851314-1949648013.png?resizeSmall&width=832)



 

## 三、[swiper官网地址](https://app.yinxiang.com/OutboundRedirect.action?dest=https%3A%2F%2Fwww.swiper.com.cn%2F)