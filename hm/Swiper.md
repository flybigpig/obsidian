```
@Observed  
export default class HomeMenuData {  
  menuList: Array<HomeMenuItem> = [];  
  // constructor(ownMenuList: HomeMenuItem[]) {  
  //   this.ownMenuList = new ObservedArray<HomeMenuItem>(ownMenuList);  // }}  
  
@Observed  
export class MyDataSource implements IDataSource {  
  private list: Array<Array<HomeMenuItem>>  
  
  constructor(list: Array<Array<HomeMenuItem>>) {  
    this.list = list  
  }  
  
  totalCount(): number {  
    return this.list.length  
  }  
  
  getData(index: number): Array<HomeMenuItem> {  
    return this.list[index]  
  }  
  
  registerDataChangeListener(listener: DataChangeListener): void {  
  }  
  unregisterDataChangeListener() {  
  }}
```

```
if (this.homeMenuDataMode.viewPageData.length > 0) {  
  Column() {  
    Swiper(this.swiperController) {  
      ForEach(this.homeMenuDataMode.viewPageData, (item: HomeMenuItem[], index: number) => {  
        CenterGridView({ homeDataList: item })  
      }, (item: HomeMenuItem[], index: number) => index.toString())  
    }  
  }  .border({ radius: 5 })  
  .margin({ top: 10 })  
  .width("95%")  
  .height("35%")  
}
```