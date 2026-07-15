Page({
  data: { 
    statusText: '未连接', 
    isConnected: false, 
    deviceId: '', 
    serviceId: '', 
    writeCharId: '',   // 用于往单片机写数据（发时间、发暗号）
    receiveBuffer: '', // 拼接收到的碎数据包
    rankList: []       // 排行榜数组
  },

  onConnected: function(deviceId) {
    this.setData({ deviceId: deviceId, statusText: '正在扫描通道...' });
    wx.getBLEDeviceServices({
      deviceId: deviceId,
      success: (res) => {
        for (let s of res.services) {
          if (s.uuid.includes('FFE0') || s.uuid.includes('ffe0')) {
            wx.getBLEDeviceCharacteristics({
              deviceId: deviceId,
              serviceId: s.uuid,
              success: (cRes) => {
                for (let c of cRes.characteristics) {
                  // 1. 寻找写入通道
                  if (c.properties.write || c.properties.writeNoResponse) {
                    this.setData({ serviceId: s.uuid, writeCharId: c.uuid });
                  }
                  // 2. 寻找监听通道，打开单片机发往手机的数据流
                  if (c.properties.notify || c.properties.indicate) {
                    wx.notifyBLECharacteristicValueChange({
                      deviceId: deviceId,
                      serviceId: s.uuid,
                      characteristicId: c.uuid,
                      state: true,
                      success: () => {
                        this.setData({ isConnected: true, statusText: '✅ 连接并监听成功' });
                        // 开启数据监听
                        wx.onBLECharacteristicValueChange((res) => {
                          let str = String.fromCharCode.apply(null, new Uint8Array(res.value));
                          this.handleIncomingData(str);
                        });
                      }
                    });
                  }
                }
              }
            });
          }
        }
      }
    });
  },

  searchBle: function() {
    this.setData({ statusText: '搜索蓝牙中...' });
    wx.openBluetoothAdapter({
      success: () => {
        wx.startBluetoothDevicesDiscovery({
          success: () => {
            wx.onBluetoothDeviceFound((res) => {
              let d = res.devices[0];
              // 匹配 DX-BT24
              if (d.name && (d.name.includes('BT24') || d.name.includes('DX'))) {
                wx.stopBluetoothDevicesDiscovery();
                this.setData({ statusText: '找到设备，正在连接...' });
                wx.createBLEConnection({
                  deviceId: d.deviceId,
                  success: () => { this.onConnected(d.deviceId); }
                });
              }
            });
          }
        });
      }
    });
  },

  // === 核心数据解析工厂 ===
  handleIncomingData: function(str) {
    console.log("👉 蓝牙收到原始数据包：", str); // 抓包用
    this.data.receiveBuffer += str;
    
    if (this.data.receiveBuffer.includes('\n')) {
      let msgs = this.data.receiveBuffer.split('\n');
      for(let i = 0; i < msgs.length - 1; i++) {
        let msg = msgs[i].trim();
        let match = msg.match(/USER:(\d+),TOTAL:(\d+)/);
        if (match) {
          let id = parseInt(match[1]);
          let totalSeconds = parseInt(match[2]);
          
          let timeText = "";
          
          // 🔴 修复了这里的显示逻辑，把秒数精确显示出来！
          if (totalSeconds < 60) {
            timeText = totalSeconds + "秒";
          } 
          else if (totalSeconds < 3600) {
            let m = Math.floor(totalSeconds / 60);
            let s = totalSeconds % 60;
            timeText = m + "分钟 " + s + "秒";
          } 
          else {
            let h = Math.floor(totalSeconds / 3600);
            let m = Math.floor((totalSeconds % 3600) / 60);
            timeText = h + "小时 " + m + "分";
          }

          this.updateRankList(id, totalSeconds, timeText);
        }
      }
      this.setData({ receiveBuffer: msgs[msgs.length - 1] }); 
    }
  },

  // === 刷新排行榜 ===
  updateRankList: function(id, totalSeconds, timeText) {
    let list = this.data.rankList;
    let index = list.findIndex(item => item.id === id);
    
    if (index > -1) {
      list[index].total = totalSeconds;
      list[index].timeText = timeText;
    } else {
      list.push({ id: id, name: '员工 0' + id, total: totalSeconds, timeText: timeText });
    }

    list.sort((a, b) => b.total - a.total);

    list.forEach((item, i) => {
      if(i === 0) item.badge = '🥇';
      else if(i === 1) item.badge = '🥈';
      else if(i === 2) item.badge = '🥉';
      else item.badge = '🔹';
    });

    this.setData({ rankList: list });
  },

  // === 同步时间 ===
  syncTime: function() {
    if (!this.data.writeCharId) {
      wx.showToast({ title: '写入通道未就绪', icon: 'none' });
      return;
    }
    let n = new Date();
    let year = n.getFullYear();
    let mon = (n.getMonth() + 1).toString().padStart(2, '0');
    let day = n.getDate().toString().padStart(2, '0');
    let hour = n.getHours().toString().padStart(2, '0');
    let min = n.getMinutes().toString().padStart(2, '0');
    let sec = n.getSeconds().toString().padStart(2, '0');
    
    let str = 'SET_TIME=' + year + '-' + mon + '-' + day + ' ' + hour + ':' + min + ':' + sec + '\n';
    
    let part1 = str.substring(0, 16);
    let part2 = str.substring(16);
    
    const send = (data) => {
      let buf = new ArrayBuffer(data.length);
      let dv = new DataView(buf);
      for (let i = 0; i < data.length; i++) dv.setUint8(i, data.charCodeAt(i));
      return new Promise((resolve) => {
        wx.writeBLECharacteristicValue({
          deviceId: this.data.deviceId, serviceId: this.data.serviceId, characteristicId: this.data.writeCharId, value: buf, success: resolve
        });
      });
    };
    send(part1).then(() => { setTimeout(() => { send(part2).then(() => { wx.showToast({ title: '同步完成' }); }); }, 150); });
  },

  // === 向单片机索要排行榜数据 ===
  requestRankData: function() {
    if (!this.data.writeCharId) {
      wx.showToast({ title: '写入通道未就绪', icon: 'none' });
      return;
    }
    
    wx.showLoading({ title: '正在呼叫打卡机...' });
    
    let cmd = "GET_RANK\n"; 
    let buf = new ArrayBuffer(cmd.length);
    let dv = new DataView(buf);
    for (let i = 0; i < cmd.length; i++) {
      dv.setUint8(i, cmd.charCodeAt(i));
    }
    
    wx.writeBLECharacteristicValue({
      deviceId: this.data.deviceId,
      serviceId: this.data.serviceId,
      characteristicId: this.data.writeCharId,
      value: buf,
      success: () => {
        setTimeout(() => { wx.hideLoading(); }, 1000);
      },
      fail: (e) => {
        wx.hideLoading();
        wx.showToast({ title: '呼叫失败', icon: 'none' });
        console.error(e);
      }
    });
  }
});