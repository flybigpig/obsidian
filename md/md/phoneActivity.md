跳转拨打电话界面



```
private void startPhone(String phone) {

        if (TextUtils.isEmpty(phone)) {
            ToastUtil.show("电话不能为空", Toast.LENGTH_SHORT);
            return;
        }
        if (Build.VERSION.SDK_INT >= 23) {

            int checkCallPhonePermission = ContextCompat.checkSelfPermission(LocationCenterActivity.this, Manifest.permission.CALL_PHONE);
            if (checkCallPhonePermission != PackageManager.PERMISSION_GRANTED) {
                ActivityCompat.requestPermissions(LocationCenterActivity.this, new String[]{Manifest.permission.CALL_PHONE}, REQUEST_CODE_ASK_CALL_PHONE);
                return;
            } else {
                Intent intent = new Intent(Intent.ACTION_DIAL, Uri.parse("tel:" + phone));
                intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                startActivity(intent);
            }
        } else {

            Intent intent = new Intent(Intent.ACTION_DIAL, Uri.parse("tel:" + phone));
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(intent);
        }
        phone = "";

    }
```

