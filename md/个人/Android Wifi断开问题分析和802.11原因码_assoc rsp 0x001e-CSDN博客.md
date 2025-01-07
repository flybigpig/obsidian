[Android](https://so.csdn.net/so/search?q=Android&spm=1001.2101.3001.7020) Wifi连接和断链分析思路。

1.密码错误导致的连接失败

2.关联被拒绝

3.热点未回复AUTH\_RSP或者[STA](https://so.csdn.net/so/search?q=STA&spm=1001.2101.3001.7020)未收到 AUTH\_RSP

4.热点未回复ASSOC\_RSP或者STA未收到ASSOC\_RSP

5.DHCP FAILURE

6.发生roaming

7.AP发送了DEAUTH帧导致断开连接

8.被AP踢出，这个原因需要sniffer log分析

下面详细介绍。

## 1.密码错误导致的连接失败

#### 1.1

其实有时候并不是用户真的输入了错误密码，有可能WIFI底层驱动存在异常。

可以查一下WIFI配置文件中保存的AP密码是否与期望值一致。

```cpp
wpa_supplicant: wlan0: WPA: 4-Way Handshake failed - pre-shared key may be incorrect

wpa_supplicant: wlan0: CTRL-EVENT-SSID-TEMP-DISABLED id=0 ssid="inhub" aut
```

#### 1.2 在4WAY\_HANDSHAKE阶段由于密码错误、丢帧或者弱信号丢包导致WRONG\_KEY：

密码错误在4WAY\_HANDSHAKE阶段中的2/4次握手会显示wrong key。如果已经连接过则显示

```cpp
I/wpa_supplicant(19043): wlan0: CTRL-EVENT-SSID-TEMP-DISABLED id=1 ssid="inhub" auth_failures=1 duration=5 reason=WRONG_KEY。
```

丢帧导致连接断开：

```cpp
wlan: [24597:E :PE ] limHandleMissedBeaconInd: 2121: Sending EXIT_BMPS_IND to SME due to Missed beacon from FW
```

信号弱导致断开：

```cpp
I/wpa_supplicant(31023): wlan0: CTRL-EVENT-DISCONNECTED bssid=c8:3a:35:2b:71:30 reason=0

E/WifiStateMachine( 821): NETWORK_DISCONNECTION_EVENT in connected state BSSID=c8:3a:35:2b:71:30 RSSI=-89 freq=2437 was debouncing=false reason=0 ajst=0
```

reason=0表示因为信号弱而断开。

## 2.关联被拒绝

#### 2.1

```cpp
wpa_supplicant: wlan0: CTRL-EVENT-ASSOC-REJECT status_code=1

wpa_supplicant: wlan0: Request to deauthenticate - bssid=00:00:00:00:00:00 pending_bssid=00:00:00:00:00:00 reason=3 state=ASSOCIATING

wpa_supplicant: wpa_driver_nl80211_disconnect(reason_code=3)
```

#### 2.2 在ASSOCIATING阶段由于丢包导致ASSOC REJECT

```cpp
Line 15551: 03-10 15:42:04.953 884 884 I wpa_supplicant: wlan0: CTRL-EVENT-DISCONNECTED bssid=f4:2a:7d:7c:65:df reason=3 locally_generated=1

Line 15552: 03-10 15:42:04.959 884 884 W wpa_supplicant: nl80211: Was expecting local disconnect but got another disconnect event first

Line 18957: 03-10 15:42:07.363 884 884 I wpa_supplicant: wlan0: Trying to associate with f4:2a:7d:7c:65:df (SSID='inhub' freq=2412 MHz)

Line 18965: 03-10 15:42:08.372 884 884 I wpa_supplicant: wlan0: CTRL-EVENT-ASSOC-REJECT bssid=f4:2a:7d:7c:65:df status_code=1

Line 18991: 03-10 15:42:08.693 884 884 I wpa_supplicant: wlan0: Trying to associate with f4:2a:7d:7c:65:df (SSID='inhub' freq=2412 MHz)
```

上面日志分析

CTRL-EVENT-DISCONNECTED bssid=f4:2a:7d:7c:65:df reason=3 ==>这是系统下断线的  
重连后有看到CTRL-EVENT-ASSOC-REJECT==>有可能跟网络有关,

若是当前环境下WiFi 比较多(2.4G环境比较糟,干扰较大，有可能某一方收发不好导致),

若是一直都出现CTRL-EVENT-ASSOC-REJECT导致连线不上，最好找WiFi 方案商帮忙协助确认一下是否是WiFi端的问题？

## 3.热点未回复AUTH\_RSP或者STA未收到 AUTH\_RSP

正常情况下，STA发送AUTH request后，会收到一个AUTH\_RSP，即正确情况下，内核LOG中会有如下两行打印，异常情况下，仅有第一行。

这种情况下，可以用其他设备接入AP，看是否存在同样的问题，则基本判断出是否为热点问题。

```cpp
4,20619,2759068545,-;AUTH - Send AUTH request seq#1 (Alg=0)...

4,20620,2759170874,-;AUTH - Receive AUTH_RSP seq#2 to me (Alg=0, Status=0)
```

## 4.热点未回复ASSOC\_RSP或者STA未收到ASSOC\_RSP

异常情况下，STA端不会有下面的第二行日志打印。

```cpp
[ 2854.218696] ASSOC - Send ASSOC request...

[ 2854.238083] PeerAssocRspAction():ASSOC - receive ASSOC_RSP to me (status=0)
```

## 5.DHCP FAILURE

四次握手成功但是获取IP地址失败：

```cpp
WifiConfigStore: message=DHCP FAILURE
```

正常情况下的流程如下：

```cpp
DhcpClient: Broadcasting DHCPDISCOVER

DhcpClient: Received packet: 10:c7:53:71:ae:7c OFFER, ip /192.168.236.146, mask /255.255.255.0, DNS servers: /192.168.236.1 , gateways [/192.168.236.1] lease time 86400, domain null

DhcpClient: Got pending lease: IP address 192.168.236.146/24 Gateway 192.168.236.1 DNS servers: [ 192.168.236.1 ] Domains DHCP server /192.168.236.1 Vendor info null lease 86400seconds

DhcpClient: Broadcasting DHCPREQUEST ciaddr=0.0.0.0 request=192.168.236.146 serverid=192.168.236.1

DhcpClient: Received packet: 10:c7:53:71:ae:7c ACK: your new IP /192.168.236.146, netmask /255.255.255.0, gateways [/192.168.236.1] DNS servers: /192.168.236.1 , lease time 86400

DhcpClient: Confirmed lease: IP address 192.168.236.146/24 Gateway 192.168.236.1 DNS servers: [ 192.168.236.1 ] Domains DHCP server /192.168.236.1 Vendor info null lease 86400 seconds
```

## 6.发生roaming

当前连接AP1信号太弱，此时又扫描到了已连接过的AP2，AP2的信号强度更好，则会断开AP1，连接AP2，这一系列动作就叫roaming。 

如下列中，断开了与10:0e:0e:20:66:15热点的连接，连接到了10:0e:0e:20:5e:6d热点。

```cpp
wpa_supplicant: nl80211: Associated on 2422 MHz

wpa_supplicant: nl80211: Associated with 10:0e:0e:20:66:15

wpa_supplicant: nl80211: Drv Event 47 (NL80211_CMD_ROAM) received for wlan0

wpa_supplicant: nl80211: Roam event

wpa_supplicant: nl80211: Associated on 2472 MHz

wpa_supplicant: nl80211: Associated with 10:0e:0e:20:5e:6d
```

日志2

```cpp
I/WifiHAL (28360): event received NL80211_CMD_ROAM, vendor_id = 0x0

I/wpa_supplicant(28064): wlan0: CTRL-EVENT-CONNECTED - Connection to c4:14:3c:29:47:25 completed [id=0 id_str=]

I/WifiHAL (28360): event received NL80211_CMD_ROAM, vendor_id = 0x0

I/wpa_supplicant(28064): wlan0: CTRL-EVENT-CONNECTED - Connection to c4:14:3c:29:47:05 completed [id=0 id_str=]

I/WifiHAL (28360): event received NL80211_CMD_ROAM, vendor_id = 0x0

I/wpa_supplicant(28064): wlan0: CTRL-EVENT-CONNECTED - Connection to 1c:1d:86:e9:e2:85 completed [id=0 id_str=]
```

## 7.AP发送了DEAUTH帧导致断开连接

此时可以连接其他AP测试一下，进而判断是否是刚才这个AP的问题

```cpp
[ 1555.321037] (1)[3319:tx_thread][wlan] Rx Deauth frame from BSSID=[aa:63:df:4c:db:c2]

[ 1555.321093] (1)[3319:tx_thread][wlan] Reason code = 7
```

## 8.被AP踢出，这个原因需要sniffer log分析

reason=2，reason=7，reason=15代表被AP踢出，在kernel log中可以找到对应的deauth信息。

```cpp
I/wpa_supplicant(28064): wlan0: CTRL-EVENT-DISCONNECTED bssid=c4:14:3c:29:47:05 reason=7

wlan: [28055:E :PE ] limProcessDeauthFrame: 144: Received Deauth frame for Addr: 44:a4:2d:52:bc:a5 (mlm state = eLIM_MLM_LINK_ESTABLISHED_STATE, sme state = 12 systemrole = 3) with reason code 7 from c4:14:3c:29:47:05

I/wpa_supplicant(28064): wlan0: CTRL-EVENT-DISCONNECTED bssid=1c:1d:86:e9:e2:85 reason=15

wlan: [28055:E :PE ] limProcessDeauthFrame: 144: Received Deauth frame for Addr: 44:a4:2d:52:bc:a5 (mlm state = eLIM_MLM_LINK_ESTABLISHED_STATE, sme state = 12 systemrole = 3) with reason code 15 from 1c:1d:86:e9:e2:85

I/wpa_supplicant(28064): wlan0: CTRL-EVENT-DISCONNECTED bssid=c4:14:3c:29:47:25 reason=2

wlan: [28055:E :PE ] limProcessDeauthFrame: 144: Received Deauth frame for Addr: 44:a4:2d:52:bc:a5 (mlm state = eLIM_MLM_LINK_ESTABLISHED_STATE, sme state = 12 systemrole = 3) with reason code 2 from c4:14:3c:29:47:25
```

其他情况后续补充。

## 附录：

#### 802.11 Association Status, 802.11 Deauth Reason codes

[802.11 Association Status, 802.11 Deauth Reason codes - Cisco Community](https://community.cisco.com/t5/wireless-mobility-knowledge-base/802-11-association-status-802-11-deauth-reason-codes/ta-p/3148055 "802.11 Association Status, 802.11 Deauth Reason codes - Cisco Community")

802.11 Association Status Codes

<table><tbody><tr><td><p><strong>Code</strong></p></td><td><p><strong>802.11 definition</strong></p></td><td><p><strong>Explanation</strong></p></td></tr><tr><td><p>0</p></td><td><p>Successful</p></td><td></td></tr><tr><td><p>1</p></td><td><p>Unspecified failure</p></td><td><p>For example : when there is no ssid specified in an association request</p></td></tr><tr><td><p>10</p></td><td><p>Cannot support all requested capabilities in the Capability Information field</p></td><td><p>Example Test: Reject when privacy bit is set for WLAN not requiring security</p></td></tr><tr><td><p>11</p></td><td><p>Reassociation denied due to inability to confirm that association exists</p></td><td><p>NOT SUPPORTED</p></td></tr><tr><td><p>12</p></td><td><p>Association denied due to reason outside the scope of this standard</p></td><td><p>Example : When controller receives assoc from an unknown or disabled SSID</p></td></tr><tr><td><p>13</p></td><td><p>Responding station does not support the specified authentication algorithm</p></td><td><p>For example, MFP is disabled but was requested by the client.</p></td></tr><tr><td><p>14</p></td><td><p>Received an Authentication frame with authentication transaction sequence number<br>out of expected sequence</p></td><td><p>If the authentication sequence number is not correct.</p></td></tr><tr><td><p>15</p></td><td><p>Authentication rejected because of challenge failure</p></td><td></td></tr><tr><td><p>16</p></td><td><p>Authentication rejected due to timeout waiting for next frame in sequence</p></td><td></td></tr><tr><td><p>17</p></td><td><p>Association denied because AP is unable to handle additional associated stations</p></td><td><p>Will happen if you run out of AIDs on the AP; so try associating a large number of stations.</p></td></tr><tr><td><p>18</p></td><td><p>Association denied due to requesting station not supporting all of the data rates in the<br>BSSBasicRateSet parameter</p></td><td><p>Will happen if the rates in the assoc request are not in the BasicRateSet in the beacon.</p></td></tr><tr><td><p>19</p></td><td><p>Association denied due to requesting station not supporting the short preamble<br>option</p></td><td><p>NOT SUPPORTED</p></td></tr><tr><td><p>20</p></td><td><p>Association denied due to requesting station not supporting the PBCC modulation<br>option</p></td><td><p>NOT SUPPORTED</p></td></tr><tr><td><p>21</p></td><td><p>Association denied due to requesting station not supporting the Channel Agility<br>option</p></td><td><p>NOT SUPPORTED</p></td></tr><tr><td><p>22</p></td><td><p>Association request rejected because Spectrum Management capability is required</p></td><td><p>NOT SUPPORTED</p></td></tr><tr><td><p>23</p></td><td><p>Association request rejected because the information in the Power Capability<br><span data-tit="element" data-pretit="element">element</span> is unacceptable</p></td><td><p>NOT SUPPORTED</p></td></tr><tr><td><p>24</p></td><td><p>Association request rejected because the information in the Supported Channels<br>element is unacceptable</p></td><td><p>NOT SUPPORTED</p></td></tr><tr><td><p>25</p></td><td><p>Association denied due to requesting station not supporting the Short Slot Time<br>option</p></td><td><p>NOT SUPPORTED</p></td></tr><tr><td><p>26</p></td><td><p>Association denied due to requesting station not supporting the DSSS-OFDM option</p></td><td><p>NOT SUPPORTED</p></td></tr><tr><td><p>27-31</p></td><td><p>Reserved</p></td><td><p>NOT SUPPORTED</p></td></tr><tr><td><p>32</p></td><td><p>Unspecified, QoS-related failure</p></td><td><p>NOT SUPPORTED</p></td></tr><tr><td><p>33</p></td><td><p>Association denied because QAP has insufficient bandwidth to handle another<br>QSTA</p></td><td><p>NOT SUPPORTED</p></td></tr><tr><td><p>34</p></td><td><p>Association denied due to excessive frame loss rates and/or poor conditions on current<br>operating channel</p></td><td><p>NOT SUPPORTED</p></td></tr><tr><td><p>35</p></td><td><p>Association (with QBSS) denied because the requesting STA does not support the<br>QoS facility</p></td><td><p>If the WMM is required by the WLAN and the client is not capable of it, the association will get rejected.</p></td></tr><tr><td><p>36</p></td><td><p>Reserved in 802.11</p></td><td><p>This is used in our code ! There is no blackbox test for this status code.</p></td></tr><tr><td><p>37</p></td><td><p>The request has been declined</p></td><td><p>This is not used in assoc response; ignore</p></td></tr><tr><td><p>38</p></td><td><p>The request has not been successful as one or more parameters have invalid values</p></td><td><p>NOT SUPPORTED</p></td></tr><tr><td><p>39</p></td><td><p>The TS has not been created because the request cannot be honored; however, a suggested<br>TSPEC is provided so that the initiating QSTA may attempt to set another TS<br>with the suggested changes to the TSPEC</p></td><td><p>NOT SUPPORTED</p></td></tr><tr><td><p>40</p></td><td><p>Invalid information element, i.e., an information element defined in this standard for<br>which the content does not meet the specifications in Clause 7</p></td><td><p>Sent when Aironet IE is not present for a CKIP WLAN</p></td></tr><tr><td><p>41</p></td><td><p>Invalid group cipher</p></td><td><p>Used when received unsupported Multicast 802.11i OUI Code</p></td></tr><tr><td><p>42</p></td><td><p>Invalid pairwise cipher</p></td><td></td></tr><tr><td><p>43</p></td><td><p>Invalid AKMP</p></td><td></td></tr><tr><td><p>44</p></td><td><p>Unsupported RSN information element version</p></td><td><p>If you put anything but version value of 1, you will see this code.</p></td></tr><tr><td><p>45</p></td><td><p>Invalid RSN information element capabilities</p></td><td><p>If WPA/RSN IE is malformed, such as incorrect length etc, you will see this code.</p></td></tr><tr><td><p>46</p></td><td><p>Cipher suite rejected because of security policy</p></td><td><p>NOT SUPPORTED</p></td></tr><tr><td><p>47</p></td><td><p>The TS has not been created; however, the HC may be capable of creating a TS, in<br>response to a request, after the time indicated in the TS Delay element</p></td><td><p>NOT SUPPORTED</p></td></tr><tr><td><p>48</p></td><td><p>Direct link is not allowed in the BSS by policy</p></td><td><p>NOT SUPPORTED</p></td></tr><tr><td><p>49</p></td><td><p>Destination STA is not present within this QBSS</p></td><td><p>NOT SUPPORTED</p></td></tr><tr><td><p>50</p></td><td><p>The Destination STA is not a QSTA</p></td><td><p>NOT SUPPORTED</p></td></tr><tr><td><p>51</p></td><td><p>Association denied because the ListenInterval is too large</p></td><td><p>NOT SUPPORTED</p></td></tr><tr><td><p>200<br>(0xC8)</p></td><td><p>Unspecified, QoS-related failure.<br>Not defined in IEEE, defined in CCXv4</p></td><td><p>Unspecified QoS Failure. This will happen if the Assoc request contains more than one TSPEC for the same AC.</p></td></tr><tr><td><p>201<br>(0xC9)</p></td><td><p>TSPEC request refused due to AP’s policy configuration (e.g., AP is configured to deny all TSPEC requests on this SSID). A TSPEC will not be suggested by the AP for this reason code.<br>Not defined in IEEE, defined in CCXv4</p></td><td><p>This will happen if a TSPEC comes to a WLAN which has lower priority than the WLAN priority settings. For example a Voice TSPEC coming to a Silver WLAN. Only applies to CCXv4 clients.</p></td></tr><tr><td><p>202<br>(0xCA)</p></td><td><p>Association Denied due to AP having insufficient bandwidth to handle a new TS. This cause code will be useful while roaming only.<br>Not defined in IEEE, defined in CCXv4</p></td><td></td></tr><tr><td><p>203<br>(0xCB)</p></td><td><p>Invalid Parameters. The request has not been successful as one or more TSPEC parameters in the request have invalid values. A TSPEC SHALL be present in the response as a suggestion.</p><p>Not defined in IEEE, defined in CCXv4</p></td><td><p>This happens in cases such as PHY rate mismatch. If the TSRS IE contains a phy rate not supported by the controller, for example. Other examples include sending a TSPEC with bad parameters, such as sending a date rate of 85K for a narrowband TSPEC.</p></td></tr></tbody></table>

802.11 Deauth Reason Codes

<table><tbody><tr><td>Code</td><td>802.11 definition</td><td>Explanation</td></tr><tr><td>0</td><td>Reserved</td><td>NOT SUPPORTED</td></tr><tr><td>1</td><td>Unspecified reason</td><td>TBD</td></tr><tr><td>2</td><td>Previous authentication no longer valid</td><td>NOT SUPPORTED</td></tr><tr><td>3</td><td>station is leaving (or has left) IBSS or ESS</td><td>NOT SUPPORTED</td></tr><tr><td>4</td><td>Disassociated due to inactivity</td><td>Do not send any data after association;</td></tr><tr><td>5</td><td>Disassociated because AP is unable to handle all currently associated stations</td><td>TBD</td></tr><tr><td>6</td><td>Class 2 frame received from nonauthenticated station</td><td>NOT SUPPORTED</td></tr><tr><td>7</td><td>Class 3 frame received from nonassociated station</td><td>NOT SUPPORTED</td></tr><tr><td>8</td><td>Disassociated because sending station is leaving (or has left) BSS</td><td>TBD</td></tr><tr><td>9</td><td>Station requesting (re)association is not authenticated with responding station</td><td>NOT SUPPORTED</td></tr><tr><td>10</td><td>Disassociated because the information in the Power Capability element is unacceptable</td><td>NOT SUPPORTED</td></tr><tr><td>11</td><td>Disassociated because the information in the Supported Channels element is unacceptable</td><td>NOT SUPPORTED</td></tr><tr><td>12</td><td>Reserved</td><td>NOT SUPPORTED</td></tr><tr><td>13</td><td>Invalid information element, i.e., an information element defined in this standard for<br>which the content does not meet the specifications in Clause 7</td><td>NOT SUPPORTED</td></tr><tr><td>14</td><td>Message integrity code (MIC) failure</td><td>NOT SUPPORTED</td></tr><tr><td>15</td><td>4-Way Handshake timeout</td><td>NOT SUPPORTED</td></tr><tr><td>16</td><td>Group Key Handshake timeout</td><td>NOT SUPPORTED</td></tr><tr><td>17</td><td>Information element in 4-Way Handshake different from (Re)Association Request/Probe<br>Response/Beacon frame</td><td>NOT SUPPORTED</td></tr><tr><td>18</td><td>Invalid group cipher</td><td>NOT SUPPORTED</td></tr><tr><td>19</td><td>Invalid pairwise cipher</td><td>NOT SUPPORTED</td></tr><tr><td>20</td><td>Invalid AKMP</td><td>NOT SUPPORTED</td></tr><tr><td>21</td><td>Unsupported RSN information element version</td><td>NOT SUPPORTED</td></tr><tr><td>22</td><td>Invalid RSN information element capabilities</td><td>NOT SUPPORTED</td></tr><tr><td>23</td><td>IEEE 802.1X authentication failed</td><td>NOT SUPPORTED</td></tr><tr><td>24</td><td>Cipher suite rejected because of the security policy</td><td>NOT SUPPORTED</td></tr><tr><td>25-31</td><td>Reserved</td><td>NOT SUPPORTED</td></tr><tr><td>32</td><td>Disassociated for unspecified, QoS-related reason</td><td>NOT SUPPORTED</td></tr><tr><td>33</td><td>Disassociated because QAP lacks sufficient bandwidth for this QSTA</td><td>NOT SUPPORTED</td></tr><tr><td>34</td><td>Disassociated because excessive number of frames need to be acknowledged, but are not<br>acknowledged due to AP transmissions and/or poor channel conditions</td><td>NOT SUPPORTED</td></tr><tr><td>35</td><td>Disassociated because QSTA is transmitting outside the limits of its TXOPs</td><td>NOT SUPPORTED</td></tr><tr><td>36</td><td>Requested from peer QSTA as the QSTA is leaving the QBSS (or resetting)</td><td>NOT SUPPORTED</td></tr><tr><td>37</td><td>Requested from peer QSTA as it does not want to use the mechanism</td><td>NOT SUPPORTED</td></tr><tr><td>38</td><td>Requested from peer QSTA as the QSTA received frames using the mechanism for which<br>a setup is required</td><td>NOT SUPPORTED</td></tr><tr><td>39</td><td>Requested from peer QSTA due to timeout</td><td>NOT SUPPORTED</td></tr><tr><td>40</td><td>Peer QSTA does not support the requested cipher suite</td><td>NOT SUPPORTED</td></tr><tr><td>46-65535</td><td>46--65 535 Reserved</td><td>NOT SUPPORTED</td></tr><tr><td>98</td><td>Cisco defined</td><td>TBD</td></tr><tr><td>99</td><td>Cisco defined<br>Used when the reason code sent in a deassoc req or deauth by the client is invalid – invalid length, invalid value etc</td><td>Example: Send a Deauth to the AP with the reason code to be invalid, say zero</td></tr></tbody></table>

MTK 断开错误码定义在

vendor/mediatek/kernel\_modules/connectivity/wlan/core/gen4-mt7668/include/nic/mac.h:662:#define REASON\_CODE\_BEACON\_TIMEOUT             100  /\* for beacon timeout, defined by mediatek \*/