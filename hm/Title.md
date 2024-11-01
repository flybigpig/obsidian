```
import { isEmptyString } from '../http/HttpRequest'  
  
@Builder  
export function TitleBar(params: Tmp) {  
  if (params.rightIcon != null || params.rightText != null) {  
    Row() {  
  
     Row(){  
       if (params.leftIcon) {  
         Image($r('app.media.icon_back'))  
           .width('27p')  
           .height('27vp').margin({ left: 15 })  
           .onClick(() =>  
           params.leftClick == null ? console.log(params.title + " rightClick") : params.leftClick()  
           )       }  
  
       if (params.leftText != null && !isEmptyString(params.leftText)) {  
         Text(params.rightText)  
           .width('27p')  
           .height('27vp')  
           .fontColor("#FFFFFFF")  
           .margin({ left: 15 })  
           .margin({ right: 5 })  
           .onClick(() =>  
           params.leftClick == null ? console.log(params.title + " rightClick") : params.leftClick()  
           )       }  
     }.layoutWeight(1)  
  
      Text(params.title)  
        .fontWeight(FontWeight.Medium)  
        .fontSize($r('app.float.title_text_size'))  
        .fontColor($r('app.color.white')).textAlign(TextAlign.Center).layoutWeight(1)  
      Row(){  
        if (params.rightIcon != null) {  
          Image($r('app.media.icon_back'))  
            .width('27p')  
            .height('27vp').margin({ right: 15 })  
            .onClick(() =>  
            params.rightClick == null ? console.log(params.title + " rightClick") : params.rightClick()  
            )        }  
  
        if (params.rightText != null && !isEmptyString(params.rightText)) {  
          Text(params.rightText)  
            .width('27p')  
            .height('27vp')  
            .fontColor("#FFFFFFF")  
            .margin({ left: 15 })  
            .margin({ right: 5 })  
            .onClick(() =>  
            params.rightClick == null ? console.log(params.title + " rightClick") : params.rightClick()  
            )        }  
      }.layoutWeight(1).justifyContent(FlexAlign.End)  
  
  
    }  
    // .alignItems(VerticalAlign.Center)  
    .width('100%')  
    .backgroundColor($r('app.color.primary'))  
    .height('45')  
    // .justifyContent(FlexAlign.SpaceBetween)  
  
  } else if (params.leftIcon || params.rightText != null) {  
  
    Row() {  
      if (params.leftIcon) {  
        Image($r('app.media.icon_back'))  
          .width('27p')  
          .height('27vp').margin({ left: 10 })  
          .onClick(() =>  
          params.leftClick == null ? console.log(params.title + " rightClick") : params.leftClick()  
          )      }  
  
      if (params.leftText != null && !isEmptyString(params.leftText)) {  
        Text(params.rightText)  
          .width('27p')  
          .height('27vp')  
          .fontColor("#FFFFFFF")  
          .margin({ left: 10 })  
          .margin({ right: 5 })  
          .onClick(() =>  
          params.leftClick == null ? console.log(params.title + " rightClick") : params.leftClick()  
          )      }  
  
      Text(params.title)  
        .fontWeight(FontWeight.Medium)  
        .fontSize($r('app.float.title_text_size'))  
        .layoutWeight(1)  
        .textAlign(TextAlign.Center)  
        .fontColor($r('app.color.white'))  
  
      Image($r('app.media.icon_add'))  
        .height('20vp')  
        .width('20vp')  
        .margin({ right: '10vp' })  
        .visibility(Visibility.Hidden)  
        .align(Alignment.Start)  
        .onClick(() => {  
        })  
  
    }.width('100%')  
    .backgroundColor($r('app.color.primary'))  
    .justifyContent(FlexAlign.SpaceBetween)  
    .height('50vp')  
  }  
  else {  
    Row() {  
  
  
      Text(params.title)  
        .fontWeight(FontWeight.Medium)  
        .fontSize($r('app.float.title_text_size'))  
        .layoutWeight(1)  
        .textAlign(TextAlign.Center)  
        .fontColor($r('app.color.white'))  
  
    }.width('100%')  
    .backgroundColor($r('app.color.primary'))  
    .height('50vp')  
  }  
}  
  
class Tmp {  
  leftIcon ?: boolean = true //  :isVisiable  
  leftText ?: string = ''  
  title: string = ""  
  rightIcon ?: Resource | object = $r('app.media.icon_add')  
  rightText ?: string = ''  
  leftClick ? = () => {  
  }  rightClick ? = () => {  
  }}
```