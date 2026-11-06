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

'use strict';

function trackMouseEvents(dc, mouseElement) {
  let mouseMode = false;

  function setMode(active) {
    mouseMode = !!active;
    mouseElement.classList.toggle('active', mouseMode);
    if (!mouseMode && document.pointerLockElement === mouseElement) {
      document.exitPointerLock();
    }
    if (typeof dc.setMouseMode === 'function') {
      dc.setMouseMode(mouseMode);
    }
    const videos = document.querySelectorAll('.device-display-video');
    videos.forEach(v => {
      v.style.cursor = mouseMode ? 'none' : '';
    });
  }

  function onMouseDown(evt) {
    evt.preventDefault();
    setMode(!mouseMode);
  }

  document.addEventListener('keydown', evt => {
    if (evt.key === 'Escape' && mouseMode) {
      setMode(false);
    }
  });

  mouseElement.addEventListener('mousedown', onMouseDown);
  mouseElement.addEventListener('contextmenu', evt => evt.preventDefault());
}

function enableMouseButton(dc) {
  function onMouseKey(evt) {
    if (evt.cancelable) {
      // Some keyboard events cause unwanted side effects, like elements losing
      // focus, if the default behavior is not prevented.
      evt.preventDefault();
    }
    dc.sendKeyEvent(evt.code, evt.type);
  }
  function onMouseWheel(evt) {
    evt.preventDefault();
    // Vertical wheel pixel events only
    if (evt.deltaMode == WheelEvent.DOM_DELTA_PIXEL && evt.deltaY != 0.0) {
      dc.sendMouseWheelEvent(evt.deltaY);
    }
  }
  let button = document.getElementById("mouse_btn");
  button.style.display = "";
  button.disabled = false;
  trackMouseEvents(dc, button);
  button.addEventListener('keydown', onMouseKey);
  button.addEventListener('keyup', onMouseKey);
  button.addEventListener('wheel', onMouseWheel,
                                { passive: false });
  return button;
}

