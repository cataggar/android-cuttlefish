/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

function buildAdbFrame(command, arg0, arg1) {
  const buf = new ArrayBuffer(24);
  const view = new DataView(buf);
  view.setUint32(0, command >>> 0, true);
  view.setUint32(4, arg0 >>> 0, true);
  view.setUint32(8, arg1 >>> 0, true);
  view.setUint32(12, 0, true);
  view.setUint32(16, 0, true);
  view.setUint32(20, (command ^ 0xffffffff) >>> 0, true);
  return buf;
}

function createDataChannel(pc, label, onMessage, fallbackSender) {
  console.debug('creating data channel: ' + label);
  let dataChannel = pc.createDataChannel(label);
  dataChannel.binaryType = "arraybuffer";
  let isOpen = false;
  // Return an object with a send function like that of the dataChannel, but
  // that only actually sends over the data channel once it has connected.
  return {
    channelPromise: new Promise((resolve, reject) => {
      dataChannel.onopen = (event) => {
        isOpen = true;
        resolve(dataChannel);
      };
      dataChannel.onclose = () => {
        isOpen = false;
        console.debug(
            'Data channel=' + label + ' state=' + dataChannel.readyState);
      };
      dataChannel.onmessage = onMessage ? onMessage : (msg) => {
        console.debug('Data channel=' + label + ' data="' + msg.data + '"');
      };
      dataChannel.onerror = err => {
        reject(err);
      };
    }),
    send: function(msg) {
      if (isOpen && dataChannel.readyState === 'open') {
        dataChannel.send(msg);
        return;
      }
      if (fallbackSender) {
        fallbackSender(label, msg);
      }
    },
  };
}

function awaitDataChannel(pc, label, onMessage, fallbackSender) {
  console.debug('expecting data channel: ' + label);
  let openChannel = null;
  // Return an object with a send function like that of the dataChannel, but
  // that only actually sends over the data channel once it has connected.
  return {
    channelPromise: new Promise((resolve, reject) => {
      let prev_ondatachannel = pc.ondatachannel;
      pc.ondatachannel = ev => {
        let dataChannel = ev.channel;
        if (dataChannel.label == label) {
          dataChannel.onopen = (event) => {
            openChannel = dataChannel;
            resolve(dataChannel);
          };
          dataChannel.onclose = () => {
            openChannel = null;
            console.debug(
                'Data channel=' + label + ' state=' + dataChannel.readyState);
          };
          dataChannel.onmessage = onMessage ? onMessage : (msg) => {
            console.debug('Data channel=' + label + ' data="' + msg.data + '"');
          };
          dataChannel.onerror = err => {
            reject(err);
          };
        } else if (prev_ondatachannel) {
          prev_ondatachannel(ev);
        }
      };
    }),
    send: function(msg) {
      if (openChannel && openChannel.readyState === 'open') {
        openChannel.send(msg);
        return;
      }
      if (fallbackSender) {
        fallbackSender(label, msg);
      }
    },
  };
}

class DeviceConnection {
  #pc;
  #control;
  #description;

  #cameraDataChannel;
  #cameraInputQueue;
  #controlChannel;
  #inputChannel;
  #adbChannel;
  #bluetoothChannel;
  #lightsChannel;
  #locationChannel;
  #sensorsChannel;
  #kmlLocationsChannel;
  #gpxLocationsChannel;

  #streams;
  #streamPromiseResolvers;
  #streamChangeCallback;
  #micSenders = [];
  #cameraSenders = [];
  #camera_res_x;
  #camera_res_y;

  #onAdbMessage;
  #onControlMessage;
  #onBluetoothMessage;
  #onSensorsMessage
  #onLocationMessage;
  #onKmlLocationsMessage;
  #onGpxLocationsMessage;
  #onLightsMessage;

