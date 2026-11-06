/*
 * Copyright (C) 2021 The Android Open Source Project
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

'use strict';

export function deviceId() {
  const params = new URLSearchParams(location.search);
  const queryId = params.get('deviceId') || params.get('device_id');
  if (queryId) {
    return queryId;
  }
  const pathElements = location.pathname.split('/');
  const devIdx = pathElements.indexOf('devices');
  if (devIdx >= 0 && devIdx + 1 < pathElements.length && pathElements[devIdx + 1]) {
    return pathElements[devIdx + 1];
  }
  return 'cf0';
}

export async function createConnector() {
  return new OperatorConnector();
}

export function createParentController() {
  return null;
}

function httpUrl(path) {
  return `${location.protocol}//${location.host}/${path}`;
}

async function ajaxPostJson(url, data) {
  const response = await fetch(url, {
    method: 'POST',
    cache: 'no-cache',
    headers: { 'Content-Type': 'application/json' },
    redirect: 'follow',
    body: JSON.stringify(data),
  });
  return response.json();
}

class OperatorConnector {
  #ws = null;
  #connId = null;
  #deviceId = null;
  #pollTimer = null;
  #reconnectTimer = null;
  #onDeviceMsgCb = (msg) =>
    console.error('Received device message without registered listener', msg);

  onDeviceMsg(cb) {
    this.#onDeviceMsgCb = cb;
  }

  async #resolveDeviceId(targetDeviceId) {
    try {
      const resp = await fetch(httpUrl('devices'), { cache: 'no-cache' });
      if (resp.ok) {
        const devices = await resp.json();
        if (Array.isArray(devices) && devices.length > 0) {
          if (targetDeviceId && devices.includes(targetDeviceId)) {
            return targetDeviceId;
          }
          return devices[0];
        }
      }
    } catch (e) {
      console.warn('Unable to query /devices:', e);
    }
    return targetDeviceId || 'cf0';
  }

  async requestDevice(targetDeviceId) {
    const resolvedDeviceId = await this.#resolveDeviceId(targetDeviceId);
    this.#deviceId = resolvedDeviceId;
    if (typeof document !== 'undefined' && document.title === targetDeviceId) {
      document.title = resolvedDeviceId;
    }
    if (typeof WebSocket !== 'undefined') {
      try {
        return await this.#requestViaWebSocket(resolvedDeviceId);
      } catch (err) {
        console.warn('WebSocket /connect_client failed, falling back to HTTP polling:', err);
      }
    }
    return await this.#requestViaPolling(resolvedDeviceId);
  }

  async #requestViaWebSocket(targetDeviceId, isReconnect = false) {
    return new Promise((resolve, reject) => {
      const wsProto = location.protocol === 'https:' ? 'wss:' : 'ws:';
      const wsUrl = `${wsProto}//${location.host}/connect_client`;
      const ws = new WebSocket(wsUrl, ['webrtc-operator']);
      let infraConfig = { ice_servers: [] };
      let resolved = false;

      const timer = setTimeout(() => {
        if (!resolved) {
          ws.close();
          reject(new Error('WebSocket connect timeout'));
        }
      }, 3000);

      ws.onopen = () => {
        ws.send(
          JSON.stringify({
            message_type: 'connect',
            device_id: targetDeviceId,
          })
        );
      };

      ws.onmessage = (evt) => {
        let msg;
        try {
          msg = JSON.parse(evt.data);
        } catch (e) {
          return;
        }
        if (msg.message_type === 'config') {
          infraConfig = msg;
          return;
        }
        if (msg.message_type === 'device_info') {
          clearTimeout(timer);
          resolved = true;
          this.#ws = ws;
          this.#deviceId = targetDeviceId;
          if (isReconnect) {
            try {
              ws.send(
                JSON.stringify({
                  message_type: 'forward',
                  payload: { type: 'request-offer' },
                })
              );
            } catch (_) {}
          }
          resolve({
            deviceInfo: msg.device_info,
            infraConfig: infraConfig,
          });
          return;
        }
        if (msg.error) {
          if (!resolved) {
            clearTimeout(timer);
            reject(new Error(msg.error));
          }
          return;
        }
        this.#onDeviceMsgCb(msg.payload !== undefined ? msg.payload : msg);
      };

      ws.onerror = () => {
        if (!resolved) {
          clearTimeout(timer);
          reject(new Error('WebSocket error'));
        }
      };

      ws.onclose = () => {
        if (this.#ws === ws) {
          this.#ws = null;
        }
        if (!resolved) {
          clearTimeout(timer);
          reject(new Error('WebSocket closed before device_info'));
          return;
        }
        if (this.#reconnectTimer === null) {
          this.#reconnectTimer = setTimeout(() => {
            this.#reconnectTimer = null;
            if (!this.#ws || this.#ws.readyState !== WebSocket.OPEN) {
              this.#requestViaWebSocket(targetDeviceId, true).catch(() => {});
            }
          }, 300);
        }
      };
    });
  }

  async #requestViaPolling(targetDeviceId) {
    const infraConfig = await (await fetch(httpUrl('infra_config'))).json();
    const res = await ajaxPostJson(httpUrl('connect'), {
      device_id: targetDeviceId,
    });
    if (res.error) {
      throw new Error(res.error);
    }
    this.#connId = res.connection_id;
    this.#startPollingLoop();
    return {
      deviceInfo: res.device_info,
      infraConfig: infraConfig,
    };
  }

  #startPollingLoop() {
    if (this.#pollTimer !== null) return;
    const pollFn = async () => {
      try {
        const msgs = await ajaxPostJson(httpUrl('poll_messages'), {
          connection_id: this.#connId,
        });
        if (Array.isArray(msgs)) {
          for (const m of msgs) {
            this.#onDeviceMsgCb(m.payload !== undefined ? m.payload : m);
          }
        }
      } catch (e) {
        console.warn('poll_messages error:', e);
      }
      this.#pollTimer = setTimeout(pollFn, 50);
    };
    this.#pollTimer = setTimeout(pollFn, 50);
  }

  async sendToDevice(msg) {
    if (this.#ws && this.#ws.readyState === WebSocket.OPEN) {
      this.#ws.send(
        JSON.stringify({
          message_type: 'forward',
          payload: msg,
        })
      );
      return;
    }
    const resolvedDeviceId =
      this.#deviceId || (await this.#resolveDeviceId(deviceId()));
    this.#deviceId = resolvedDeviceId;
    if (typeof WebSocket !== 'undefined') {
      try {
        await this.#requestViaWebSocket(resolvedDeviceId, true);
        if (this.#ws && this.#ws.readyState === WebSocket.OPEN) {
          this.#ws.send(
            JSON.stringify({
              message_type: 'forward',
              payload: msg,
            })
          );
          return;
        }
      } catch (_) {}
    }
    if (this.#connId === null) {
      try {
        await this.#requestViaPolling(resolvedDeviceId);
      } catch (e) {
        console.warn('Auto-reconnect polling failed:', e);
        return;
      }
    }
    if (this.#connId !== null) {
      const msgs = await ajaxPostJson(httpUrl('forward'), {
        connection_id: this.#connId,
        payload: msg,
      });
      if (Array.isArray(msgs)) {
        for (const m of msgs) {
          this.#onDeviceMsgCb(m.payload !== undefined ? m.payload : m);
        }
      }
    }
  }

  expectMessagesSoon(_durationMs) {}
}
