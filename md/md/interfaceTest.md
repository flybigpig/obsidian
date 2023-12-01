接口尝试



```
 onWindowClickListener = new onWindowClickListener() {
            @Override
            public void onWindowClick(@Nullable View v, int type, ResponseResultPositionDto.BranchInfoList info) {
                Log.d("infooo", type + " " + info.getBranchAddress() + " " + info.getBranchName() + " ");
                postStationDialog = new PostStationDialog(LocationCenterActivity.this, info, new PostStationDialog.OnItemClickListener() {
                    @Override
                    public void onNaviClick(View v) {
                        ArrayList<String> mMaponeList = new ArrayList<>();
                        mMaponeList.add("百度地图");
                        mMaponeList.add("高德地图");
                        SelectListDialog selectListDialog = new SelectListDialog(LocationCenterActivity.this, mMaponeList, new SelectListDialog.OnItemClickListener() {

                            @Override
                            public void onCancelClick(View v, int position) {

                            }

                            @Override
                            public void onOkClick(View v, int position) {
                                try {
                                    if (position == 0) {
                                        NaviUtils.goBaiduMap(LocationCenterActivity.this, info.getLocation().getLat(), info.getLocation().getLon(), info.getBranchName());
                                    } else {
                                        NaviUtils.goGaodeMap(LocationCenterActivity.this, info.getLocation().getLat(), info.getLocation().getLon(), info.getBranchName());
                                    }
                                } catch (Exception e) {
                                    e.printStackTrace();
                                }

                            }
                        });
                        selectListDialog.show();
                    }

                    @Override
                    public void onPhone(View v) {


                        TextView tvPhone = (TextView) v;
                        phone = tvPhone.getText().toString();
                        Log.d("phoneeee", phone);
                        startPhone(phone);

                    }
                });
                postStationDialog.show();
            }
        };
```



```
  interface onWindowClickListener {

        void onWindowClick(@Nullable View v, int type, ResponseResultPositionDto.BranchInfoList info);
    }
```