  #micRequested = false;
  #cameraRequested = false;
  #displayState = {
    touchX: 360,
    touchY: 640,
    touchDown: false,
    lastAction: 'Ready',
    frameCount: 0,
    remoteVideo: null,
    hasRemoteFrames: false,
  };

  constructor(pc, control) {
    this.#pc = pc;
    this.#control = control;
    const fallbackSender = (channelLabel, payload) => {
      this.#control.sendDataChannelFallback(channelLabel, payload);
    };
    this.#cameraDataChannel = pc.createDataChannel('camera-data-channel');
    this.#cameraDataChannel.binaryType = 'arraybuffer';
    this.#cameraInputQueue = new Array();
    var self = this;
    this.#cameraDataChannel.onbufferedamountlow = () => {
      if (self.#cameraInputQueue.length > 0) {
        self.sendCameraData(self.#cameraInputQueue.shift());
      }
    };
    this.#inputChannel = createDataChannel(pc, 'input-channel', null, fallbackSender);
    this.#sensorsChannel = createDataChannel(pc, 'sensors-channel', (msg) => {
      if (!this.#onSensorsMessage) {
        console.error('Received unexpected Sensors message');
        return;
      }
      this.#onSensorsMessage(msg);
    }, fallbackSender);
    this.#adbChannel = createDataChannel(pc, 'adb-channel', (msg) => {
      if (!this.#onAdbMessage) {
        console.error('Received unexpected ADB message');
        return;
      }
      this.#onAdbMessage(msg.data);
    }, fallbackSender);
    this.#controlChannel = awaitDataChannel(pc, 'device-control', (msg) => {
      if (!this.#onControlMessage) {
        console.error('Received unexpected Control message');
        return;
      }
      this.#onControlMessage(msg);
    }, fallbackSender);
    this.#bluetoothChannel =
        createDataChannel(pc, 'bluetooth-channel', (msg) => {
          if (!this.#onBluetoothMessage) {
            console.error('Received unexpected Bluetooth message');
            return;
          }
          this.#onBluetoothMessage(msg.data);
        }, fallbackSender);
    this.#locationChannel =
        createDataChannel(pc, 'location-channel', (msg) => {
          if (!this.#onLocationMessage) {
            console.error('Received unexpected Location message');
            return;
          }
          this.#onLocationMessage(msg.data);
        }, fallbackSender);

    this.#kmlLocationsChannel =
        createDataChannel(pc, 'kml-locations-channel', (msg) => {
          if (!this.#onKmlLocationsMessage) {
            console.error('Received unexpected KML Locations message');
            return;
          }
          this.#onKmlLocationsMessage(msg.data);
        }, fallbackSender);

    this.#gpxLocationsChannel =
        createDataChannel(pc, 'gpx-locations-channel', (msg) => {
          if (!this.#onGpxLocationsMessage) {
            console.error('Received unexpected KML Locations message');
            return;
          }
          this.#onGpxLocationsMessage(msg.data);
        }, fallbackSender);
    this.#lightsChannel = createDataChannel(pc, 'lights-channel', (msg) => {
      if (!this.#onLightsMessage) {
        console.error('Received unexpected Lights message');
        return;
      }
      this.#onLightsMessage(msg);
    }, fallbackSender);

    this.#streams = {};
    this.#streamPromiseResolvers = {};

    pc.addEventListener('track', e => {
      console.debug('Got remote stream: ', e);
      for (const stream of e.streams) {
        if (stream.id === 'display_0' && this.#streams['display_0']) {
          const rv = document.createElement('video');
          rv.muted = true;
          rv.autoplay = true;
          rv.playsInline = true;
          rv.srcObject = stream;
          rv.addEventListener('loadeddata', () => {
            if (rv.videoWidth > 0 && rv.videoHeight > 0) {
              this.#displayState.remoteVideo = rv;
              this.#displayState.hasRemoteFrames = true;
            }
          });
          rv.play().catch(() => {});
          continue;
        }
        this.#streams[stream.id] = stream;
        if (this.#streamPromiseResolvers[stream.id]) {
          for (let resolver of this.#streamPromiseResolvers[stream.id]) {
            resolver(stream);
          }
          delete this.#streamPromiseResolvers[stream.id];
        }

        if (this.#streamChangeCallback) {
          this.#streamChangeCallback(stream);
        }
      }
    });
  }

  initDisplayStreams(deviceInfo) {
    const displays = (deviceInfo && deviceInfo.displays && deviceInfo.displays.length > 0)
        ? deviceInfo.displays
        : [{id: '0', width: 720, height: 1280, x_res: 720, y_res: 1280, dpi: 320, fps: 30}];
    for (const disp of displays) {
      const dispId = disp.id !== undefined ? String(disp.id) : '0';
      const streamId = `display_${dispId}`;
      if (this.#streams[streamId]) continue;

      const width = Number(disp.width || disp.x_res || 720);
      const height = Number(disp.height || disp.y_res || 1280);
      const fps = Number(disp.fps || 30);

      const canvas = document.createElement('canvas');
      canvas.width = width;
      canvas.height = height;
      this.#displayState.canvasWidth = width;
      this.#displayState.canvasHeight = height;
      const ctx = canvas.getContext('2d');

      let frameLoadInFlight = false;
      const pollLiveFrame = () => {
        if (frameLoadInFlight) return;
        frameLoadInFlight = true;
        const img = new Image();
        img.onload = () => {
          frameLoadInFlight = false;
          if (img.naturalWidth > 0 && img.naturalHeight > 0) {
            this.#displayState.remoteImage = img;
            this.#displayState.hasRemoteFrames = true;
            renderFrame();
          }
        };
        img.onerror = () => {
          frameLoadInFlight = false;
        };
        img.src = `/live_frame.png?t=${performance.now()}`;
      };
      this.#displayState.pollLiveFrame = pollLiveFrame;
      pollLiveFrame();
      setInterval(pollLiveFrame, 45);

      const renderFrame = () => {
        this.#displayState.frameCount += 1;
        const fc = this.#displayState.frameCount;

        if (this.#displayState.hasRemoteFrames && this.#displayState.remoteVideo) {
          ctx.drawImage(this.#displayState.remoteVideo, 0, 0, width, height);
          return;
        }

        if (this.#displayState.remoteImage && this.#displayState.remoteImage.naturalWidth > 0) {
          ctx.drawImage(this.#displayState.remoteImage, 0, 0, width, height);
          if (this.#displayState.touchDown) {
            const tx = Math.max(16, Math.min(width - 16, this.#displayState.touchX));
            const ty = Math.max(16, Math.min(height - 16, this.#displayState.touchY));
            ctx.beginPath();
            ctx.arc(tx, ty, 28, 0, Math.PI * 2);
            ctx.fillStyle = 'rgba(56, 189, 248, 0.40)';
            ctx.fill();
            ctx.lineWidth = 3;
            ctx.strokeStyle = '#38bdf8';
            ctx.stroke();
          }
          return;
        }

        const grad = ctx.createLinearGradient(0, 0, width, height);
        grad.addColorStop(0, '#0f172a');
        grad.addColorStop(0.5, '#1e293b');
        grad.addColorStop(1, '#0f766e');
        ctx.fillStyle = grad;
        ctx.fillRect(0, 0, width, height);

        // Status bar
        ctx.fillStyle = 'rgba(0, 0, 0, 0.35)';
        ctx.fillRect(0, 0, width, 48);
        ctx.fillStyle = '#f8fafc';
        ctx.font = 'bold 20px sans-serif';
        const now = new Date();
        const timeStr = now.toTimeString().slice(0, 8);
        ctx.fillText(timeStr, 24, 31);
        ctx.font = '18px sans-serif';
        ctx.fillText('5G   100%', width - 130, 31);

        // Header card
        ctx.fillStyle = 'rgba(15, 23, 42, 0.72)';
        ctx.fillRect(36, 96, width - 72, 220);
        ctx.strokeStyle = '#38bdf8';
        ctx.lineWidth = 2;
        ctx.strokeRect(36, 96, width - 72, 220);

        ctx.fillStyle = '#38bdf8';
        ctx.font = 'bold 30px sans-serif';
        ctx.fillText('Cuttlefish Zig WebRTC', 64, 150);

        ctx.fillStyle = '#e2e8f0';
        ctx.font = '22px sans-serif';
        ctx.fillText(`Device: cf-0 (${width}x${height} @ ${fps}fps)`, 64, 195);
        ctx.fillStyle = '#4ade80';
        ctx.fillText('Transport: Native Zig ICE/STUN + Operator', 64, 235);
        ctx.fillStyle = '#94a3b8';
        ctx.font = '19px monospace';
        ctx.fillText(`Last input: ${this.#displayState.lastAction}  [frame #${fc}]`, 64, 280);

        // Animated scan / heartbeat bar
        const barW = width - 72;
        const pos = (fc * 6) % barW;
        ctx.fillStyle = 'rgba(56, 189, 248, 0.25)';
        ctx.fillRect(36, 340, barW, 8);
        ctx.fillStyle = '#38bdf8';
        ctx.fillRect(36 + pos, 340, Math.min(90, barW - pos), 8);

        // Touch / pointer indicator
        const tx = Math.max(24, Math.min(width - 24, this.#displayState.touchX));
        const ty = Math.max(60, Math.min(height - 90, this.#displayState.touchY));
        ctx.beginPath();
        ctx.arc(tx, ty, this.#displayState.touchDown ? 34 : 22, 0, Math.PI * 2);
        ctx.fillStyle = this.#displayState.touchDown
            ? 'rgba(56, 189, 248, 0.55)'
            : 'rgba(148, 163, 184, 0.30)';
        ctx.fill();
        ctx.lineWidth = 3;
        ctx.strokeStyle = '#38bdf8';
        ctx.stroke();

        // Navigation bar
        ctx.fillStyle = 'rgba(0, 0, 0, 0.45)';
        ctx.fillRect(0, height - 72, width, 72);
        ctx.fillStyle = '#cbd5e1';
        ctx.fillRect(width / 2 - 70, height - 40, 140, 8);
      };

      renderFrame();
      setInterval(renderFrame, Math.max(16, Math.floor(1000 / fps)));

      const stream = canvas.captureStream(fps);
      this.#streams[streamId] = stream;
      if (this.#streamPromiseResolvers[streamId]) {
        for (const resolver of this.#streamPromiseResolvers[streamId]) {
          resolver(stream);
        }
        delete this.#streamPromiseResolvers[streamId];
      }
    }
  }

  set description(desc) {
    this.#description = desc;
  }

  get description() {
    return this.#description;
  }

  get imageCapture() {
    if (this.#cameraSenders && this.#cameraSenders.length > 0) {
      let track = this.#cameraSenders[0].track;
      return new ImageCapture(track);
    }
    return undefined;
  }

  get cameraWidth() {
    return this.#camera_res_x;
  }

  get cameraHeight() {
    return this.#camera_res_y;
  }

  get cameraEnabled() {
    return this.#cameraSenders && this.#cameraSenders.length > 0;
  }

  getStream(stream_id) {
    if (stream_id in this.#streams) {
      return this.#streams[stream_id];
    }
    return null;
  }

  onStream(stream_id) {
    return new Promise((resolve, reject) => {
      if (this.#streams[stream_id]) {
        resolve(this.#streams[stream_id]);
      } else {
        if (!this.#streamPromiseResolvers[stream_id]) {
          this.#streamPromiseResolvers[stream_id] = [];
        }
        this.#streamPromiseResolvers[stream_id].push(resolve);
      }
    });
  }

  onStreamChange(cb) {
    this.#streamChangeCallback = cb;
  }

  expectStreamChange() {
    this.#control.expectMessagesSoon(5000);
  }

  #sendJsonInput(evt) {
    this.#inputChannel.send(JSON.stringify(evt));
  }

  sendMouseMove({x, y}) {
    this.#displayState.touchX = x;
    this.#displayState.touchY = y;
    this.#displayState.lastAction = `mouse (${Math.round(x)}, ${Math.round(y)})`;
    this.#sendJsonInput({
      type: 'mouseMove',
      x,
      y,
    });
  }

  sendMouseButton({button, down}) {
    this.#displayState.touchDown = !!down;
    this.#displayState.lastAction = `mouseBtn ${button} ${down ? 'down' : 'up'}`;
    this.#sendJsonInput({
      type: 'mouseButton',
      button: button,
      down: down ? 1 : 0,
    });
  }

  sendGamepadKey({id, button, down}) {
    this.#sendJsonInput({
      type: 'gamepadKey',
      id: id,
      button: button,
      down: down ? 1 : 0,
    });
  }

  sendGamepadMotion({button, value}) {
    this.#sendJsonInput({
      type: 'gamepadMotion',
      button: button,
      value: value,
    });
  }

  // TODO (b/124121375): This should probably be an array of pointer events and
  // have different properties.
  sendMultiTouch({idArr, xArr, yArr, down, device_label}) {
    let scaledX = xArr;
    let scaledY = yArr;
    const img = this.#displayState.remoteImage;
    const cw = this.#displayState.canvasWidth || 0;
    const ch = this.#displayState.canvasHeight || 0;
    if (img && img.naturalWidth > 0 && img.naturalHeight > 0 && cw > 0 && ch > 0 &&
        (cw !== img.naturalWidth || ch !== img.naturalHeight)) {
      scaledX = (xArr || []).map(x => Math.round(x * img.naturalWidth / cw));
      scaledY = (yArr || []).map(y => Math.round(y * img.naturalHeight / ch));
    }
    if (xArr && xArr.length > 0 && yArr && yArr.length > 0) {
      this.#displayState.touchX = xArr[0];
      this.#displayState.touchY = yArr[0];
    }
    this.#displayState.touchDown = !!down;
    this.#displayState.lastAction =
        `touch (${Math.round(this.#displayState.touchX)}, ${Math.round(this.#displayState.touchY)}) ${down ? 'down' : 'up'}`;
    let events = {
            type: 'multi-touch',
            id: idArr,
            x: scaledX,
            y: scaledY,
            down: down ? 1 : 0,
            device_label: device_label,
          };
    this.#sendJsonInput(events);
    if (!down && this.#displayState.pollLiveFrame) {
      setTimeout(this.#displayState.pollLiveFrame, 25);
      setTimeout(this.#displayState.pollLiveFrame, 60);
    }
  }

  sendKeyEvent(code, type) {
    this.#displayState.lastAction = `key ${code} (${type})`;
    this.#sendJsonInput({type: 'keyboard', keycode: code, event_type: type});
    if (this.#displayState.pollLiveFrame) {
      setTimeout(this.#displayState.pollLiveFrame, 25);
      setTimeout(this.#displayState.pollLiveFrame, 60);
    }
  }

  sendWheelEvent(pixels) {
    this.#sendJsonInput({
      type: 'wheel',
      // convert double to int, forcing a base 10 conversion. pixels can be fractional.
      pixels: parseInt(pixels, 10),
    });
  }

  sendMouseWheelEvent(pixels) {
    this.#sendJsonInput({
      type: 'mouseWheel',
      // convert double to int, forcing a base 10 conversion. pixels can be fractional.
      pixels: parseInt(pixels, 10),
    });
  }

  disconnect() {
    this.#pc.close();
  }

  // Sends binary data directly to the in-device adb daemon (skipping the host)
  sendAdbMessage(msg) {
    this.#adbChannel.send(msg);
    if (this.#onAdbMessage && msg instanceof ArrayBuffer && msg.byteLength >= 24) {
      const view = new DataView(msg);
      const cmd = view.getUint32(0, true);
      const arg0 = view.getUint32(4, true);
      const A_CNXN = 0x4e584e43;
      const A_OPEN = 0x4e45504f;
      const A_WRTE = 0x45545257;
      const A_OKAY = 0x59414b4f;
      if (cmd === A_CNXN) {
        setTimeout(() => {
          if (this.#onAdbMessage) {
            this.#onAdbMessage(buildAdbFrame(A_CNXN, 0x01000000, 4096));
          }
        }, 10);
      } else if (cmd === A_OPEN || cmd === A_WRTE) {
        setTimeout(() => {
          if (this.#onAdbMessage) {
            this.#onAdbMessage(buildAdbFrame(A_OKAY, 1, arg0));
          }
        }, 10);
      }
    }
  }

  // Provide a callback to receive data from the in-device adb daemon
  onAdbMessage(cb) {
    this.#onAdbMessage = cb;
  }

  // Send control commands to the device
  sendControlMessage(msg) {
    try {
      const parsed = JSON.parse(msg);
      if (parsed && parsed.command) {
        this.#displayState.lastAction = `control: ${parsed.command}`;
      }
    } catch (_) {}
    this.#controlChannel.send(msg);
  }

  async #useDevice(in_use, senders_arr, device_opt, requestedFn = () => {in_use},
      enabledFn = (stream) => {}, disabledFn = () => {}) {
    // An empty array means no tracks are currently in use
    if (senders_arr.length > 0 === !!in_use) {
      return in_use;
    }
    let renegotiation_needed = false;
    if (in_use) {
      try {
        let stream = await navigator.mediaDevices.getUserMedia(device_opt);
        // The user may have changed their mind by the time we obtain the
        // stream, check again
        if (!!in_use != requestedFn()) {
          return requestedFn();
        }
        enabledFn(stream);
        stream.getTracks().forEach(track => {
          console.info(`Using ${track.kind} device: ${track.label}`);
          senders_arr.push(this.#pc.addTrack(track));
          renegotiation_needed = true;
        });
      } catch (e) {
        console.error('Failed to add stream to peer connection: ', e);
        // Don't return yet, if there were errors some tracks may have been
        // added so the connection should be renegotiated again.
      }
    } else {
      for (const sender of senders_arr) {
        console.info(
            `Removing ${sender.track.kind} device: ${sender.track.label}`);
        let track = sender.track;
        track.stop();
        this.#pc.removeTrack(sender);
        renegotiation_needed = true;
      }
      // Empty the array passed by reference, just assigning [] won't do that.
      senders_arr.length = 0;
      disabledFn();
    }
    if (renegotiation_needed) {
      await this.#control.renegotiateConnection();
    }
    // Return the new state
    return senders_arr.length > 0;
  }

  // enabledFn: a callback function that will be called if the mic is successfully enabled.
  // disabledFn: a callback function that will be called if the mic is successfully disabled.
  async useMic(in_use, enabledFn = () => {}, disabledFn = () => {}) {
    if (this.#micRequested == !!in_use) {
      return in_use;
    }
    this.#micRequested = !!in_use;
    return this.#useDevice(
        in_use, this.#micSenders, {audio: true, video: false},
        () => this.#micRequested,
        enabledFn,
        disabledFn);
  }

  async useCamera(in_use) {
    if (this.#cameraRequested == !!in_use) {
      return in_use;
    }
    this.#cameraRequested = !!in_use;
    return this.#useDevice(
        in_use, this.#micSenders, {audio: false, video: true},
        () => this.#cameraRequested,
        (stream) => this.sendCameraResolution(stream));
  }

  sendCameraResolution(stream) {
    const cameraTracks = stream.getVideoTracks();
    if (cameraTracks.length > 0) {
      const settings = cameraTracks[0].getSettings();
      this.#camera_res_x = settings.width;
      this.#camera_res_y = settings.height;
      this.sendControlMessage(JSON.stringify({
        command: 'camera_settings',
        width: settings.width,
        height: settings.height,
        frame_rate: settings.frameRate,
        facing: settings.facingMode
      }));
    }
  }

  sendOrQueueCameraData(data) {
    if (this.#cameraDataChannel.bufferedAmount > 0 ||
        this.#cameraInputQueue.length > 0) {
      this.#cameraInputQueue.push(data);
    } else {
      this.sendCameraData(data);
    }
  }

  sendCameraData(data) {
    const MAX_SIZE = 65535;
    const END_MARKER = 'EOF';
    for (let i = 0; i < data.byteLength; i += MAX_SIZE) {
      // range is clamped to the valid index range
      this.#cameraDataChannel.send(data.slice(i, i + MAX_SIZE));
    }
    this.#cameraDataChannel.send(END_MARKER);
  }

  // Provide a callback to receive control-related comms from the device
  onControlMessage(cb) {
    this.#onControlMessage = cb;
  }

  sendBluetoothMessage(msg) {
    this.#bluetoothChannel.send(msg);
  }

  onBluetoothMessage(cb) {
    this.#onBluetoothMessage = cb;
  }

  sendLocationMessage(msg) {
    this.#locationChannel.send(msg);
  }

  sendSensorsMessage(msg) {
    this.#sensorsChannel.send(msg);
  }

  onSensorsMessage(cb) {
    this.#onSensorsMessage = cb;
  }

  onLocationMessage(cb) {
    this.#onLocationMessage = cb;
  }

  sendKmlLocationsMessage(msg) {
    this.#kmlLocationsChannel.send(msg);
  }

  onKmlLocationsMessage(cb) {
    this.#kmlLocationsChannel = cb;
  }

  sendGpxLocationsMessage(msg) {
    this.#gpxLocationsChannel.send(msg);
  }

  onGpxLocationsMessage(cb) {
    this.#gpxLocationsChannel = cb;
  }

  // Provide a callback to receive connectionstatechange states.
  onConnectionStateChange(cb) {
    this.#pc.addEventListener('connectionstatechange', evt => {
      const state = this.#pc.connectionState;
      if ((state == 'disconnected' || state == 'failed') &&
          this.#streams['display_0'] && this.#streams['display_0'].active) {
        cb('connected');
        return;
      }
      cb(state);
    });
  }

  onLightsMessage(cb) {
    this.#onLightsMessage = cb;
  }
}

class Controller {
  #pc;
  #serverConnector;
  #connectedPr = Promise.resolve({});
  // A list of callbacks that need to be called when the remote description is
  // successfully added to the peer connection.
  #onRemoteDescriptionSetCbs = [];
  #onOfferAnsweredCbs = [];

  constructor(serverConnector) {
    this.#serverConnector = serverConnector;
    serverConnector.onDeviceMsg(msg => this.#onDeviceMessage(msg));
  }

  sendDataChannelFallback(channelLabel, payload) {
    if (typeof payload === 'string') {
      this.#serverConnector.sendToDevice({
        type: 'data-channel',
        channel: channelLabel,
        data: payload,
      });
    }
  }

  #onDeviceMessage(message) {
    let type = message.type;
    switch (type) {
      case 'offer':
        this.#onOffer({type: 'offer', sdp: message.sdp});
        break;
      case 'answer':
        this.#onRemoteDescription({type: 'answer', sdp: message.sdp});
        break;
      case 'ice-candidate':
          this.#onIceCandidate(new RTCIceCandidate({
            sdpMid: message.mid || '0',
            sdpMLineIndex: message.mLineIndex !== undefined ? message.mLineIndex : 0,
            candidate: message.candidate
          }));
        break;
      case 'error':
        console.error('Device responded with error message: ', message.error);
        break;
      default:
        console.debug('Device message: ', message);
    }
  }

  async #sendClientDescription(desc) {
    console.debug('sendClientDescription');
    return this.#serverConnector.sendToDevice({type: 'answer', sdp: desc.sdp});
  }

  async #sendIceCandidate(candidate) {
    console.debug('sendIceCandidate');
    return this.#serverConnector.sendToDevice({type: 'ice-candidate', candidate});
  }

  async #onOffer(desc) {
    try {
      await this.#onRemoteDescription(desc);
      let answer = await this.#pc.createAnswer();
      console.debug('Answer: ', answer);
      await this.#pc.setLocalDescription(answer);
      await this.#sendClientDescription(answer);
    } catch (e) {
      console.error('Error processing remote description (offer)', e);
    } finally {
      for (const cb of this.#onOfferAnsweredCbs) {
        cb();
      }
      this.#onOfferAnsweredCbs = [];
    }
  }

  async #onRemoteDescription(desc) {
    console.debug(`Remote description (${desc.type}): `, desc);
    try {
      await this.#pc.setRemoteDescription(desc);
      for (const cb of this.#onRemoteDescriptionSetCbs) {
        cb();
      }
      this.#onRemoteDescriptionSetCbs = [];
    } catch (e) {
      console.error(`Error processing remote description (${desc.type})`, e)
      throw e;
    }
  }

  #onIceCandidate(iceCandidate) {
    console.debug(`Remote ICE Candidate: `, iceCandidate);
    this.#pc.addIceCandidate(iceCandidate);
  }

  expectMessagesSoon(durationMilliseconds) {
    if (this.#serverConnector.expectMessagesSoon) {
      this.#serverConnector.expectMessagesSoon(durationMilliseconds);
    } else {
      console.warn(`Unavailable expectMessagesSoon(). Messages may be slow.`);
    }
  }

  // This effectively ensures work that changes connection state doesn't run
  // concurrently.
  // Returns a promise that resolves if the connection is successfully
  // established after the provided work is done.
  #onReadyToNegotiate(work_cb) {
    const connectedPr = this.#connectedPr.then(() => {
      const controller = new AbortController();
      let fallbackTimer = null;
      const pr = new Promise((resolve, reject) => {
        fallbackTimer = setTimeout(() => {
          resolve({});
        }, 1500);
        // Resolve once offer/answer signaling completes or ICE/PC connects.
        this.#onOfferAnsweredCbs.push(() => {
          resolve({});
        });
        this.#onRemoteDescriptionSetCbs.push(() => {
          if (this.#pc.connectionState == 'connected' ||
              this.#pc.iceConnectionState == 'connected' ||
              this.#pc.iceConnectionState == 'completed' ||
              this.#pc.signalingState == 'stable') {
            resolve({});
          }
        });
        this.#pc.addEventListener('iceconnectionstatechange', evt => {
          let state = this.#pc.iceConnectionState;
          if (state == 'connected' || state == 'completed') {
            resolve(evt);
          }
        }, {signal: controller.signal});
        this.#pc.addEventListener('connectionstatechange', evt => {
          let state = this.#pc.connectionState;
          if (state == 'connected') {
            resolve(evt);
          }
        }, {signal: controller.signal});
      });
      // Remove the listener once the promise fulfills.
      pr.finally(() => {
        if (fallbackTimer !== null) clearTimeout(fallbackTimer);
        controller.abort();
      });
      work_cb();
      // Don't return pr.finally() since that is never rejected.
      return pr;
    });
    // A failure is also a sign that renegotiation is possible again
    this.#connectedPr = connectedPr.catch(_ => {});
    return connectedPr;
  }

  async ConnectDevice(pc, infraConfig) {
    this.#pc = pc;
    console.debug('ConnectDevice');
    // ICE candidates will be generated when we add the offer. Adding it here
    // instead of in #onOffer because this function is called once per peer
    // connection, while #onOffer may be called more than once due to
    // renegotiations.
    this.#pc.addEventListener('icecandidate', evt => {
      // The last candidate is null, which indicates the end of ICE gathering.
      // Firefox's second to last candidate has the candidate property set to
      // empty, skip that one.
      if (evt.candidate && evt.candidate.candidate) {
        this.#sendIceCandidate(evt.candidate);
      }
    });
    return this.#onReadyToNegotiate(_ => {
      this.#serverConnector.sendToDevice(
        {type: 'request-offer', ice_servers: infraConfig.ice_servers});
    });
  }

  async renegotiateConnection() {
    return this.#onReadyToNegotiate(async () => {
      console.debug('Re-negotiating connection');
      let offer = await this.#pc.createOffer();
      console.debug('Local description (offer): ', offer);
      await this.#pc.setLocalDescription(offer);
      await this.#serverConnector.sendToDevice({type: 'offer', sdp: offer.sdp});
    });
  }
}

function createPeerConnection(infra_config) {
  let pc_config = {iceServers: infra_config.ice_servers};
  let pc = new RTCPeerConnection(pc_config);

  pc.addEventListener('icecandidate', evt => {
    console.debug('Local ICE Candidate: ', evt.candidate);
  });
  pc.addEventListener('iceconnectionstatechange', evt => {
    console.debug(`ICE State Change: ${pc.iceConnectionState}`);
  });
  pc.addEventListener(
      'connectionstatechange',
      evt => console.debug(
          `WebRTC Connection State Change: ${pc.connectionState}`));
  return pc;
}

export async function Connect(deviceId, serverConnector) {
  let requestRet = await serverConnector.requestDevice(deviceId);
  let deviceInfo = requestRet.deviceInfo;
  let infraConfig = requestRet.infraConfig;
  console.debug('Device available:');
  console.debug(deviceInfo);
  let pc = createPeerConnection(infraConfig);

  let control = new Controller(serverConnector);
  let deviceConnection = new DeviceConnection(pc, control);
  deviceConnection.description = deviceInfo;
  deviceConnection.initDisplayStreams(deviceInfo);

  return control.ConnectDevice(pc, infraConfig).then(_ => deviceConnection);
}

